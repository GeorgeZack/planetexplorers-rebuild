#ifndef PE_TRAINING_NPC_VIEW_H
#define PE_TRAINING_NPC_VIEW_H
#include "pe_training_npc.h"
#include "pe_actor_scene_view.h"
typedef struct {
    PeActorSceneView actors[PE_TRAINING_NPC_COUNT];
    PeVec3 positions[PE_TRAINING_NPC_COUNT];
    float label_heights[PE_TRAINING_NPC_COUNT];
    size_t idle_clips[PE_TRAINING_NPC_COUNT];
    size_t count;
    double seconds;
} PeTrainingNpcView;
/* Optional as a complete group: absent group succeeds empty; a partial or
 * invalid supplied group fails. NPCs retain their own rigs and idle clips. */
bool pe_training_npc_view_load(PeTrainingNpcView *view,const char *asset_dir,
    const PeSceneCollision *room,char *error,size_t capacity);
void pe_training_npc_view_unload(PeTrainingNpcView *view);
bool pe_training_npc_view_draw(PeTrainingNpcView *view);
bool pe_training_npc_view_position(const PeTrainingNpcView *view,int id,PeVec3 *position);
#endif
