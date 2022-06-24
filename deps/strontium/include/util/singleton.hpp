#ifndef INCLUDE_UTIL_SINGLETON_HPP__
#define INCLUDE_UTIL_SINGLETON_HPP__

namespace strontium {

template <class T>
class Singleton {
 public:
  static T& Instance() {
    static T instance;
    return instance;
  }

 protected:
  Singleton() {}
  virtual ~Singleton() {}

 private:
  Singleton(const Singleton&);
  Singleton& operator=(const Singleton&);
};

}  // namespace strontium

#endif  // INCLUDE_UTIL_SINGLETON_HPP__
