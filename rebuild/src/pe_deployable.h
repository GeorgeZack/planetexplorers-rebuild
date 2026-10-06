#ifndef PE_DEPLOYABLE_H
#define PE_DEPLOYABLE_H
#include "pe_game.h"

/* Torch10's two source capsules are identical triggers. They participate only
 * in selection, never player/camera/tool obstruction or placement overlap. */
bool pe_deployable_state_valid(const PeGame *game);
const PeDeployable *pe_deployable_find(const PeGame *game,uint32_t instance_id);
/* Maps raw item-local XYZ through source R_x(-90) followed by local Y turns. */
PeVec3 pe_deployable_point(const PeDeployable *item,PeVec3 local);
/* Caller refreshes the collision snapshot first. Preview is pure; placement
 * re-queries it and moves the exact inventory instance atomically. */
bool pe_deployable_preview(const PeGame *game,const PeSandboxCollision *collision,
    uint32_t instance_id,PeVec3 ray_origin,PeVec3 ray_direction,PeVec3 *position);
bool pe_deployable_place(PeGame *game,const PeSandboxCollision *collision,
    uint32_t instance_id,PeVec3 ray_origin,PeVec3 ray_direction);
/* Source4m bounds range,100m capsule ray, and MousePicker obstacle+10 allowance.
 * Output arguments remain unchanged on failure. Host owns the transient menu. */
bool pe_deployable_pick(const PeGame *game,PeVec3 ray_origin,PeVec3 ray_direction,
    float obstacle_distance,uint32_t *instance_id,float *distance);
bool pe_deployable_in_range(const PeGame *game,uint32_t instance_id);
/* Source Get destroys the original torch and grants one Charcoal1001. It does
 * not return a fresh/equivalent torch. Capacity failure preserves everything. */
bool pe_deployable_get(PeGame *game,uint32_t instance_id);
bool pe_deployable_turn(PeGame *game,uint32_t instance_id);
#endif
