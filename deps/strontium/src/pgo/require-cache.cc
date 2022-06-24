#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cwalk.h>
#include <node_buffer.h>
#include <util.h>

#include <base_object-inl.h>
#include <string_bytes.h>
#include <util/common.h>
#include "require-cache.h"

namespace strontium {
namespace PGO {
namespace RequireCache {

using node::StringBytes;

using v8::FunctionCallback;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::MaybeLocal;
using v8::Null;
using v8::String;

#define METACACHE_MAX_SIZE (1024 * 1024 * 100)
#define REQUIRE_CACHE_RELATION_SLOT (0)
#define REQUIRE_CACHE_CONTENT_SLOT (1)
#define REQUIRE_CACHE_INTERNAL_META_SLOT (2)

void DummyFreeCallback(char* buf, void* hint) {
  // do nothing
}

RequireCacheWrap::~RequireCacheWrap() {
  char* ptr;
  for (auto it = alloc_buffer.begin(); it != alloc_buffer.end(); it++) {
    ptr = const_cast<char*>(*it);
    free(ptr);
  }
  alloc_buffer.clear();
}

void RequireCacheWrap::Initialize(Environment* env, Local<Object> target) {
  HandleScope scope(env->isolate());

  Local<FunctionTemplate> t = env->NewFunctionTemplate(New);

  t->InstanceTemplate()->SetInternalFieldCount(
      RequireCacheWrap::kInternalFieldCount);

  env->SetProtoMethod(t, "isLoaded", IsLoaded);

  env->SetProtoMethod(t, "addRelation", AddRelation);
  env->SetProtoMethod(t, "addCode", AddCode);
  env->SetProtoMethod(t, "dump", Dump);

  env->SetProtoMethod(t, "queryCode", QueryCode);
  env->SetProtoMethod(t, "queryRelation", QueryRelation);
  env->SetProtoMethod(t, "getInternalMeta", GetInternalMeta);

  env->SetProtoMethod(t, "transformFilename", TransformFilename);
  env->SetProtoMethod(t, "transformFilenameBack", TransformFilenameBack);

  target
      ->Set(env->context(),
            node::FIXED_ONE_BYTE_STRING(env->isolate(), "RequireCache"),
            t->GetFunction(env->context()).ToLocalChecked())
      .Check();
}

RequireCacheWrap::RequireCacheWrap(Environment* env,
                                   Local<Object> obj,
                                   const char* cache_filename,
                                   Local<Array> entries)
    : BaseObject(env, obj),
      mc_(metacache::load(cache_filename)),
      error_info_(""),
      cache_filename_(cache_filename),
      type_(READER),
      contents_(),
      relationship_(),
      internal_meta_kv_() {
  MakeWeak();

  if (mc_ == nullptr) {
    error_info_ = "MetaCache environment failed to initialize.";
    return;
  }

  // 初始化 Require relation
  void* ptr = const_cast<void*>(mc_->get_slots(REQUIRE_CACHE_RELATION_SLOT));
  if (ptr == nullptr) {
    error_info_ = "failed to get require cache relation slot.";
    return;
  }

  relationship_ = new MetaCacheRelationR(ptr);

  // 初始化 Cache KV
  ptr = const_cast<void*>(mc_->get_slots(REQUIRE_CACHE_CONTENT_SLOT));
  if (ptr == nullptr) {
    delete relationship_.reader_ptr();
    relationship_ = nullptr;
    error_info_ = "failed to get require cache content slot.";
    return;
  }

  contents_ = new MetaCacheFileBufferR(ptr);

  // 初始化 Internal meta
  ptr = const_cast<void*>(mc_->get_slots(REQUIRE_CACHE_INTERNAL_META_SLOT));
  if (ptr != nullptr) {
    // 读文件的时候 Internal meta 可以不存在
    internal_meta_kv_ = new MetaCacheMetaKVR(ptr);
  }

  InitializeEntries(env, entries);
}

RequireCacheWrap::RequireCacheWrap(Environment* env,
                                   Local<Object> obj,
                                   Local<Array> entries)
    : BaseObject(env, obj),
      mc_(metacache::create(METACACHE_MAX_SIZE)),
      cache_filename_(""),
      type_(WRITER) {
  MakeWeak();

  if (mc_ == nullptr) {
    return;
  }

  relationship_ = new MetaCacheRelationW();
  contents_ = new MetaCacheFileBufferW();
  internal_meta_kv_ = new MetaCacheMetaKVW();

  InitializeEntries(env, entries);
}

bool RequireCacheWrap::IsLoaded() {
  return mc_ != nullptr && contents_.ptr() != nullptr &&
         relationship_.ptr() != nullptr;
}

void RequireCacheWrap::AddRelation(const char* requester,
                                   const char* requestee,
                                   const char* dummy_filename) {
  MetaCacheRelationW& mcrw = relationship_.writer();
  mcrw[strontium::metacache::kkeyw(requester, requestee)] = dummy_filename;

#ifdef DEBUG_PRINTF
  printf("W rc_relation[%s, %s] = %s\n", requester, requestee, dummy_filename);
#endif
}

void RequireCacheWrap::AddCode(const char* dummy_filename,
                               const string& source_code,
                               size_t byte_code_length,
                               const char* byte_code) {
  size_t len = sizeof(size_t) +
               (source_code.length() + 1 + byte_code_length) * sizeof(char);

  // + sizeof(size_t): buffer size
  // + byte_code_length * sizeof(char): byte code buffer
  // + (source_code.length() + 1) * sizeof(char): source code

  char* buff = node::Calloc(len);
  memcpy(buff, &byte_code_length, sizeof(size_t));

#ifdef DEBUG_PRINTF
  printf("W rc_code[%s] = code_len: %lu, bytecode_len: %lu\n",
         dummy_filename,
         source_code.length(),
         byte_code_length);
#endif

  char* ptr = reinterpret_cast<char*>((reinterpret_cast<size_t*>(buff) + 1));
  if (byte_code_length > 0) {
    memcpy(ptr, byte_code, byte_code_length * sizeof(char));
  }

  ptr += byte_code_length;
  memcpy(ptr, source_code.c_str(), sizeof(char) * (source_code.length() + 1));

  alloc_buffer.push_back(buff);

  MetaCacheBufW meta_buff(buff, len);
  MetaCacheFileBufferW& mcfbw = contents_.writer();
  mcfbw[dummy_filename] = meta_buff;
}

void RequireCacheWrap::Dump(const char* filename) {
  // 先往 internal meta 写一些固定内容
  WriteInternalMetaKV();

  void* ptr = relationship_.writer_ptr()->makeup();
  mc_->set_slots(REQUIRE_CACHE_RELATION_SLOT, ptr);
  ptr = contents_.writer_ptr()->makeup();
  mc_->set_slots(REQUIRE_CACHE_CONTENT_SLOT, ptr);
  ptr = internal_meta_kv_.writer_ptr()->makeup();
  mc_->set_slots(REQUIRE_CACHE_INTERNAL_META_SLOT, ptr);

  // 只支持一个 mc，所以 savefile 的时候就是之前创建出来的 mc_
  strontium::metacache::metacache::savefile(filename);
}

const char* RequireCacheWrap::QueryRelation(const char* requester,
                                            const char* requestee) {
  const MetaCacheStrR* ret =
      relationship_.reader_ptr()->find(requester, requestee);

#ifdef DEBUG_PRINTF
  printf("R rc_relation->find(%s, %s): %s\n",
         requester,
         requestee,
         ret ? ret->c_str() : nullptr);
#endif

  if (!ret) return nullptr;
  return ret->c_str();
}

const MetaCacheBufR* RequireCacheWrap::QueryCode(const char* dummy_filename) {
  const MetaCacheBufR* buff = contents_.reader_ptr()->find(dummy_filename);
  return buff;
}

const char* RequireCacheWrap::GetInternalMeta(const char* key) {
  auto reader = internal_meta_kv_.reader_ptr();
  if (!reader) return nullptr;

  auto ret = reader->find(key);
  if (!ret) return nullptr;
  return ret->c_str();
}

// Refs:
// https://code.aone.alibaba-inc.com/strontium-project/strontium/blob/953823dd16b0fab7963c9d43bd8a03409af8945d/lib/internal/relational_require_cache/base_object.js#L20-43
string RequireCacheWrap::TransformFilename(const char* filename) {
  string* entry = entries_;
  const char* entry_str = nullptr;

  int cand_i = -1;
  int cand_len = 0;
  char* cand_filename = nullptr;

  int semi_len = 0;
  char* semi_filename = nullptr;

  for (size_t i = 0; i < entries_count_; i++) {
    entry = (entries_ + i);
    entry_str = entry->c_str();

    if (Util::starts_with(filename, entry_str)) {
      semi_filename =
          const_cast<char*>(filename) + (entry->length() * sizeof(char));
      semi_len = strlen(semi_filename);

      if (cand_i == -1 || semi_len < cand_len) {
        cand_filename = semi_filename;
        cand_i = i;
        cand_len = semi_len;
      }
    }
  }

  node::MaybeStackBuffer<char, PATH_MAX_BYTES> ret;
  if (cand_filename != nullptr) {
    // `$` semi_i semi_filename `\0`
    snprintf(*ret, PATH_MAX_BYTES, "$%d%s", cand_i, cand_filename);

#ifdef DEBUG_PRINTF
    printf("TransformFilename: %s -> %s\n", *filename, *ret);
#endif

    return *ret;
  }

  // fallback
  cwk_path_get_relative(
      entries_[0].c_str(), filename, *ret, PATH_MAX_BYTES - 1);

#ifdef DEBUG_PRINTF
  printf("TransformFilename: %s -> %s\n", *filename, *ret);
#endif

  return *ret;
}

// Refs:
// https://code.aone.alibaba-inc.com/strontium-project/strontium/blob/953823dd16b0fab7963c9d43bd8a03409af8945d/lib/internal/relational_require_cache/base_object.js#L45-69
string RequireCacheWrap::TransformFilenameBack(const char* filename,
                                               Environment* env) {
  node::MaybeStackBuffer<char, PATH_MAX_BYTES> ret;
  int pos = -1;

  if (*filename != '$') {
    cwk_path_join(entries_[0].c_str(), filename, *ret, PATH_MAX_BYTES - 1);
    return *ret;
  }

  char num[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  for (int i = 1; filename[i] && i < 13; i++) {
    if (filename[i] == '/') {
      pos = i;
      break;
    }

    num[i - 1] = filename[i];
  }

  if (pos == -1) {
    cwk_path_join(entries_[0].c_str(), filename, *ret, PATH_MAX_BYTES - 1);
    return *ret;
  }

  size_t index = -1;
  node::MaybeStackBuffer<char, PATH_MAX_BYTES> temp;
  sscanf(num, "%lu", &index);
  snprintf(*temp, PATH_MAX_BYTES - 1, ".%s", filename + pos);

  if (index < 0 || index >= entries_count_) {
    env->ThrowError("Broken entry index.");
    return "";
  }

  cwk_path_join(entries_[index].c_str(), *temp, *ret, PATH_MAX_BYTES - 1);
  return *ret;
}

ST_BINDING_METHOD(RequireCacheWrap::New) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  RequireCacheWrap* rcw = nullptr;
  bool is_writer;
  if (args[0]->IsString()) {
    // Reader
    node::Utf8Value cache_filename(isolate, args[0]);
    CHECK(args[1]->IsArray());
    rcw = new RequireCacheWrap(
        env, args.This(), *cache_filename, args[1].As<Array>());
    is_writer = false;
  } else {
    // Writer
    CHECK(args[1]->IsArray());
    rcw = new RequireCacheWrap(env, args.This(), args[1].As<Array>());
    is_writer = true;
  }

  CHECK(rcw);

  if (!rcw->IsLoaded()) {
    char info[128];
    snprintf(info,
             sizeof(info),
             "Cannot initialize require cache object [%s]",
             rcw->error_info_.c_str());
    env->ThrowError(info);
    return;
  }

  // 判断兼容性
  if (!is_writer && !rcw->CheckRequireCacheFileCompatible()) {
    env->ThrowError("PGO file version not compatible.");
    return;
  }
}

ST_BINDING_METHOD(RequireCacheWrap::IsLoaded) {
  RequireCacheWrap* rcw;
  ASSIGN_OR_RETURN_UNWRAP(&rcw, args.Holder());
  args.GetReturnValue().Set(rcw->IsLoaded());
}

ST_BINDING_METHOD(RequireCacheWrap::TransformFilename) {
  auto isolate = args.GetIsolate();

  RequireCacheWrap* rcw;
  ASSIGN_OR_RETURN_UNWRAP(&rcw, args.Holder());

  node::Utf8Value input(isolate, args[0]);
  auto result = String::NewFromUtf8(isolate,
                                    rcw->TransformFilename(*input).c_str(),
                                    v8::NewStringType::kNormal);
  args.GetReturnValue().Set(result.ToLocalChecked());
}

ST_BINDING_METHOD(RequireCacheWrap::TransformFilenameBack) {
  Environment* env = Environment::GetCurrent(args);
  auto isolate = args.GetIsolate();

  RequireCacheWrap* rcw;
  ASSIGN_OR_RETURN_UNWRAP(&rcw, args.Holder());

  node::Utf8Value input(isolate, args[0]);
  string ret = rcw->TransformFilenameBack(*input, env);
  if (ret == "") {
    return;
  }

  auto result =
      String::NewFromUtf8(isolate, ret.c_str(), v8::NewStringType::kNormal);
  args.GetReturnValue().Set(result.ToLocalChecked());
}

ST_BINDING_METHOD(RequireCacheWrap::AddRelation) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  RequireCacheWrap* rcw;
  ASSIGN_OR_RETURN_UNWRAP(&rcw, args.Holder());

  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsString());

  node::Utf8Value requester(isolate, args[0]);
  node::Utf8Value requestee(isolate, args[1]);
  node::Utf8Value dummy_filename(isolate, args[2]);

  rcw->AddRelation(*requester, *requestee, *dummy_filename);
}

ST_BINDING_METHOD(RequireCacheWrap::AddCode) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  RequireCacheWrap* rcw;
  ASSIGN_OR_RETURN_UNWRAP(&rcw, args.Holder());

  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsObject() || args[2]->IsNull() || args[2]->IsUndefined());

  node::Utf8Value dummy_filename(isolate, args[0]);
  node::Utf8Value source_code(isolate, args[1]);

  size_t buff_len = 0;
  char* buff = nullptr;
  if (!args[2]->IsNull() && !args[2]->IsUndefined()) {
    Local<Object> v8_buff;
    if (args[2]->ToObject(env->context()).ToLocal(&v8_buff)) {
      buff_len = node::Buffer::Length(v8_buff);
      buff = node::Buffer::Data(v8_buff);
    }
  }

  rcw->AddCode(*dummy_filename, source_code.ToString(), buff_len, buff);
}

ST_BINDING_METHOD(RequireCacheWrap::Dump) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  RequireCacheWrap* rcw;
  ASSIGN_OR_RETURN_UNWRAP(&rcw, args.Holder());

  CHECK(args[0]->IsString());

  node::Utf8Value filename(isolate, args[0]);

  rcw->Dump(*filename);
}

ST_BINDING_METHOD(RequireCacheWrap::QueryRelation) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  RequireCacheWrap* rcw;
  ASSIGN_OR_RETURN_UNWRAP(&rcw, args.Holder());

  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());

  node::Utf8Value requester(isolate, args[0]);
  node::Utf8Value requestee(isolate, args[1]);

  auto ret = rcw->QueryRelation(*requester, *requestee);
  if (ret == nullptr) {
    args.GetReturnValue().SetNull();
  } else {
    auto str = String::NewFromUtf8(isolate, ret, v8::NewStringType::kNormal)
                   .ToLocalChecked();
    args.GetReturnValue().Set(str);
  }
}

ST_BINDING_METHOD(RequireCacheWrap::QueryCode) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  auto context = env->context();

  RequireCacheWrap* rcw;
  ASSIGN_OR_RETURN_UNWRAP(&rcw, args.Holder());

  CHECK(args[0]->IsString());

  node::Utf8Value dummy_filename(isolate, args[0]);

  auto ret = rcw->QueryCode(*dummy_filename);
  if (ret == nullptr) {
    args.GetReturnValue().SetNull();

#ifdef DEBUG_PRINTF
    printf("R rc_code->find(%s): (null)\n", *dummy_filename);
#endif
  } else {
    auto total_size = ret->size();
    auto total_buff =
        const_cast<char*>(reinterpret_cast<const char*>(ret->buffer()));
    size_t buff_len;
    memcpy(&buff_len, total_buff, sizeof(size_t));
    auto ptr = total_buff + (sizeof(size_t) / sizeof(char));

    Local<Object> obj = Object::New(isolate);
    if (buff_len) {
      // 自己管理生命周期，所以不用 node::Buffer::Copy()
      Local<Object> bytecode =
          node::Buffer::New(
              isolate, ptr, buff_len * sizeof(char), DummyFreeCallback, nullptr)
              .ToLocalChecked();
      obj->Set(context, env->bytecode_string(), bytecode).Check();
    } else {
      obj->Set(context, env->bytecode_string(), Null(isolate)).Check();
    }

    ptr += buff_len;
    auto string_size = total_size - sizeof(size_t) - buff_len;
    auto code = String::NewFromUtf8(
                    isolate, ptr, v8::NewStringType::kNormal, string_size - 1)
                    .ToLocalChecked();
    obj->Set(context, env->sourcecode_string(), code).Check();
    args.GetReturnValue().Set(obj);

#ifdef DEBUG_PRINTF
    printf("R rc_code->find(%s): code_len: %lu, bytecode_len: %lu\n",
           *dummy_filename,
           code->Length(),
           buff_len);
#endif
  }
}

ST_BINDING_METHOD(RequireCacheWrap::GetInternalMeta) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  RequireCacheWrap* rcw;
  ASSIGN_OR_RETURN_UNWRAP(&rcw, args.Holder());

  CHECK(args[0]->IsString());

  node::Utf8Value key(isolate, args[0]);

  auto ret = rcw->GetInternalMeta(*key);
  if (ret == nullptr) {
    args.GetReturnValue().SetNull();
  } else {
    auto str = String::NewFromUtf8(isolate, ret, v8::NewStringType::kNormal)
                   .ToLocalChecked();
    args.GetReturnValue().Set(str);
  }
}

void RequireCacheWrap::InitializeEntries(Environment* env,
                                         Local<Array> entries) {
  auto isolate = env->isolate();
  auto context = env->context();
  entries_count_ = entries->Length();

  for (size_t i = 0; i < entries_count_; i++) {
    Local<String> str = entries->Get(context, i)
                            .ToLocalChecked()
                            ->ToString(context)
                            .ToLocalChecked();

    node::Utf8Value temp(isolate, str);
    entries_[i] = *temp;
  }
}

void RequireCacheWrap::WriteInternalMetaKV() {
  // 写入当前 Strontium 版本
  MetaCacheMetaKVW& mcmkvw = internal_meta_kv_.writer();
  mcmkvw["strontium_version"] = STRONTIUM_VERSION;
  mcmkvw["require_cache_file_format_version"] =
      STRONTIUM_REQUIRE_CACHE_FILE_FORMAT_VERSION;
}

bool RequireCacheWrap::CheckRequireCacheFileCompatible() {
  auto version = GetInternalMeta("require_cache_file_format_version");

  // 当前版本为 1，兼容 `null` / `1`
  if (version == nullptr) return true;
  if (version[0] == '1' && version[1] == '\0') return true;

  // 其余不兼容
  return false;
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  RequireCacheWrap::Initialize(env, target);
}
}  // namespace RequireCache
}  // namespace PGO
}  // namespace strontium

NODE_MODULE_CONTEXT_AWARE_INTERNAL(st_require_cache,
                                   strontium::PGO::RequireCache::Initialize)
