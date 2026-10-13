#ifndef PE_ACTOR_VIEW_H
#define PE_ACTOR_VIEW_H
#include "pe_actor.h"
#include "pe_actor_scene.h"
#include <raylib.h>

typedef struct {
    PeActor *actor;
    PeActorVertex *pose;
    Model model;
    Texture2D texture;
    PeActorMaterial material; /* Source policy; scene owns the borrowed shader. */
    int cutoff_location,alpha_location;
    bool loaded;
    const unsigned char *ik_driver; /* Borrowed per-joint solver bones; NULL is undriven. */
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
/* As draw_blend, with each joint moved by bone_delta (PE_ACTOR_DRIVE_BONES
 * row-major 3x4 motions) through ik_driver. Without a driver or motions, or
 * when the driven pose is refused, this is exactly draw_blend. */
bool pe_actor_view_draw_blend_driven(PeActorView *view,const PeActorBlendSample *samples,size_t count,const float *bone_delta,
                                     Vector3 position,float yaw,float scale,Color tint);
/* Standalone graphics preview; owns window/context. Nonzero return on asset,
 * screenshot, animation, or incomplete smoke failure. */
int pe_actor_preview(const char *peac,int smoke,const char *screenshot);
#endif
