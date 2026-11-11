#define _DARWIN_C_SOURCE 1
// Diagnostic build only. Early markers use C I/O; periodic I/O uses native GCD.
#include "startup_diagnostics.h"
#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#ifndef NGB_IOS_PERFORMANCE_DIAGNOSTICS
static char stage_path[PATH_MAX];
static void (*show_stage)(const char *);
void ngb_diag_stage(const char *stage){
 if(stage_path[0]){FILE *f=fopen(stage_path,"a");if(f){fprintf(f,"%lld %s\n",(long long)time(NULL),stage);fclose(f);}}
 if(show_stage)show_stage(stage);
}
#if defined(NGB_DIAG_EARLY_ENTRY)
static void normal_exit(void){show_stage=NULL;ngb_diag_stage("process exit/normal return");}
__attribute__((constructor)) static void early_entry(void){
 const char *home=getenv("HOME");
 if(home && snprintf(stage_path,sizeof stage_path,"%s/Documents/ngb-startup-diagnostics.log",home)>=(int)sizeof stage_path)stage_path[0]=0;
 if(stage_path[0]){FILE *f=fopen(stage_path,"w");if(f)fclose(f);}
 ngb_diag_stage("executable initializer entered (before main)");atexit(normal_exit);
}
#endif
void ngb_diag_documents(const char *documents){
 char preferred[PATH_MAX];
 if(!documents || snprintf(preferred,sizeof preferred,"%s/ngb-startup-diagnostics.log",documents)>=(int)sizeof preferred)return;
 if(strcmp(preferred,stage_path)){
  FILE *out=fopen(preferred,"w");
  if(out){FILE *in=stage_path[0]?fopen(stage_path,"r"):NULL;if(in){char row[512];while(fgets(row,sizeof row,in))fputs(row,out);fclose(in);}fclose(out);}
  memcpy(stage_path,preferred,strlen(preferred)+1);
 }
 ngb_diag_stage("Documents path configured");
}
void ngb_diag_ui_ready(void){show_stage=(void(*)(const char*))dlsym(RTLD_DEFAULT,"ngb_macos_on_ios_diag_stage");ngb_diag_stage("UIKit event loop ready");}
#else
void ngb_diag_stage(const char *stage){(void)stage;}
void ngb_diag_documents(const char *documents){(void)documents;}
void ngb_diag_ui_ready(void){}
#endif
static dispatch_queue_t writer_queue;
static FILE *motion_file;
static unsigned pending;
static int writer_active;
static void open_motion(void *context){
 char *path=context;motion_file=fopen(path,"w");free(path);
 if(!motion_file){__atomic_store_n(&writer_active,0,__ATOMIC_RELEASE);ngb_diag_stage("motion log open failed");return;}
 #ifdef NGB_IOS_PERFORMANCE_DIAGNOSTICS
 fprintf(motion_file,"NGB performance diagnostics v4 unix=%lld; frame boundary after submission; CPU wall times, nested phases overlap; no per-draw timers, water dumps or automatic sampler\n",(long long)time(NULL));
#else
 fprintf(motion_file,"NGB motion diagnostics v2 unix=%lld; native GCD writer; CPU fields are render-phase elapsed times; midpoint includes replay\n",(long long)time(NULL));
#endif
 fflush(motion_file);
 ngb_diag_stage("motion log writer ready");
}
int ngb_diag_writer_start(const char *path){
 if(writer_queue)return ngb_diag_writer_active();
 if(!path || !*path)return 0;
 char *copy=strdup(path);if(!copy)return 0;
#ifdef NGB_IOS_PERFORMANCE_DIAGNOSTICS
 dispatch_queue_attr_t attributes=dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,QOS_CLASS_UTILITY,0);
#else
 dispatch_queue_attr_t attributes=DISPATCH_QUEUE_SERIAL;
#endif
 writer_queue=dispatch_queue_create("com.ngbrecomp.motion-diagnostics",attributes);
 if(!writer_queue){free(copy);return 0;}
 __atomic_store_n(&writer_active,1,__ATOMIC_RELEASE);
 ngb_diag_stage("first game frame; scheduling native log writer");
 dispatch_async_f(writer_queue,copy,open_motion);return 1;
}
int ngb_diag_writer_active(void){return __atomic_load_n(&writer_active,__ATOMIC_ACQUIRE);}
struct Record{size_t length;char text[];};
static void write_motion(void *context){
 struct Record *r=context;
 if(motion_file && ngb_diag_writer_active()){
  fwrite(r->text,1,r->length,motion_file);fflush(motion_file);
  if(ftell(motion_file)>=8*1024*1024){fputs("[limit] 8 MiB reached; recording stopped\n",motion_file);fclose(motion_file);motion_file=NULL;__atomic_store_n(&writer_active,0,__ATOMIC_RELEASE);}
 }
 free(r);__atomic_fetch_sub(&pending,1,__ATOMIC_RELEASE);
}
int ngb_diag_writer_enqueue(const char *text,size_t length){
 if(!ngb_diag_writer_active() || length>4096*10)return 0;
 if(__atomic_fetch_add(&pending,1,__ATOMIC_ACQ_REL)>=4){__atomic_fetch_sub(&pending,1,__ATOMIC_RELEASE);return 0;}
 struct Record *r=malloc(sizeof *r+length);
 if(!r){__atomic_fetch_sub(&pending,1,__ATOMIC_RELEASE);return 0;}
 r->length=length;memcpy(r->text,text,length);dispatch_async_f(writer_queue,r,write_motion);return 1;
}
