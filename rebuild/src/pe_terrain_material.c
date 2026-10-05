#include "pe_terrain_material.h"
#include "../generated/pe_terrain_materials.h"
int pe_terrain_source_material(uint32_t source,uint8_t material) {
    if(source==1)return material>=1&&material<=PE_TERRAIN_MATERIAL_COUNT?material:0;
    if(source!=0)return 0;
    switch(material) {
        case PE_MAT_DIRT:return PE_TERRAIN_SOURCE_DIRT;
        case PE_MAT_STONE:return PE_TERRAIN_SOURCE_STONE;
        case PE_MAT_GRASS:return PE_TERRAIN_SOURCE_GRASS;
        default:return 0;
    }
}
bool pe_terrain_material_tile(int id,PeTerrainTile *out) {
    if(!out||id<1||id>PE_TERRAIN_MATERIAL_COUNT)return false;
    int index=id-1,atlas=index/16,slot=index%16,grid=4;
    if(index>=68){atlas=5;slot=index-68;}
    else if(index>=64){atlas=4;slot=index-64;grid=2;}
    *out=(PeTerrainTile){atlas,(float)(slot%grid)/grid,(float)(slot/grid)/grid,
        1.0f/grid,grid==2?PE_TERRAIN_REPETITION_LARGE:PE_TERRAIN_REPETITION,
        PE_TERRAIN_INDENT*(grid==2?.001953125f:.00390625f)};
    return true;
}
