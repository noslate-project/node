
#define _GNU_SOURCE
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

// GLIBC HOOKS
#define HOOK(fn, ptr) \
    if (!ptr) { \
        ptr = dlsym(RTLD_NEXT, #fn); \
    }

static void _mc_preload_init(void)  __attribute__ ((constructor));
static void _mc_preload_exit(void)  __attribute__ ((destructor));

int (*real_xstat)(int ver, const char *path, struct stat *buf) = NULL;
int (*real_lxstat)(int ver, const char *path, struct stat *buf) = NULL;
int (*real_xstat64)(int ver, const char * path, struct stat64 * stat_buf) = NULL;
int (*real_lxstat64)(int ver, const char * path, struct stat64 * stat_buf) = NULL;

int (*real_fxstatat)(int ver, int dirfd, const char *path, struct stat *buf, int flags) = NULL;

ssize_t (*real_readlink)(const char *pathname, char *buf, size_t bufsiz) = NULL;
int (*real_access)(const char *pathname, int mode) = NULL;
int (*real_open)(const char *pathname, int flags, ...) = NULL;

static void _mc_preload_init()
{
    HOOK(__xstat, real_xstat);
    HOOK(__lxstat, real_lxstat);
    HOOK(__xstat64, real_xstat64);
    HOOK(__lxstat64, real_lxstat64);
    HOOK(__fxstatat, real_fxstatat);
    HOOK(readlink, real_readlink);
    //HOOK(access, real_access);
    //HOOK(open, real_open);
}

static void _mc_preload_exit()
{

}

// metacache intf
int __xstat(int ver, const char *path, struct stat *buf)
{
    fprintf(stderr, "%s(%d, %s, %p)\n", __func__, ver, path, buf);
    return real_xstat(ver, path, buf);
}

int __lxstat(int ver, const char *path, struct stat *buf)
{
    fprintf(stderr, "%s(%d, %s, %p)\n", __func__, ver, path, buf);
    return real_lxstat(ver, path, buf);
}

int __xstat64(int ver, const char * path, struct stat64 * buf) 
{
    fprintf(stderr, "%s(%d, %s, %p)\n", __func__, ver, path, buf);
    return real_xstat64(ver, path, buf);
}

int __lxstat64(int ver, const char * path, struct stat64 * buf) 
{
    fprintf(stderr, "%s(%d, %s, %p)\n", __func__, ver, path, buf);
    return real_lxstat64(ver, path, buf);
}

int __fxstatat(int ver, int dirfd, const char *path, struct stat *buf, int flags)
{
    fprintf(stderr, "%s(%d, %d, %s, %p, %d)\n", __func__, ver, dirfd, path, buf, flags);
    return real_fxstatat(ver, dirfd, path, buf, flags);
}

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz)
{
    //fprintf(stderr, "%s(%s, %p, %d)\n", __func__, pathname, buf, bufsiz);
    return real_readlink(pathname, buf, bufsiz);
}

