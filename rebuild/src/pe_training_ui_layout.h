#ifndef PE_TRAINING_UI_LAYOUT_H
#define PE_TRAINING_UI_LAYOUT_H
#include <stdbool.h>
#include <stddef.h>

/* Screen pixels, top-left origin. Geometry only: no graphics dependency. */
typedef struct { float x, y, w, h; } PeTrainingUIRect;
typedef enum {
    PE_UI_TOP_LEFT, PE_UI_TOP_CENTER, PE_UI_TOP_RIGHT,
    PE_UI_CENTER, PE_UI_BOTTOM_CENTER, PE_UI_BOTTOM_RIGHT
} PeTrainingUIAnchor;
enum { PE_UI_INVENTORY_SLOTS=42, PE_UI_STORAGE_SLOTS=30,
       PE_UI_QUICKSLOTS=9, PE_UI_INVENTORY_TABS=5, PE_UI_STORAGE_TABS=4, PE_UI_MENU_ITEMS=5 };
typedef struct {
    float width, height, scale;
    PeTrainingUIRect hud, hand_slot, build_button, quickslots[PE_UI_QUICKSLOTS];
    PeTrainingUIRect health, stamina, minimap, mission_tracker, notification, mission_button;
    PeTrainingUIRect minimap_texture, minimap_clip, minimap_player;
    PeTrainingUIRect dialogue, dialogue_portrait, dialogue_name, dialogue_text, dialogue_next;
    PeTrainingUIRect mission, mission_close;
    PeTrainingUIRect menu_button, menu_panel, menu_items[PE_UI_MENU_ITEMS];
    PeTrainingUIRect inventory, inventory_close, inventory_title;
    PeTrainingUIRect inventory_tabs[PE_UI_INVENTORY_TABS];
    PeTrainingUIRect inventory_slots[PE_UI_INVENTORY_SLOTS], inventory_page;
    PeTrainingUIRect storage, storage_close, storage_title;
    PeTrainingUIRect storage_tabs[PE_UI_STORAGE_TABS];
    PeTrainingUIRect storage_slots[PE_UI_STORAGE_SLOTS], storage_page;
    /* Source prefab dimensions and UIDefaultPostion defaults. Character
       has its source default at x=-20; the movable window layer clamps
       panels into the viewport. Positions are session-local presentation state. */
    PeTrainingUIRect equipment, equipment_close, equipment_weapon, equipment_preview;
    PeTrainingUIRect equipment_slots[10];
    PeTrainingUIRect replicator, replicator_close, replicator_recipes;
    PeTrainingUIRect replicator_recipe, replicator_material, replicator_craft;
    PeTrainingUIRect replicator_output, replicator_progress, replicator_craft_icon;
    /* replicator_material remains an interim ingredient presentation slot;
       the source dynamically lays out a recipe dependency graph. */
} PeTrainingUILayout;

/* Ordered back-to-front; HUD/dialogue stay anchored. Zero-init session state. */
typedef enum { PE_UI_WINDOW_STORAGE, PE_UI_WINDOW_CHARACTER, PE_UI_WINDOW_INVENTORY,
    PE_UI_WINDOW_REPLICATOR, PE_UI_WINDOW_MISSION, PE_UI_WINDOW_COUNT } PeUIWindow;
typedef struct {
    float x[PE_UI_WINDOW_COUNT],y[PE_UI_WINDOW_COUNT]; /* Reference-pixel offsets. */
    int dragging; /* Window+1, or zero. */
    float grab_x,grab_y;
} PeUIWindows;
PeTrainingUIRect pe_training_ui_window_rect(const PeTrainingUILayout *layout,int window);
/* Regenerate base layout, apply offsets, and clamp whole panels to the screen. */
bool pe_training_ui_windows_layout(float width,float height,PeUIWindows *windows,PeTrainingUILayout *layout);
/* Topmost visible panel under pointer; -1 for none. */
int pe_training_ui_window_at(const PeTrainingUILayout *layout,unsigned visible,float x,float y);
/* Header drag only, excluding close buttons. Active drags own their release
 * frame. Cancel on loss of focus, modal interruption or panel closure. */
bool pe_training_ui_window_drag(PeUIWindows *windows,PeTrainingUILayout *layout,int window,
    float x,float y,bool pressed,bool released,bool cancel);

/* All groups use min(width/1280,height/720), with separate screen anchors.
   Invalid/nonfinite/nonpositive dimensions leave *out unchanged. */
bool pe_training_ui_layout(float width, float height, PeTrainingUILayout *out);
/* Map a rectangle expressed in the measured 1280x720 reference canvas.
   Anchor identifies which reference edge/center stays attached to screen. */
PeTrainingUIRect pe_training_ui_map(const PeTrainingUILayout *layout,
                                  PeTrainingUIRect reference, PeTrainingUIAnchor anchor);
/* Inverse mapping for e.g. sprite vertices; rejects invalid layout/anchor.
   Both outputs remain untouched on failure. */
bool pe_training_ui_unmap(const PeTrainingUILayout *layout, PeTrainingUIAnchor anchor,
                          float screen_x, float screen_y, float *reference_x, float *reference_y);
/* Source minimap Unity UV rectangle; finite deltas clamp to the source's
   refresh-bounded [-62,62]m range, giving a crop fully inside the texture.
   Invalid input leaves output untouched. Unity v points upward. */
bool pe_training_ui_minimap_uv(float player_x, float player_z, float center_x, float center_z,
                               PeTrainingUIRect *uv);
/* Half-open hit bounds: right/bottom edges are excluded. Nonfinite inputs miss. */
bool pe_training_ui_contains(PeTrainingUIRect rect, float x, float y);
/* Returns first matching index, or -1 (including gaps between slots). */
int pe_training_ui_hit(const PeTrainingUIRect *rects, size_t count, float x, float y);
#endif
