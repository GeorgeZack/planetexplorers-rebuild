#ifndef PE_TRAINING_UI_MINIMAP_H
#define PE_TRAINING_UI_MINIMAP_H
#include "pe_training_ui_layout.h"
#include "pe_ui_assets.h"
#include "pe_world.h"
typedef struct {
    Shader shader;
    int center_location;
    bool initialized;
    float center_x,center_z;
    double refresh_time;
} PeTrainingUIMinimap;
/* Requires a live graphics context. Owns only its shader; zero/init once. */
bool pe_training_ui_minimap_load(PeTrainingUIMinimap *map,char *error,size_t capacity);
void pe_training_ui_minimap_unload(PeTrainingUIMinimap *map);
/* Draw texture/9 NPC markers/player arrow, then caller draws frame/buttons.
   Texture and atlas are borrowed. game_seconds is source game time, not an
   internally invented wall-clock rate. First draw emulates RT recreation. */
bool pe_training_ui_minimap_draw(PeTrainingUIMinimap *map,const PeUiAssets *assets,
    Texture2D texture,const PeTrainingUILayout *layout,PeVec3 player,
    float heading_degrees,double game_seconds);
#endif
