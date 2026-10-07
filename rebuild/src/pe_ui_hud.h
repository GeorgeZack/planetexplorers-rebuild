#ifndef PE_UI_HUD_H
#define PE_UI_HUD_H
#include "pe_ui_draw.h"

/* Presentation snapshots only. Hosts retain item identity, clocks, mission
 * progression, map content and all input/action ownership. */
typedef struct { int item_id,count; float cooldown; } PeUiQuickslotView;
typedef struct {
    float health,health_max,stamina,stamina_max;
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
