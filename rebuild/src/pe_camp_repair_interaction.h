#ifndef PE_CAMP_REPAIR_INTERACTION_H
#define PE_CAMP_REPAIR_INTERACTION_H
#include "pe_game.h"
#include "pe_sandbox_collision.h"

/* One archived entity20006 in either qualified settlement crop. The caller
 * supplies readiness of the matching rendered settlement and its CURRENT
 * collision snapshot; no asset loading, saved state or graphics ownership here.
 * Failed presence leaves optional anchor unchanged. */
bool pe_camp_repair_present(const PeGame *game,const PeSandboxCollision *collision,
                           bool model_ready,PeVec3 *anchor);
/* Source closes strictly beyond8m from the root. Native admission uses that
 * same inclusive boundary, avoiding source open-then-close at longer range.
 * Death/get-up/nonfinite player coordinates also close or prevent admission. */
bool pe_camp_repair_in_range(const PeGame *game,const PeSandboxCollision *collision,
                            bool model_ready);
/* Source box and100m ray; direction need not be normalized. Includes source
 *19m collider reach and native8m admission. Strict geometry occlusion replaces
 * MousePicker's +10m allowance; .0001m only resolves a box/snapshot surface tie.
 * Caller owns UI-hover/focus/modal/gesture gates and nearest-target arbitration.
 * Ray starts inside the collider fail closed. No mutation on hit or miss. */
PeSceneHit pe_camp_repair_pick(const PeGame *game,const PeSandboxCollision *collision,
                             bool model_ready,PeVec3 origin,PeVec3 direction);
#endif
