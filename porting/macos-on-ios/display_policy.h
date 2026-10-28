#ifndef NGB_CONVERSION_DISPLAY_POLICY_H
#define NGB_CONVERSION_DISPLAY_POLICY_H
/* UIKit retires scanned-out drawables asynchronously. Keep one spare image
 * without exceeding the driver's advertised limit. Standalone Mac is unchanged. */
static inline unsigned ngb_output_image_count(unsigned requested,unsigned maximum,int converted)
{
    unsigned count=converted && requested<3 ? 3 : requested;
    return maximum && count>maximum ? maximum : count;
}
int ngb_conversion_is_ios(void);
int ngb_conversion_is_foreground(void);
#endif
