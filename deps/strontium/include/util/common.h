#ifndef INCLUDE_UTIL_COMMON_H__
#define INCLUDE_UTIL_COMMON_H__

namespace strontium {
namespace Util {

inline bool starts_with(const char* suspected_long,
                        const char* suspected_short) {
  int i = 0;
  do {
    if (suspected_short[i] == '\0') {
      return true;
    }

    if (suspected_long[i] != suspected_short[i]) {
      return false;
    }
  } while (++i);

  return true;
}

}  // namespace Util
}  // namespace strontium

#endif  // INCLUDE_UTIL_COMMON_H__
