"""Real settings read/write, isolated config, no SDL/window or user's data."""
from pathlib import Path
import subprocess,tempfile,os,re
root=Path(__file__).resolve().parents[3]
s=(root/'src/launcher.c').read_text().split('/* ---------- drawing ---------- */',1)[0]
s=re.sub(r'^#include .*\n','',s,flags=re.M)
prefix=r'''
#define _DEFAULT_SOURCE
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "launcher.h"
#define SDL_strncasecmp strncasecmp
static const char *config;
static void ngb_config_dir(char *out,size_t n,int make){(void)make;snprintf(out,n,"%s",config);}
void ngb_ios_screen_size(int *w,int *h){*w=2868;*h=1320;}
'''
main=r'''
int main(int argc,char **argv){
 assert(argc==2);config=argv[1];char path[1024];snprintf(path,sizeof path,"%s/settings.ini",config);
 FILE *f=fopen(path,"w");assert(f);
 fputs("aspect = iphone-native\nrender_scale = 11\nframe_interpolation = 3\naudio_output = 2\ntext_language = 2\ntext_language_options = 1\nsmooth_motion = 1\n",f);fclose(f);
 NgbSettings a,b;ngb_settings_load(&a);
 assert(a.ios_text_language==2 && a.text_language_options==1 && a.text_language==0);
 assert(a.frame_interp==3 && a.audio_output==2 && a.scale==11 && a.aspect==ASPECT_IPHONE);
 assert(ASPECT_32_9==4 && ASPECT_IPHONE==5 && ASPECT_COUNT==6);
 assert(ngb_settings_runtime_scale(&a)==3 && a.width==2868 && a.height==1320);
 assert(fabs(ngb_settings_aspect_ratio(&a)-2868.0/1320)<1e-12);
 for(int i=0;i<4;i++){
  a.frame_interp=i;ngb_settings_save(&a);ngb_settings_load(&b);
  assert(b.frame_interp==i && b.audio_output==2 && b.ios_text_language==2 && b.text_language_options);
 }
 f=fopen(path,"w");assert(f);fputs("text_language = japanese\nios_text_language = 1\nframe_interpolation = 1\n",f);fclose(f);
 ngb_settings_load(&b);assert(b.text_language==2 && b.ios_text_language==1 && !b.text_language_options && b.frame_interp==1);
 f=fopen(path,"w");assert(f);fputs("text_language = english\n",f);fclose(f);
 ngb_settings_load(&b);assert(b.text_language==1 && b.frame_interp==0);
 puts("PASS: legacy Korean settings migrate separately; all upstream interpolation modes and audio output round-trip; native display and system language retained");
}
'''
with tempfile.TemporaryDirectory(prefix='ngb-072-settings-') as temp:
 folder=Path(temp);source=folder/'fixture.c';exe=folder/'fixture';source.write_text(prefix+s+main)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-DNGB_CONVERTED_DISPLAY=1','-DNGB_IOS_KOREAN=1','-I'+str(root/'src'),str(source),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe),str(folder)],check=True)
