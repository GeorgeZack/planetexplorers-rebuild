#ifndef PE_TRAINING_TERRAIN_H
#define PE_TRAINING_TERRAIN_H
#include "pe_scene_collision.h"
#define PE_TRAINING_TERRAIN_SEED 751u
#define PE_TRAINING_TERRAIN_ORIGIN_X (-8)
#define PE_TRAINING_TERRAIN_ORIGIN_Y (-4)
#define PE_TRAINING_TERRAIN_ORIGIN_Z (-8)
#define PE_TRAINING_TERRAIN_SAMPLES 726u
/* Source1/material1; exact authored mound in otherwise-air bounded volume.
 * Seed751 identifies this lesson, not a procedural generator. Null is a no-op. */
void pe_training_terrain_init(PeWorld *world);
bool pe_training_terrain_valid(const PeWorld *world);
/* Overlay/clear only the726 authored coordinates, preserving unrelated data.
 * Revision advances once iff changed. Foreign identity/revision overflow
 * rejects atomically; clearing twice succeeds without another revision. */
bool pe_training_terrain_apply(PeWorld *world);
bool pe_training_terrain_clear(PeWorld *world);
/* EmitlineTask753:32 volume255/material1 anchors, absolute source coordinates.
 * Overlay/clear only these coordinates; same lesson-world identity. */
bool pe_training_terrain_build_anchors(PeWorld *world,bool clear);
/* Mesh vertices are local; draw them translated by world origin. Invalid
 * identity/nonfinite input returns allNAN, never a plausible wrong location. */
PeVec3 pe_training_terrain_to_local(const PeWorld *world,PeVec3 room_position);
PeVec3 pe_training_terrain_to_world(const PeWorld *world,PeVec3 local_position);
/* MC-consistent room-space hit position and normal. Sample xyz remain LOCAL
 * indices for pe_world_get/set and resource receipts. Direction unchanged. */
PeHit pe_training_terrain_raycast(const PeWorld *world,PeVec3 room_origin,PeVec3 direction,float range);
PeHit pe_training_terrain_dig_target(const PeWorld *world,PeVec3 room_origin,PeVec3 direction,PeVec3 room_player,float reach);
/* Build exact rendered-MC-triangle capsule collision in room coordinates.
 * Caller initializes*snapshot=NULL. Success replaces/frees old BVH; failure
 * preserves it. Rebuild after edits; snapshot does not retain world pointers. */
bool pe_training_terrain_collision(const PeWorld *world,PeSceneCollision **snapshot,
                                    char *error,size_t error_capacity);
#endif
