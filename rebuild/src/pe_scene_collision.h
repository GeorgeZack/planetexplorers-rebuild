#ifndef PE_SCENE_COLLISION_H
#define PE_SCENE_COLLISION_H
#include "pe_world.h"

typedef struct PeSceneCollision PeSceneCollision;
typedef struct {
    bool hit;
    PeVec3 position, normal;
    float distance, fraction;
    uint32_t triangle;
} PeSceneHit;
typedef struct {
    size_t vertices, triangles, objects, nodes, degenerate_triangles;
    PeVec3 minimum, maximum;
} PeSceneStats;

/* Bounded, transactional OBJ loader. Triangular positive-index faces only;
 * optional UV/normal references are validated but not used for collision.
 * Coordinates are already baked world meters. Never loads external files. */
PeSceneCollision *pe_scene_load_obj(const char *path, char *error, size_t error_capacity);
/* Copy triangle-list positions with a world offset into an independent BVH.
 * Same bounds as OBJ; normals/materials unused. Empty mesh is a valid empty
 * snapshot; invalid geometry/all-degenerate nonempty mesh fails atomically. */
PeSceneCollision *pe_scene_from_mesh(const PeMeshData *mesh,PeVec3 origin,
                                     char *error,size_t error_capacity);
void pe_scene_free(PeSceneCollision *scene);
PeSceneStats pe_scene_stats(const PeSceneCollision *scene);
PeSceneHit pe_scene_raycast(const PeSceneCollision *scene, PeVec3 origin,
                           PeVec3 direction, float range);
/* Triangle surface versus open box interior; exact boundary contact is allowed.
 * Conservative placement uses a shape's inset bounds. This does not classify
 * closed-mesh interiors as solid (merged building scenes can contain rooms).
 * NULL scene is empty; invalid box inputs conservatively overlap. */
bool pe_scene_overlaps_aabb(const PeSceneCollision *scene,PeVec3 minimum,PeVec3 maximum);
/* Capsule includes hemispheres: feet is bottom, axis y=[radius,height-radius].
 * Radius>0, height>=2*radius. Exact touching is not penetrating overlap.
 * Invalid inputs conservatively overlap; invalid sweeps return no hit. */
bool pe_scene_capsule_overlap(const PeSceneCollision *scene, PeVec3 feet,
                              float radius, float height);
/* Continuous translation against static triangles; no sliding/step response.
 * Earliest contact fraction is in [0,1], normal points toward the capsule.
 * Initial tangent/separating contacts do not block motion. */
PeSceneHit pe_scene_capsule_sweep(const PeSceneCollision *scene, PeVec3 feet,
                                 float radius, float height, PeVec3 delta);
#endif
