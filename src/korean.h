#ifndef NGB_KOREAN_H
#define NGB_KOREAN_H
typedef void (*ngb_korean_fn)(void);
int ngb_korean_active(void);
ngb_korean_fn ngb_korean_lookup(unsigned address);
ngb_korean_fn ngb_korean_lookup_raw(unsigned address);
#endif
