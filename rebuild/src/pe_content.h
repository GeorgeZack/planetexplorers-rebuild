#ifndef PE_CONTENT_H
#define PE_CONTENT_H
#include <stddef.h>
#include <stdbool.h>
#define PE_RECIPE_INPUTS 6
#define PE_ITEM_LEARNS 16
typedef enum { PE_LEARN_NONE, PE_LEARN_RECIPE, PE_LEARN_SCAN } PeLearnKind;
#define PE_ITEM_WOOD 279
#define PE_ITEM_STONE 280
#define PE_ITEM_DIRT 268
#define PE_ITEM_FENCE 308
#define PE_ITEM_LADDER 310
#define PE_ITEM_MEDPACK 925
#define PE_ITEM_BANDAGE 924
/* Qualified instant HP healing only, sharing source cooldown group9. */
typedef struct { int item_id; float heal,cooldown_seconds; } PeInstantHealDef;
extern const PeInstantHealDef pe_instant_heals[];
extern const size_t pe_instant_heal_count;
const PeInstantHealDef *pe_instant_heal_def(int item_id);
extern const float pe_medpack_heal,pe_medpack_cooldown_seconds;
typedef enum { PE_TOOL_NONE, PE_TOOL_PICKAXE, PE_TOOL_SHOVEL, PE_TOOL_AXE } PeToolKind;
typedef struct {
    int id;
    const char *name;
    int stack_limit;
    int item_type, durability_max;
    float attack, resource_damage, resource_range;
    int skill_id;
    float durability_cost, stamina_cost;
    PeToolKind tool_kind;
    float cut_damage, cut_bonus;
    int tab, equip_position;
    const char *icon, *icon_overlay1, *icon_overlay2;
    PeLearnKind learn_kind;
    int learn_count, learn_ids[PE_ITEM_LEARNS];
} PeItemDef;
typedef struct {
    float stamina_max, stamina_recover_speed, stamina_recover_delay;
    float stamina_reduce_percent, resource_range, resource_damage, attack;
    float cut_damage, cut_bonus;
    float hp, hp_max, defense;
} PePlayerDefaults;
extern const PePlayerDefaults pe_player_defaults;
/* Legacy byte material IDs. Unmapped entries are -1, not resource IDs. */
extern const int pe_terrain_material_items[256];
extern const float pe_terrain_material_durability[256];
float pe_terrain_material_duration(int material);
int pe_terrain_material_item(int material);
#define PE_RESOURCE_DROPS 2
typedef struct { int item_id; float threshold; } PeResourceDrop;
typedef struct {
    float fixed_count, self_count;
    int drop_count;
    PeResourceDrop drops[PE_RESOURCE_DROPS];
} PeTerrainResourceDef;
extern const PeTerrainResourceDef pe_terrain_resources[256];
const PeTerrainResourceDef *pe_terrain_resource(int material);
typedef struct { int item_id, count; } PeIngredient;
extern const PeIngredient pe_adventure_starter[], pe_adventure_supplies[];
extern const size_t pe_adventure_starter_count, pe_adventure_supplies_count;
typedef struct {
    int id, output_id, output_count;
    float seconds;
    int workspace, ingredient_count;
    PeIngredient ingredients[PE_RECIPE_INPUTS];
} PeRecipeDef;
extern const PeItemDef pe_items[];
extern const size_t pe_item_count;
extern const PeRecipeDef pe_recipes[];
extern const size_t pe_recipe_count;
/* Actual source component membership; positive prototype default alone is
 * not evidence of durability. Unknown/out-of-catalog IDs return false. */
bool pe_item_has_durability(int id);
const PeItemDef *pe_item_def(int id);
const PeRecipeDef *pe_recipe_def(int id);
#endif
