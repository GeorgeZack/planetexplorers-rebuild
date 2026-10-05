#ifndef PE_SANDBOX_UI_H
#define PE_SANDBOX_UI_H
#include "pe_game.h"
#include "pe_adventure_dialogue.h"
#include "pe_town_interaction.h"
#include "pe_ui_assets.h"
#include "pe_training_ui_layout.h"
#include "pe_training_ui_shader.h"

/* Presentation only. All possessions, equipment, recipes and quickslots live
 * in PeGame. The route API is deterministic and does not call window/GPU APIs. */
typedef struct {
    float mouse_x, mouse_y, wheel;
    bool focused, left_pressed, left_released, right_pressed;
    bool inventory, character, missions, replicator, escape, tab;
    bool save, load, backspace, advance;
    int quickslot; /* -1 means no key; otherwise 0..8. */
    const char *typed; /* Optional UTF-8 input; search currently accepts ASCII. */
} PeSandboxUIInput;
typedef struct {
    bool consume_world, cursor_visible, paused;
    bool npc_interaction; /* Hover/click allowed even with the free cursor. */
    bool toggle_build, save, load, return_main;
    bool rotate_build;
    int build_shape,build_brush; /* Zero means no selection request. */
    int selected_item; /* Explicit resource quickslot selection, otherwise0. */
} PeSandboxUIResult;
typedef struct { uint32_t entity_id; PeVec3 position; } PeSandboxMapNpc;
typedef void (*PeSandboxUIPreview)(void *context,const PeGame *game);
typedef struct {
    PeUiAssets *assets;
    PeTrainingUiShader *effects;
    PeTrainingUILayout layout;
    PeUIWindows windows;
    RenderTexture2D preview;
    Texture2D minimap;
    Texture2D welcome_portrait;
    PeVec3 welcome_position;
    bool welcome_present;
    Texture2D supplier_portrait; /* Borrowed from the NPC view. */
    Texture2D town_portrait,annot_portrait;
    uint32_t town_entity;
    bool town_open,shop_open,shop_dragging;
    char town_name[129],start_town_name[129];
    int shop_selection;
    unsigned shop_quantity;
    float shop_offset_x,shop_offset_y,shop_grab_x,shop_grab_y;
    PeAdventureDialogue dialogue;
    uint32_t supplier_entity;
    PeVec3 supplier_position;
    bool supplier_present;
    bool loaded, inventory_open, character_open, missions_open, replicator_open;
    bool paused, menu_open, free_cursor, search_focus;
    bool discard_input; /* Host-cancelled modal: consume next route frame completely. */
    bool building;
    int build_item,build_shape,build_rotation,build_brush;
    int inventory_tab, inventory_page, recipe_scroll, recipe_id, batches;
    int selected_slot, drag_item, drag_slot, drag_quickslot;
    uint32_t drag_instance;
    float drag_x, drag_y, mouse_x, mouse_y;
    bool dragging;
    char search[64];
    double seconds, message_until;
    char last_message[160];
    uint32_t map_revision, map_seed, map_source;
    int32_t map_origin_x, map_origin_y, map_origin_z;
    int map_world_height;
    bool map_valid;
    const PeSandboxMapNpc *resident_markers; /* Borrowed until replaced/cleared. */
    size_t resident_marker_count;
    PeSandboxUIPreview draw_preview;
    void *preview_context;
} PeSandboxUI;

void pe_sandbox_ui_init(PeSandboxUI *ui,float width,float height);
/* Requires active GL context. asset_root contains ui/atlas.peui and effects/.
 * No optional tutorial state, room map, fabricated vitals or player clone. */
bool pe_sandbox_ui_load(PeSandboxUI *ui,const char *asset_root,char *error,size_t capacity);
void pe_sandbox_ui_unload(PeSandboxUI *ui);
void pe_sandbox_ui_resize(PeSandboxUI *ui,float width,float height);
/* Host supplies only actually present, supported residents. Not saved; storage
 * must remain valid through drawing. NULL/count0 clears after load/teardown. */
bool pe_sandbox_ui_set_resident_markers(PeSandboxUI *ui,const PeSandboxMapNpc *markers,size_t count);
/* Borrowed callback: UI begins/clears preview target; callback owns its 3D
 * camera/BeginMode3D and must return with 3D mode ended. No callback means no
 * character model is claimed. State remains owned by the game/actor host. */
void pe_sandbox_ui_set_preview(PeSandboxUI *ui,PeSandboxUIPreview callback,void *context);
PeSandboxUIResult pe_sandbox_ui_route(PeSandboxUI *ui,PeGame *game,PeSandboxUIInput input);
/* Reads one frame of raylib input, routes it, advances UI notice/effect time.
 * Host MUST apply consume_world before movement, look, attack or placement. */
PeSandboxUIResult pe_sandbox_ui_update(PeSandboxUI *ui,PeGame *game,float dt);
/* Call before BeginDrawing if character preview is requested. */
void pe_sandbox_ui_prepare(PeSandboxUI *ui,const PeGame *game);
/* Call inside BeginDrawing after 3D mode. Does not advance simulation. */
void pe_sandbox_ui_draw(PeSandboxUI *ui,const PeGame *game,bool build_mode);
bool pe_sandbox_ui_cursor(const PeSandboxUI *ui);
int pe_sandbox_ui_inventory_slot(const PeSandboxUI *ui,const PeGame *game,int visible);
int pe_sandbox_ui_inventory_count(const PeSandboxUI *ui,const PeGame *game);
int pe_sandbox_ui_recipe_at(const PeSandboxUI *ui,const PeGame *game,int row);
int pe_sandbox_ui_recipe_count(const PeSandboxUI *ui,const PeGame *game);
/* Shared hit geometry for renderer and routed actions. */
PeTrainingUIRect pe_sandbox_ui_recipe_row(const PeSandboxUI *ui,int row);
PeTrainingUIRect pe_sandbox_ui_quantity(const PeSandboxUI *ui,int button); /* minus,plus,min,max */
PeTrainingUIRect pe_sandbox_ui_pause_button(const PeSandboxUI *ui,int button); /* resume/save/load/main */
PeTrainingUIRect pe_sandbox_ui_cancel_button(const PeSandboxUI *ui);
PeTrainingUIRect pe_sandbox_ui_search(const PeSandboxUI *ui);
/* Host mirrors building/selection fields before routing. Build canvas retains
 * world input even if the normal mode free_cursor flag was set; toolbar and
 * modal UI consume it. Requests never mutate ordinary quickslot bindings. */
PeTrainingUIRect pe_sandbox_ui_build_toolbar(const PeSandboxUI *ui);
PeTrainingUIRect pe_sandbox_ui_build_slot(const PeSandboxUI *ui,int index);
PeTrainingUIRect pe_sandbox_ui_build_tool(const PeSandboxUI *ui,int index);
PeTrainingUIRect pe_sandbox_ui_supplier_menu(const PeSandboxUI *ui);
PeTrainingUIRect pe_sandbox_ui_supplier_choice(const PeSandboxUI *ui,int index);
PeTrainingUIRect pe_sandbox_ui_supplier_close(const PeSandboxUI *ui);
PeTrainingUIRect pe_sandbox_ui_shop_panel(const PeSandboxUI *ui);
PeTrainingUIRect pe_sandbox_ui_shop_row(const PeSandboxUI *ui,int row);
PeTrainingUIRect pe_sandbox_ui_shop_button(const PeSandboxUI *ui,int button); /* minus,plus,buy,back,close */
void pe_sandbox_ui_cancel_town(PeSandboxUI *ui);
bool pe_sandbox_ui_open_town(PeSandboxUI *ui,const PeGame *game,uint32_t entity_id,const char *name,Texture2D portrait,bool eligible);
bool pe_sandbox_ui_open_annot(PeSandboxUI *ui,const PeGame *game,uint32_t entity_id,Texture2D portrait,bool eligible);
bool pe_sandbox_ui_open_welcome(PeSandboxUI *ui,const PeGame *game,uint32_t entity_id,bool eligible);
bool pe_sandbox_ui_open_supplier(PeSandboxUI *ui,const PeGame *game,uint32_t entity_id,bool eligible);
#endif
