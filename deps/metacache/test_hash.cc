#include "metacache.h"
#include "city.h"

using namespace strontium::metacache;

void do_test()
{
    int i;
    for (i=1; i<10*1024*1024; i+=3)
    {
        std::string a('a', i);
        uint64_t h1 = CityHash64(a.c_str(), a.size());
        uint64_t h2 = city::CityHash64(a.c_str(), a.size());
        assert(h1 == h2);

        uint128_t m1 = CityHash128(a.c_str(), a.size());
        uint128_t m2 = city::CityHash128(a.c_str(), a.size());
        assert(m1 == m2);
    }
}

int main()
{
    do_test();
    return 0;
}

