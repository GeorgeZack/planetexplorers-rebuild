#ifndef PE_ACTOR_VIEW_H
#define PE_ACTOR_VIEW_H
#include "pe_actor.h"
#include <raylib.h>

typedef struct {
    PeActor *actor;
    PeActorVertex *pose;
    Model model;
    Texture2D texture;
    bool loaded;
} PeActorView;
/* Zero initialize. Requires an active graphics context. Actor validation occurs
 * before graphics allocations; diffuse PNG is optional. No implicit paths. */
bool pe_actor_view_load(PeActorView *view,const char *actor_path,
                        const char *diffuse_png,char *error,size_t capacity);
/* Takes ownership of actor on EVERY call, including failure. Diffuse image
 * is borrowed during upload only; caller retains/frees its CPU pixels. Uses no
 * filesystem paths, enabling exact validated scene snapshots to reach the GPU. */
bool pe_actor_view_adopt(PeActorView *view,PeActor *actor,const Image *diffuse,
                         char *error,size_t capacity);
void pe_actor_view_unload(PeActorView *view);
/* Model-local Unity coordinates, root translation/yaw/degrees/uniform scale.
 * Source diffuse pixels retain their original row order; raylib receives UVs
 * with V flipped so Unity bottom-origin UVs sample the same source pixels. */
bool pe_actor_view_draw(PeActorView *view,size_t clip,double seconds,
                        Vector3 position,float yaw,float scale,Color tint);
bool pe_actor_view_draw_blend(PeActorView *view,const PeActorBlendSample *samples,size_t count,
                              Vector3 position,float yaw,float scale,Color tint);
/* Standalone graphics preview; owns window/context. Nonzero return on asset,
 * screenshot, animation, or incomplete smoke failure. */
int pe_actor_preview(const char *peac,int smoke,const char *screenshot);
#endif
