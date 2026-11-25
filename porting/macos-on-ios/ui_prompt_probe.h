#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct NgbUiPromptRecord {
    uint32_t frame,sprite,texture,caller,parent;
    int32_t letter,tw,th,mode,depth,input_x,input_y;
    unsigned sample,menu,loading;
    uint32_t wall_ms,phase,ticks,prompt,guide,pending,screen_mode,fade_bits,host_buttons,host_flags,pad_index,guest[4];
    uint32_t card_ticks,card_counter,card_block,card_ready,card_route;
    int32_t tab,visible,header_visible;
    uint32_t state_flags;
    float alpha,rect[4],uv[4];
} NgbUiPromptRecord;
void ngb_ui_prompt_host_state(uint32_t *buttons,uint32_t *flags,uint32_t *ms);
int ngb_ui_prompt_hook_enabled(void);
int ngb_ui_prompt_start(const char *path);
void ngb_ui_prompt_stop(void);
void ngb_ui_prompt_count(unsigned stage);
void ngb_ui_prompt_record(const NgbUiPromptRecord *record);
#ifdef __cplusplus
}
#endif
