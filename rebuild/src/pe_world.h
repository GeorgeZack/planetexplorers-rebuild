#ifndef PE_WORLD_H
#define PE_WORLD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PE_WORLD_X 64
#define PE_WORLD_Y 32
#define PE_WORLD_Z 64
#define PE_VOXEL_COUNT (PE_WORLD_X * PE_WORLD_Y * PE_WORLD_Z)
#define PE_ISO_LEVEL 128

typedef struct { float x, y, z; } PeVec3;
typedef struct { uint8_t volume, material; } PeVoxel;
enum { PE_MAT_AIR, PE_MAT_DIRT, PE_MAT_STONE, PE_MAT_GRASS };
/* x-fast samples: ((z * PE_WORLD_Y) + y) * PE_WORLD_X + x; 1 m spacing. */
typedef struct {
    uint32_t seed, revision;
    uint32_t source; /* 0: generated palette, 1: original terrain material IDs */
    int32_t origin_x, origin_y, origin_z; /* original file-local crop origin */
    PeVoxel voxels[PE_VOXEL_COUNT];
} PeWorld;
typedef struct { bool hit; PeVec3 position, normal; int x,y,z; float distance; } PeHit;
typedef struct { PeVec3 position, normal; uint8_t material; } PeVertex;
typedef struct { PeVertex *vertices; size_t count; } PeMeshData;
void pe_world_init(PeWorld *world, uint32_t seed);
PeVoxel pe_world_get(const PeWorld *world, int x, int y, int z);
bool pe_world_set(PeWorld *world, int x, int y, int z, PeVoxel voxel);
float pe_world_density(const PeWorld *world, PeVec3 position);
float pe_world_height(const PeWorld *world, float x, float z);
bool pe_world_collides(const PeWorld *world, PeVec3 feet, float radius, float height);
PeHit pe_world_raycast(const PeWorld *world, PeVec3 origin, PeVec3 direction, float range);
/* Source DigVoxelIndicator targeting: centered sample boxes, threshold retries,
 * axial-solid-neighbor validation and strict reach from player feet + (0,1,0).
 * xyz is the selected sample, including sub-isosurface boundary samples. */
PeHit pe_world_dig_target(const PeWorld *world, PeVec3 origin, PeVec3 direction,
                         PeVec3 player_feet, float reach);
/* Returns count of formerly solid samples made non-solid. */
int pe_world_dig(PeWorld *world, PeVec3 center, float radius);
/* Initialize mesh to {0}; successful rebuild replaces its owned vertex buffer. */
bool pe_world_mesh(const PeWorld *world, PeMeshData *mesh);
/* Up to five source marching-cubes triangles for an interior cell; no allocation.
 * Returns vertex count (0..15), or zero for an invalid/empty cell. */
int pe_world_cell_triangles(const PeWorld *world, int x, int y, int z, PeVec3 vertices[15]);
bool pe_world_mesh_region(const PeWorld *world, int x, int y, int z,
                          int cells_x, int cells_y, int cells_z, PeMeshData *mesh);
/* Includes the neighboring samples used by gradient normals. Invalid regions
 * are conservatively dirty. Source palette changes invalidate rendered material attributes. */
bool pe_world_mesh_region_changed(const PeWorld *world, const PeWorld *previous,
                                 int x, int y, int z, int cells_x, int cells_y, int cells_z);
int pe_world_material_item(const PeWorld *world, uint8_t material);
void pe_mesh_free(PeMeshData *mesh);
uint64_t pe_world_hash(const PeWorld *world);
#endif
