#ifndef PE_EXPERIMENTAL_HUMAN_ROTATION_MATH_H
#define PE_EXPERIMENTAL_HUMAN_ROTATION_MATH_H
#include <stdbool.h>
/* Experimental Avatar.GetZYRoll arithmetic only: radians in limit arrays,
 * normalized muscles within +/-16 (values beyond +/-1 supported), finite
 * ordered limits within +/-100 radians, signs exactly +/-1.
 * No PreQ/PostQ composition, retarget hierarchy, axes-type dispatch or IK.
 * FE_TONEAREST is required. Invalid input leaves out untouched. */
bool pe_human_rotation(const float muscles[3],const float minimum[3],
    const float maximum[3],const float sign[3],float out[4]);
/* True uses the original x86 approximate reciprocal-square-root seed; false
 * uses portable 1/sqrtf, an explicitly separate numerical fallback. */
bool pe_human_rotation_native_rsqrt(void);
#endif
