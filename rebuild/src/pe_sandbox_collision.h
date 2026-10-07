#ifndef PE_SANDBOX_COLLISION_H
#define PE_SANDBOX_COLLISION_H
#include "pe_block.h"
#include "pe_scene_collision.h"
#include "pe_tree_collision.h"

#define PE_SANDBOX_COLLISION_MAX_CELLS 1024
#define PE_SANDBOX_COLLISION_MAX_BOXES 256
#define PE_SANDBOX_COLLISION_MAX_TREES 64
typedef struct PeSandboxCollision PeSandboxCollision;
typedef struct { PeVec3 minimum,maximum; } PeSandboxBox;
typedef struct { PeVec3 position,velocity; bool grounded; } PeSandboxMotion;
typedef struct { uint64_t generation,terrain_rebuilds,construction_rebuilds; size_t triangles; } PeSandboxCollisionStats;

PeSandboxCollision *pe_sandbox_collision_create(void);
void pe_sandbox_collision_free(PeSandboxCollision *collision);
/* Caller owns this cache outside saved/copied gameplay state. Coordinates are
 * crop-local meters, matching sandbox rendering; world.origin is not applied.
 * Content comparison catches reloads with reused revisions. Failure retains the
 * complete previous snapshot. Inputs must not be mutated during this call. */
bool pe_sandbox_collision_update(PeSandboxCollision *collision,const PeWorld *world,
    const PeB45Cell *cells,size_t cell_count,const PeSandboxBox *boxes,size_t box_count,
    char *error,size_t error_capacity);
/* Include only live, solid source capsules. No PeGame/GPU dependency; tree
 * changes commit atomically with geometry and never require a world revision.
 * The original update API explicitly replaces the tree list with empty. */
bool pe_sandbox_collision_update_with_trees(PeSandboxCollision *collision,const PeWorld *world,
    const PeB45Cell *cells,size_t cell_count,const PeSandboxBox *boxes,size_t box_count,
    const PeTreeCapsule *trees,size_t tree_count,char *error,size_t error_capacity);
/* Borrowed immutable building triangles; owner must outlive this snapshot.
 * NULL clears a previous static scene. Legacy update entry points also clear it. */
bool pe_sandbox_collision_update_with_static(PeSandboxCollision *collision,const PeWorld *world,
    const PeB45Cell *cells,size_t cell_count,const PeSandboxBox *boxes,size_t box_count,
    const PeTreeCapsule *trees,size_t tree_count,const PeSceneCollision *static_scene,
    char *error,size_t error_capacity);
PeSandboxCollisionStats pe_sandbox_collision_stats(const PeSandboxCollision *collision);
typedef enum { PE_SANDBOX_SCENE_TERRAIN,PE_SANDBOX_SCENE_CONSTRUCTION,PE_SANDBOX_SCENE_STATIC } PeSandboxSceneKind;
/* Read-only camera snapshot traversal. No trees or synthetic crop planes.
 * Borrowed scenes remain valid only until the next successful snapshot update;
 * a NULL static entry represents an intentionally absent settlement. */
size_t pe_sandbox_collision_scene_count(const PeSandboxCollision *collision);
bool pe_sandbox_collision_scene(const PeSandboxCollision *collision,size_t index,
    const PeSceneCollision **scene,PeSandboxSceneKind *kind);
/* All queries use the last successful snapshot. Unready/invalid overlap is
 * conservatively true; invalid sweep/support is no hit. Bounds are solid at
 * x/z=0..WORLD-1, feet>=0, head<=WORLD_Y-1. Exact touching is not penetration.
 * Solid containment complements surface overlap: oriented MC crossings and
 * explicit Block45/box interiors reject capsules wholly embedded in solids. */
bool pe_sandbox_collision_overlap(const PeSandboxCollision *collision,PeVec3 feet,float radius,float height);
PeSceneHit pe_sandbox_collision_sweep(const PeSandboxCollision *collision,PeVec3 feet,float radius,float height,PeVec3 delta);
/* Tree-excluding queries preserve camera geometry policy and support explicit
 * old-save migration. Bounds, terrain and construction remain included. */
bool pe_sandbox_collision_overlap_geometry(const PeSandboxCollision *collision,PeVec3 feet,float radius,float height);
PeSceneHit pe_sandbox_collision_sweep_geometry(const PeSandboxCollision *collision,PeVec3 feet,float radius,float height,PeVec3 delta);
/* Exact nearest triangle ray against the cached MC/Block45/furniture geometry.
 * Excludes trees, NPCs and synthetic crop-boundary planes. Direction normalized
 * internally; invalid/nonpositive range is no hit. Source layer11/12 picking
 * adapter, with current furniture boxes retaining their existing approximation. */
PeSceneHit pe_sandbox_collision_ray_geometry(const PeSandboxCollision *collision,
    PeVec3 origin,PeVec3 direction,float range);
/* Perception obstruction geometry: nearest existing geometry-ray hit OR live
 * source tree capsule hit. Exact finite tree ray, no radius inflation; interior
 * tree origins do not select exits. Excludes NPCs/synthetic crop planes. Uses
 * the last successful snapshot. The camera geometry ray remains tree-free. */
PeSceneHit pe_sandbox_collision_ray_perception(const PeSandboxCollision *collision,
    PeVec3 origin,PeVec3 direction,float range);
PeSceneHit pe_sandbox_collision_support(const PeSandboxCollision *collision,PeVec3 feet,float radius,float height,float drop,float minimum_normal_y);
/* Spawn/import recovery only: search upward in .25m increments, then settle on
 * walkable support. No horizontal relocation. max_lift[0,4], max_drop(0,5].
 * A clear initial position is never lifted through a ceiling. An embedded
 * initial position may be lifted out of solids as explicit recovery policy.
 * Failure leaves out unchanged; this is not an ordinary movement operation. */
bool pe_sandbox_collision_settle(const PeSandboxCollision *collision,PeVec3 preferred,
    float radius,float height,float max_lift,float max_drop,PeVec3 *out);
/* Replacement slide/step controller, not Unity physics: skin.003m, step.35m,
 * walkable normalY>=.65. Host supplies velocity AFTER gravity/jump and owns all
 * action/stamina clocks. grounded is prior support; allow_step must be false
 * during upward jump. dt in(0,.25]. Invalid/embedded input leaves out unchanged. */
bool pe_sandbox_collision_move(const PeSandboxCollision *collision,PeVec3 feet,PeVec3 velocity,
    float dt,float radius,float height,bool grounded,bool allow_step,PeSandboxMotion *out);
#endif
