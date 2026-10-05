#ifndef PE_TRAINING_NPC_H
#define PE_TRAINING_NPC_H
#include "pe_scene_collision.h"
#define PE_TRAINING_NPC_COUNT 9u
typedef struct {
    int id,prototype_id;
    const char *name,*bundle,*model,*behavior;
    PeVec3 position; /* Authored npc.training_pos model root, not capsule feet. */
    float yaw; /* Unity +Y degrees; Andy739 Plot470 changes spawn0 to275. */
    float radius,height,center_y;
    bool tutorial_neutralized; /* Seven IDs in SetNPCsAtrribType; not all nine. */
} PeTrainingNpc;
const PeTrainingNpc *pe_training_npc_at(size_t index);
const PeTrainingNpc *pe_training_npc_find(int entity_id);
/* New bounded C settlement approximates source gravity, not source placement
 * editing. Uses the actual source Y capsule/root offset; x/z are unchanged.
 * Tries authored root then .25m lifts up to3m and sweeps downward at most5m.
 * Requires a clear capsule and walkable support; output untouched on failure.
 * No NPC/player collision is added: both physical capsules use source layer10,
 * whose self-pair is disabled (the separate layer8-vs10 pair is enabled). */
bool pe_training_npc_ground(const PeTrainingNpc *npc,const PeSceneCollision *room,
                            PeVec3 *root);
#endif
