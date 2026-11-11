#define _DARWIN_C_SOURCE 1
// Synthetic native-writer check. Constructor disabled; scratch paths only.
#include "startup_diagnostics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(int argc,char **argv){
 assert(argc==2);ngb_diag_documents(argv[1]);ngb_diag_stage("test main marker");
 char motion[4096],startup[4096];snprintf(motion,sizeof motion,"%s/test-motion.log",argv[1]);snprintf(startup,sizeof startup,"%s/ngb-startup-diagnostics.log",argv[1]);
 assert(ngb_diag_writer_start(motion));const char *text="[test] native writer received a report\n";assert(ngb_diag_writer_enqueue(text,strlen(text)));
 int found=0;for(unsigned i=0;i<200&&!found;i++){FILE *f=fopen(motion,"r");if(f){char row[1024];while(fgets(row,sizeof row,f))if(strstr(row,"native writer received"))found=1;fclose(f);}if(!found)usleep(10000);}
 assert(found);
#ifdef NGB_IOS_PERFORMANCE_DIAGNOSTICS
 assert(access(startup,F_OK)!=0);
#else
 FILE *f=fopen(startup,"r");assert(f);found=0;char row[1024];while(fgets(row,sizeof row,f))if(strstr(row,"test main marker"))found=1;fclose(f);assert(found);
#endif
#ifdef NGB_IOS_PERFORMANCE_DIAGNOSTICS
 puts("PASS: native asynchronous performance writer; no startup file or UI markers");
#else
 puts("PASS: early stage file and native asynchronous writer in scratch Documents");
#endif
 return 0;
}
