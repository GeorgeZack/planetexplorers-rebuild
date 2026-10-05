#ifndef PE_TRAINING_TERRAIN_VIEW_H
#define PE_TRAINING_TERRAIN_VIEW_H
#include "pe_training_terrain.h"
#include <raylib.h>
typedef struct {
    PeWorld world;
    PeSceneCollision *collision;
    Model model;
    uint32_t rendered_revision;
    bool initialized,loaded;
    size_t triangles;
} PeTrainingTerrainView;
/* Refresh render and collision snapshots together from the same MC mesh. */
bool pe_training_terrain_view_update(PeTrainingTerrainView *view);
void pe_training_terrain_view_draw(const PeTrainingTerrainView *view);
void pe_training_terrain_view_unload(PeTrainingTerrainView *view);
#endif
