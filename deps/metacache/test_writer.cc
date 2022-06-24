#include "metacache.h"
#include <sys/time.h>

using namespace strontium::metacache;

MC_GLOBAL();

#if 0
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

static double timediff(struct timeval &a, struct timeval &b)
{
    struct timeval result = _utime_diff(a, b); 
    double gap = result.tv_sec + result.tv_usec / 1000000.0;
    return gap;
}
#endif

void test_sz(metacache_w* wc)
{
    const str_w& w1 = wc->str("abcd");
    const str_w& w2 = wc->str("abcd");
    printf("w1:%p, w2:%p\n", &w1, &w2);
    assert(&w1 == &w2);
    str_w w3 = w1;

    // TEST 2
    {
        str_w s1("abcdef");
        assert(s1.calc_size() == 7);
       
        void* ptr = cm::_alloc(7);
        s1.makeup(ptr);
        assert(memcmp(ptr, s1.c_str(), 7) == 0);
    } 
}

void test_buf(metacache_w* wc)
{
    {
        const buf_w& w1 = wc->buf("aaa", 3);
        const buf_w& w2 = wc->buf("bbb", 3);
        printf("w1:%p, w2:%p\n", &w1, &w2);
    }

    // TEST 2
    {
        buf_w s1("aaa", 3);
        void* ptr = cm::_alloc(4);
        s1.makeup(ptr);

        lv *c1 = (lv*)ptr; 
        assert(c1->length() == 3);
    }
}

void test_int(metacache_w* wc)
{
    int_w a(1);
    int b(0);
    a.makeup(b);
    assert(b==1);
}

void test_city()
{
    uint64_t hash = city::CityHash64("aaaa", 4);
    printf("%lx\n", hash);
    uint128_t h2 = city::CityHash128("aaaa", 4);
    printf("%lx, %lx\n", h2.first, h2.second);
}

void test_avltree(metacache_w* wc)
{
    // TEST basic int
    using my = avltree_w<int_w, int_w>;
    my t;
    // test nodes
    int arr[]= {3,2,1,4,5,6,7,16,15,14,13,12,11,10,8,9};
    for (int i=0; i<(int)sizeof(arr)/4; i++) {
        t.insert(arr[i]) = i;
        //t[i] = i;
    }
    assert(t._root->_height == 5);
    //t.debug_print();
    t.walk_inorder();
    //t.walk_preorder();

    int size = t.calc_size(); 
    printf("calc_size:%d\n", size);

    void *m = t.makeup();
    bintree<int, int> *rd=(bintree<int,int>*)m;
    for (int i=1; i<=16; i++) {
        std::cout << rd->find(i) << ":" << *rd->find(i) << std::endl;
    }
    assert(rd->find(0) == NULL);
    assert(rd->find(100) == NULL);
}

void test_avltree2(metacache_w* wc)
{
    // TEST large tree
    using my = avltree_w<int_w, int_w>;
    
    // 1million test
    my t;
    {
        ts_scope a("avltree add");
        for (int i=1; i<=1000000; i++) {
            t.insert(i) = i;
        }
    }
    assert(t._root);
    
    printf("t.height = %d\n", t._root->_height);
    assert(t._root->_height == 20);
    void *tm = t.makeup();

    // 1million test
    std::map<int, int> t2;
    {
        ts_scope a("std::map add");
        for (int i=0; i<1000000; i++) {
            t2[i] = i;
        }
    }

    // 1m avl search
    bintree<int, int> *rd = (bintree<int,int>*)tm;
    {
        ts_scope a("avltree search");
        for (int i=1; i<=1000000; i++) {
            assert(*rd->find(i) == i);
        }
    }

    // 1m map search 
    {
        ts_scope a("std::map search");
        for (int i=0; i<1000000; i++) {
            assert(t2[i] == i);
        }
    }
}

void test_avltree3(metacache_w* wc)
{
    // TEST str kv 
    using my = avltree_w<str_w, str_w>;
    my tr; 
    tr["a"] = "apple";
    tr["b"] = "box";
    tr["c"] = "cat";
    tr["d"] = "dog";
    tr["e"] = "egg";
    tr["f"] = "food";

    void *m = tr.makeup();
    bintree<str,str> *rd= (bintree<str, str>*)m;
    
    assert(rd->find("food") == NULL);
    assert(*rd->find("a") == "apple");
    assert(*rd->find("e") == "egg");

    {
        str *a = (str*)cm::malloc(4);
        std::string b("apple");
        str_w c(b);
        c.makeup(*a);
        
        assert(*a < "box");
        assert(*a == b);

        str *a2 = (str*) cm::malloc(4);
        std::string b2("box");
        str_w c2(b2);
        c2.makeup(*a2);

        assert((*a < *a2) == true);
        assert((*a > *a2) == false);
        assert((*a == *a2) == false);
    }
}

void test_avltree4(metacache_w* wc)
{
    // TEST str kkv 
    using my = avltree_w<pair_w<str_w, str_w>, str_w>;
    my tr;
    tr[kkeyw("zhejiang", "hangzhou")] = "0571"; 
    tr[kkeyw("zhejiang", "jiaxing")] = "0573";
    tr[kkeyw("shanghai", "shanghai")] = "021";
    tr[kkeyw("guangdong", "guangzhou")] = "020";
    void *m = tr.makeup();

    using reader = bintree<pair<str, str>, str>;
    reader *rd = (reader*)m;

    std::string hz = rd->find("zhejiang","hangzhou")->c_str();
    std::cout << hz << std::endl;

    assert(*rd->find("zhejiang", "jiaxing") == "0573");
    assert(*rd->find("guangdong", "guangzhou") == "020");
    assert(*rd->find("shanghai", "shanghai") == "021");
    assert(rd->find("jiangsu", "suzhou") == NULL);
    assert(rd->find("zhejiang", "ningbo") == NULL);
}

void test_avltree5(metacache_w* wc)
{
    using my = avltree_w<int_w, str_w>;
    my tr;
    tr[1] = "/";
    tr[2] = "node_modules";
    void *m = tr.makeup();
    
    using reader = bintree<int, str>;
    reader *rd = (reader*)m;
    std::cout << "inode: 1 is " << rd->find(1)->c_str() << std::endl;
}

void test_array(metacache_w* wc)
{
    arr_w<int_w> a1;  
    a1.push_back(1);
    a1.push_back(2);
    a1.push_back(3);
    assert(a1.size() == 3);
    assert(a1[0]._value == 1);
    assert(a1.at(1)._value == 2);
    assert(a1.at(2)._value == 3);
}

void do_test()
{
    test_city(); 
    
    metacache_w *wc = new metacache_w();
    metacache::create(100*1024*1024);// 100M
    assert(wc);
    test_int(wc);
    test_sz(wc);
    test_buf(wc);
    test_avltree(wc);
    test_avltree2(wc);
    test_avltree3(wc);
    test_avltree4(wc);
    test_avltree5(wc);
    test_array(wc);
}

int main()
{
    do_test();
    return 0;
}
