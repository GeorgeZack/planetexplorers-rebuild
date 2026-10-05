#ifndef PE_TRAINING_BUILD_H
#define PE_TRAINING_BUILD_H
#include "pe_block.h"
#include "pe_scene_collision.h"
#define PE_TRAINING_BUILD_CAPACITY 1024
#define PE_TRAINING_BUILD_BATCH 64
#define PE_TRAINING_BUILD_ITEM_LIMIT 1000000u
/* Safety extent, not a recovered source building restriction: half-meter
 * indices [-256,255], i.e. world meters [-128,128]. */
typedef struct {
    PeB45Cell cells[PE_TRAINING_BUILD_CAPACITY];
    size_t count;
    uint32_t revision;
} PeTrainingBuild;
void pe_training_build_init(PeTrainingBuild *build);
bool pe_training_build_valid(const PeTrainingBuild *build);
/* Borrow the authoritative lesson dirt count; never copies it into state.
 * Host authorizes mission/input/reach/room placement. Source material0 only.
 * All failures preserve cells, revision and dirt. Player is a real capsule. */
bool pe_training_build_add(PeTrainingBuild *build,uint32_t *dirt,
    const PeB45Cell *cells,size_t count,PeVec3 feet,float radius,float height);
bool pe_training_build_remove(PeTrainingBuild *build,uint32_t *dirt,
    const size_t *indices,size_t count);
/* Source task cleanup: clear tracked construction without a resource refund. */
bool pe_training_build_clear(PeTrainingBuild *build);
/* World-space geometry; caller passes an empty mesh and owns its allocation.
 * Collision replacement is transactional; old snapshot is freed on success. */
bool pe_training_build_mesh(const PeTrainingBuild *build,PeMeshData *mesh);
bool pe_training_build_collision(const PeTrainingBuild *build,
    PeSceneCollision **snapshot,char *error,size_t capacity);
/* Actual shape hit for receiver tests; inside-origin rays return exit surface,
 * matching pe_b45_raycast (not Unity's inside-collider suppression). */
PeB45Hit pe_training_build_raycast(const PeTrainingBuild *build,PeVec3 origin,
    PeVec3 direction,float range,size_t *index);
typedef struct {
    bool hit,terrain;
    PeB45Cell cell; /* snapped add/remove cell; cube/material0 default */
    PeVec3 position,normal; /* voxel-grid face and unit axis normal */
    float distance;
    size_t index; /* existing block index for subtraction, SIZE_MAX otherwise */
} PeTrainingBuildTarget;
/* Source size1 point-brush occupancy picking: terrain sample boxes centered
 * on integer samples, block half-meter boxes (including ramps). Add chooses
 * nearest datasource; subtract selects only blocks. No room collider target
 * or artificial3mreach. Uses bounded world and source10000m search ceiling. */
PeTrainingBuildTarget pe_training_build_target(const PeWorld *world,
    const PeTrainingBuild *build,PeVec3 origin,PeVec3 direction,bool subtract);
#endif
