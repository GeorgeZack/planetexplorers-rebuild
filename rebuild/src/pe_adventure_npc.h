#ifndef PE_ADVENTURE_NPC_H
#define PE_ADVENTURE_NPC_H
#include "pe_sandbox_collision.h"

enum { PE_ADVENTURE_NPC_ENTITY=20016, PE_ADVENTURE_NPC_PROTOTYPE=22, PE_ADVENTURE_WELCOME_ENTITY=20008 };
typedef struct {
    PeVec3 position,spawn_position;
    float yaw;
    bool placed;
    uint64_t collision_generation;
} PeAdventureNpc;
/* Caller-owned transient encounter. Initialize to zero on new game/load; no
 * source town placement or NPC persistence is claimed. World and collision
 * must describe the same crop-local snapshot. Search is deterministic within
 * 8m of (32,27), using the source physical capsule and real walkable support.
 * Later updates keep XZ when possible. spawn_position freezes the first valid
 * pose for the source role-2 arrival anchor; subsequent settling never edits it.
 * Failure leaves the encounter unchanged; host must hide it until refreshed.
 * Physical layer10-vs10 is disabled in source: do not add it to player solids. */
bool pe_adventure_npc_refresh(PeAdventureNpc *npc,const PeWorld *world,
    const PeSandboxCollision *collision,char *error,size_t error_capacity);
/* Same placement contract around an explicit finite crop-local XZ anchor. */
bool pe_adventure_npc_refresh_at(PeAdventureNpc *npc,const PeWorld *world,
    const PeSandboxCollision *collision,PeVec3 anchor,char *error,size_t capacity);
/* Archived pose: retain exact XZ and heading; allow vertical settling after
 * terrain edits. If ordinary settling fails, retain exact existing/archived
 * XYZ only for a clear capsule with upward real contact within .06m and a
 * walkable center floor within .15m. Never search nearby rings. */
bool pe_adventure_npc_refresh_source(PeAdventureNpc *npc,const PeWorld *world,
    const PeSandboxCollision *collision,PeVec3 anchor,float yaw,char *error,size_t capacity);
PeTreeCapsule pe_adventure_npc_capsule(const PeAdventureNpc *npc);
/* Source MousePickableNPC: rough root-axis filter, 4.5m AABB distance, capsule
 * camera ray to 100m, then obstacle-distance +10m allowance (layers11/12).
 * obstacle_distance is the nearest exact ray hit, or +INFINITY for no hit.
 * Physical capsule is checked first, then the source Defence trigger, then
 * generated OutCollider (.1m larger). This is idle geometry eligibility;
 * caller owns alive/talk/UI,
 * priority and hover-delay gates. No facing cone. Invalid/miss leaves out alone. */
bool pe_adventure_npc_pick_sex(const PeAdventureNpc *npc,bool male,PeVec3 player_position,
    PeVec3 ray_origin,PeVec3 ray_direction,float obstacle_distance,float *out_distance);
bool pe_adventure_npc_pick(const PeAdventureNpc *npc,PeVec3 player_position,
    PeVec3 ray_origin,PeVec3 ray_direction,float obstacle_distance,float *out_distance);
#endif
