#ifndef PE_IK_SKIN_H
#define PE_IK_SKIN_H
#include "pe_fbbik.h"
/* Disconnected bridge from a solve to skinned geometry: for each solver
 * transform, the rigid model-space motion taking its animated pose to its
 * solved pose. Anything rigidly attached to that bone in the animated pose
 * (a skin palette row, a capsule, an attachment) follows by this motion.
 * No host calls this; which bone drives each skin joint is the caller's. */
/* delta[bone] is a row-major 3x4: solved = delta * animated. Invalid input
 * leaves the output untouched. */
bool pe_ik_skin_deltas(const PeFbbikRig *rig,const PeFbbikTransform *animated_local,const PeFbbikTransform *solved_local,
                       float delta[PE_FBBIK_MAX_BONES][12]);
/* Aliasing is allowed. */
void pe_ik_skin_point(const float delta[12],const float point[3],float output[3]);
#endif
