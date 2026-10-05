#ifndef PE_ADVENTURE_NPC_VIEW_H
#define PE_ADVENTURE_NPC_VIEW_H
#include "pe_actor_scene_view.h"
#include "pe_world.h"
/* One recovered reference-world supplier; no town/population generator. */
typedef struct {
    PeActorSceneView actor;
    size_t idle;
    RenderTexture2D portrait;
    float label_height;
} PeAdventureNpcView;
bool pe_adventure_npc_view_load(PeAdventureNpcView *view,const char *asset_root,char *error,size_t capacity);
bool pe_adventure_npc_view_load_role(PeAdventureNpcView *view,const char *asset_root,int role,char *error,size_t capacity);
bool pe_adventure_npc_view_load_scene(PeAdventureNpcView *view,const char *path,char *error,size_t capacity);
void pe_adventure_npc_view_unload(PeAdventureNpcView *view);
bool pe_adventure_npc_view_draw(PeAdventureNpcView *view,PeVec3 position,float yaw,double seconds);
#endif
