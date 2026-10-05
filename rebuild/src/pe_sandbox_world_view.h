#ifndef PE_SANDBOX_WORLD_VIEW_H
#define PE_SANDBOX_WORLD_VIEW_H
#include "pe_game.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct PeSandboxWorldView PeSandboxWorldView;
/* Requires an active raylib graphics context. Required assets live under
 * asset_root/sandbox-world; failed loading returns NULL with no owned GPU state.
 * One source-local original tree model is shared by all provisional spawns. */
PeSandboxWorldView *pe_sandbox_world_view_load(const char *asset_root,char *error,size_t capacity);
/* Call inside BeginMode3D. Reads current positions/remaining every draw, including
 * after save reload. No animation state or gameplay writes. Invalid tree data
 * fails before drawing any instances. Depleted trees are completely absent. */
bool pe_sandbox_world_view_draw_trees(const PeSandboxWorldView *view,const PeGame *game);
/* CPU query against the SAME source-local triangles (including leaf cards),
 * translated by each live tree position. This is rendered geometry, not a port
 * of original species-specific Unity trunk colliders. Normalizes direction.
 * Valid miss: index=-1, distance=max_distance. Invalid: outputs unchanged. */
bool pe_sandbox_world_view_ray_tree(const PeSandboxWorldView *view,const PeGame *game,
    PeVec3 origin,PeVec3 direction,float max_distance,int *index,float *distance);
/* Source tree11_grassland capsule proven to reference the same source mesh as
 * this visual. Ignores leaf cards; origins strictly inside a collider miss that
 * collider, matching Physics.Raycast. Same valid-miss/invalid-output contract. */
bool pe_sandbox_world_view_ray_trunk(const PeSandboxWorldView *view,const PeGame *game,
    PeVec3 origin,PeVec3 direction,float max_distance,int *index,float *distance);
void pe_sandbox_world_view_unload(PeSandboxWorldView *view);
#endif
