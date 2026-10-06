#ifndef PE_ACTOR_SCENE_H
#define PE_ACTOR_SCENE_H
#include "pe_actor.h"
#define PE_ACTOR_SCENE_MAX_PARTS 64u
#define PE_ACTOR_SCENE_MAX_CLIPS PE_ACTOR_MAX_CLIPS
#define PE_ACTOR_SCENE_PATH_CAPACITY 4096u
typedef struct PeActorSceneManifest PeActorSceneManifest;
typedef struct {
    char actor[PE_ACTOR_SCENE_PATH_CAPACITY];
    char diffuse[PE_ACTOR_SCENE_PATH_CAPACITY];
} PeActorScenePart;
/* Headless preflight. PEAS1 + 1..64 ASCII basename pairs, no traversal or
 * duplicate actor. Every required file must be regular, every actor valid with
 * 1..64 clips with matching ordered names and exact timing/loop/terminal metadata,
 * every PNG envelope bounded. Arbitrary clip names are supported.
 * PNG decompression is deferred to the graphical adapter before InitWindow.
 * Retains the exact loaded actors and PNG bytes; later disk changes are ignored.
 * Limits: actor+PNG bytes512MiB, vertices1M, indices3M, texture pixels64M.
 * Bounds combine all clip-zero first-frame posed vertices in the shared model space. */
PeActorSceneManifest *pe_actor_scene_manifest_load(const char *path,char *error,size_t capacity);
void pe_actor_scene_manifest_free(PeActorSceneManifest *scene);
size_t pe_actor_scene_part_count(const PeActorSceneManifest *scene);
const PeActorScenePart *pe_actor_scene_part(const PeActorSceneManifest *scene,size_t part);
/* Borrowed immutable assets; valid until scene free or actor ownership transfer.
 * take_actor clears the scene slot and transfers ownership exactly once. */
const PeActor *pe_actor_scene_actor(const PeActorSceneManifest *scene,size_t part);
PeActor *pe_actor_scene_take_actor(PeActorSceneManifest *scene,size_t part);
const unsigned char *pe_actor_scene_png(const PeActorSceneManifest *scene,size_t part,size_t *size);
/* clip() is the backward-compatible alias for clip_at(scene,0).
 * Out-of-range clip_at returns NULL; find_clip returns PE_ACTOR_BIND_POSE
 * when absent (including a NULL scene/name). */
size_t pe_actor_scene_clip_count(const PeActorSceneManifest *scene);
const PeActorClip *pe_actor_scene_clip_at(const PeActorSceneManifest *scene,size_t clip);
size_t pe_actor_scene_find_clip(const PeActorSceneManifest *scene,const char *name);
const PeActorClip *pe_actor_scene_clip(const PeActorSceneManifest *scene);
bool pe_actor_scene_bounds(const PeActorSceneManifest *scene,float min[3],float max[3]);
/* Timeline comparison only; not an asset-identity or content check. */
bool pe_actor_scene_clip_matches(const PeActorSceneManifest *scene,const PeActor *actor);
/* Graphical adapter owns its window; smoke=0 interactive or1..36000 fixed60Hz
 * frames. Space pause, R restart, left/right arrows exact sampled-frame step,
 * up/down select clip and reset time (preserving pause), RMB orbit, wheel zoom.
 * Smoke always selects clip zero. No fixed idle/run clip names or gameplay side effects. */
int pe_actor_scene_preview(const char *path,int smoke,const char *screenshot);
#endif
