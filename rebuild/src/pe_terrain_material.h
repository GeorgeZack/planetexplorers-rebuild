#ifndef PE_TERRAIN_MATERIAL_H
#define PE_TERRAIN_MATERIAL_H
#include "pe_world.h"
/* Rendering palette only: never changes voxel IDs, resources or saved data. */
typedef struct { int atlas; float tile_x,tile_y,tile_size,repetition,inset; } PeTerrainTile;
int pe_terrain_source_material(uint32_t world_source,uint8_t material);
bool pe_terrain_material_tile(int source_id,PeTerrainTile *out);
#endif
