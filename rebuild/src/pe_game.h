#ifndef PE_GAME_H
#define PE_GAME_H
#include "pe_world.h"
#include "pe_conversation_history.h"
#include "pe_camp_repair_state.h"
#include "pe_hunt_loot_state.h"
#include "pe_content.h"
#include "pe_block.h"
#include "pe_outfit.h"
#include "pe_passive_recovery.h"
#include "pe_stamina_recovery.h"
#include "pe_revival.h"
#include "pe_sandbox_collision.h"
#define PE_INVENTORY_SLOTS 32 /* Literal v1-v18 wire prefix and Legacy capacity. */
#define PE_INVENTORY_CATEGORIES 4
#define PE_INVENTORY_CATEGORY_SLOTS 420
#define PE_INVENTORY_MAX_SLOTS (PE_INVENTORY_CATEGORIES*PE_INVENTORY_CATEGORY_SLOTS)
#define PE_MAX_BLOCKS 256
#define PE_MAX_TREES 16
#define PE_MAX_B45_CELLS 1024
#define PE_MAX_BUILD_BATCH 64
#define PE_BUILD_HISTORY_CAPACITY 5
/* Bounded source construction undo journal; never part of the PECS payload. */
typedef struct { bool addition; unsigned count; PeB45Cell cells[PE_MAX_BUILD_BATCH]; } PeBuildEdit;
typedef struct { unsigned count,cursor; PeBuildEdit edits[PE_BUILD_HISTORY_CAPACITY]; } PeBuildHistory;
#define PE_PLAYER_RADIUS 0.30f
#define PE_PLAYER_HEIGHT 1.75f
#define PE_DAY_SECONDS (26.0 * 3600.0)
#define PE_FIXED_DT (1.0f / 60.0f)
#define PE_KNOWN_RECIPES 128
#define PE_QUICKSLOTS 9
#define PE_MAX_DEPLOYABLES 64
/* V10 keeps a fixed-size extension so earlier payloads remain literal prefixes. */
#define PE_SAVE_V10_APPEND (20u + 28u * PE_MAX_DEPLOYABLES)
#define PE_SAVE_V11_APPEND 4u
/* V12 changes hunt-state interpretation only; the V11 payload is unchanged. */
#define PE_SAVE_V12_APPEND 0u
#define PE_SAVE_V13_APPEND 4u
#define PE_SAVE_V14_APPEND 36u
/* V15 appends only the secondary reference; bag remains the durability owner. */
#define PE_SAVE_V15_APPEND 4u
/* V16 appends raw Comfort only. Recovery clocks/derived delay stay transient. */
#define PE_SAVE_V16_APPEND 4u
/* V17 retains v16 then stores count8 and eight entity/balance/latch records. */
#define PE_SAVE_V17_APPEND 100u
/* V18 retains v17, then fixture flag/count and32 prototype/XYZ/depletion records. */
#define PE_SAVE_V18_APPEND 648u
/* V19 appends inventory layout/count and1680 full16-byte slot records. */
#define PE_SAVE_V19_APPEND (8u + 16u * PE_INVENTORY_MAX_SLOTS)
/* V20 retains v19 and appends machine identity, exact item and tick clock. */
#define PE_SAVE_V20_APPEND 32u
/* V21 preserves v20, then stores count and20 canonical fixed UTF-8 pairs. */
#define PE_SAVE_V21_APPEND (4u + PE_CONVERSATION_HISTORY_CAPACITY * (PE_CONVERSATION_SPEAKER_CAPACITY + PE_CONVERSATION_CONTENT_CAPACITY))
#define PE_SAVE_V22_APPEND (4u + 32u * PE_HUNT_LOOT_CAPACITY)
#define PE_SAVE_VERSION 22u
#define PE_MAX_PLANTS 32
typedef struct { uint32_t prototype_id; PeVec3 position; bool depleted; } PePlant;
typedef struct { bool installed; unsigned count; PePlant plants[PE_MAX_PLANTS]; } PeVegetation;
typedef struct { int item_id; uint32_t instance_id; float durability; PeVec3 position; unsigned quarter_turn; } PeDeployable;
#define PE_TOWN_V8_STOCK_CAPACITY 17
#define PE_TOWN_STOCK_CAPACITY 284
typedef struct { bool initialized; unsigned remaining; double create_time; } PeTownStock;
#define PE_TOWN_WALLET_CAPACITY 8
typedef struct { uint32_t balance; bool added; } PeTownWallet;
typedef enum { PE_GAME_LEGACY = 0, PE_GAME_ADVENTURE = 1 } PeGameMode;
enum { PE_ADVENTURE_WELCOME_PAGES = 9, PE_ADVENTURE_ARRIVAL = 9031, PE_ADVENTURE_SUPPLIES = 9032 };
typedef struct { int item_id; uint32_t instance_id; } PeQuickslot;
/* Normal/Mine style; MaleModel and FemaleModel MoveParam serialized values. */
#define PE_WALK_SPEED 2.0f
#define PE_RUN_SPEED 5.0f
#define PE_SPRINT_SPEED 8.0f
#define PE_SPRINT_MIN_STAMINA 15.0f
#define PE_SPRINT_STAMINA_PER_SECOND 5.0f
#define PE_JUMP_STAMINA 10.0f
/* Non-stack items have stable IDs; durable equipment preserves wear per instance. */
typedef struct {
    PeSlot slots[PE_INVENTORY_MAX_SLOTS];
    uint32_t next_instance_id;
    bool categorized; /* Adventure: four independent source category lists. */
} PeInventory;
/* Source repurchase lists are session-only. Never serialize these records;
 * each unstackable remains the sole owner of its original instance/wear. */
#define PE_TOWN_BUYBACK_CAPACITY 32
typedef struct { unsigned count; PeSlot slots[PE_TOWN_BUYBACK_CAPACITY]; } PeTownBuyback;
/* Integer minimum corner of a one-meter construction proxy. */
typedef struct { int x,y,z,item_id; } PeBlock;
typedef struct { PeVec3 position; int remaining; float health; } PeTree;
typedef struct {
    int recipe_id, batches_left;
    float elapsed;
    int batches_pending; /* Inputs consumed; outputs owed, not yet in inventory. */
    float delivery_elapsed;
} PeCraftJob;
typedef struct {
    float attack, mining_damage, mining_radius, stamina_cost, durability_cost;
    int tool_kind;
} PeToolStats;
typedef struct { float move_x, move_z; bool jump, sprint, walk, stamina_action; } PeInput;
typedef struct { bool changed,removed,capacity_blocked; } PeResourceEdit;
typedef struct PeGame {
    PeWorld world;
    PeVec3 player, velocity; /* player is feet position in meters */
    bool grounded;
    /* Action state is transient, like held input; loads/imports clear it. */
    bool sprinting, jumping;
    float yaw, pitch;
    double game_seconds;
    PeInventory inventory;
    PeConversationHistory conversation_history; /* Displayed/resolved dialogue; PECSv21. */
    PeHuntLootState hunt_loot; /* Persistent bounded prototype13 world drops; PECSv22. */
    PeCampRepair repair; /* Archived camp machine20006 only; PECSv20. */
    PeOutfit outfit;
    PeCraftJob craft;
    uint32_t equipped_instance_id, rng_state;
    uint32_t shield_instance_id; /* Optional exact bag-owned Wood Shield34; no saved block flag. */
    float stamina, stamina_max, stamina_regen;
    float comfort;
    PeStaminaRecovery stamina_recovery;
    float stamina_recover_delay, stamina_idle, tool_cooldown;
    PeBlock blocks[PE_MAX_BLOCKS];
    int block_count;
    PeB45Cell cells[PE_MAX_B45_CELLS];
    int cell_count;
    PeBuildHistory build_history;
    PeTree trees[PE_MAX_TREES];
    int tree_count;
    PeVegetation vegetation;
    uint32_t gathered, crafted, built;
    char message[160];
    PeGameMode mode;
    int adventure_mission;
    unsigned welcome_page; /* Next source welcome page;9 means Arrival accepted. */
    bool arrival_complete, supplies_claimed;
    unsigned medicine_state; /* 0 unaccepted, 1 active9072, 2 completed. */
    unsigned hunt_state; /* 0 none,1 travel,2 kill,3 return,4/5 rewarded4105/4106,6 reported. */
    unsigned hunt_kills; /* Authoritative prototype13 death credit,0..4; no transient actor HP. */
    PeVec3 hunt_destination; /* Original source/world XYZ, not crop-local. */
    PeDeployable deployed[PE_MAX_DEPLOYABLES];
    unsigned deployed_count;
    bool annot_complete; /* Source discovery4503 / completed mission9032. */
    PeTownStock town_stock[PE_TOWN_STOCK_CAPACITY]; /* Stable exported offer order. */
    PeTownWallet town_wallet[PE_TOWN_WALLET_CAPACITY]; /* Stable source vendor order; one balance owner. */
    PeTownBuyback town_buyback[PE_TOWN_WALLET_CAPACITY]; /* Transient, same stable vendor order. */
    uint32_t money, scanner_mask;
    int known_recipe_count, known_recipes[PE_KNOWN_RECIPES];
    PeQuickslot quickslots[PE_QUICKSLOTS];
    float hp, max_hp;
    PePassiveRecovery passive_recovery; /* Transient source buff phase/hurt stamp, never serialized. */
    PeRevival revival; /* Transient get-up/contact grace; living loads rebuild normal defence. */
    double juice_cooldown, juice_buff_elapsed;
    float medpack_cooldown; /* Source cooldown group9; independent of juice group11. */
    unsigned juice_buff_tick;
    bool juice_buff_active;
    float jump_action_elapsed; /* Transient action state, reset on reload. */
} PeGame;
void pe_game_init(PeGame *game, uint32_t seed);
/* Owning copies require zeroed/initialized destination; destroy before re-init. */
void pe_game_destroy(PeGame *game);
bool pe_game_clone(PeGame *destination,const PeGame *source);
void pe_game_move(PeGame *destination,PeGame *source);
/* Legacy remains the old explicit prototype; Adventure grants the source start. */
bool pe_game_init_mode(PeGame *game, uint32_t seed, PeGameMode mode);
bool pe_game_welcome_advance(PeGame *game,unsigned expected_page);
bool pe_game_arrival_complete(PeGame *game);
bool pe_game_accept_supplies(PeGame *game);
bool pe_game_recipe_known(const PeGame *game, int recipe_id);
bool pe_game_use_slot(PeGame *game, int inventory_slot);
/* Repair transaction adapters require a living Adventure player and the
 * qualified settlement world. Host additionally owns machine model/reach.
 * Reserved items survive clone/move/menu/save; occupied machines block world
 * replacement. No fixed tick advances this independent display-frame clock. */
bool pe_game_repair_state_valid(const PeGame *game);
/* Advisory row filter only: no full bag validation or capacity reservation. */
bool pe_game_repair_candidate(const PeGame *game,int slot);
bool pe_game_repair_can_insert(const PeGame *game,int slot);
bool pe_game_repair_can_start(const PeGame *game);
bool pe_game_repair_insert(PeGame *game,int slot,const PeSlot *expected);
bool pe_game_repair_start(PeGame *game);
bool pe_game_repair_cancel(PeGame *game);
bool pe_game_repair_take(PeGame *game,int *destination);
bool pe_game_repair_frame(PeGame *game,float dt,bool enabled,bool *completed);
/* Confirmed bag deletion destroys the complete exact slot. Split first for a
 * partial stack. Protected source prototypes and worn/equipped owners fail.
 * expected is the confirmation snapshot; any intervening change rejects
 * without mutation. Prototype quickslots retain the existing exhaustion policy. */
bool pe_game_can_discard_slot(const PeGame *game,int inventory_slot);
bool pe_game_discard_slot(PeGame *game,int inventory_slot,const PeSlot *expected);
/* Supported live-player healing916/924/925, including empty/cooldown retry
 * input ownership. Revival937 and unsupported medicine are deliberately false. */
bool pe_game_item_is_healing_consumable(int item_id);
/* Remaining supported source item cooldown; unknown items return0. */
float pe_game_item_cooldown(const PeGame *game,int item_id);
/* inventory_slot=-1 clears a shortcut. Assignment never consumes an item. */
bool pe_game_quickslot_assign(PeGame *game, int quickslot, int inventory_slot);
bool pe_game_quickslot_use(PeGame *game, int quickslot);
/* Replace only world-bound state; preserve player possessions/progression.
 * Reserved repair items or pending hunt drops reject replacement atomically. */
bool pe_game_set_world(PeGame *game, const PeWorld *world, PeVec3 preferred);
void pe_game_step(PeGame *game, PeInput input, float dt);
/* Notify only an admitted stamina debit or active cost-owning action. */
void pe_game_stamina_activity(PeGame *game);
/* Cache stays external to saved state. Refresh after every world/edit/load change. */
bool pe_game_collision_update(const PeGame *game, PeSandboxCollision *collision, char *error, size_t capacity);
/* Spawn/import recovery only, bounded upward 4m/downward 5m; no x/z relocation.
 * Failure preserves the complete gameplay state. Never use to repair a save. */
bool pe_game_collision_settle(PeGame *game, const PeSandboxCollision *collision);
/* Spawn/v4 migration only: escape live trees within 3m laterally, preserving Y
 * and velocity. Never crosses geometry or enters another tree; no mutation on
 * failure or when already clear. Cache must match game including live trees. */
bool pe_game_collision_recover_trees(PeGame *game, const PeSandboxCollision *collision);
bool pe_game_step_collision(PeGame *game, PeInput input, float dt, const PeSandboxCollision *collision);
/* One action-owned horizontal velocity request (m/s), replacing WASD/jump/sprint.
 * Keeps vertical gravity/support and ordinary clocks, with one collision sweep.
 * No motion accumulation: blocked displacement is consumed on this tick.
 * The caller owns attack clocks and any source motor velocity correction.
 * Requires finite XZ speed<=100, Y=0; invalid input leaves game unchanged. */
bool pe_game_step_collision_motion(PeGame *game, PeInput input, float dt,
                                  const PeSandboxCollision *collision, PeVec3 motion);
bool pe_inventory_state_valid(const PeInventory *inventory);
int pe_inventory_first_empty(const PeInventory *inventory, int item_id);
bool pe_inventory_insert(PeInventory *inventory, PeSlot slot, int *destination);
int pe_inventory_capacity(const PeInventory *inventory);
int pe_inventory_category_start(const PeInventory *inventory, int category);
int pe_inventory_category_capacity(const PeInventory *inventory, int category);
bool pe_inventory_slot_valid(const PeInventory *inventory, int index);
/* Source Quest Item category and creation-ID quotient9 cannot be discarded.
 * Unknown source prototypes fail closed, independently of catalog coverage. */
bool pe_inventory_item_discardable(int item_id);
/* Stable migration: category-local ordering, exact identities and wear survive. */
bool pe_inventory_enable_categories(PeInventory *inventory);
/* Split to the first empty same-category cell; failure leaves both outputs intact. */
bool pe_inventory_split(PeInventory *inventory, int from, int count, int *destination);
/* Source bag drag: move or exchange within one category, including equal items. */
bool pe_inventory_move(PeInventory *inventory, int from, int to);
/* Explicit convenience merge, also performed by source Sort/Reduce. */
bool pe_inventory_merge(PeInventory *inventory, int from, int to);
/* Source Reduce then ascending sortLabel/prototype/count, stable equal ties. */
bool pe_inventory_sort(PeInventory *inventory, int category);
int pe_inventory_count(const PeInventory *inventory, int item_id);
bool pe_inventory_add(PeInventory *inventory, int item_id, int count);
bool pe_inventory_remove(PeInventory *inventory, int item_id, int count);
bool pe_game_start_craft(PeGame *game, int recipe_id, int batches);
bool pe_game_cancel_craft(PeGame *game);
bool pe_game_equip_slot(PeGame *game, int slot_index);
void pe_game_unequip(PeGame *game);
const PeSlot *pe_game_equipped_tool(const PeGame *game);
bool pe_game_tool_stats(const PeGame *game, PeToolStats *stats);
bool pe_game_mine(PeGame *game, PeVec3 origin, PeVec3 direction);
bool pe_game_gather(PeGame *game);
/* Atomic resource transactions for an external authored-event action host.
 * No action clocks, stamina or durability charge; caller owns those exactly
 * once per successful event. Empty samples succeed unchanged; capacity blocks
 * succeed with capacity_blocked and preserve all gameplay state. No reach test
 * here: the source action locks its first target before the later event. */
/* Immutable settlement obstruction only; target contact tolerates .005 m.
 * Ordinary worlds are clear; missing required settlement assets fail closed. */
bool pe_game_static_segment_clear(const PeGame *game,PeVec3 start,PeVec3 end);
bool pe_game_dig_sample(PeGame *game,int x,int y,int z,PeResourceEdit *edit);
bool pe_game_cut_tree(PeGame *game,int tree_index,PeResourceEdit *edit);
bool pe_game_place(PeGame *game, PeVec3 origin, PeVec3 direction, int item_id);
bool pe_game_remove_block(PeGame *game, PeVec3 origin, PeVec3 direction);
bool pe_game_build_cells(PeGame *game, const PeB45Cell *cells, int count);
/* Pure placement validation: geometry, reach, occupancy and complete inventory
 * cost use the same checks as build_cells, without changing state or messages. */
bool pe_game_can_build_cells(const PeGame *game, const PeB45Cell *cells, int count);
bool pe_game_remove_cells(PeGame *game, const int *indices, int count);
/* Read-only candidate for voxel Dirt/Stone/Wood cube/ramp brushes (1 or 4).
 * Returns a target even if blocked or unaffordable; use can_build_cells for
 * validity. Furniture is unsupported. On failure outputs are unchanged; on
 * success only out[0..*count) is written. Output storage must not alias game. */
bool pe_game_preview_shape(const PeGame *game, PeVec3 origin, PeVec3 direction,
                          int item_id, int shape, int rotation, int brush_count,
                          PeB45Cell out[4], size_t *count);
bool pe_game_place_shape(PeGame *game, PeVec3 origin, PeVec3 direction,
                         int item_id, int shape, int rotation, int brush_count);
bool pe_game_remove_shape(PeGame *game, PeVec3 origin, PeVec3 direction, int brush_count);
bool pe_game_save(const PeGame *game, const char *path, char *error, size_t capacity);
bool pe_game_load(PeGame *game, const char *path, char *error, size_t capacity);
#endif
