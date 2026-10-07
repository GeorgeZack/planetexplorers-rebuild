#include "pe_game.h"
#include "pe_deployable.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"followup catalog:%d %s\n",__LINE__,#x);failures++;}}while(0)
static int slot(const PeGame*g,int id){for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==id&&g->inventory.slots[i].count)return i;return -1;}
static void clear_bag(PeGame*g){uint32_t next=g->inventory.next_instance_id;g->inventory=(PeInventory){.next_instance_id=next,.categorized=true};g->equipped_instance_id=g->shield_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);}
int pe_test_followup_catalog(void){
 failures=0;PeGame*g=calloc(1,sizeof *g);CHECK(g);if(!g)return failures;
 CHECK(pe_game_init_mode(g,415,PE_GAME_ADVENTURE));CHECK(pe_item_count==341&&pe_recipe_count==108&&pe_recipe_count<PE_KNOWN_RECIPES);
 static const int scripts[]={465,473,478,481,490,657,660,662,696};
 static const int formulas[]={43,46,48,60,68,73,76,85,252,255,257,291,295};
 for(unsigned i=0;i<sizeof scripts/sizeof *scripts;i++)CHECK(pe_inventory_count(&g->inventory,scripts[i])==0);
 CHECK(g->adventure_mission==0&&!g->known_recipe_count);clear_bag(g);
 for(unsigned i=0;i<sizeof scripts/sizeof *scripts;i++){
  int id=scripts[i];const PeItemDef*d=pe_item_def(id);CHECK(d&&d->learn_kind==PE_LEARN_RECIPE&&d->learn_count>0&&d->learn_count<=PE_ITEM_LEARNS);
  CHECK(pe_inventory_add(&g->inventory,id,1));int found=slot(g,id);CHECK(found>=0&&pe_game_use_slot(g,found));CHECK(pe_inventory_count(&g->inventory,id)==0);
  for(int k=0;k<d->learn_count;k++)CHECK(pe_game_recipe_known(g,d->learn_ids[k]));
  int learned=g->known_recipe_count;CHECK(pe_inventory_add(&g->inventory,id,1));found=slot(g,id);PeInventory bag=g->inventory;
  CHECK(!pe_game_use_slot(g,found)&&g->known_recipe_count==learned&&!memcmp(&bag,&g->inventory,sizeof bag));
 }
 CHECK(g->known_recipe_count<=PE_KNOWN_RECIPES&&g->adventure_mission==0);
 for(unsigned i=0;i<sizeof formulas/sizeof *formulas;i++){
  const PeRecipeDef*r=pe_recipe_def(formulas[i]);CHECK(r&&r->workspace==0&&r->seconds>0&&pe_game_recipe_known(g,r->id));clear_bag(g);
  /* Learning alone never grants the output or its materials. */
  CHECK(!pe_game_start_craft(g,r->id,1)&&!g->craft.recipe_id);
  for(int k=0;k<r->ingredient_count;k++)CHECK(pe_inventory_add(&g->inventory,r->ingredients[k].item_id,r->ingredients[k].count));
  unsigned crafted=g->crafted;CHECK(pe_game_start_craft(g,r->id,1));
  pe_game_step(g,(PeInput){0},fminf(.1f,r->seconds*.25f));CHECK(g->craft.recipe_id==r->id&&pe_inventory_count(&g->inventory,r->output_id)==0);
  unsigned ticks=0;for(;g->craft.recipe_id&&ticks<1000;ticks++)pe_game_step(g,(PeInput){0},.1f);
  CHECK(!g->craft.recipe_id&&ticks<1000&&pe_inventory_count(&g->inventory,r->output_id)==r->output_count&&g->crafted==crafted+1);
  for(int k=0;k<r->ingredient_count;k++)CHECK(pe_inventory_count(&g->inventory,r->ingredients[k].item_id)==0);
  CHECK(pe_inventory_state_valid(&g->inventory)&&g->adventure_mission==0);
 }
 /* Colony definitions are inventory/crafting data, not enabled deployment. */
 PeSandboxCollision*c=pe_sandbox_collision_create();char error[128];CHECK(c&&pe_game_collision_update(g,c,error,sizeof error));
 const int colony[]={1127,1128,1131};for(unsigned i=0;i<3;i++){
  clear_bag(g);CHECK(pe_inventory_add(&g->inventory,colony[i],1));int found=slot(g,colony[i]);CHECK(found>=0);PeInventory bag=g->inventory;
  PeVec3 eye={g->player.x,g->player.y+1,g->player.z};CHECK(!pe_game_use_slot(g,found));
  CHECK(!pe_game_place(g,eye,(PeVec3){0,-1,0},colony[i]));CHECK(!pe_deployable_place(g,c,g->inventory.slots[found].instance_id,eye,(PeVec3){0,-1,0}));
  CHECK(!memcmp(&bag,&g->inventory,sizeof bag)&&!g->deployed_count&&g->adventure_mission==0);
 }
 pe_sandbox_collision_free(c);pe_game_destroy(g);free(g);return failures;
}
