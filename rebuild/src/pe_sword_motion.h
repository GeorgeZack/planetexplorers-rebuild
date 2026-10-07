#ifndef PE_SWORD_MOTION_H
#define PE_SWORD_MOTION_H
#include "pe_world.h"

/* Source Action_Sword.UpdateSkillState horizontal request, shared by all sword
 * skill variants. Input is observed WORLD displacement for this update, not
 * velocity, local root coordinates, absolute position or a movement key.
 * Project delta/dt and current velocity to XZ, then cap their difference to12
 * per update (not12*dt); output Y is0. No state, clock, collision or allocation.
 * Double input/intermediates intentionally preserve the existing native running
 * adapter; Unity uses float Vector3 math, so this is not a bitwise engine oracle.
 * All input components must be finite, dt positive. Failure preserves *request.
 * Caller must provide qualified observed displacement; zero is an explicit
 * caller policy and does not establish an unmeasured clip has zero root motion. */
bool pe_sword_motion_request(const double world_delta[3],PeVec3 current_velocity,
                             double dt,PeVec3 *request);
#endif
