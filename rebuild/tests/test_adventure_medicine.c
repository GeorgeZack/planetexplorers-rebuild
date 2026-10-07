#include "adventure_test_helpers.h"
#include "pe_adventure_medicine.h"
#include <math.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"medicine:%d %s\n",__LINE__,#c);failures++;}}while(0)
static int slot(const PeGame *g,int id){for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==id&&g->inventory.slots[i].count)return i;return -1;}
static void empty(PeGame *g){memset(g->inventory.slots,0,sizeof g->inventory.slots);}
int pe_test_adventure_medicine(void){
    PeGame *g=calloc(1,sizeof *g);if(!g)return 1;
    CHECK(!pe_adventure_medicine_state_valid(NULL));CHECK(!pe_adventure_medicine_ready(NULL));
    CHECK(!pe_adventure_medicine_accept(NULL));CHECK(!pe_adventure_medicine_complete(NULL));
    CHECK(pe_game_init_mode(g,42,PE_GAME_ADVENTURE));CHECK(pe_adventure_medicine_state_valid(g));
    PeInventory initial=g->inventory;CHECK(!pe_adventure_medicine_accept(g));CHECK(!memcmp(&initial,&g->inventory,sizeof initial));
    CHECK(pe_test_ack_welcome(g));CHECK(pe_game_arrival_complete(g));CHECK(pe_game_accept_supplies(g));
    CHECK(!pe_adventure_medicine_accept(g));g->annot_complete=true;
    g->hp=0;CHECK(!pe_adventure_medicine_accept(g));g->hp=NAN;CHECK(!pe_adventure_medicine_accept(g));g->hp=g->max_hp;
    empty(g);CHECK(pe_inventory_add(&g->inventory,877,PE_INVENTORY_CATEGORY_SLOTS));initial=g->inventory;
    CHECK(!pe_adventure_medicine_accept(g));CHECK(g->medicine_state==PE_MEDICINE_NONE&&!memcmp(&initial,&g->inventory,sizeof initial));
    empty(g);CHECK(pe_adventure_medicine_accept(g));CHECK(g->medicine_state==PE_MEDICINE_ACTIVE);
    CHECK(pe_inventory_count(&g->inventory,877)==1);CHECK(!pe_game_recipe_known(g,337));
    initial=g->inventory;CHECK(!pe_adventure_medicine_accept(g));CHECK(!memcmp(&initial,&g->inventory,sizeof initial));
    CHECK(!pe_adventure_medicine_ready(g));CHECK(!pe_adventure_medicine_complete(g));
    CHECK(pe_game_use_slot(g,slot(g,877)));CHECK(pe_inventory_count(&g->inventory,877)==0);
    CHECK(pe_game_recipe_known(g,337)&&pe_game_recipe_known(g,388)&&pe_game_recipe_known(g,390));
    const PeRecipeDef *recipe=pe_recipe_def(337);CHECK(recipe&&recipe->output_id==917&&recipe->output_count==1);
    if(recipe){
        for(int i=0;i<recipe->ingredient_count;i++)CHECK(pe_inventory_add(&g->inventory,recipe->ingredients[i].item_id,recipe->ingredients[i].count*5));
        CHECK(pe_game_start_craft(g,337,5));
        for(int i=0;i<600;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
        CHECK(!g->craft.recipe_id&&pe_inventory_count(&g->inventory,917)==5);
    }
    CHECK(pe_adventure_medicine_ready(g));CHECK(pe_inventory_remove(&g->inventory,917,1));
    CHECK(!pe_adventure_medicine_ready(g));initial=g->inventory;CHECK(!pe_adventure_medicine_complete(g));CHECK(!memcmp(&initial,&g->inventory,sizeof initial));
    CHECK(pe_inventory_add(&g->inventory,917,1));
    empty(g);CHECK(pe_inventory_add(&g->inventory,917,5));CHECK(pe_inventory_add(&g->inventory,877,PE_INVENTORY_CATEGORY_SLOTS-1));
    initial=g->inventory;CHECK(pe_adventure_medicine_ready(g));CHECK(!pe_adventure_medicine_complete(g));
    CHECK(g->medicine_state==PE_MEDICINE_ACTIVE&&!memcmp(&initial,&g->inventory,sizeof initial));
    /* Removing five drinks creates one slot. The first reward fits; failure of
     * the second must roll back the removal, first reward and instance IDs. */
    CHECK(pe_inventory_remove(&g->inventory,877,1));uint32_t money=g->money;uint32_t crafted=g->crafted;
    CHECK(pe_adventure_medicine_complete(g));CHECK(g->medicine_state==PE_MEDICINE_COMPLETE);
    CHECK(pe_inventory_count(&g->inventory,917)==0&&pe_inventory_count(&g->inventory,925)==10&&pe_inventory_count(&g->inventory,554)==1);
    CHECK(g->money==money&&g->crafted==crafted&&g->adventure_mission==PE_ADVENTURE_SUPPLIES);
    CHECK(!pe_game_recipe_known(g,149));CHECK(pe_game_use_slot(g,slot(g,554)));CHECK(pe_game_recipe_known(g,149));
    initial=g->inventory;CHECK(!pe_adventure_medicine_complete(g));CHECK(!pe_adventure_medicine_accept(g));CHECK(!memcmp(&initial,&g->inventory,sizeof initial));
    CHECK(pe_adventure_medicine_state_valid(g));g->annot_complete=false;CHECK(!pe_adventure_medicine_state_valid(g));g->annot_complete=true;
    g->medicine_state=3;CHECK(!pe_adventure_medicine_state_valid(g));g->medicine_state=PE_MEDICINE_ACTIVE;
    g->mode=PE_GAME_LEGACY;CHECK(!pe_adventure_medicine_state_valid(g));g->mode=PE_GAME_ADVENTURE;
    g->welcome_page=8;CHECK(!pe_adventure_medicine_state_valid(g));g->welcome_page=9;
    g->supplies_claimed=false;CHECK(!pe_adventure_medicine_state_valid(g));g->supplies_claimed=true;
    g->arrival_complete=false;CHECK(!pe_adventure_medicine_state_valid(g));g->arrival_complete=true;
    g->adventure_mission=PE_ADVENTURE_ARRIVAL;CHECK(!pe_adventure_medicine_state_valid(g));
    pe_game_destroy(g);free(g);return failures;
}
