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
    float frame_dt; /* Display-frame elapsed time for transient modal operations. */
    bool focused, left_pressed, left_released, right_pressed;
    bool inventory, character, missions, replicator, escape, tab;
    bool save, load, backspace, advance;
    bool page_up,page_down,merge_stacks;
    bool undo_build,redo_build; /* Source standalone Ctrl+fresh Z / X. */
    bool build_history_chord,build_history_keys_down; /* Retain chord ownership through release. */
    int quickslot; /* -1 means no key; otherwise 0..8. */
    unsigned response; /* Visible conversation response1..9; zero means no key. */
    const char *typed; /* Optional UTF-8 input; search currently accepts ASCII. */
} PeSandboxUIInput;
typedef struct {
    bool consume_world, cursor_visible, paused;
    bool consume_actions; /* Nonmodal HUD/use owns action presses, not movement/camera/simulation. */
    bool npc_interaction; /* Hover/click allowed even with the free cursor. */
    bool camera_cycle_allowed; /* UI admission before the Tab-only world block; host still gates held buttons/focus. */
    bool toggle_build, save, load, return_main, revive;
    bool rotate_build;
    bool undo_build,redo_build;
    int build_shape,build_brush; /* Zero means no selection request. */
    int selected_item; /* Explicit resource quickslot selection, otherwise0. */
} PeSandboxUIResult;
typedef struct { uint32_t entity_id; PeVec3 position; } PeSandboxMapNpc;
typedef enum {PE_SANDBOX_SHOP_BUY,PE_SANDBOX_SHOP_SELL,PE_SANDBOX_SHOP_BUYBACK} PeSandboxShopMode;
typedef struct {
    PeSlot item;
    int bag_slot,store_id;
    uint32_t unit_price;
    bool eligible;
    const char *reason;
} PeSandboxShopRow;
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
    uint32_t town_entity,deploy_entity;
    bool town_open,shop_open,shop_dragging;
    bool deploy_get_pending;
    uint32_t deploy_get_ticks; /* UTimer: 100,000 ticks/s, commit strictly >200,000. */
    char town_name[129],start_town_name[129];
    int shop_selection,shop_scroll;
    PeSandboxShopMode shop_mode;
    unsigned shop_quantity;
    float shop_offset_x,shop_offset_y,shop_grab_x,shop_grab_y;
    PeAdventureDialogue dialogue;
    PeAdventureDialogueResult dialogue_result; /* Last modal action, for explicit failure feedback. */
    uint32_t supplier_entity;
    PeVec3 supplier_position;
    bool supplier_present;
    bool loaded, inventory_open, character_open, missions_open, replicator_open;
    bool paused, menu_open, free_cursor, search_focus;
    bool revival_open; /* Host/game-derived death/get-up modal, never saved. */
    bool camera_cursor; /* Free-mouse camera pointer; not a modal or Tab override. */
    bool discard_input; /* Host-cancelled modal: consume next route frame completely. */
    bool starter_pair_available; /* Host-qualified body/weapon/block/contact group; never saved. */
    bool hunt_encounter_available; /* Transient host readiness, reset/recompute after load; no saved progression. */
    bool building;
    bool build_history_keys_owned;
    int build_item,build_shape,build_rotation,build_brush;
    int inventory_tab, inventory_page, recipe_scroll, recipe_id, batches;
    int selected_slot, drag_item, drag_slot, drag_quickslot;
    uint32_t drag_instance;
    PeSlot drag_source,split_source;
    bool split_open,split_armed,split_edit_started;
    int split_slot,split_count;
    bool delete_armed,delete_open;
    int delete_slot;
    PeSlot delete_source; /* Exact confirmation snapshot; transient, never saved. */
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

/* A UI-owned press remains owned until every mouse button is released. The
 * release frame is consumed too; cursor recapture may start on the next frame.
 * Kept outside PeGame so no held desktop input crosses a save/load boundary. */
typedef struct { bool waiting_for_release,actions_only; } PeSandboxPointerGate;
bool pe_sandbox_pointer_release_gate(PeSandboxPointerGate *gate,bool ui_owned,unsigned buttons_down);
/* Preserve action-only or full-modal press ownership through release. A real
 * modal escalates ownership; an action-only press never freezes simulation. */
bool pe_sandbox_pointer_route(PeSandboxPointerGate *gate,PeSandboxUIResult *result,unsigned buttons_down);

void pe_sandbox_ui_init(PeSandboxUI *ui,float width,float height);
/* Requires active GL context. asset_root contains ui/atlas.peui and effects/.
 * No optional tutorial state, room map, fabricated vitals or player clone. */
bool pe_sandbox_ui_load(PeSandboxUI *ui,const char *asset_root,char *error,size_t capacity);
void pe_sandbox_ui_unload(PeSandboxUI *ui);
void pe_sandbox_ui_resize(PeSandboxUI *ui,float width,float height);
/* Headless quest presentation shared by HUD/mission window. Hunt availability
 * is host-supplied; return/report uses the supported source9127 flow. */
int pe_sandbox_ui_hunt_mission_id(const PeGame *game);
const char *pe_sandbox_ui_mission_title(const PeGame *game);
bool pe_sandbox_ui_hunt_goal(const PeSandboxUI *ui,const PeGame *game,char *out,size_t capacity);
/* Host supplies only actually present, supported residents. Not saved; storage
 * must remain valid through drawing. NULL/count0 clears after load/teardown. */
bool pe_sandbox_ui_set_resident_markers(PeSandboxUI *ui,const PeSandboxMapNpc *markers,size_t count);
/* Borrowed callback: UI begins/clears preview target; callback owns its 3D
 * camera/BeginMode3D and must return with 3D mode ended. No callback means no
 * character model is claimed. State remains owned by the game/actor host. */
void pe_sandbox_ui_set_preview(PeSandboxUI *ui,PeSandboxUIPreview callback,void *context);
PeSandboxUIResult pe_sandbox_ui_route(PeSandboxUI *ui,PeGame *game,PeSandboxUIInput input);
/* Reads one frame of raylib input, routes it, advances UI notice/effect time.
 * Host MUST apply consume_world before movement/look/simulation and
 * consume_actions before new action presses; action-only use keeps clocks live. */
PeSandboxUIResult pe_sandbox_ui_update(PeSandboxUI *ui,PeGame *game,float dt);
/* Call before BeginDrawing if character preview is requested. */
void pe_sandbox_ui_prepare(PeSandboxUI *ui,const PeGame *game);
/* Call inside BeginDrawing after 3D mode. Does not advance simulation. */
void pe_sandbox_ui_draw(PeSandboxUI *ui,const PeGame *game,bool build_mode);
bool pe_sandbox_ui_cursor(const PeSandboxUI *ui);
int pe_sandbox_ui_inventory_slot(const PeSandboxUI *ui,const PeGame *game,int visible);
int pe_sandbox_ui_inventory_count(const PeSandboxUI *ui,const PeGame *game);
int pe_sandbox_ui_inventory_pages(const PeSandboxUI *ui,const PeGame *game);
/* Fixed bag cell, including empty cells; equipped ownership is not a drop target. */
int pe_sandbox_ui_inventory_cell(const PeSandboxUI *ui,const PeGame *game,int visible);
PeTrainingUIRect pe_sandbox_ui_inventory_action(const PeSandboxUI *ui,int action); /* split,sort,delete */
PeTrainingUIRect pe_sandbox_ui_delete_panel(const PeSandboxUI *ui);
PeTrainingUIRect pe_sandbox_ui_delete_button(const PeSandboxUI *ui,int button); /* confirm,cancel */
PeTrainingUIRect pe_sandbox_ui_split_panel(const PeSandboxUI *ui);
PeTrainingUIRect pe_sandbox_ui_split_button(const PeSandboxUI *ui,int button); /* minus,plus,confirm,cancel */
int pe_sandbox_ui_recipe_at(const PeSandboxUI *ui,const PeGame *game,int row);
int pe_sandbox_ui_recipe_count(const PeSandboxUI *ui,const PeGame *game);
/* Shared hit geometry for renderer and routed actions. */
PeTrainingUIRect pe_sandbox_ui_recipe_row(const PeSandboxUI *ui,int row);
PeTrainingUIRect pe_sandbox_ui_quantity(const PeSandboxUI *ui,int button); /* minus,plus,min,max */
PeTrainingUIRect pe_sandbox_ui_pause_button(const PeSandboxUI *ui,int button); /* resume/save/load/main */
PeTrainingUIRect pe_sandbox_ui_revival_button(const PeSandboxUI *ui,int button); /* revive/load */
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
/* Compact visible CHOICE order; -1 when that row is absent. Shared by draw/input. */
int pe_sandbox_ui_dialogue_choice_index(const PeSandboxUI *ui,const PeGame *game,int row);
PeTrainingUIRect pe_sandbox_ui_supplier_close(const PeSandboxUI *ui);
PeTrainingUIRect pe_sandbox_ui_shop_panel(const PeSandboxUI *ui);
PeTrainingUIRect pe_sandbox_ui_shop_row(const PeSandboxUI *ui,int row);
PeTrainingUIRect pe_sandbox_ui_shop_tab(const PeSandboxUI *ui,PeSandboxShopMode mode);
int pe_sandbox_ui_shop_count(const PeSandboxUI *ui,const PeGame *game);
bool pe_sandbox_ui_shop_item(const PeSandboxUI *ui,const PeGame *game,int index,PeSandboxShopRow *out);
bool pe_sandbox_ui_shop_can_trade(const PeSandboxUI *ui,const PeGame *game);
PeTrainingUIRect pe_sandbox_ui_shop_button(const PeSandboxUI *ui,int button); /* minus,plus,buy,back,close */
void pe_sandbox_ui_cancel_town(PeSandboxUI *ui);
bool pe_sandbox_ui_open_deployable(PeSandboxUI *ui,const PeGame *game,uint32_t instance_id);
bool pe_sandbox_ui_open_town(PeSandboxUI *ui,const PeGame *game,uint32_t entity_id,const char *name,Texture2D portrait,bool eligible);
bool pe_sandbox_ui_open_annot(PeSandboxUI *ui,const PeGame *game,uint32_t entity_id,Texture2D portrait,bool eligible);
bool pe_sandbox_ui_open_welcome(PeSandboxUI *ui,const PeGame *game,uint32_t entity_id,bool eligible);
bool pe_sandbox_ui_open_supplier(PeSandboxUI *ui,const PeGame *game,uint32_t entity_id,bool eligible);
#endif
