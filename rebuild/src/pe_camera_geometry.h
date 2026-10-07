#ifndef PE_CAMERA_GEOMETRY_H
#define PE_CAMERA_GEOMETRY_H
#include "pe_world.h"

/* Disconnected source GeometryClamp evaluator. No native physics/projection
 * policy, saved fields, desired-distance owner, or display clock is embedded. */
typedef struct { float rise; } PeCameraGeometryState;
typedef struct {
    PeVec3 position,forward,right,up;
    float distance,near_clip,fov_degrees,aspect,activity;
    bool shoot_mode;
} PeCameraGeometryInput;
typedef enum {
    PE_CAMERA_GEOMETRY_RAY, PE_CAMERA_GEOMETRY_LINE,
    PE_CAMERA_GEOMETRY_OVERLAP, PE_CAMERA_GEOMETRY_SPHERE_CAST
} PeCameraGeometryQueryKind;
typedef struct {
    PeCameraGeometryQueryKind kind;
    PeVec3 origin,direction,end; /* end is authoritative for LINE only. */
    float radius,distance;
    uint32_t mask;
    bool ignore_triggers;
} PeCameraGeometryQuery;
typedef struct { bool hit; float distance; } PeCameraGeometryHit;
/* Return false for query failure, not for a successful miss. Hit distance must
 * be finite0..request.distance for ray/line/cast; overlap distance is0. Miss
 * distance is0. LINE preserves both endpoints, including a zero-length line.
 * Provider owns touching, initial-overlap, collider-granularity/crop semantics.
 * Evaluation cannot roll back external callback side effects: use read-only
 * physics snapshots. Zero-normalized source directions are retained. */
typedef bool (*PeCameraGeometryQueryFn)(void *context,const PeCameraGeometryQuery *request,PeCameraGeometryHit *hit);
typedef struct {
    PeVec3 position;
    float near_clip,ncr,distance,rise;
    bool clamped; /* Event: caller may zero velocity/set latch ONLY on commit. */
} PeCameraGeometryResult;
void pe_camera_geometry_init(PeCameraGeometryState *state);
/* Preview leaves state and input unchanged. Outputs commit only after every
 * input, query and computed result is valid. Basis must be orthonormal and
 * right-handed (right cross up=forward), tolerance.0005; never repaired here.
 * Callback may run during previews, but no persistent rise/latch is committed. */
bool pe_camera_geometry_evaluate(const PeCameraGeometryState *state,const PeCameraGeometryInput *input,
    PeCameraGeometryQueryFn query,void *context,PeCameraGeometryResult *output);
/* Exactly one step per final active display evaluation, never aim/catch-up.
 * Rise lerps by source.1 per call, not elapsed time. Pause/mode gates are the
 * future host's responsibility. On failure both state and output are intact. */
bool pe_camera_geometry_step(PeCameraGeometryState *state,const PeCameraGeometryInput *input,
    PeCameraGeometryQueryFn query,void *context,PeCameraGeometryResult *output);
#endif
