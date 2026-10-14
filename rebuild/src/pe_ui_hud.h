#ifndef PE_UI_HUD_H
#define PE_UI_HUD_H
#include "pe_ui_draw.h"

/* Presentation snapshots only. Hosts retain item identity, clocks, mission
 * progression, map content and all input/action ownership. */
typedef struct { int item_id,count; float cooldown; } PeUiQuickslotView;
typedef struct {
    float health,health_max,stamina,stamina_max;
    /* Adventure only: the host's summed hunger. Without it the track stays empty. */
    bool hunger_available;
    float hunger,hunger_max;
    /* Adventure only: the host's pe_comfort_hud presentation (slider 0..1, foreground
     * alpha, face 1..3). Without it the track stays empty and the prefab's face3 shows. */
    bool comfort_available;
    float comfort_slider,comfort_alpha;
    int comfort_face;
    PeUiQuickslotView slots[PE_UI_QUICKSLOTS];
    int hand_item_id;
    bool building;
} PeUiHudView;
typedef struct {
    bool coordinates_available,clock_available;
    double x,z;
    int hour,minute;
} PeUiMinimapView;
typedef struct { const char *text; bool completed; } PeUiTrackerObjective;
typedef struct {
    const char *title;
    bool main_story;
    const PeUiTrackerObjective *objectives;
    size_t objective_count;
} PeUiTrackerView;
void pe_ui_hud_draw(const PeUiCanvas *canvas,const PeUiHudView *view);
/* Draw after the host's texture/markers; never selects a map projection. */
void pe_ui_minimap_chrome_draw(const PeUiCanvas *canvas,const PeUiMinimapView *view);
/* Title and goals share bounded wrapping. Returns used screen-pixel height. */
float pe_ui_tracker_draw(const PeUiCanvas *canvas,const PeUiTrackerView *view);
#endif
