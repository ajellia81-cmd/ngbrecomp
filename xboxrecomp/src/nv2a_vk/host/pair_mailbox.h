/* Bounded latest-frame mailbox. Caller serializes access; GPU readers retain
 * the active lease until their source-copy fence has completed. */
#ifndef NGB_PAIR_MAILBOX_H
#define NGB_PAIR_MAILBOX_H
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint64_t mid,end,epoch; uint32_t w,h; } NgbPresentPair;
typedef struct {
    NgbPresentPair pending,active;
    bool has_pending,has_active;
    uint64_t epoch,superseded;
} NgbPairMailbox;
static inline bool ngb_pair_busy(const NgbPairMailbox *m,uint64_t image) {
    return image && ((m->has_pending && (m->pending.mid==image || m->pending.end==image)) ||
        (m->has_active && (m->active.mid==image || m->active.end==image)));
}
static inline bool ngb_pair_offer(NgbPairMailbox *m,NgbPresentPair p) {
    if (!p.mid || !p.end || ngb_pair_busy(m,p.mid) || ngb_pair_busy(m,p.end)) return false;
    if (m->has_pending) m->superseded++;
    p.epoch=m->epoch; m->pending=p; m->has_pending=true; return true;
}
static inline bool ngb_pair_take(NgbPairMailbox *m,NgbPresentPair *out) {
    if (m->has_active || !m->has_pending) return false;
    m->active=m->pending; m->has_active=true; m->has_pending=false; *out=m->active; return true;
}
static inline void ngb_pair_cancel(NgbPairMailbox *m) {
    if (m->has_pending) m->superseded++;
    m->epoch++; m->has_pending=false;
}
static inline void ngb_pair_complete(NgbPairMailbox *m) { m->has_active=false; }
#endif
