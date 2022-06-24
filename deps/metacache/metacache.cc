#include "metacache.h"

#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <utime.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <syslog.h>
#include <ctype.h>
#include <regex.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>

#include <iostream>
#include <fstream>
#include <string.h>
#include <atomic>

namespace strontium {

// malloc pointer
static std::atomic<void *> vmem_cur(0);

void *mc_calloc(size_t num, size_t type_size)
{
    if (!vmem_cur)
        return NULL;
    size_t d = num * type_size;
    void *ptr = vmem_cur.fetch_add(d);
    printf("%s(%lu) = %p\n", __func__, d, ptr);
    return ptr;
}

int metacache::create(uint32_t max_size)
{
    // attach cache
    void *cache = mmap(MC_ADDR(0), max_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	assert(cache);

    // for malloc 
    vmem_cur.store(cache);
    return 0;    
}

void *metacache::load(const char *file)
{
    int fd;
	struct stat s;
	fd = open(file, O_RDONLY);
	assert(fd > 0);
	int rc = fstat(fd, &s);
	assert(!rc);
	size_t flen = s.st_size;
	void *cache = mmap(MC_ADDR(0), flen, PROT_READ, MAP_PRIVATE, fd, 0);
	assert(cache);
    return cache;
}

int metacache::savefile(const char *file)
{
    std::ofstream f(file, std::ios::binary);
    void *ptr = vmem_cur;
    size_t size = (uint64_t)ptr - MC_VMEM;
    f.write((char *)MC_VMEM, ALIGN_PAGE(size));
    f.close();
    printf("%s(%s) = %lu\n", __func__, file, size);
    return 0; 
}

#if 0
int metacache::kv::get(std::string &key, struct lv *&val)
{
    
    return 0;
}

int metacache::kv::set(std::string &key, std::string &val)
{
    return 0;
}

int metacache::lv::get(const void *&buf, int &size)
{
    if ((_val[0] & 0x1) == 0) {
        size = _val[0]>>1;
        buf = &_val[1];
    } else if ((_val[0] & 0x3) == 0x1) {
        size = *(uint16_t *)_val >> 2;
        buf = &_val[2];
    } else if ((_val[0] & 0x3) == 0x3){
        size = *(uint32_t *)_val >> 2;
        buf = &_val[4];
    } else {
        return -1;
    }
    return 0;
}

int metacache::lv::copy(const void *buf, int len)
{
    if (len < 0x80) { // < 128 bytes 
        uint8_t i = len<<1;
        _val[0] = i;
        memcpy(&_val[1], buf, len);
    } else if (len < 0x4000) {  // < 16k
        uint16_t i = ((uint32_t)len << 2) | 0x1;
        *(uint16_t *)_val = i;
        memcpy(&_val[2], buf, len);
    } else {
        uint32_t i = ((uint32_t)len << 2) | 0x3;
        *(uint32_t *)_val = i;
        memcpy(&_val[4], buf, len);
    }
    return 0;
}

int metacache::lhash::put(uint32_t hash, uint32_t ofs_addr, pf_lhash_cmp *cmp)
{
    return 0;
}

int metacache::lhash::find(uint32_t hash, uint32_t &ofs_addr, pf_lhash_cmp *cmp)
{
    return 0;
}

uint32_t metacache::lhash::hash(const char *str)
{
    uint32_t seed=131 ;// 31 131 1313 13131 131313 etc..  
    uint32_t hash=0 ;     
    while(*str) 
    { 
        hash=hash*seed+(*str++); 
    }     
    return(hash); 
}

metacache_writer::kv *metacache_writer::add_kv(const std::string &name)
{
    kv *n = new kv(name);
    vec_kv.push_back(n);
    return n;
}

metacache_writer::kkv *metacache_writer::add_kkv(const std::string &name)
{
    kkv *n = new kkv(name);
    vec_kkv.push_back(n);
    return n;
}

metacache_writer::fs *metacache_writer::add_fs(const std::string &entry)
{
    fs *n = new fs(entry);
    return n;
}

int metacache_writer::kv::set(const std::string &key, const std::string &val)
{
    map_kv[key] = val; 
    return 0;
}

int metacache_writer::kkv::set(const std::string &k1, const std::string &k2, const std::string &val)
{
    kkv_key key(k1, k2);
    map_kkv[key] = val;
    return 0;
}

int metacache_writer::string(const std::string &val)
{
    sz *s = NULL;
    auto it = map_str.find(val);
    if (it == map_str.end()) {
        s = new sz(val);
    } else {
        s = it->second;
    }
    s->hash = metacache::lhash::hash(val.c_str()); 
    return 0; 
}

int metacache_writer::makeup()
{
    size_t i;

    // hdr
     
    for (i=0; i<vec_kv.size(); i++) {
        metacache_writer::kv *kv = vec_kv[i];
        std::cout << kv->name << std::endl;
        std::map<std::string, std::string>::iterator it;
        for (it=kv->map_kv.begin(); it!=kv->map_kv.end(); it++) {
            std::cout << it->first << ":" << it->second << std::endl;
        }
    }

    for (i=0; i<vec_kkv.size(); i++) {
        metacache_writer::kkv *kkv = vec_kkv[i];
        std::cout << kkv->name << std::endl;
        std::map<metacache_writer::kkv::kkv_key, std::string>::iterator it;
        for (it=kkv->map_kkv.begin(); it!=kkv->map_kkv.end(); it++) {
            std::cout << "(" << it->first.first << "," << it->first.second << "):" << it->second << std::endl;
        }
    }

    return 0; 
}
#endif

};
