#ifndef INCLUDE_STRONTIUM_H_
#define INCLUDE_STRONTIUM_H_

#include <node.h>

#ifndef strontium_under_dev
#include <strontium_version.h>
#else
#define STRONTIUM_VERSION "dev"
#endif

#include <chrono>  // NOLINT
#include <map>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

namespace strontium {

inline milliseconds now() {
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch());
}

struct kv_struct {
  char* key;
  char* value;
};

#define ST_BINDING_METHOD(name)                                                \
  void name(const FunctionCallbackInfo<Value>& args)

}  // namespace strontium

#endif  // INCLUDE_STRONTIUM_H_
