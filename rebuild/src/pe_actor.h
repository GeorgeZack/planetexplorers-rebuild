#ifndef PE_ACTOR_H
#define PE_ACTOR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct PeActor PeActor;
typedef struct { float position[3], normal[3], uv[2]; } PeActorVertex;
typedef struct {
    char name[32];
    float fps, duration;
    uint32_t frames;
    bool loop;
    bool terminal_at_duration; /* PEAC2: final sample is at duration, not (frames-1)/fps. */
} PeActorClip;
typedef struct { size_t vertices, indices, joints, clips; } PeActorStats;
#define PE_ACTOR_MAX_CLIPS 64u
#define PE_ACTOR_BIND_POSE ((size_t)-1)
#define PE_ACTOR_BLEND_MAX 13u
#define PE_ACTOR_DRIVE_BONES 32u
typedef struct { size_t clip; double seconds; float weight; } PeActorBlendSample;

/* Strict bounded PEAC v1/v2 loader. V1 keeps uniform frame/fps timestamps.
 * V2 has >=2 samples: interior frame/fps, final sample exactly at duration.
 * No graphics, external files, or global state.
 * All source coordinates are Unity model-local meters; palette matrices already
 * include inverse bind. Callers own the returned actor. Failure returns NULL. */
PeActor *pe_actor_load(const char *path, char *error, size_t error_capacity);
void pe_actor_free(PeActor *actor);
PeActorStats pe_actor_stats(const PeActor *actor);
const uint32_t *pe_actor_indices(const PeActor *actor);
/* FNV-1a over LE vertex/index/joint counts, vertex+skin data, indices and
 * inverse binds. Anchors exact skin order; independent of appended clips. */
uint64_t pe_actor_skeleton_hash(const PeActor *actor);
const PeActorClip *pe_actor_clip(const PeActor *actor, size_t clip);
size_t pe_actor_find_clip(const PeActor *actor, const char *name);
/* Animation seconds >=0. Loop by authored duration (terminal samples may be
 * guards), linear interpolation of frame-skinned vertices and normals.
 * Normals use inverse-transpose palettes, then normalize. No allocation.
 * Invalid arguments leave output untouched. Bind-pose ignores seconds. */
bool pe_actor_pose(const PeActor *actor, size_t clip, double seconds,
                   PeActorVertex *output, size_t capacity);
bool pe_actor_pose_vertex(const PeActor *actor, size_t clip, double seconds,
                          size_t vertex, PeActorVertex *output);
/* A rigid point attached to one joint, expressed in mesh bind space. Uses
 * exactly the same sampled palette/time interpolation as vertex positions.
 * No allocation; invalid input leaves output unchanged (aliasing is allowed). */
bool pe_actor_pose_joint_point(const PeActor *actor,size_t clip,double seconds,
                               size_t joint,const float bind_mesh_point[3],float output[3]);
/* Bounded presentation adapter: blend already frame-skinned poses, not Unity
 * TRS/controller blends. All entries (including zero weights) must be valid;
 * count1..13, finite nonnegative weights, positive total normalized internally.
 * One positive entry is exactly the existing pose. Final normals normalize,
 * with bind-normal fallback on cancellation. No allocations; invalid arguments
 * leave output untouched. Joint points use the identical position blend. */
bool pe_actor_blend_valid(const PeActor *actor,const PeActorBlendSample *samples,size_t count);
bool pe_actor_pose_blend(const PeActor *actor,const PeActorBlendSample *samples,size_t count,
                         PeActorVertex *output,size_t capacity);
bool pe_actor_pose_vertex_blend(const PeActor *actor,const PeActorBlendSample *samples,size_t count,
                                size_t vertex,PeActorVertex *output);
bool pe_actor_pose_joint_point_blend(const PeActor *actor,const PeActorBlendSample *samples,size_t count,
                                     size_t joint,const float bind_mesh_point[3],float output[3]);
/* Presentation adapter for a solved skeleton. Each joint's blended palette is
 * moved by bone_delta[driver[joint]*12..], a row-major 3x4 rigid motion in
 * model space, before skinning; bone_delta holds PE_ACTOR_DRIVE_BONES of them. Positions with identity deltas equal the blend above
 * to float rounding; normals come from the moved blended palette rather than
 * per-sample normals. driver holds one index below PE_ACTOR_DRIVE_BONES per
 * joint. No allocations; invalid arguments, nonfinite deltas or a singular
 * blended palette leave output untouched. */
bool pe_actor_pose_blend_driven(const PeActor *actor,const PeActorBlendSample *samples,size_t count,
                                const unsigned char *driver,const float *bone_delta,
                                PeActorVertex *output,size_t capacity);
/* The joint point of pe_actor_pose_joint_point_blend carried by the same motion. */
bool pe_actor_pose_joint_point_blend_driven(const PeActor *actor,const PeActorBlendSample *samples,size_t count,
                                            const unsigned char *driver,const float *bone_delta,
                                            size_t joint,const float bind_mesh_point[3],float output[3]);
#endif
