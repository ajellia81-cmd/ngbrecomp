#define NGB_SHARED_MEMORY_TEST 1
#import "../libSystem_NGBSharedMemory.m"
#include <assert.h>
int main(void){@autoreleasepool{
    char name[32];snprintf(name,sizeof name,"/xbox_map_%d_test",getpid());
    int fd=shm_open(name,O_CREAT|O_RDWR|O_EXCL,0600);assert(fd>=0);
    errno=0;assert(shm_open(name,O_CREAT|O_RDWR|O_EXCL,0600)==-1 && errno==EEXIST);
    int same=shm_open(name,O_RDWR,0600);assert(same>=0);
    assert(shm_unlink(name)==0);
    assert(shm_open(name,O_RDWR,0600)==-1 && errno==ENOENT);
    const size_t size=64u<<20;assert(ftruncate(fd,size)==0);
    unsigned char *a=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    unsigned char *b=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,same,0);
    assert(a!=MAP_FAILED && b!=MAP_FAILED && a!=b);
    close(fd);close(same);
    a[0]=23;a[size-1]=91;assert(b[0]==23 && b[size-1]==91);
    b[4096]=77;assert(a[4096]==77);
    int fresh=shm_open(name,O_CREAT|O_RDWR|O_EXCL,0600);assert(fresh>=0);
    assert(ftruncate(fresh,size)==0);
    unsigned char *c=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,fresh,0);assert(c!=MAP_FAILED);
    assert(c[0]==0 && c[size-1]==0 && b[0]==23);
    assert(shm_unlink(name)==0);close(fresh);
    assert(munmap(a,size)==0 && munmap(b,size)==0 && munmap(c,size)==0);
    assert(shm_open("/xbox_map_bad/name",O_CREAT|O_RDWR,0600)==-1 && errno==EINVAL);
    puts("Xbox shared-memory compatibility: 64 MB mirrors, descriptor lifetime, unlink and exclusivity passed");
}return 0;}
