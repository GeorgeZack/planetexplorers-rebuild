#ifndef PE_ACTOR_SCENE_VIEW_H
#define PE_ACTOR_SCENE_VIEW_H
#include "pe_actor_scene.h"
#include "pe_actor_view.h"
/* Zero initialize. Owns the retained manifest, actor views and temporary CPU
 * images. Treat fields as read-only; dispose before closing the GL context. */
typedef struct {
    PeActorSceneManifest *manifest;
    PeActorView *parts;
    Image *images;
    size_t count;
    bool loaded;
} PeActorSceneView;
/* CPU-only prepare works without a window. All PNGs decode and aggregate
 * dimensions validate before upload. No files are reread during upload. */
bool pe_actor_scene_view_prepare(PeActorSceneView *view,const char *path,
                                 char *error,size_t capacity);
/* Requires prepared state and active graphics context. Failure releases the
 * newly prepared view; success releases CPU images and retains actor palettes. */
bool pe_actor_scene_view_upload(PeActorSceneView *view,char *error,size_t capacity);
/* Convenience prepare+upload for callers already owning a graphics context. */
bool pe_actor_scene_view_load(PeActorSceneView *view,const char *path,
                              char *error,size_t capacity);
/* Draw all parts with an explicit clip at one shared time and model-root transform.
 * Invalid clip indices fail before drawing any part; draw() aliases clip zero.
 * Seconds>=0, yaw degrees, uniform scale(0,1000]. Does not own BeginMode3D. */
bool pe_actor_scene_view_draw_clip(PeActorSceneView *view,size_t clip,double seconds,
                                   Vector3 position,float yaw,float scale,Color tint);
/* Common ordered clip indices shared by all parts. Validate the full blend and
 * every part before mutating pose buffers or issuing any draw. No allocations. */
bool pe_actor_scene_view_draw_blend(PeActorSceneView *view,const PeActorBlendSample *samples,size_t count,
                                    Vector3 position,float yaw,float scale,Color tint);
bool pe_actor_scene_view_draw(PeActorSceneView *view,double seconds,
                              Vector3 position,float yaw,float scale,Color tint);
/* Prepared CPU-only views can unload without a window; uploaded views require
 * their live graphics context. Repeated unload on an empty view is safe. */
void pe_actor_scene_view_unload(PeActorSceneView *view);
size_t pe_actor_scene_view_part_count(const PeActorSceneView *view);
size_t pe_actor_scene_view_clip_count(const PeActorSceneView *view);
const PeActorClip *pe_actor_scene_view_clip_at(const PeActorSceneView *view,size_t clip);
size_t pe_actor_scene_view_find_clip(const PeActorSceneView *view,const char *name);
const PeActorClip *pe_actor_scene_view_clip(const PeActorSceneView *view);
/* Combined first-pose bounds in untransformed shared model coordinates. */
bool pe_actor_scene_view_bounds(const PeActorSceneView *view,Vector3 *min,Vector3 *max);
#endif
