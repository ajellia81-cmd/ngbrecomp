#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void ngb_diag_stage(const char *stage);
void ngb_diag_documents(const char *documents);
void ngb_diag_ui_ready(void);
int ngb_diag_writer_start(const char *path);
int ngb_diag_writer_active(void);
int ngb_diag_writer_enqueue(const char *text,size_t length);
#ifdef __cplusplus
}
#endif
