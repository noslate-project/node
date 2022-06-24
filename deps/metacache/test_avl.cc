#include <assert.h>
#include <string.h>
#include "metacache.h"

using namespace strontium::metacache;

MC_GLOBAL();

void test_malloc(metacache *mc)
{
    // TEST, malloc always aligned
    char *a = (char*)cm::malloc(4); 
    assert(((uint64_t)a&0x3) == 0);

    // TEST _alloc() without align 
    char *c1 = (char*)cm::_alloc(1);
    assert(((a + 1) == c1) == 0);
    char *c2 = (char*)cm::_alloc(1);
    assert(((a + 2) == c2) == 0);
    char *c3 = (char*)cm::_alloc(1);
    assert(((a + 3) == c3) == 0);
    // leave a byte

    // TEST malloc aligned 
    char *b = (char*)cm::malloc(4); 
    assert(((uint64_t)b&0x3) == 0);
    assert(((a + 4) == b) == 0);
}

void test_ptr(metacache *mc)
{
    struct t1 : cm {
        ptr<t1> parent, left, right;
        int i;

        t1(int x): i(x) {}
    };

    // template ptr<any> should only 4bytes.
    assert(sizeof(ptr<t1>) == 4);

    // [TEST 1] not allow alloc ptr in stack. (runtime error)
    //ptr<t1> n;


    // [TEST 2] convert mc_ptr to ptr.
    int *x1 = (int *)cm::malloc(sizeof(int));   // create a int in cache memory
    ptr<int> *x2 = new ptr<int>;                // create a ptr<int> in cm
    *x2 = x1;                                   // ptr<int> pointer to x1
    // now the x2 pointer pointed to x1 in cache memory.
    assert(x2->addr() == x1);

    // [TEST 3] test ptr assign
    ptr<int> *x3 = new ptr<int>;    // get a new pointer
    *x3 = *x2;
    assert(x2->addr() == x3->addr()); 

    // [TEST 4] struct referentce
    t1 *a = new t1(1);  //new 3 t1 struct holds in a,b,c 
    t1 *b = new t1(2);  
    t1 *c = new t1(3);

    /*
     *     a 
     *    / \  
     *   b   c  
     */
    b->parent = a;  // set reference 
    c->parent = a;
    a->left = b;
    a->right = c; 
   
    b->parent.dbg(); 
    c->parent.dbg(); 
    a->left.dbg(); 
    a->right.dbg(); 
   
    // check pointer 
    assert(a->parent.addr() == NULL);
    assert(a->left.addr() == b);
    assert(a->right.addr() == c);
    assert(b->parent.addr() == a);
    assert(b->left.addr() == NULL);
    assert(b->right.addr() == NULL);
    assert(c->parent.addr() == a);
    assert(c->left.addr() == NULL);
    assert(c->right.addr() == NULL);

    // [TEST 5] compare
    int *y1 = (int *)cm::malloc(sizeof(int));   // create a int in cache memory
    int *y2 = (int *)cm::malloc(sizeof(int));   // create a int in cache memory
    ptr<int> *py1 = new ptr<int>;                // create a ptr<int> in cm
    ptr<int> *py2 = new ptr<int>;                // create a ptr<int> in cm
    *py1 = y1;
    *py2 = y2;
    *y1 = 100;
    *y2 = 101;
    assert(*py1 < *py2); 
    assert(*py2 > *py1); 
}

void test_lv(metacache *mc)
{
    int rc;
    
    // [TEST 1] size accuracy 
    uint32_t a = 0;
    lv::u_lv *u = (lv::u_lv *)&a;
    u->L1.b = LV_L1_B;
    u->L1.l = 127; 
    printf("l1: 0x%x, %x, %d\n", a, a&1, a>>1);
    assert((a&1) == 0);
    assert(a>>1 == 127);
    u->L2.b = LV_L2_B;
    u->L2.l = 10*1024;
    printf("l2: 0x%x, %x, %d\n", a, a&3, a>>2);
    assert((a&3) == 1);
    assert(a>>2 == 10*1024);
    u->L3.b = LV_L3_B;
    u->L3.l = 1*1000*1000;
    printf("l3: 0x%x, %x, %d\n", a, a&7, a>>3);
    assert((a&7) == 3);
    assert(a>>3 == 1*1000*1000);
    u->L4.b = LV_L4_B;
    u->L4.l = 100*1000*1000;
    printf("l4: 0x%x, %x, %d\n", a, a&7, a>>3);
    assert((a&7) == 7);
    assert(a>>3 == 100*1000*1000);

    // [TEST 2] size test
    std::string small = std::string(127, 's');
    std::string mid = std::string(0x3fff, 'm');
    std::string big = std::string(768*1024, 'b');
    std::string large = std::string(2*1000*1000, 'b');
    const void *ptr = NULL;
    int len;

    lv *s = (lv *)cm::malloc(2*1024*1024);

    // small test
    rc = s->copy(small.c_str(), small.size());
    assert(rc == 0);
    s->get(ptr, len);
    assert(len == (int)small.size());
    assert(memcmp(ptr, small.c_str(), len) == 0);

    // middium test
    rc = s->copy(mid.c_str(), mid.size());
    assert(rc == 0);
    s->get(ptr, len);
    assert(len == (int)mid.size());
    assert(memcmp(ptr, mid.c_str(), len) == 0);

    // big test 
    rc = s->copy(big.c_str(), big.size());
    assert(rc == 0);
    s->get(ptr, len);
    assert(len == (int)big.size());
    assert(memcmp(ptr, big.c_str(), len) == 0);


    // [TEST 3] size aligned and new(size) 
    lv *s1 = new(1) lv;
    assert(((uint64_t)s1&0x3)==0);
    assert(lv::calc_size(1) == 2);
    assert(s1->length() == 1);

    lv *s2 = new(999) lv;
    assert(((uint64_t)s2&0x3)==0);
    assert(lv::calc_size(999) == 1001);
    assert(s2->length() == 999);

    rc = s2->copy(mid.c_str(), 999);
    assert(rc == 0);

    s2->get(ptr, len);
    assert(len == 999);
    assert(memcmp(ptr, mid.c_str(), len) == 0);
}

void test_sz(metacache *mc)
{
    // [TEST 1] basic function
    //sz *s0 = new sz;    // SHOULD be failed
    std::string a1("from code");
    std::string a2("from std::string");

    sz *s1 = new sz(a1.c_str()); // from const char* 
    sz *s2 = new sz(a2); // from std::string

    printf("s1: %s\n", s1->c_str());
    printf("s2: %s\n", s2->c_str());

    assert(strcmp(a1.c_str(), s1->c_str()) == 0);
    assert(strcmp(a2.c_str(), s2->c_str()) == 0);

    // [TEST 2] ptr<sz> function 
    ptr<sz> *p1 = new ptr<sz>;
    *p1 = s1;
    assert(p1->addr() == s1);

    // [TEST 3] sz equal test
    // TBD:  
}

void test_buf(metacache *mc)
{
    buf *b1 = new buf(1000);
    assert(b1->_lv->length() == 1000);
}

void test_bfptr(metacache *mc)
{
    // bftpr size
    assert(sizeof(bfptr<int>) == 4);
   
    // TEST bf function 
    buf *k1 = new buf(1);
    char *ptr = (char *)k1->buffer(); 
    *ptr = 'c';

    // default bfptr is null 
    bfptr<buf> *p1 = new bfptr<buf>;
    assert(p1->addr() == NULL);

    // let bfptr pointer to k1
    *p1 = k1;
    assert(p1->addr() == k1);
    assert(p1->bf() == 0);
    p1->set_bf(-1);
    assert(p1->addr() == k1);
    assert(p1->bf() == -1);
    p1->set_bf(0);
    assert(p1->addr() == k1);
    assert(p1->bf() == 0);
    p1->set_bf(1);
    assert(p1->addr() == k1);
    assert(p1->bf() == 1);
    p1->set_bf(-2);
    assert(p1->addr() == k1);
    assert(p1->bf() == -2);
    p1->set_bf(999);
    assert(p1->addr() == k1);
    assert(p1->bf() == 1);
    p1->set_bf(-99);
    assert(p1->addr() == k1);
    assert(p1->bf() == -2);
}

void test_avltree(metacache *mc)
{
    // [ TEST 1 ]  test bpptr

    // TEST basic avltree
    using rd = bintree<int, int>; // for short
    rd *t1 = new rd; 

    rd::node *n1 = new rd::node(1,11);
    rd::node *n2 = new rd::node(2,12);
    rd::node *n3 = new rd::node(3,13);
    rd::node *n4 = new rd::node(4,14);
    rd::node *n5 = new rd::node(5,15);
    rd::node *n6 = new rd::node(6,16);

    /*       3
     *     2   5
     *    1   4 6
     */
    t1->_root = n3;
    n3->_left = n2;
    n3->_right = n5; 
    n2->_left = n1;
    n5->_left = n4;
    n5->_right = n6; 

    const int *f;
    f = t1->find(999);
    assert(f == NULL);
    for (int i=1; i<=6; i++) { 
        f = t1->find(i);
        assert(*f == (10+i)); 
    }
    
    // TBD: support [] for searching.
    //int i = *t1[1];
}

void test_array(metacache *mc)
{
    // TEST 1: int array
    int test_data[] = {3,1,2,3};
    arr<int> *a1 = (arr<int>*)test_data; 
    assert(a1->size() == 3);
    assert((*a1)[0] == 1);
    assert(a1->at(1) == 2);
    assert(a1->at(2) == 3);

    // TEST 2: ptr array
    int* i1 = (int*)cm::malloc(4);
    int* i2 = (int*)cm::malloc(4);
    int* i3 = (int*)cm::malloc(4);
    arr<ptr<int>> *a2 = (arr<ptr<int>>*)cm::malloc(12);
    a2->_size = 3;
    a2->_array[0] = i1;
    a2->_array[1] = i2;
    a2->_array[2] = i3;
    *i1 = 10000;
    *i2 = 0x7fffffff;
    *i3 = -1;
    arr<ptr<int>>& a3 = *a2;
    assert(*(*a2)[0] == 10000); 
    assert(*a2->at(1) == 0x7fffffff); 
    assert(*a3[2] == -1); 
}

void test_hashtbl(metacache *mc)
{
    
}

void do_test()
{
    metacache *mc = metacache::create(100*1024*1024);
    printf("magic:%s\n", mc->magic);
    test_malloc(mc);
    test_ptr(mc);
    test_lv(mc);
    test_sz(mc);
    test_buf(mc);
    test_bfptr(mc);
    test_avltree(mc);
    test_hashtbl(mc);
    test_array(mc);
}

int main(int argc, char *argv[])
{
    do_test();
    return 0;
}
