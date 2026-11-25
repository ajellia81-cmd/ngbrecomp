#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct NgbUiPromptRecord {
    uint32_t frame,sprite,texture,caller,parent;
    int32_t letter,tw,th,mode,depth,input_x,input_y;
    unsigned sample;
    float rect[4],uv[4];
} NgbUiPromptRecord;
int ngb_ui_prompt_start(const char *path);
void ngb_ui_prompt_stop(void);
void ngb_ui_prompt_count(unsigned stage);
void ngb_ui_prompt_record(const NgbUiPromptRecord *record);
#ifdef __cplusplus
}
#endif
