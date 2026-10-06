#ifndef PE_RUNNING_ATTACK_MOTION_H
#define PE_RUNNING_ATTACK_MOTION_H
#include "pe_combat.h"
#include "pe_root_motion.h"

/* Pure source Action_Sword.UpdateSkillState horizontal request adapter.
 * motion must come from the qualified source-bound exporter, not a fallback.
 * Accepts only female third-person RUNNING stage0 outgoing authored intervals;
 * the interval remains authoritative after the action ends/resets its clock.
 * Source Unity XZ is rotated by the same convention as body/hitbox placement:
 * x=c*x+s*z, z=c*z-s*x. Root Y and delta rotation are not applied.
 * Divide interval displacement by positive game dt, then limit horizontal
 * request-current velocity difference to12 PER UPDATE, not12*dt acceleration.
 * A valid zero-length interval requests zero before this velocity correction.
 * No clock/state advance, dt integration, walking addition or collision occurs.
 * Unsupported stages/invalid inputs returnfalse and leave *request unchanged;
 * the caller explicitly owns the zero-horizontal policy for later stages. */
bool pe_running_attack_motion_request(const PeRootMotion *motion,
                                     const PeCombatInterval *interval,
                                     PeVec3 current_velocity,
                                     double body_yaw_degrees,double dt,
                                     PeVec3 *request);
#endif
