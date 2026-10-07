#ifndef PE_MELEE_FACING_H
#define PE_MELEE_FACING_H

#include "pe_sword_targeting.h"

/* Optional native QoL request basis, before ordinary stage direction/targeting.
 * Default/off and captured camera modes preserve the finite movement vector
 * unchanged. Enabled free third person uses the camera's reflected horizontal
 * forward (sin(yaw),0,-cos(yaw)); camera yaw is radians and is validated only
 * when used. The caller captures requests on fresh attack/continuation presses;
 * changing the camera does not steer a frozen stage. Failure preserves output. */
bool pe_melee_facing_request(PeVec3 movement,bool camera_facing,
    bool free_third_person,float camera_yaw_radians,PeVec3 *out_request);

/* Grounded third-person Action_Sword direction policy. Angles are degrees:
 * yaw0 faces world+Z and yaw90 faces+X. The host captures accepted input at
 * stage admission/continuation, never actual attack-root velocity or cursor.
 * Source Vector3.Normalize's <=1e-5 zero threshold falls back to body forward.
 * The caller owns this transient direction and its stage lifetime. */
bool pe_melee_facing_direction(PeVec3 accepted_movement,float current_yaw_degrees,
    PeVec3 *out_direction);

/* Frozen target selection is supplied from stage admission; NULL is no target.
 * Source active targeting sets horizontal yaw immediately. A vertical-only
 * target retains yaw (bounded upright adapter); a zero target direction falls
 * through to attack-direction rotation. Without a target, use normalized
 * shortest-path quaternion lerp with clamp(serialized rotate acceleration*dt),
 * not linear yaw interpolation. Zero dt freezes only the lerp branch.
 * attack_direction must have a nonzero horizontal component. All arguments
 * used by the operation must be finite; failure leaves output untouched.
 * No IK, animation, locomotion, target tracking or saved state is owned here. */
bool pe_melee_facing_yaw(float current_yaw_degrees,PeVec3 attack_direction,
    const PeSwordTargetResult *target,float dt,float *out_yaw);

#endif
