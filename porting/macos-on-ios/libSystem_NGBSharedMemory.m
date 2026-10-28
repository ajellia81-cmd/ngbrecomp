// macOS' named shared-memory namespace is unavailable in an iOS sandbox.
// The Mac engine opens and immediately unlinks /xbox_map_<pid>_<sequence>.
// Sandbox-local files preserve the same descriptor and MAP_SHARED aliasing.
#import <Foundation/Foundation.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>

#if defined(NGB_SHARED_MEMORY_TEST)
#define shm_open ngb_test_shm_open
#define shm_unlink ngb_test_shm_unlink
#endif

static BOOL NgbMappingName(const char *name) {
    return name && strncmp(name,"/xbox_map_",10)==0;
}
static int NgbSharedDirectory(void) {
    NSString *directory=[NSTemporaryDirectory() stringByAppendingPathComponent:@"ngb-macos-shared-memory"];
    if(!directory){errno=ENOENT;return -1;}
    const char *path=directory.fileSystemRepresentation;
    if(mkdir(path,0700)!=0 && errno!=EEXIST)return -1;
    return open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
}
static BOOL NgbSharedKey(const char *name,char key[65]) {
    size_t length=strlen(name);
    if(length>31){errno=ENAMETOOLONG;return NO;}
    if(length<2 || name[0]!='/' || strchr(name+1,'/')){errno=EINVAL;return NO;}
    static const char hex[]="0123456789abcdef";
    for(size_t i=0;i<length;i++){unsigned char c=(unsigned char)name[i];key[i*2]=hex[c>>4];key[i*2+1]=hex[c&15];}
    key[length*2]=0;return YES;
}
__attribute__((visibility("default")))
int shm_open(const char *name,int flags,...) {
    mode_t mode=0600;
    if(flags&O_CREAT){va_list args;va_start(args,flags);mode=(mode_t)va_arg(args,int);va_end(args);}
    if(!NgbMappingName(name)) {
        int (*original)(const char *,int,...)=dlsym(dlopen("/usr/lib/libSystem.B.dylib",RTLD_LAZY),"shm_open");
        if(!original){errno=ENOSYS;return -1;}return original(name,flags,mode);
    }
    char key[65];if(!NgbSharedKey(name,key))return -1;
    int directory=NgbSharedDirectory();if(directory<0)return -1;
    int fd=openat(directory,key,flags|O_CLOEXEC|O_NOFOLLOW,mode);
    int saved=errno;close(directory);errno=saved;
    fprintf(stderr,"[Startup] Xbox shared-memory backing: %s%s\n",fd>=0 ? "ready" : "failed",fd>=0 ? "" : strerror(errno));
    errno=saved;return fd;
}
__attribute__((visibility("default")))
int shm_unlink(const char *name) {
    if(!NgbMappingName(name)) {
        int (*original)(const char *)=dlsym(dlopen("/usr/lib/libSystem.B.dylib",RTLD_LAZY),"shm_unlink");
        if(!original){errno=ENOSYS;return -1;}return original(name);
    }
    char key[65];if(!NgbSharedKey(name,key))return -1;
    int directory=NgbSharedDirectory();if(directory<0)return -1;
    int result=unlinkat(directory,key,0);int saved=errno;close(directory);errno=saved;return result;
}
