#ifndef NGB_LANGUAGE_POLICY_H
#define NGB_LANGUAGE_POLICY_H
/* Host selector: Japanese, English, Korean. Game selector: Japanese, English.
 * Text table columns reverse that order: English then Japanese. */
static inline int ngb_language_step(int selected,int step) { return (selected+(step>0 ? 1 : 2))%3; }
static inline unsigned ngb_language_game_index(int selected) { return selected==1 ? 1u : 0u; }
static inline unsigned ngb_language_text_column(int korean,unsigned requested) { return korean ? 1u : requested; }
#endif
