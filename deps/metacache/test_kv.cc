#include "metacache.h"
#include <sys/time.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

using namespace strontium::metacache;

MC_GLOBAL();

static const char* short_options = "lf:";   
struct option long_options[] = {  
     { "load",     0,   NULL,    'l'     },  
     { "file",     1,   NULL,    'f'     },  
     {      0,     0,     0,     0},  
};

static struct cfg {
    const char *file;
    enum {
        TEST_WRITER,
        TEST_READER,
    } type;
} cfg = {.file="metacache.raw"};


void test_writer()
{
    metacache *mc = metacache::create(100*1024*1024);
    assert(mc);

    // str kv 
    {
        using kv_t=kv_w<str_w, str_w>;
        kv_t kv1;
       
        kv1["a"] = "apple";
        kv1["b"] = "box";
        kv1["c"] = "cat";
        
        void *m = kv1.makeup(); 
        assert(m);
        mc->set_slots(0, m);
    }

    // buf kv
    {
        using kv_buf = kv_w<str_w, buf_w>;
        kv_buf kv2;

        char ali[] = "alibaba";
        char tc[] = "tencent";
        buf_w  buf1(ali, 8);
        buf_w  buf2(tc, 8);
        kv2["ali"] = buf1;
        kv2["qq"] = buf2; 

        void *m = kv2.makeup();
        assert(m);
        mc->set_slots(1, m);
    }

    // kkv
    {
        using kkv_t = kkv_w<str_w, str_w, str_w>;
        kkv_t tr;
        tr[kkeyw("zhejiang", "hangzhou")] = "0571"; 
        tr[kkeyw("zhejiang", "jiaxing")] = "0573";
        tr[kkeyw("shanghai", "shanghai")] = "021";
        tr[kkeyw("guangdong", "guangzhou")] = "020";
        void *m = tr.makeup();
        assert(m);
        mc->set_slots(2, m);
    }

    metacache::savefile(cfg.file);
}

void test_reader()
{
    metacache *mc = metacache::load(cfg.file);
    assert(mc);
   
    {
        const void *m = mc->get_slots(0);
        using kv_t = kv<str,str>;
        kv_t kv1(m);
        const str *a = kv1.find("a");
        std::cout << a->c_str() << std::endl; 
    }

    {
        const void *m = mc->get_slots(1);
        using kv_t = kv<str,buf>;
        kv_t kv1(m);
        const buf *a = kv1.find("ali");
        std::cout << (char *)a->buffer() << std::endl; 
    }

    {
        const void *m = mc->get_slots(2);
        using kv_t = kkv<str,str,str>;
        kv_t kv1(m);
        const str *a = kv1.find("zhejiang", "hangzhou");
        std::cout << a->c_str() << std::endl; 
    }

}

int main(int argc, char *argv[])  
{  
     int c;  
     while((c = getopt_long (argc, argv, short_options, long_options, NULL)) != -1)  
     {  
         switch (c)  
         {  
         case 'l':  
             cfg.type = cfg::TEST_READER;
             break;  
         case 'f':  
             cfg.file = optarg;
             break;  
         }  
     }

     if (cfg.type == cfg::TEST_READER) {
        test_reader();
     }
     if (cfg.type == cfg::TEST_WRITER) {
        test_writer();
     }

     return 0;  
}  
