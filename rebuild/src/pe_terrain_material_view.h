#ifndef PE_TERRAIN_MATERIAL_VIEW_H
#define PE_TERRAIN_MATERIAL_VIEW_H
#include "pe_terrain_material.h"
#include <raylib.h>
typedef struct {
    Shader shader;
    Texture2D diffuse[6];
    int origin_location,camera_location;
    bool loaded;
} PeTerrainMaterialView;
bool pe_terrain_material_view_load(PeTerrainMaterialView *view,const char *root,char *error,size_t capacity);
void pe_terrain_material_view_unload(PeTerrainMaterialView *view);
/* Model borrows resources; UnloadModel frees its maps, not these textures/shader. */
bool pe_terrain_material_view_bind(const PeTerrainMaterialView *view,Model *model);
void pe_terrain_material_view_frame(PeTerrainMaterialView *view,const PeWorld *world,Vector3 camera);
#endif
