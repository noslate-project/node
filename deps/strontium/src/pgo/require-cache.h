#ifndef SRC_PGO_REQUIRE_CACHE_H__
#define SRC_PGO_REQUIRE_CACHE_H__

#include <base_object.h>
#include <metacache.h>
#include <strontium.h>

#include <string>
#include <vector>

namespace strontium {
namespace PGO {
namespace RequireCache {

#define STRONTIUM_REQUIRE_CACHE_FILE_FORMAT_VERSION ("1")
#define STRONTIUM_REQUIRE_CACHE_MAX_ENTRIES (256)

using node::BaseObject;
using node::Environment;

using std::string;
using std::vector;

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

using strontium::metacache::metacache;

typedef strontium::metacache::buf MetaCacheBufR;
typedef strontium::metacache::str MetaCacheStrR;
typedef strontium::metacache::kv<MetaCacheStrR, MetaCacheBufR>
    MetaCacheFileBufferR;
typedef strontium::metacache::kkv<MetaCacheStrR, MetaCacheStrR, MetaCacheStrR>
    MetaCacheRelationR;
typedef strontium::metacache::kv<MetaCacheStrR, MetaCacheStrR> MetaCacheMetaKVR;

typedef strontium::metacache::buf_w MetaCacheBufW;
typedef strontium::metacache::str_w MetaCacheStrW;
typedef strontium::metacache::kv_w<MetaCacheStrW, MetaCacheBufW>
    MetaCacheFileBufferW;
typedef strontium::metacache::kkv_w<MetaCacheStrW, MetaCacheStrW, MetaCacheStrW>
    MetaCacheRelationW;
typedef strontium::metacache::kv_w<MetaCacheStrW, MetaCacheStrW>
    MetaCacheMetaKVW;

template <class ReaderType, class WriterType>
class MetaCacheTypeUnion {
 public:
  MetaCacheTypeUnion() { body_.ptr = nullptr; }
  ~MetaCacheTypeUnion() {
    if (body_.ptr != nullptr) {
      // TODO(zl131478): 需要完整的析构
      delete body_.ptr;
      body_.ptr = nullptr;
    }
  }

  void* operator=(void* ptr) {
    body_.ptr = ptr;
    return ptr;
  }

  void* ptr() { return body_.ptr; }
  ReaderType* reader_ptr() { return body_.reader; }
  WriterType* writer_ptr() { return body_.writer; }

  ReaderType& reader() { return *body_.reader; }
  WriterType& writer() { return *body_.writer; }

 private:
  union InnerUnion {
    void* ptr;
    ReaderType* reader;
    WriterType* writer;
  } body_;
};

enum WrapType {
  READER = 0,
  WRITER,
};

class RequireCacheWrap : public BaseObject {
 public:
  ~RequireCacheWrap() override;

  static void Initialize(Environment* env, v8::Local<v8::Object> target);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(RequireCacheWrap)
  SET_SELF_SIZE(RequireCacheWrap)

 protected:
  // As Reader
  RequireCacheWrap(Environment* env,
                   Local<Object> obj,
                   const char* cache_filename,
                   Local<Array> entries);
  // As Writer
  RequireCacheWrap(Environment* env, Local<Object> obj, Local<Array> entries);

  bool IsLoaded();

  // TODO(kaidi.zkd): 线程安全。
  void AddRelation(const char* requester,
                   const char* requestee,
                   const char* dummy_filename);
  void AddCode(const char* dummy_filename,
               const string& source_code,
               size_t byte_code_length = 0,
               const char* byte_code = nullptr);
  void Dump(const char* filename);

  const char* QueryRelation(const char* requester, const char* requestee);
  const MetaCacheBufR* QueryCode(const char* dummy_filename);
  const char* GetInternalMeta(const char* key);

  string TransformFilename(const char* filename);
  string TransformFilenameBack(const char* filename, Environment* env);

  // Common
  static ST_BINDING_METHOD(New);
  static ST_BINDING_METHOD(IsLoaded);
  static ST_BINDING_METHOD(TransformFilename);
  static ST_BINDING_METHOD(TransformFilenameBack);

  // Writer
  static ST_BINDING_METHOD(AddRelation);
  static ST_BINDING_METHOD(AddCode);
  static ST_BINDING_METHOD(Dump);

  // Reader
  static ST_BINDING_METHOD(QueryRelation);
  static ST_BINDING_METHOD(QueryCode);
  static ST_BINDING_METHOD(GetInternalMeta);

 private:
  void InitializeEntries(Environment* env, Local<Array> entries);
  void WriteInternalMetaKV();
  bool CheckRequireCacheFileCompatible();

  metacache* mc_;
  string error_info_;
  string cache_filename_;

  WrapType type_;
  MetaCacheTypeUnion<MetaCacheFileBufferR, MetaCacheFileBufferW> contents_;
  MetaCacheTypeUnion<MetaCacheRelationR, MetaCacheRelationW> relationship_;
  MetaCacheTypeUnion<MetaCacheMetaKVR, MetaCacheMetaKVW> internal_meta_kv_;

  vector<const char*> alloc_buffer;
  string entries_[STRONTIUM_REQUIRE_CACHE_MAX_ENTRIES];
  size_t entries_count_;
};

}  // namespace RequireCache
}  // namespace PGO
}  // namespace strontium

#endif  // SRC_PGO_REQUIRE_CACHE_H__
