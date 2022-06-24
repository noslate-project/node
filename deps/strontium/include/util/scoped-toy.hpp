#ifndef INCLUDE_UTIL_SCOPED_TOY_HPP__
#define INCLUDE_UTIL_SCOPED_TOY_HPP__

namespace strontium {
namespace Util {

class ScopedToyProcessor {
 public:
  virtual void ResponseScoped() = 0;
};

template <typename T, typename... Args>
class ScopedToy {
 public:
  explicit ScopedToy(Args... args) : processor_(args...) {}

  ~ScopedToy() { processor_.ResponseScoped(); }

 private:
  T processor_;
};

}  // namespace Util
}  // namespace strontium

#endif  // INCLUDE_UTIL_SCOPED_TOY_HPP__
