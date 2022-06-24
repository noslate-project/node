#include <assert.h>
#include <malloc.h>
#include <string.h>
#include "metacache.h"

using namespace strontium;
void test_lv()
{
    int rc;
    metacache::lv *s = (metacache::lv *)malloc(1024*1024);
    std::string small = std::string(127, 's');
    std::string mid = std::string(0x3fff, 'm');
    std::string big = std::string(768*1024, 'b');
    const void *ptr = NULL;
    int len;

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

    free(s);
    s = NULL;
}

#if 0
void test_writer()
{
    int rc;
    metacache *mc = (metacache *)metacache::create(100*1024*1024);
    assert(mc);

    metacache_writer *wr = new metacache_writer();
    assert(wr);

    // test kv
    metacache_writer::kv *kv = wr->add_kv("words");
    assert(kv);
    kv->set("a", "apple");
    kv->set("b", "box");
    kv->set("c", "cat");
    metacache_writer::kv *kv2 = wr->add_kv("domain");
    kv2->set("163", "www.163.com");
    kv2->set("taobao", "www.taobao.com");
    kv2->set("aliyun", "www.aliyun.com");

    // test kkv
    metacache_writer::kkv *kkv = wr->add_kkv("testkkv");
    assert(kkv);
    kkv->set("cat", "eat", "fish");
    kkv->set("dog", "bake", "danger");
    metacache_writer::kkv *kkv2 = wr->add_kkv("quhao");
    kkv2->set("0086", "0571", "hangzhou");
    kkv2->set("0086", "020", "guangzhou");
    kkv2->set("0086", "010", "beijing");
    kkv2->set("001", "212", "newyork");

    // add new fs cache
    metacache_writer::fs *fs = wr->add_fs("node_modules");
    
    // ent 
    rc = wr->makeup();
    assert(rc);

    rc = metacache::savefile("./metacache.raw");
    assert(rc);
}
#endif

void test_reader()
{
     
}

void do_test()
{
    test_lv();
}

int main(int argc, char *argv[])
{
    do_test();    
    return 0;
}
