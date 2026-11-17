/* Read the documented, size-negotiated MoltenVK configuration prefix.
 * No Vulkan instance, window, shader compilation or GPU work is created. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
int main(int argc,char **argv){
    if(argc!=3)return 2;
    void *library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!library){fprintf(stderr,"Cannot load packaged MoltenVK for policy verification\n");return 1;}
    int32_t (*readConfig)(void *,void *,size_t *)=dlsym(library,"vkGetMoltenVKConfigurationMVK");
    if(!readConfig){fprintf(stderr,"Packaged MoltenVK has no configuration query\n");return 1;}
    uint32_t prefix[3]={0xffffffff,0xffffffff,0xffffffff};size_t bytes=sizeof prefix;
    int32_t status=readConfig(NULL,prefix,&bytes);
    if((status!=0 && status!=5) || bytes!=sizeof prefix || prefix[2]!=(uint32_t)atoi(argv[2])){
        fprintf(stderr,"MoltenVK queue policy was not applied: status=%d bytes=%zu synchronous=%u\n",status,bytes,prefix[2]);return 1;
    }
    printf("Packaged MoltenVK queue policy verified: synchronous=%u\n",prefix[2]);
    dlclose(library);return 0;
}
