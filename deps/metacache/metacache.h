#ifndef _STRONTIUM_METACACHE_H_
#define _STRONTIUM_METACACHE_H_

//#define _GNU_SOURCE
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <new>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <list>
#include <utility>
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
#include <atomic>
#include <scoped_allocator>
//#include "city.h"

#if __DARWIN_ONLY_64_BIT_INO_T
struct stat64 __DARWIN_STRUCT_STAT64;
#endif

namespace strontium {
namespace metacache {

// likely and unlikey
#define LIKELY(x) (__builtin_expect(!!(x), 1))
#define UNLIKELY(x) (__builtin_expect(!!(x), 0))

// default vm address the cache loading to.
// it could be a dynamic virtual address, depends on plantform.
#define MC_DEF_VMADDR  (0x4000000000ULL)
#define MC_LIMIT (1*1024*1024*1024) // 1G size limit

// place holder for Metacache globals
#define MC_GLOBAL() \
    char* cm::_vmem = 0; \
    std::atomic<char*> cm::_vmcur(0);

// page aligned
// const int kPageSize = sysconf(_SC_PAGE_SIZE);
#define PAGE_SIZE               (4096)
#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))
//#define ALIGN(x,a)              __ALIGN_MASK(x,(typeof(x))(a)-1)
#define ALIGN(x,a)              __ALIGN_MASK(x,((a)-1))
#define ALIGN_PAGE(x)           ALIGN(x,PAGE_SIZE) 

// metacache uses offset for memory saving.
// the whole metacache using 32b pointer in 64b platform.
typedef int32_t mc_ptr;

// may need to use 128b hash value
typedef std::pair<uint64_t, uint64_t> uint128_t;
inline uint64_t Uint128Low64(const uint128_t& x) { return x.first; }
inline uint64_t Uint128High64(const uint128_t& x) { return x.second; }

// min/max helper
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

// time stamp for debug
struct ts {
    struct timeval _begin, _end;
 
#if 0
    /* only work on x86-64, test purpose.
     */
    static uint64_t rdtsc(){
        unsigned int lo,hi;
        __asm__ __volatile__ ("rdtsc": "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
    }
#endif
    
    static struct timeval _utime_diff(struct timeval &a, struct timeval &b)
    {
        struct timeval ret;
        if (b.tv_usec >= a.tv_usec) {
            ret.tv_usec = b.tv_usec - a.tv_usec;
            ret.tv_sec = b.tv_sec - a.tv_sec;
        } else {
            ret.tv_usec = 1000000 + b.tv_usec - a.tv_usec;
            ret.tv_sec = b.tv_sec - a.tv_sec - 1;
        }
        return ret;
    }

    void begin()
    {
        gettimeofday(&_begin, 0);
    }   

    void end()
    {
        gettimeofday(&_end, 0);
    }

    double timediff()
    {
        struct timeval result = _utime_diff(_begin, _end); 
        double gap = result.tv_sec + result.tv_usec / 1000000.0;
        return gap;
    }
};

// print time cost in scope
struct ts_scope : ts {
    std::string _name;  

    ts_scope() : _name(toString()) {}

    ts_scope(const std::string& name) : _name(name) 
    {
        printf("%s begin\n", _name.c_str());
        begin();
    }

    ~ts_scope() 
    {
        end();
        printf("%s end, cost %f seconds.\n", _name.c_str(), timediff());
    }

    std::string toString()
    {
        if (_name != "") {
            return _name;
        }
        
        char buf[20];
        sprintf(buf, "%p", this);
        return std::string(buf);
    }
};

/*
 * Begin of City Hash (used for string hash)
 */
class city {
    
    // ONLY For little endian
    #define uint32_in_expected_order(x) (x)
    #define uint64_in_expected_order(x) (x)

    static uint64_t UNALIGNED_LOAD64(const char *p) {
      uint64_t result;
      memcpy(&result, p, sizeof(result));
      return result;
    }

    static uint32_t UNALIGNED_LOAD32(const char *p) {
      uint32_t result;
      memcpy(&result, p, sizeof(result));
      return result;
    }

    static uint64_t Fetch64(const char *p) {
      return uint64_in_expected_order(UNALIGNED_LOAD64(p));
    }

    static uint32_t Fetch32(const char *p) {
      return uint32_in_expected_order(UNALIGNED_LOAD32(p));
    }

    // Some primes between 2^63 and 2^64 for various uses.
    static const uint64_t k0 = 0xc3a5c85c97cb3127ULL;
    static const uint64_t k1 = 0xb492b66fbe98f273ULL;
    static const uint64_t k2 = 0x9ae16a3b2f90404fULL;
    static const uint64_t k3 = 0xc949d7c7509e6557ULL;

    // Bitwise right rotate.  Normally this will compile to a single
    // instruction, especially if the shift is a manifest constant.
    static uint64_t Rotate(uint64_t val, int shift) {
      // Avoid shifting by 64: doing so yields an undefined result.
      return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
    }

    // Equivalent to Rotate(), but requires the second arg to be non-zero.
    // On x86-64, and probably others, it's possible for this to compile
    // to a single instruction if both args are already in registers.
    static uint64_t RotateByAtLeast1(uint64_t val, int shift) {
      return (val >> shift) | (val << (64 - shift));
    }

    static uint64_t ShiftMix(uint64_t val) {
      return val ^ (val >> 47);
    }

    // Hash 128 input bits down to 64 bits of output.
    // This is intended to be a reasonably good hash function.
    static uint64_t Hash128to64(const uint128_t& x) {
      // Murmur-inspired hashing.
      const uint64_t kMul = 0x9ddfea08eb382d69ULL;
      uint64_t a = (Uint128Low64(x) ^ Uint128High64(x)) * kMul;
      a ^= (a >> 47);
      uint64_t b = (Uint128High64(x) ^ a) * kMul;
      b ^= (b >> 47);
      b *= kMul;
      return b;
    }

    static uint64_t HashLen16(uint64_t u, uint64_t v) {
      return Hash128to64(uint128_t(u, v));
    }

    static uint64_t HashLen0to16(const char *s, size_t len) {
      if (len > 8) {
        uint64_t a = Fetch64(s);
        uint64_t b = Fetch64(s + len - 8);
        return HashLen16(a, RotateByAtLeast1(b + len, len)) ^ b;
      }
      if (len >= 4) {
        uint64_t a = Fetch32(s);
        return HashLen16(len + (a << 3), Fetch32(s + len - 4));
      }
      if (len > 0) {
        uint8_t a = s[0];
        uint8_t b = s[len >> 1];
        uint8_t c = s[len - 1];
        uint32_t y = static_cast<uint32_t>(a) + (static_cast<uint32_t>(b) << 8);
        uint32_t z = len + (static_cast<uint32_t>(c) << 2);
        return ShiftMix(y * k2 ^ z * k3) * k2;
      }
      return k2;
    }

    // This probably works well for 16-byte strings as well, but it may be overkill
    // in that case.
    static uint64_t HashLen17to32(const char *s, size_t len) {
      uint64_t a = Fetch64(s) * k1;
      uint64_t b = Fetch64(s + 8);
      uint64_t c = Fetch64(s + len - 8) * k2;
      uint64_t d = Fetch64(s + len - 16) * k0;
      return HashLen16(Rotate(a - b, 43) + Rotate(c, 30) + d,
                       a + Rotate(b ^ k3, 20) - c + len);
    }

    // Return an 8-byte hash for 33 to 64 bytes.
    static uint64_t HashLen33to64(const char *s, size_t len) {
      uint64_t z = Fetch64(s + 24);
      uint64_t a = Fetch64(s) + (len + Fetch64(s + len - 16)) * k0;
      uint64_t b = Rotate(a + z, 52);
      uint64_t c = Rotate(a, 37);
      a += Fetch64(s + 8);
      c += Rotate(a, 7);
      a += Fetch64(s + 16);
      uint64_t vf = a + z;
      uint64_t vs = b + Rotate(a, 31) + c;
      a = Fetch64(s + 16) + Fetch64(s + len - 32);
      z = Fetch64(s + len - 8);
      b = Rotate(a + z, 52);
      c = Rotate(a, 37);
      a += Fetch64(s + len - 24);
      c += Rotate(a, 7);
      a += Fetch64(s + len - 16);
      uint64_t wf = a + z;
      uint64_t ws = b + Rotate(a, 31) + c;
      uint64_t r = ShiftMix((vf + ws) * k2 + (wf + vs) * k0);
      return ShiftMix(r * k0 + vs) * k2;
    }

    // Return a 16-byte hash for 48 bytes.  Quick and dirty.
    // Callers do best to use "random-looking" values for a and b.
    static std::pair<uint64_t, uint64_t> WeakHashLen32WithSeeds(
        uint64_t w, uint64_t x, uint64_t y, uint64_t z, uint64_t a, uint64_t b) {
      a += w;
      b = Rotate(b + a + z, 21);
      uint64_t c = a;
      a += x;
      a += y;
      b += Rotate(a, 44);
      return std::make_pair(a + z, b + c);
    }

    // Return a 16-byte hash for s[0] ... s[31], a, and b.  Quick and dirty.
    static std::pair<uint64_t, uint64_t> WeakHashLen32WithSeeds(
        const char* s, uint64_t a, uint64_t b) {
      return WeakHashLen32WithSeeds(Fetch64(s),
                                    Fetch64(s + 8),
                                    Fetch64(s + 16),
                                    Fetch64(s + 24),
                                    a,
                                    b);
    }

public:
    // 
    static uint64_t CityHash64(const char *s, size_t len) {
      if (len <= 32) {
        if (len <= 16) {
          return HashLen0to16(s, len);
        } else {
          return HashLen17to32(s, len);
        }
      } else if (len <= 64) {
        return HashLen33to64(s, len);
      }

      // For strings over 64 bytes we hash the end first, and then as we
      // loop we keep 56 bytes of state: v, w, x, y, and z.
      uint64_t x = Fetch64(s + len - 40);
      uint64_t y = Fetch64(s + len - 16) + Fetch64(s + len - 56);
      uint64_t z = HashLen16(Fetch64(s + len - 48) + len, Fetch64(s + len - 24));
      std::pair<uint64_t, uint64_t> v = WeakHashLen32WithSeeds(s + len - 64, len, z);
      std::pair<uint64_t, uint64_t> w = WeakHashLen32WithSeeds(s + len - 32, y + k1, x);
      x = x * k1 + Fetch64(s);

      // Decrease len to the nearest multiple of 64, and operate on 64-byte chunks.
      len = (len - 1) & ~static_cast<size_t>(63);
      do {
        x = Rotate(x + y + v.first + Fetch64(s + 8), 37) * k1;
        y = Rotate(y + v.second + Fetch64(s + 48), 42) * k1;
        x ^= w.second;
        y += v.first + Fetch64(s + 40);
        z = Rotate(z + w.first, 33) * k1;
        v = WeakHashLen32WithSeeds(s, v.second * k1, x + w.first);
        w = WeakHashLen32WithSeeds(s + 32, z + w.second, y + Fetch64(s + 16));
        std::swap(z, x);
        s += 64;
        len -= 64;
      } while (len != 0);
      return HashLen16(HashLen16(v.first, w.first) + ShiftMix(y) * k1 + z,
                       HashLen16(v.second, w.second) + x);
    }

    static uint64_t CityHash64WithSeed(const char *s, size_t len, uint64_t seed) {
      return CityHash64WithSeeds(s, len, k2, seed);
    }

    static uint64_t CityHash64WithSeeds(const char *s, size_t len,
                               uint64_t seed0, uint64_t seed1) {
      return HashLen16(CityHash64(s, len) - seed0, seed1);
    }

    // A subroutine for CityHash128().  Returns a decent 128-bit hash for strings
    // of any length representable in signed long.  Based on City and Murmur.
    static uint128_t CityMurmur(const char *s, size_t len, uint128_t seed) {
      uint64_t a = Uint128Low64(seed);
      uint64_t b = Uint128High64(seed);
      uint64_t c = 0;
      uint64_t d = 0;
      signed long l = len - 16;
      if (l <= 0) {  // len <= 16
        a = ShiftMix(a * k1) * k1;
        c = b * k1 + HashLen0to16(s, len);
        d = ShiftMix(a + (len >= 8 ? Fetch64(s) : c));
      } else {  // len > 16
        c = HashLen16(Fetch64(s + len - 8) + k1, a);
        d = HashLen16(b + len, c + Fetch64(s + len - 16));
        a += d;
        do {
          a ^= ShiftMix(Fetch64(s) * k1) * k1;
          a *= k1;
          b ^= a;
          c ^= ShiftMix(Fetch64(s + 8) * k1) * k1;
          c *= k1;
          d ^= c;
          s += 16;
          l -= 16;
        } while (l > 0);
      }
      a = HashLen16(a, c);
      b = HashLen16(d, b);
      return uint128_t(a ^ b, HashLen16(b, a));
    }

    static uint128_t CityHash128WithSeed(const char *s, size_t len, uint128_t seed) {
      if (len < 128) {
        return CityMurmur(s, len, seed);
      }

      // We expect len >= 128 to be the common case.  Keep 56 bytes of state:
      // v, w, x, y, and z.
      std::pair<uint64_t, uint64_t> v, w;
      uint64_t x = Uint128Low64(seed);
      uint64_t y = Uint128High64(seed);
      uint64_t z = len * k1;
      v.first = Rotate(y ^ k1, 49) * k1 + Fetch64(s);
      v.second = Rotate(v.first, 42) * k1 + Fetch64(s + 8);
      w.first = Rotate(y + z, 35) * k1 + x;
      w.second = Rotate(x + Fetch64(s + 88), 53) * k1;

      // This is the same inner loop as CityHash64(), manually unrolled.
      do {
        x = Rotate(x + y + v.first + Fetch64(s + 8), 37) * k1;
        y = Rotate(y + v.second + Fetch64(s + 48), 42) * k1;
        x ^= w.second;
        y += v.first + Fetch64(s + 40);
        z = Rotate(z + w.first, 33) * k1;
        v = WeakHashLen32WithSeeds(s, v.second * k1, x + w.first);
        w = WeakHashLen32WithSeeds(s + 32, z + w.second, y + Fetch64(s + 16));
        std::swap(z, x);
        s += 64;
        x = Rotate(x + y + v.first + Fetch64(s + 8), 37) * k1;
        y = Rotate(y + v.second + Fetch64(s + 48), 42) * k1;
        x ^= w.second;
        y += v.first + Fetch64(s + 40);
        z = Rotate(z + w.first, 33) * k1;
        v = WeakHashLen32WithSeeds(s, v.second * k1, x + w.first);
        w = WeakHashLen32WithSeeds(s + 32, z + w.second, y + Fetch64(s + 16));
        std::swap(z, x);
        s += 64;
        len -= 128;
      } while (LIKELY(len >= 128));
      x += Rotate(v.first + z, 49) * k0;
      z += Rotate(w.first, 37) * k0;
      // If 0 < len < 128, hash up to 4 chunks of 32 bytes each from the end of s.
      for (size_t tail_done = 0; tail_done < len; ) {
        tail_done += 32;
        y = Rotate(x + y, 42) * k0 + v.second;
        w.first += Fetch64(s + len - tail_done + 16);
        x = x * k0 + w.first;
        z += w.second + Fetch64(s + len - tail_done);
        w.second += v.first;
        v = WeakHashLen32WithSeeds(s + len - tail_done, v.first + z, v.second);
      }
      // At this point our 56 bytes of state should contain more than
      // enough information for a strong 128-bit hash.  We use two
      // different 56-byte-to-8-byte hashes to get a 16-byte final result.
      x = HashLen16(x, v.first);
      y = HashLen16(y + z, w.first);
      return uint128_t(HashLen16(x + v.second, w.second) + y,
                     HashLen16(x + w.second, y + v.second));
    }

    static uint128_t CityHash128(const char *s, size_t len) {
      if (len >= 16) {
        return CityHash128WithSeed(s + 16,
                                   len - 16,
                                   uint128_t(Fetch64(s) ^ k3,
                                           Fetch64(s + 8)));
      } else if (len >= 8) {
        return CityHash128WithSeed(NULL,
                                   0,
                                   uint128_t(Fetch64(s) ^ (len * k0),
                                           Fetch64(s + len - 8) ^ k1));
      } else {
        return CityHash128WithSeed(s, len, uint128_t((uint64_t)k0, (uint64_t)k1));
      }
    }
};  // city

/*
 * begin of cache memory base
 */

// defines all section types
enum mc_sec_type {
    MC_SEC_STR = 1,     // string table section
    MC_SEC_BUF,         // buffer table section
    MC_SEC_KV,          // Key-value section
    MC_SEC_KKV,         // Key-Key-Val section
    MC_SEC_FS,          // filesystem cache section
    MC_SEC_MAX
};

// base class, short for 'cache memory', alloced from metacache.
struct cm {
    char __pad[0];

    // indicates the cache memory startof address.
    static char* _vmem;
 
    // indicates malloc last start vmaddr.
    static std::atomic<char*> _vmcur;

    // get vmaddr from offset.
    static inline void* vmaddr(mc_ptr& offs) 
    {
        return (void*)((uint64_t)_vmem + offs);
    }

    // get offset from pointer.
    static inline mc_ptr offset(void *ptr) 
    {
        return (mc_ptr)((uint64_t)ptr - (uint64_t)_vmem);
    }

    // is cache memory.
    static inline bool iscm(void *ptr) {
        int64_t off = (uint64_t)ptr - (uint64_t)_vmem;
        if (off >= 0 && off < MC_LIMIT) {
            return 1;
        }
        return 0;
    }

    // macro for round word size
    #define __va_rounded_size(size)  \
        ((((size) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))

    // memory alloc from cache memory (word size aligned).
    static void *malloc(uint32_t size)
    {
        if (!_vmcur) return NULL;
        if (!size) return NULL;
        
        void *ptr = NULL; 
        bool exchanged = false; 
        do {
            char* cur = _vmcur.load();  // currut pointer is not aligned.
            char* cur_aligned = (char*)__va_rounded_size((uint64_t)cur);  // aligned to word size 
            uint64_t size_aligned = __va_rounded_size(size);
            char* new_aligned = (char*)((uint64_t)cur_aligned + size_aligned); 
            exchanged = _vmcur.compare_exchange_strong(cur, new_aligned);
            //printf("exchanged = %d\n", exchanged);
            ptr = (void*)cur_aligned;
        } while (!exchanged);
        //printf(" %s(%u) = %p\n", __func__, size, ptr);
        return ptr;
    }

    // alloc memory from cache memory (not aligned).
    static void *_alloc(uint32_t size)
    {
        if (!_vmcur) return NULL;
        if (!size) return NULL;
         
        void *ptr = _vmcur.fetch_add(size);
        //printf(" %s(%u) = %p\n", __func__, size, ptr);
        return ptr;
    }

    // create new cache memory
    static void* mmap(void *vmaddr, uint32_t max_size)
    {
        // attach cache
        char *cache = (char*)::mmap(vmaddr, max_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    	assert(cache);

        // for malloc
        _vmem = cache;
        _vmcur.store(cache);
        return (void*)cache;
    }
    
    // load cache to process memory
    static void* mmap(void *vmaddr, const char *file)
    {
        int fd;
        struct stat s;

        // open file
        fd = open(file, O_RDONLY);
        assert(fd > 0);

        int rc = fstat(fd, &s);
        assert(!rc);
       
        size_t flen = s.st_size;
        printf("%s(%s) = %lu\n", __func__, file, flen);

#ifdef __linux__
        // file advice
        rc = posix_fadvise(fd, 0, flen, POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED);
        assert(rc == 0);
#endif

#if 1
        char* cache = (char*)::mmap(vmaddr, flen, PROT_READ, MAP_PRIVATE, fd, 0);
#else
        void *cache = ::malloc(flen);
        rc = read(fd, cache, flen);
        assert(rc == flen);
#endif

        assert(cache);
        // TBD: turning memory advice 
        //int rc = madvise(vmaddr, flen, MADV_WILLNEED | MADV_SEQUENTIAL);
        //assert(rc == 0);

        // map to global. 
        _vmem = cache; 
        _vmcur.store(cache);
        return cache;
    }

    // save cache memory to file
    static int dump(const char* file)
    {
        std::ofstream f(file, std::ios::binary);
        void *ptr = _vmcur;
        size_t size = (uint64_t)ptr - MC_DEF_VMADDR;
        f.write((char *)MC_DEF_VMADDR, ALIGN_PAGE(size));
        f.close();
        printf("%s(%s) = %lu\n", __func__, file, size);
        return 0;
    }
  
    // alloc memory from cache memory, but no delete.
    void* operator new(size_t size) { return malloc(size); }
    
    // do some memory checks. 
    cm () 
    {
        printf("  %s: addr(%p), iscm(%d)\n", __func__, this, iscm(this));
        // assert if not a cache memory.
        assert(iscm(this));
    }
};

// mc_ptr for saving memory.
template <typename _Tp>
struct ptr : cm {

    mc_ptr _offset;     // 32b pointer
    
    /*
     * mc_ptr using 32b
     * value in mc_ptr is relative offset of the ptr it' self.
     */
#define MC_PTR_OFFS(vma) ((int64_t)vma - (int64_t)this)
#define MC_PTR_ADDR(offs) ((uint64_t)this + offs)
    ptr() : _offset(0) 
    {
    }
    
    ptr(_Tp* v) : _offset(MC_PTR_OFFS(v)) 
    {
    }
    
    ptr(mc_ptr v) : _offset(MC_PTR_OFFS(v)) 
    {
    }

   // only for Debug 
    void dbg() 
    { 
        printf("  %s:%d addr(%p), offset(%d)\n", __func__, __LINE__, addr(), _offset); 
    };
    
    // ptr to _vmem means null. 
    inline bool isNull() const
    {
        // case 1, zero is null, so we DON'T support point to self pointer.
        if (_offset == 0)
            return true;
#if 0
        // case 2, ponter to the self storage
        if (_offset >0 && _offset <4)
            return true;
#endif    
        return false;
    }

    // get real pointer 
    const _Tp* addr() const 
    { 
        void *ptr = (void *)MC_PTR_ADDR(_offset);
        if (isNull()) {
            return NULL;
        }
        return (_Tp *)ptr;
    }

    // get offset value 
    mc_ptr offset() 
    { 
        return _offset; 
    }

    // overloaded assignment (from pointer)
    void operator=(_Tp* v)
    { 
        _offset = MC_PTR_OFFS(v); 
    }

    // overloaded assignment (from ptr<T>)
    void operator=(ptr& p)
    { 
        const _Tp* v = p.addr();
        _offset = MC_PTR_OFFS(v);
    }

    // overloaded '->' for directly access 
    const _Tp* operator->() const { 
        assert(!isNull());
        return (_Tp *)MC_PTR_ADDR(_offset);
    }
   
    const _Tp& operator*()
    {
        return *addr();
    }

    // compare 
    bool operator< (ptr& p)
    {
        const _Tp *t1 = addr();
        const _Tp *t2 = p.addr();
        return *t1 < *t2; 
    }

    bool operator> (ptr& p)
    {
        const _Tp *t1 = addr();
        const _Tp *t2 = p.addr();
        return *t1 > *t2; 
    }
};

// lv structure, holds the Length and Buffer memory.
// L1 : **0 :  7b length <128 bytes
// L2 : *01 : 14b length <16K
// L3 : 011 : 21b length <2M 
// L4 : 111 : 29b length <512M
struct lv : cm {
    uint8_t _val[0];

    // ONLY for Little Endian
    union u_lv {
        struct {
            #define LV_L1_B 0 
            uint8_t b:1;    // b0
            uint8_t l:7;
        } L1;
        struct {
            #define LV_L2_B 1 
            uint16_t b:2;   // b01
            uint16_t l:14; 
        } L2;
        struct {
            #define LV_L3_B 3 
            uint32_t b:3;   // b011
            uint32_t l:21; 
            uint32_t __pad:8; 
        } L3;
        struct {
            #define LV_L4_B 7 
            uint32_t b:3;   // b111
            uint32_t l:29; 
        } L4;
    };
    
    // get LV_B size
    #define  MC_LV_B(x)   (1<<(x))

    // default construct
    lv() {
        printf("  %s: length(%d), buf(%p)\n", __func__, length(), buf());    
    }

    //  
    static int calc_size(int size_buf) {
        int size = size_buf;
        if (size_buf < MC_LV_B(7)) { // < 128 bytes
            size += 1; 
        } else if (size_buf < MC_LV_B(14)) { // < 16k
            size += 2; 
        } else if (size_buf < MC_LV_B(21)) { // < 2M 
            size += 3; 
        } else if (size_buf < MC_LV_B(29)) { // < 512M  
            size += 4; 
        } else {
            // too large
            assert(0); 
        }
        return size;
    }

    int length() const
    {
        int len = 0;
        u_lv *u = (u_lv *)_val;
        if (u->L1.b == LV_L1_B) { // < 128 bytes
            len = u->L1.l;
        } else if (u->L2.b == LV_L2_B) { // < 16K 
            len = u->L2.l;
        } else if (u->L3.b == LV_L3_B) { // < 2M 
            len = u->L3.l;
        } else if (u->L4.b == LV_L4_B) { // < 512M
            len = u->L4.l;
        } else {
            // too large
            assert(0);
        }
        return len; 
    }

    const void* buf() const
    {
        const void *buf = 0;
        u_lv *u = (u_lv *)_val;
        if (u->L1.b == LV_L1_B) { // < 128 bytes
            buf = &_val[1];
        } else if (u->L2.b == LV_L2_B) { // < 16K 
            buf = &_val[2];
        } else if (u->L3.b == LV_L3_B) { // < 2M 
            buf = &_val[3];
        } else if (u->L4.b == LV_L4_B) { // < 512M
            buf = &_val[4];
        } else {
            // too large
            assert(0);
        }
        return buf; 
    }

    // get buf pointer and length from lv. 
    int get(const void* &buf, int &len)
    {
        u_lv *u = (u_lv *)_val;
        if (u->L1.b == LV_L1_B) { // < 128 bytes
            len = u->L1.l;
            buf = &_val[1];
        } else if (u->L2.b == LV_L2_B) { // < 16K 
            len = u->L2.l;
            buf = &_val[2];
        } else if (u->L3.b == LV_L3_B) { // < 2M 
            len = u->L3.l;
            buf = &_val[3];
        } else if (u->L4.b == LV_L4_B) { // < 512M
            len = u->L4.l;
            buf = &_val[4];
        } else {
            // too large
            assert(0);
            return -1; 
        }
        return 0;
    }

    // copy memory to lv. 
    int copy(const void *buf, int len)
    {
        uint8_t *ptr = 0; 
        u_lv *u = (u_lv *)_val;
        if (len < MC_LV_B(7)) { // < 128 bytes
            u->L1.b = LV_L1_B; 
            u->L1.l = len; 
            ptr = &_val[1];
        } else if (len < MC_LV_B(14)) {  // < 16k
            u->L2.b = LV_L2_B; 
            u->L2.l = len; 
            ptr = &_val[2];
        } else if (len < MC_LV_B(21)) {  // < 16k
            u->L3.b = LV_L3_B; 
            u->L3.l = len; 
            ptr = &_val[3];
        } else if (len < MC_LV_B(29)) {  // < 16k
            u->L4.b = LV_L4_B; 
            u->L4.l = len; 
            ptr = &_val[4];
        } else {
            // too large
            assert(0);
            return -1;
        }
        
        memcpy(ptr, buf, len);
        return 0;
    }

    static int set_length(uint8_t *ptr, int len)
    {
        u_lv *u = (u_lv *)ptr;
        if (len < MC_LV_B(7)) { // < 128 bytes
            u->L1.b = LV_L1_B; 
            u->L1.l = len; 
        } else if (len < MC_LV_B(14)) {  // < 16k
            u->L2.b = LV_L2_B; 
            u->L2.l = len; 
        } else if (len < MC_LV_B(21)) {  // < 16k
            u->L3.b = LV_L3_B; 
            u->L3.l = len; 
        } else if (len < MC_LV_B(29)) {  // < 16k
            u->L4.b = LV_L4_B; 
            u->L4.l = len; 
        } else {
            // too large
            assert(0);
            return -1;
        }
        return 0; 
    }

    // create a lv structure with specified size of buffer.
    void *operator new (size_t size_lv, int size_buf) {
        uint32_t size = calc_size(size_lv + size_buf);
        printf("%s(%lu,%d)\n", __func__, size_lv, size_buf);
        uint8_t *ptr = (uint8_t *)cm::malloc(size);
        set_length(ptr, size_buf);
        return ptr; 
    }
  
    private: 
    // space MUST be reserved.
    void *operator new (size_t count) {  
        printf("%s(%lu)\n", __func__, count);
        return cm::malloc(count);; 
    };
};

/* simple a string table based on LV.
 *  all the string was insert to a tree 
 *  same strings only saved once
 */
struct sz : cm {
    ptr<lv> _lv;        // string data  

    // return the string buffer size (NOT whole LV size) 
    int size() const 
    {
        return _lv->length();
    }

    // return the string in const char*
    const char* c_str()
    {
        return (const char *)_lv->buf();
    }

    // TBD: converge the same string for space reducing. 
    static std::map<std::string, ptr<lv>> _map;     // hold a global map of string to CM mapping. 

    // disable consturctor without string.
    sz() = delete;

    // constructor by std::string 
    sz(const std::string &s) {
        int len = s.size();
        const char* buf = s.c_str();
        lv *v = new(len) lv;
        _lv = v;
        v->copy(buf, len);
    }

    // constructor by char*
    sz(const char* s) { 
        int len = strlen(s);
        const char* buf = s;
        lv *v = new(len) lv;
        _lv = v;
        v->copy(buf, len);
    }
};

// triple union
template <class T1, class T2, class T3>
struct triple : cm {
    T1 first;
    T2 second;
    T3 third;
};

template<class T1, class T2, class T3>
inline triple<T1,T2,T3> mk_triple(const T1& _t1, const T2& _t2, const T3& _t3)
{
    return triple<T1,T2,T3>(_t1, _t2, _t3);
}

struct metacache_w;

struct _wT {
    /*
     * Data with relationship maintained in heap memory,
     * Classes hold the data and duty to write Cache memory calls a Writer, 
     * with a _w in each class/structure,
     * operating from heap to CM calls 'makeup'.
     *
     * to omit vtable pointer, _wT has nothing in context.
     */
};


/* Begin of basic structure.
 */


struct int_w {
    typedef int raw_type;

    int _value;

    int_w() : _value(0) 
    {
    }

    int_w(int _v) : _value(_v) 
    {
    }

    friend std::ostream& operator<<(std::ostream& os, int_w& v)
    {
        os << v._value;
        return os;
    }    

    // compare to self 
    bool operator< (const int_w& v) const
    {
        return _value < v._value; 
    }

    bool operator> (const int_w& v) const
    {
        return _value > v._value; 
    }

    bool operator== (const int_w& v) const
    {
        return _value == v._value; 
    }

    void *makeup(raw_type& _v)
    {
        _v = _value;
        return &_v;
    }
};

/*  String
 */
struct str : cm {
    ptr<char> _str;     // char string.

    int size()
    {
        return strlen(_str.addr());
    }

    const char* c_str() const
    {
        return _str.addr();
    }

    // support str compare to std::string 
    bool operator< (const std::string& v) const
    {
        return strcmp(_str.addr(), v.c_str()) < 0;
    }

    bool operator> (const std::string& v) const 
    {
        return strcmp(_str.addr(), v.c_str()) > 0;
    }
    bool operator== (const std::string& v) const
    {
        return strcmp(_str.addr(), v.c_str()) == 0;  
    }

    // compare to char*
    bool operator< (const char* v) const
    {
        return strcmp(_str.addr(), v) < 0;
    }

    bool operator> (const char* v) const 
    {
        return strcmp(_str.addr(), v) > 0;
    }
    
    // compare to str
    bool operator< (str& v) 
    {
        const char *p1 = _str.addr();
        const char *p2 = v.c_str();
        return strcmp(p1, p2) < 0;
    }

    bool operator> (str& v)
    {
        const char *p1 = _str.addr();
        const char *p2 = v.c_str();
        return strcmp(p1, p2) > 0;
    }
     bool operator== (str& v)
    {
        const char *p1 = _str.addr();
        const char *p2 = v.c_str();
        return strcmp(p1, p2) == 0;
    }

    friend std::ostream& operator<<(std::ostream& os, str& v)
    {
        os << v._str.addr();
        return os;
    }
};

#if 0  // TBD: std::string compare to str. 
inline bool operator> (const str& d, const std::string& s)
{
    return strcmp(d.c_str(), s.c_str()) > 0;
}

inline bool operator< (const str& d, const std::string& s)
{
    return strcmp(d.c_str(), s.c_str()) < 0;
}

inline bool operator== (const str& d, const std::string& s)
{
    return strcmp(d.c_str(), s.c_str()) == 0;
}

inline bool operator> (const std::string& s, const str& d)
{
    return strcmp(s.c_str(), d.c_str()) > 0;
}

inline bool operator< (const std::string& s, const str& d)
{
    return strcmp(s.c_str(), d.c_str()) < 0;
}

inline bool operator== (const std::string& s, const str& d)
{
    return strcmp(s.c_str(), d.c_str()) == 0;
}
#endif

// the string writer
struct str_w : _wT {
    /* all the strings metacache used will be writed to separate section.
     * string writed by char[] and closed by '\0'.
     * string table didn't align by word width.
     */
   
    typedef str raw_type;       // raw type in cache memory

    // represent a sz in cache_memory.
    std::string _sz;            // reference to key string
    uint64_t _hash;             // hash value in 64b
    char *_cm_ptr;              // pointer to chr[] in cache memory.
   
    std::vector<void *> _pending_slots;     // ptr<> slot in cache waiting update 
    
    // constructor
    str_w() : _hash(0), _cm_ptr(0)
    {
    }
    
    // global strs 
    static std::set<str_w> _set_str;

    str_w(const char* s) : _sz(s), _hash(0), _cm_ptr(0)
    {
        // calculate hash value
        _hash = city::CityHash64(s, strlen(s));    
    }

    str_w(const std::string& s) : _sz(s), _hash(0), _cm_ptr(0)
    {
        // calculate hash value
        _hash = city::CityHash64(s.c_str(), s.size());    
    }

    // show char[]
    const char*c_str()
    {
        return _sz.c_str();
    }
 
    friend std::ostream& operator<<(std::ostream& os, str_w& v)
    {
        os << v._sz;
        return os;
    }    
 
    // assignment operator 
    str_w& operator= (const std::string& s)
    {
        _sz = s;
        return *this;
    }

    str_w& operator= (const char* s)
    {
        _sz = s;
        return *this;
    }

    // relative operator
    bool operator< (const str_w& v) const
    {
        //return _sz < v._sz; 
        return strcmp(_sz.c_str(), v._sz.c_str()) < 0;
    }

    bool operator> (const str_w& v) const
    {
        //return _sz > v._sz; 
        return strcmp(_sz.c_str(), v._sz.c_str()) > 0;
    }

    bool operator== (const str_w& v) const
    {
        //return _sz == v._sz; 
        return strcmp(_sz.c_str(), v._sz.c_str()) == 0;
    }

    // calculate size, include '\0'
    int calc_size()
    {
        return _sz.length() + 1;
    }

    // makeup a string cache on specified address
    int makeup(void* m)
    {
        if (_cm_ptr) {
            return -1;
        }
        memcpy(m, _sz.c_str(), _sz.length()+1);
        _cm_ptr = (char *)m;
        return 0; 
    }

    void *makeup()
    {
        void *m= cm::_alloc(calc_size());
        makeup(m);
        return m;
    }

    void *makeup(raw_type& _v)
    {
        char *m = (char *)makeup();
        _v._str = m;
        return &_v;
    }
};

// pair
template <class T1, class T2>
struct pair {
    T1 first; 
    T2 second;

    pair() {}
    pair(const T1& _t1, const T2& _t2) : first(_t1), second(_t2) {}


    bool operator< (const pair& _v) const
    {
        return (first < _v.first) || \
            ((first == _v.first) && (second < _v.second));
    }
    bool operator> (const pair& _v) const 
    {
        return (first > _v.first) || \
            ((first == _v.first) && (second > _v.second));
    }

    template <typename U1, typename U2>
    bool operator< (const pair<U1, U2>& _v) const
    {
        return (first < _v.first) || \
            ((first == _v.first) && (second < _v.second));
    }
    template <typename U1, typename U2>
    bool operator> (const pair<U1, U2>& _v) const 
    {
        return (first > _v.first) || \
            ((first == _v.first) && (second > _v.second));
    }
};

// same as std::pair
template <typename T1, typename T2>
struct pair_w : _wT {
    T1 first;
    T2 second;
    
    typedef pair<typename T1::raw_type, typename T2::raw_type> raw_type;

    pair_w() {}
    pair_w(const T1& _t1, const T2& _t2) : first(_t1), second(_t2) {}

    bool operator< (const pair_w& _v) const
    {
        return (first < _v.first) || \
            ((first == _v.first) && (second < _v.second));
    }
    bool operator> (const pair_w& _v) const 
    {
        return (first > _v.first) || \
            ((first == _v.first) && (second > _v.second));
    }

    friend std::ostream& operator<<(std::ostream& os, pair_w& v)
    {
        os << v.first << "," << v.second;
        return os;
    }    


    void *makeup(raw_type& _v)
    {
        first.makeup(_v.first); 
        second.makeup(_v.second);
        return &_v;
    }
}; 

inline pair_w<str_w, str_w> kkeyw(const std::string& _t1, const std::string& _t2)
{
    return pair_w<str_w,str_w>(_t1, _t2);
}

// simple buffer storage 
struct buf : cm {
    ptr<lv> _lv;
 
    // return the buffer size (NOT whole LV size) 
    int size() const
    {
        return _lv->length();
    }

    // return the begin of buffer space.
    const void* buffer() const 
    {
       return _lv->buf(); 
    }

    buf(int size) 
    {
        lv *v = new(size) lv;
        _lv = v;
    }
};

// the buffer writer
struct buf_w : _wT {
    /* buf_w dosen't copy the data from buf,
     * shoule keep origin buffer valid until makeup.
     */

    typedef buf raw_type;    // raw type in cache memory.

    const void *_buf;   // the buffer pointer in memory.   
    uint32_t _size;     // the size of buffer.
    lv *_cm_lv;         // pointer to len-val structure in Cache memory.

    std::vector<void *> _pending_slots;     // ptr<> in cache waiting for update 

    buf_w() : _buf(0), _size(0), _cm_lv(0)
    {

    }

    buf_w(const void *buf, uint32_t size) : _buf(buf), _size(size), _cm_lv(0)
    {
         
    } 

    friend std::ostream& operator<<(std::ostream& os, buf_w& v)
    {
        os << "buf(" << v._size <<  ")"; 
        return os;
    }    
 
    int calc_size()
    {
        return lv::calc_size(_size);
    }

    int makeup(void* m)
    {
        lv *v = (lv*)m;
        v->copy(_buf, _size);
        _cm_lv = v;
        return 0;
    }

    void *makeup()
    {
        void *m= cm::_alloc(calc_size());
        makeup(m);
        return m;
    }

    void *makeup(raw_type& _v)
    {
        void *m = makeup();
        _v._lv = (lv *)m;
        return &_v;
    }

    // relative operator
    bool operator< (const buf_w& v) const
    {
        return _buf < v._buf; 
    }
};

/* Begin of containers 
 */

// Balance Factor MC_PTR 
// MUST aligned with 4 bytes, last 2b is for balance factor.
template <typename T>
struct bfptr : cm {
    mc_ptr _offset;

    /* bf : -2, -1, 0, 1
     */
#define MC_PTR_OFFS(vma) ((int64_t)vma - (int64_t)this)
#define MC_PTR_ADDR(offs) ((uint64_t)this + offs)
    bfptr() : _offset(MC_PTR_OFFS(0)) 
    {
        assert(((uint64_t)this&3) == 0);
    }
    bfptr(T* v) : _offset(MC_PTR_OFFS(v)) 
    {
        assert(((uint64_t)v&3) == 0);
    }
    bfptr(mc_ptr v) : _offset(MC_PTR_OFFS(v)) 
    {
        assert((v&3) == 0);
    }

#define MC_BFPTR_VAL(x)    ((x)&~(3))
#define MC_BFPTR_BF(x)      ((x)&3)

   // only for Debug 
    void dbg() 
    { 
        printf("  %s:%d addr(%p), offset(%d)\n", __func__, __LINE__, addr(), _offset); 
    };
    
    // ptr to _vmem means null. 
    inline bool isNull()
    {
        return (int32_t)MC_PTR_OFFS(0) == (int32_t)MC_BFPTR_VAL(_offset);
    }

    void set_bf(int bf) 
    {
        mc_ptr l = 0; 
        if (bf < -1) {
            l = 3;  // b11 -1
        } else if (bf == -1) {
            l = 2;  // b10 -2
        } else if (bf >= 1) {
            l = 1;  // b01 1
        }  
        _offset = (MC_BFPTR_VAL(_offset)) | (MC_BFPTR_BF(l)); 
    }

    int bf() 
    {
        mc_ptr l = _offset & 3; 
        if (l == 1) { // b01 1
            return 1;  
        } else if (l == 2) { // b10 -2
            return -1;
        } else if (l == 3) { // b11 -1
            return -2;
        }
        return 0;
    }

    // get real pointer 
    T* addr() 
    { 
        void *ptr = (void *)MC_PTR_ADDR(MC_BFPTR_VAL(_offset));
        if (isNull()) {
            return 0;
        }
        return (T *)ptr;
    }

    // get offset value 
    mc_ptr offset() 
    { 
        return _offset; 
    }

    // overloaded assignment (from pointer)
    void operator=(T* v)
    { 
        _offset = MC_PTR_OFFS(v); 
    }

    // overloaded assignment (from ptr<T>)
    void operator=(bfptr& p)
    {
        mc_ptr bf = p.bf(); 
        T* v = p.addr();
        _offset = MC_PTR_OFFS(v) | bf;
    }

    // overloaded '->' for directly access 
    T* operator->() {
        assert(!isNull());
        return (T *)MC_PTR_ADDR(MC_BFPTR_VAL(_offset));
    }
  
}; 

// binary tree 
template <typename _K, typename _V>
struct bintree : cm {
    // type defination
    typedef _K key_type;
    typedef _V data_type;
    //typedef pair< _K, _V > value_type;

    // node object in 
    struct node : cm {          // Node in tree
        ptr<node> _left, _right;    // left and right children
        //value_type _value;          // key and value storing in pair<K,V>
        key_type _key;
        data_type _data;

        // constructor
        node() 
        {
        } 
       
        // constructor with data 
        node(key_type& key, data_type& value) 
        {
            _key = key; 
            _data = value; 
        } 

        // constructor with data 
        node(key_type key, data_type value) 
        {
            _key = key; 
            _data = value; 
        } 
    };

    // binary tree header.
    ptr<node> _root;                // pointer to first node 
    node _nodes[0];                 // all nodes

    // find bintree, return pointer to value.
    const data_type* find(const key_type& k)
    {
        const node *cur = _root.addr();
       
        while (cur)
        {   
            // left < right
            if (k < cur->_key) {
                cur = cur->_left.addr();
            } else if (k > cur->_key) {
                cur = cur->_right.addr(); 
            } else { // equal
                return &cur->_data;  
            }
        }
        
        // not found
        return NULL; 
    }
     
    const data_type* find(const std::string& k)
    {
        const node *cur = _root.addr();
       
        while (cur)
        {   
            // left < right
            if (cur->_key > k) {
                cur = cur->_left.addr();
            } else if (cur->_key < k) {
                cur = cur->_right.addr(); 
            } else { // equal
                return &cur->_data;  
            }
        }
 
        return NULL;
    }

    const data_type* find(const std::string& k1, const std::string& k2)
    {
        const node *cur = _root.addr();
        
        pair<std::string, std::string> key(k1, k2);

        while (cur)
        {  
            // left < right
            if (cur->_key > key) {
                cur = cur->_left.addr();
            } else if (cur->_key < key){
                cur = cur->_right.addr(); 
            } else { // equal
                return &cur->_data;  
            }
        }
 
        return NULL;
    }

};

// Hash Table
template <typename _K, typename _V>
struct hashtbl : cm {
    // TBD
};

// the shadow avltree writer
template <typename _K, typename _V>
struct avltree_w {

    // type declare
    typedef _K  key_type;
    typedef _V  data_type;
   
    // raw memory type declare 
    typedef typename key_type::raw_type     raw_key_type;
    typedef typename data_type::raw_type    raw_data_type;
    typedef bintree<raw_key_type, raw_data_type> bintree_type;


    // tree node holder
    struct node_w {
        node_w *_left, *_right;     // children
        node_w *_parent;            // parent node
        int _height;

        key_type   _key;            // key
        data_type  _data;           // value

        typename bintree_type::node *_mc_node;  // metacache node

        // constructor
        node_w(const key_type& k) : _left(0), _right(0), _parent(0), _mc_node(0)
        {
            _key = k;
        }

    };

    // avltree 
    node_w *_root;      // root node
    int _size;          // how many nodes in tree.

    
    // avltree constructor
    avltree_w() : _root(0), _size(0)
    {
        
    }
    
    // get height from root-node
    int _height (node_w *rn)
    {
        if (rn == NULL) {
            return 0;       // blank tree has 0 height.
        }

        return rn->_height;
    }
  
    // four rotations 
    node_w *_rotation_LL(node_w *rn)
    {
        node_w* n;

        //       right rotate
        //   rn             n 
        //  n      ==>    l   rn
        // l r               r
        n = rn->_left;
        rn->_left = n->_right;
        n->_right = rn;

        n->_parent = rn->_parent;
        rn->_parent = n;
        if (rn->_left)
            rn->_left->_parent = rn;

        rn->_height = MAX(_height(rn->_left), _height(rn->_right)) + 1;
        n->_height = MAX(_height(n->_left), rn->_height) + 1;
        return n; 
    }
    
    node_w *_rotation_RR(node_w *rn)
    {
        node_w* n;
       
        //        left rotate
        //   rn              n
        //     n    ==>   rn   r 
        //    l r           l 
        n = rn->_right;
        rn->_right = n->_left;
        n->_left = rn;

        n->_parent = rn->_parent;
        rn->_parent = n;
        if (rn->_right)
            rn->_right->_parent = rn;

        rn->_height = MAX(_height(rn->_left), _height(rn->_right)) + 1;
        n->_height = MAX(_height(n->_right), rn->_height) + 1;
        return n;
    }

    node_w *_rotation_LR(node_w *rn)
    {
        rn->_left = _rotation_RR(rn->_left);
        return _rotation_LL(rn);
    }

    node_w *_rotation_RL(node_w *rn)
    {
        rn->_right = _rotation_LL(rn->_right);
        return _rotation_RR(rn);
    }

    node_w* _insert(node_w* &tree, const key_type& key, node_w* &found)
    {
        if (tree == NULL) {
            // new node
            tree = new node_w(key);
            assert(tree);
            found = tree;
            _size++;

        } else if (key < tree->_key) {  // left < right

            // insert to left tree
            tree->_left = _insert(tree->_left, key, found);    // recursive
            if (!tree->_left->_parent)
                tree->_left->_parent = tree;

            // balance the tree
            if ((_height(tree->_left) - _height(tree->_right)) == 2) {
                if (key < tree->_left->_key) {
                    tree = _rotation_LL(tree);
                } else {
                    tree = _rotation_LR(tree);
                }
            }
        } else if (key > tree->_key) {

            // insert to right tree
            tree->_right = _insert(tree->_right, key, found);  // recursive
            if (!tree->_right->_parent)
                tree->_right->_parent = tree;
           
            // balance the tree 
            if (_height(tree->_right) - _height(tree->_left) == 2) {
                if (key > tree->_right->_key) {
                    tree = _rotation_RR(tree);
                } else {
                    tree = _rotation_RL(tree);
                }
            } 
        } else { // equal
            // nothing changed
            found = tree;
        }

        // calculate new height 
        tree->_height = MAX(_height(tree->_left), _height(tree->_right)) + 1;
        return tree;
    }

    void _print(node_w* tree, key_type& key, int direction)
    {
        if(!tree) {
            return;
        }

        if(direction) { // left:0 , right:1 
            std::cout << tree->_key << " is " <<  key << "'s "  << (direction==1?"right child" : "left child") << std::endl;
            if (tree->_parent)
                std::cout << tree->_key << "'s parent is " << tree->_parent->_key << std::endl;
        } else { 
            std::cout << tree->_key << " is root" << std::endl;
        }

        _print(tree->_left, tree->_key, -1);
        _print(tree->_right,tree->_key, 1);
    }

    /*
     * public methods
     */
    // 
    data_type& insert(const key_type& key)
    {
        node_w *found;
        _insert(_root, key, found);
        //_print_node(*found);
        return found->_data;
    }

    // using [] for convenience
    // eg. a[k] = v;
    data_type& operator[] (const key_type& k)
    {
        //printf(" avltree_w[%s]\n", k.c_str());
        return insert(k); 
    }

    // debug print 
    void debug_print()
    {
        if (!_root) {
            return;
        }
        _print(_root, _root->_key, 0);
    }
    
    // total node count
    int size()
    {
        return _size;
    }

    int calc_size()
    {
        // 4 + 16*count
        int size = sizeof(bintree_type) + (sizeof(typename bintree_type::node) * _size);
        return size;
    }

    typedef void (*pfWalkCB)(node_w& n);
    static void _print_node(node_w& n) 
    {
        
        std::cout << n._key << ": ";
        if (n._parent)
            std::cout << "P(" << n._parent->_key << ") ";
        if (n._left)
            std::cout << "L(" << n._left->_key << ") "; 
        if (n._right)
            std::cout << "R(" << n._right->_key << ") " ;
        std::cout << "= " << n._data << std::endl;
    }

    void walk_inorder(pfWalkCB fn = _print_node)
    {
        node_w *ptr = _root; 

        while (ptr && ptr->_left) {
            ptr = ptr->_left;
        }
        // reach most left

        while (ptr)
        {
            /*                    7
             *             4             13
             *          2     6      11      15 
             *         1 3   5     9   12  14  16
             *                    8 10
             */
            (*fn)(*ptr);
            if (ptr->_right) {
                ptr = ptr->_right;
                while (ptr->_left) {
                    ptr = ptr->_left;
                }
            } else {
                node_w* p = ptr->_parent;
                while (p && p->_right == ptr) {
                    ptr = p;
                    p = p->_parent; 
                }
                if (ptr->_right != p)
                    ptr = p;
            }
        }
    }

    void walk_preorder(pfWalkCB fn = _print_node)
    {
        node_w*ptr = _root;

        while (ptr) {
            (*fn)(*ptr);
            if (ptr->_left) {
                ptr = ptr->_left;
            } else if(ptr->_right) {
                ptr = ptr->_right;
            } else {
                node_w *p = ptr->_parent;
                while (p) {
                    if ( ptr == p->_left && p->_right) { // ptr is p's left child
                        ptr = p->_right;
                        break; 
                    } else { // ptr is p's right child
                        ptr = p;
                        p = p->_parent;
                    }
                }
                if (!p) {
                    ptr = p;
                }
            }
        }
    }

    void walk_postorder(pfWalkCB fn = _print_node)
    {
        // TBD
    }

    void _makeup(typename bintree_type::node *m)
    {
        using list_node_type = std::list<node_w*>;
        list_node_type ln, ln_pending;
        
        if (!_root)
            return;
      
        typename bintree_type::node *nod = m; 

        // add first entry
        ln.push_back(_root);
        
        for(;;) {
            while(!ln.empty()) 
            {
                node_w *p = ln.front();

                //_print_node(*p);
                p->_mc_node = nod;

                // makeup key and data
                p->_key.makeup(nod->_key);
                p->_data.makeup(nod->_data);

                // has child, add to next round.
                if (p->_left) {     
                    ln_pending.push_back(p->_left);
                }
                if (p->_right) {
                    ln_pending.push_back(p->_right);
                }
                
                // has parent, update parent's child to me.
                if (p->_parent) {   
                    typename bintree_type::node *pn = p->_parent->_mc_node;
                    if (p == p->_parent->_left) {
                        pn->_left = nod;
                    } else {
                        pn->_right = nod;
                    }
                }
                ln.pop_front(); // pop 
                nod++; 
            }

            if (ln_pending.empty()) { // exit condition
                break;
            }

            ln.swap(ln_pending);
        }
    }

    // makeup the bintree in metacache 
    void makeup(void *_m)
    {
        bintree_type *r = (bintree_type*)_m;
        _makeup(r->_nodes);
        r->_root = r->_nodes;
    } 

    void *makeup()
    {
        void *m = cm::malloc(calc_size());
        makeup(m);
        return m; 
    }
};

// kv cache reader 
template <typename _K, typename _V>
struct kv {
    typedef _K      key_type;
    typedef _V      data_type;

    typedef bintree<_K,_V>  bintree_type;

    bintree_type *_tree;

    kv (const void *_m) : _tree((bintree_type*)_m)
    {

    }

    const data_type* find(const std::string& _k)
    {
        return _tree->find(_k);
    }

};

// KV cache writer
template <typename _K, typename _V>
struct kv_w : _wT {
    typedef _K  key_type;
    typedef _V  data_type;

    // raw memory type declare 
    typedef typename key_type::raw_type     raw_key_type;
    typedef typename data_type::raw_type    raw_data_type;
    
    typedef avltree_w<_K, _V>  writer_type;

    // avltree
    writer_type _tree;

    kv_w()
    {
    }

   data_type& operator[] (const key_type& key)
    {
        return _tree.insert(key);
    }

    int size()
    {
        return _tree.size();
    }

    // return cache memory size.
    int calc_size()
    {
        return _tree.calc_size();
    }

    void* makeup()
    {
        return _tree.makeup(); 
    }
};

// kv cache reader 
template <typename _K1, typename _K2, typename _V>
struct kkv {
    typedef _K1     key1_type;
    typedef _K2     key2_type;
    typedef _V      data_type;

    typedef bintree<pair<_K1,_K2>,_V>  bintree_type;

    bintree_type *_tree;

    kkv (const void *_m) : _tree((bintree_type*)_m)
    {
         
    }

    const data_type* find(const std::string& _k1, const std::string& _k2)
    {
        return _tree->find(_k1, _k2);
    }
};


// KKV cache writer
template <typename _K1, typename _K2, typename _V>
struct kkv_w : _wT {
    typedef _K1  key1_type;
    typedef _K2  key2_type;
    typedef _V  data_type;

    // raw memory type declare 
    typedef typename key1_type::raw_type    raw_key1_type;
    typedef typename key1_type::raw_type    raw_key2_type;
    typedef typename data_type::raw_type    raw_data_type;
    
    typedef avltree_w<pair_w<_K1, _K2>, _V>   writer_type;

    // avltree
    writer_type _tree;

    kkv_w()
    {
    }

    data_type& operator[] (const pair_w<key1_type, key2_type>& key)
    {
        return _tree.insert(key);
    }

    int size()
    {
        return _tree.size();
    }

    // return cache memory size.
    int calc_size()
    {
        return _tree.calc_size();
    }

    void* makeup()
    {
        return _tree.makeup(); 
    }
};


template<typename _T>
struct arr
{
    uint32_t _size;
    _T _array[0];

    arr() : _size(0) {}

    int size()
    {
        return _size;
    }

    _T& at(int _i)
    {
        return _array[_i]; 
    }

    _T& operator[] (int _i)
    {
        return at(_i); 
    }
};

template<typename _T>
struct arr_w
{
    typedef typename _T::raw_type raw_type;

    std::vector<_T> _vector;

    int size()
    {
        return _vector.size();
    }

    _T& at(int _i)
    {
        return _vector[_i];
    }

    void push_back(const _T& _v)
    {
        _vector.push_back(_v); 
    }  
    
    _T& operator[] (int _i)
    {
        return _vector[_i];
    }

    int calc_size(int count)
    {
        return sizeof(arr<_T>) + (sizeof(_T)*count);
    }

    int makeup(void* _m)
    {
        return 0;
    }
};

// metacache in raw memory.
struct metacache : cm {

    // holds the entry of metacache.
    #define MC_HDR_MAGIC "STONE_METACACHE"
    const char magic[16] = MC_HDR_MAGIC; // MC_HDR_MAGIC

    // sections table
    ptr<char> slots[16];

    // create a new metacache
    static metacache *create(uint32_t size) 
    {
        void *ptr = cm::mmap((char*)MC_DEF_VMADDR, size);   
        assert(ptr);
        return new metacache();
    }

    // load a existed metacache
    static metacache *load(const char* file) 
    {
        metacache* mc = (metacache *)cm::mmap((char*)MC_DEF_VMADDR, file);
        assert(mc);
        printf("metacache loaded on %p.\n", mc);
        return mc;
    }

    // save metacache to file
    static int savefile(const char* file)
    {
        return cm::dump(file);
    }

    const void* get_slots(int _index)
    {
        return slots[_index].addr();
    }

    int set_slots(int _index, const void *_vma)
    {
        if (_index < 0 || _index > 15) {
            return -1;
        }       

        slots[_index] = (char*)_vma;
        printf("%d: %p, offset(%d)\n", _index, _vma, slots[_index].offset());
        return 0;
    }

};

struct lstat {
    ptr<lv> _data;              // cached file context
    //uint16_t mode;          // rwx r--r--r--(444)
    //uint16_t uid;           // owner of user-id === root(0)
    //uint16_t gid;           // owner of group-id === root(0) 
    //uint16_t num_dentry;    // how many dentries the inode refered to
};

struct inode {
    ptr<char> _name;            // name of the file/dir
    ptr<lstat> _stat[0];        // 
    //uint32_t _size:28;           // file size
    //uint32_t _type:4;            // file, dir, symbollink ...
};

class real_path_scope {
    std::string _path;
    const char *_real_path;
    int _errno;
    const char *_delim = "/";   // linux path
public:
    real_path_scope(const char *p): _path(p) {
        _real_path = realpath(p, NULL);
        //_real_path = canonicalize_file_name(p);
        _errno = errno; 
    }

    ~real_path_scope() {
        if (_real_path)
            ::free((void *)_real_path);
    }

    const char* path()
    {
        return _path.c_str(); 
    }

    const char* real_path()
    {
        return _real_path;
    }

    int error()
    {
        return _errno;
    }

    bool entry_check(std::string& entry)
    {
        if (!_real_path) {
            return false;
        }
        std::string s(_real_path);
        if (s.length() > entry.length() &&
                s.find(entry) == 0) {
            return true;
        }
        return false;
    }
    
    // split path to names
    bool entry_split(std::string& entry, std::vector<std::string>& vpath)
    {
        if (!_real_path) {
            return false;
        }

        std::string s(_real_path);
        size_t start = 0; // _entry_realpath.length();
        size_t end = entry.length();
        while ((start = s.find_first_not_of(_delim, end)) != std::string::npos)
        {
            end = s.find(_delim, start);
            vpath.push_back(s.substr(start, end - start));
        }
        return true;
    }
};

// hold a inode in mc writer 
struct inode_w {
    uint32_t _ino;
    std::string _name;
    std::string _real_path;
    struct stat64 _stat;
    uint32_t _parent_ino;
    inode_w() {}
    inode_w(uint32_t n) : _ino(n)
    {
        
    }
};

#include <dlfcn.h>

#if defined(RTLD_NEXT)
#  define REAL_LIBC RTLD_NEXT
#else
#  define REAL_LIBC ((void *) -1L)
#endif

struct fs_op {
    typedef int fn_xstat64(int ver, const char * path, struct stat64 * stat_buf);
    typedef int fn_lxstat64(int ver, const char * path, struct stat64 * stat_buf);
    typedef int fn_open64(const char *pathname, int flags, ...);
    typedef ssize_t fn_read(int fd, void *buf, size_t count);
    typedef ssize_t fn_pread64(int fd, void *buf, size_t count, off_t offset); 
    typedef long fn_syscall(long number, ...);

    fn_xstat64  *_real_xstat64;
    fn_lxstat64 *_real_lxstat64;
    fn_open64   *_real_open64;
    fn_read     *_real_read;
    fn_pread64  *_real_pread64;
    fn_syscall  *_real_syscall;
    
    template <typename T>
    void _hook(const char* fn, T *& pf)
    {
        pf = (T*) dlsym(REAL_LIBC, fn);
    }

    void inject()
    {
         
    }
};

// fs entry, holds a dir entry cache.
struct fs_w : _wT {
    std::string _entry;                     // the entry dir name
    std::string _entry_realpath;            // the entry dir real path
    
    typedef std::pair<uint32_t, std::string> tree_key_t;
    typedef std::map<tree_key_t, inode_w*> inode_tree_t;
    inode_tree_t _tree;                     // fs tree
    
    std::vector<inode_w*> _tbl_inodes;      // all inodes
    int _ino_next;                          // next inode index, from 1
    inode_w * _root;                        // root inode

    /* 
     * Please take attention to MT-safe
     */
    
    struct op_cnt
    {
        uint32_t _stat;
        uint32_t _open;
        
        op_cnt() : _stat(0), _open(0) 
        {
        }
    };

    std::map<std::string, op_cnt> _records;

    fs_w(const char* d) : _entry(d), _ino_next(1), _root(0)
    {
        real_path_scope rp(d);
        _entry_realpath = rp.real_path();

        // add root inode to tree
        _root = new inode_w(_ino_next++); 
        _root->_name = _entry;
        _root->_real_path = _entry_realpath;
        tree_key_t key = std::make_pair(0, _entry);
        _tree[key] = _root;
    }

    // TBD: change to shared_ptr
    inode_w* get_inode(std::vector<std::string>& vpath)
    {
        inode_w* parent = _root;
        inode_w* file;
        for (size_t i=0; i<vpath.size(); i++) {
            tree_key_t key = std::make_pair(parent->_ino, vpath[i]);
            inode_tree_t::iterator it = _tree.find(key);
            if (it == _tree.end()) {
                file = new inode_w(_ino_next++); 
                file->_name = vpath[i];
                file->_parent_ino = parent->_ino;
                _tree[key] = file;
            } else {
                file = it->second;
            }
            parent = file;
        }
        return file;
    }

    // cache for stat 
    int stat(const char* path)
    {
        real_path_scope rp(path);
        if (rp.entry_check(_entry_realpath)) {
            _records[rp.real_path()]._stat++;
        
            std::vector<std::string> vpath; 
            if(rp.entry_split(_entry_realpath, vpath)) {
                inode_w* f = get_inode(vpath);
                f->_real_path = rp.real_path();
                std::cout << f->_name << std::endl;
            }
        }
#if 0
        const char *real_path = rp.real_path();
        if (!real_path) {
            return rp.error();
        }
        
        std::string s(real_path);
        const char *delim = "/";
        if (s.find(_entry_realpath) == 0)  // match 
        {
            //printf("%s: %s\n", __func__, path);
            size_t start = 0; // _entry_realpath.length();
            size_t end = _entry_realpath.length();
            while ((start = s.find_first_not_of(delim, end)) != std::string::npos)
            {
                end = s.find(delim, start);
                std::cout << " " <<  s.substr(start, end - start);
            }
            std::cout << std::endl;
        }
#endif
        return 0;
    }

    // cache for read 
    int open(const char *path)
    {
        real_path_scope rp(path);
        if (rp.entry_check(_entry_realpath)) {
            _records[rp.real_path()]._open++;
        }
        return 0;
    }

#if 0
    int readdir(const char *path)
    {
        return 0;
    }
#endif

    void makeup()
    {
         
    }

    void debug_print()
    {
        // show records table
        printf("Records table: %u\n", _records.size());
        printf("%4s %4s %s\n", "STAT", "OPEN", "FILE_PATH");
        auto it = _records.begin();
        for (; it != _records.end(); it++) {
            printf("%4u %4u %s\n", it->second._stat, it->second._open, it->first.c_str());
        }

        // show tree
        auto it2 = _tree.begin();
        for (; it2 != _tree.end(); it2++) {
            printf("(%u,%s), %u, %s\n", it2->first.first, it2->first.second.c_str(), it2->second->_ino, it2->second->_real_path.c_str());
        }
    }
};

struct fsal {

    int _pathat(const std::string& path)
    {
        return 0;  // inode
    }

    int xstat64(int ver, const char *path, struct stat64 *buf)
    {
        
        return 0;
    }
};

// hold the metacache entry
struct metacache_w : _wT {
    /*  
     * write all data with relation to host memory,
     * then makeup a metacache in cache memory.
     */

    // represent a cache section
    struct sec_w {
        mc_sec_type _type;              // indicate the which reader to read the section.
        void *_cm_start;                // start address of the section in cache memory.
        uint32_t _size;                 // size of the section, aliged word width. 
    };

    struct sec_kv {
        const std::string _name;
        void *_cm_start;
        uint32_t _size; 
        
        sec_kv(const std::string& table, void *start, int size) : 
            _name(table), _cm_start(start), _size(size)
        {
            
        }

    };

    std::set<str_w>  _set_str;          // string table writer
    std::set<buf_w>  _set_buf;          // buffer table writer
    std::set<sec_kv> _set_kv;

    // add string to table
    const str_w& str(const std::string& s)
    {
        str_w w(s);
        auto it = _set_str.insert(w);
        printf(" wc::str('%s'), hash(%lx), cmptr(%p)\n", w._sz.c_str(), w._hash, w._cm_ptr);
        return *it.first;
    }

    // add buffer to table
    const buf_w& buf(const void* buf, int len)
    {
        buf_w w(buf, len);
        auto it = _set_buf.insert(w);
        return *it.first;
    }

    // add kv_table
    template <class _K, class _V>
    int create_kv(kv_w<_K, _V>& _kv)
    {
        return 0;
    }

    // make the whole data to cache memory.
    void makeup()
    {
         
    }
};

}; // namespace metacache
}; // namespace strontium

#endif // _STRONTIUM_METACACHE_H_
