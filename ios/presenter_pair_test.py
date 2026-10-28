"""Run the production pair worker with simulated copies, including failed endpoint acquisition."""
from pathlib import Path
import subprocess
import tempfile
from presenter_descriptor_test import function, SOURCE

PREFIX=r'''
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
typedef int VkFence;
typedef int VkDevice;
typedef struct { int unused; } NgbPresentPair;
typedef struct { int unused; } Mailbox;
static Mailbox s_pairs;
static pthread_mutex_t s_pair_mu=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_present_mu=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_pair_cv=PTHREAD_COND_INITIALIZER;
static VkDevice s_pair_copy_device;
static VkFence s_pair_copy_fence;
static uint64_t s_pair_mids,s_pair_ends,s_pair_acquire_us,s_pair_lock_us,s_pair_submit_us,s_pair_present_us;
static int take_count,stage,fail_end;
#define VK_NULL_HANDLE 0
#define VK_TRUE 1
static int64_t g_get_monotonic_time(void) { return 0; }
static void g_usleep(unsigned long us) { assert(us==8333); assert(stage==1); }
static bool ngb_pair_take(Mailbox *m,NgbPresentPair *p) {
 (void)m;(void)p;
 if(take_count++) { pthread_mutex_unlock(&s_pair_mu); pthread_exit(NULL); }
 return true;
}
static bool pair_present(const NgbPresentPair *p,bool mid) {
 (void)p;
 if(mid) { assert(stage==0);stage=1;s_pair_copy_fence=1;return true; }
 assert(stage==1);stage=2;
 if(fail_end) return false;
 s_pair_copy_fence=2;return true;
}
static void vkWaitForFences(VkDevice d,int n,VkFence *f,int all,uint64_t timeout) {
 (void)d;(void)n;(void)all;(void)timeout;
 assert(stage==2 && *f==(fail_end ? 1 : 2));stage=3;
}
static void ngb_pair_complete(Mailbox *m) {
 (void)m;assert(stage==3 && s_pair_copy_fence==0);stage=4;
}
'''
SUFFIX=r'''
int main(void) {
 for(int i=0;i<2;i++) {
  fail_end=i;take_count=stage=0;
  pthread_t t;assert(!pthread_create(&t,NULL,pair_worker,NULL));pthread_join(t,NULL);
  assert(stage==4);
 }
 return 0;
}
'''
def main():
    production=function(SOURCE.read_text(),'pair_worker')
    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory)
        for name,body,success in [('fixed',production,True),('unsafe',production.replace(
            'vkWaitForFences(s_pair_copy_device,1,&s_pair_copy_fence,VK_TRUE,UINT64_MAX);',''),False)]:
            source=root/(name+'.c');exe=root/name;source.write_text(PREFIX+body+SUFFIX)
            subprocess.run(['cc','-std=c11','-pthread',str(source),'-o',str(exe)],check=True,capture_output=True)
            result=subprocess.run([str(exe)],capture_output=True)
            assert (result.returncode==0)==success
    print('Presenter pair worker: one final copy wait preserves both source leases, including failed endpoint; unsafe control rejected')
if __name__=='__main__':main()
