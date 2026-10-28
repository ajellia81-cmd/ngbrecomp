#ifndef NGB_CONVERSION_BRIDGE_H
#define NGB_CONVERSION_BRIDGE_H
#include <stddef.h>
#include <stdint.h>
typedef struct NgbConversionHost {
    uint32_t version,size;
    int (*check_xbe)(const char *);
    int (*install)(const char *,const char *,char *,size_t);
    void (*progress)(void (*)(uint64_t,uint64_t,void *),void *);
    unsigned (*was_init)(unsigned);
    int (*init_subsystem)(unsigned);
    int (*attach_virtual)(int,int,int,int);
    void *(*open_joystick)(int);
    int (*axis)(void *,int,int16_t);
    int (*button)(void *,int,uint8_t);
    int32_t (*instance)(void *);
    void (*close_joystick)(void *);
    int (*num_joysticks)(void);
    int32_t (*device_instance)(int);
    int (*detach_virtual)(int);
    void (*audio_pause)(int);
    int (*audio_paused)(void);
} NgbConversionHost;
typedef int (*NgbConversionPrepare)(const NgbConversionHost *);
int ngb_conversion_startup(void);
void ngb_conversion_tick(void);
void ngb_conversion_wait_foreground(void);
#endif
