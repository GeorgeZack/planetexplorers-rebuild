#define _POSIX_C_SOURCE 200809L
#include "pe_debug.h"
#include "pe_adventure_hunt.h"
#include "pe_adventure_medicine.h"
#include "pe_build_history.h"
#include "pe_hunt_loot.h"
#include "pe_settlement.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static PeGame *copy(const PeGame *source){
    PeGame *g=calloc(1,sizeof *g);assert(g&&pe_game_clone(g,source));return g;
}
static void release(PeGame *g){pe_game_destroy(g);free(g);}
static int slot(const PeGame *g,int item){
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==item)return i;
    return -1;
}
static void discard_knife(PeGame *g){
    int i=slot(g,33);assert(i>=0);pe_game_unequip(g);PeSlot expected=g->inventory.slots[i];
    assert(pe_game_discard_slot(g,i,&expected)&&!pe_inventory_count(&g->inventory,33));
}
static void apply(PeGame *g,PeDebugAction action){
    char error[256]="unchanged";
    if(!pe_debug_apply(g,action,error,sizeof error)){fprintf(stderr,"debug action %d: %s\n",action,error);abort();}
    assert(!error[0]);
}
static void reject(PeGame *g,PeDebugAction action){
    /* Byte snapshot deliberately does not own/destroy the shared world. */
    unsigned char *before=malloc(sizeof *g);assert(before);memcpy(before,g,sizeof *g);
    uint64_t world=pe_world_hash(&g->world);char error[256]={0};
    assert(!pe_debug_apply(g,action,error,sizeof error)&&error[0]);
    assert(!memcmp(before,g,sizeof *g)&&world==pe_world_hash(&g->world));free(before);
}
static void roundtrip(const PeGame *g,const char *path){
    char error[256];PeGame *loaded=calloc(1,sizeof *loaded);assert(loaded);
    if(!pe_game_save(g,path,error,sizeof error)){fprintf(stderr,"debug save: %s\n",error);abort();}
    if(!pe_game_load(loaded,path,error,sizeof error)){fprintf(stderr,"debug load: %s\n",error);abort();}
    assert(!memcmp(&loaded->inventory,&g->inventory,sizeof g->inventory));
    assert(!memcmp(&loaded->hunt_loot,&g->hunt_loot,sizeof g->hunt_loot));
    assert(!memcmp(&loaded->repair,&g->repair,sizeof g->repair));
    assert(loaded->medicine_state==g->medicine_state&&loaded->hunt_state==g->hunt_state&&loaded->hunt_kills==g->hunt_kills);
    assert(loaded->hp==g->hp&&loaded->stamina==g->stamina&&loaded->equipped_instance_id==g->equipped_instance_id);
    assert(pe_world_hash(&loaded->world)==pe_world_hash(&g->world));release(loaded);
}
static void medicine(const PeGame *base,const char *path){
    PeGame *g=copy(base);assert(pe_inventory_add(&g->inventory,917,2));
    g->welcome_page=PE_ADVENTURE_WELCOME_PAGES;g->adventure_mission=PE_ADVENTURE_SUPPLIES;
    g->arrival_complete=g->supplies_claimed=g->annot_complete=true;g->medicine_state=PE_MEDICINE_ACTIVE;
    int herbs=pe_inventory_count(&g->inventory,918),rewards=pe_inventory_count(&g->inventory,554);
    uint32_t rng=g->rng_state,counter=g->inventory.next_instance_id;
    apply(g,PE_DEBUG_MEDICINE);
    assert(pe_inventory_count(&g->inventory,917)==5&&g->medicine_state==PE_MEDICINE_ACTIVE);
    assert(pe_adventure_medicine_ready(g)&&pe_inventory_count(&g->inventory,918)==herbs);
    assert(pe_inventory_count(&g->inventory,554)==rewards&&g->rng_state==rng&&g->inventory.next_instance_id==counter);
    PeInventory bag=g->inventory;apply(g,PE_DEBUG_MEDICINE);assert(!memcmp(&bag,&g->inventory,sizeof bag));
    assert(pe_inventory_add(&g->inventory,917,2));apply(g,PE_DEBUG_MEDICINE);assert(pe_inventory_count(&g->inventory,917)==7);
    roundtrip(g,path);release(g);
}
static void combat_supplies(const PeGame *base,const char *path){
    PeGame *g=copy(base);discard_knife(g);g->hp=0;g->stamina=1;g->medpack_cooldown=8;g->juice_cooldown=7;
    g->revival=(PeRevival){.phase=PE_REVIVAL_GETTING_UP,.last_time=2,.enable_at=3,.frame=4};
    g->outfit.items[0].durability=0;PeOutfit outfit=g->outfit;
    assert(pe_inventory_remove(&g->inventory,937,pe_inventory_count(&g->inventory,937)-2));
    assert(pe_inventory_add(&g->inventory,925,22));uint32_t equipped=g->equipped_instance_id,rng=g->rng_state;
    apply(g,PE_DEBUG_COMBAT_SUPPLIES);
    assert(pe_inventory_count(&g->inventory,33)==1&&pe_inventory_count(&g->inventory,925)==22);
    assert(pe_inventory_count(&g->inventory,916)==20&&pe_inventory_count(&g->inventory,937)==5);
    assert(g->hp==g->max_hp&&g->stamina==g->stamina_max&&g->revival.phase==PE_REVIVAL_IDLE);
    PeRevival empty={0};assert(!memcmp(&g->revival,&empty,sizeof empty));
    assert(!memcmp(&g->outfit,&outfit,sizeof outfit)&&g->equipped_instance_id==equipped&&g->rng_state==rng);
    assert(g->medpack_cooldown==8&&g->juice_cooldown==7&&!g->medicine_state&&!g->hunt_state);
    PeInventory bag=g->inventory;apply(g,PE_DEBUG_COMBAT_SUPPLIES);assert(!memcmp(&bag,&g->inventory,sizeof bag));
    roundtrip(g,path);release(g);
}
static void ownership(PeGame *g){
    assert(pe_inventory_add(&g->inventory,1,1));int i=slot(g,1);assert(i>=0);
    g->inventory.slots[i].durability=100;PeSlot item=g->inventory.slots[i];
    assert(pe_game_repair_insert(g,i,&item));assert(pe_game_repair_state_valid(g));
    assert(pe_hunt_loot_spawn(g,&g->player,1));
    PeB45Cell cell={.x=10,.y=50,.z=10,.type=PE_B45_CUBE<<2,.material=11};
    assert(pe_build_history_record(g,&cell,1,true));
}
static void start_hunt(const PeGame *base,const char *path){
    PeGame *g=copy(base);ownership(g);PeCampRepair repair=g->repair;PeHuntLootState loot=g->hunt_loot;
    PeBuildHistory history=g->build_history;PeOutfit outfit=g->outfit;uint32_t rng=g->rng_state;
    int recipes=g->known_recipe_count,script=pe_inventory_count(&g->inventory,554);uint64_t world=pe_world_hash(&g->world);
    apply(g,PE_DEBUG_START_HUNT);
    assert(g->hunt_state==PE_HUNT_ENCOUNTER_PENDING&&!g->hunt_kills&&g->medicine_state==PE_MEDICINE_COMPLETE);
    assert(g->welcome_page==9&&g->adventure_mission==PE_ADVENTURE_SUPPLIES&&g->arrival_complete&&g->supplies_claimed&&g->annot_complete);
    assert(pe_adventure_medicine_state_valid(g)&&pe_adventure_hunt_state_valid(g));
    assert(pe_game_equipped_tool(g)&&pe_game_equipped_tool(g)->item_id==33&&g->grounded);
    assert(g->velocity.x==0&&g->velocity.y==0&&g->velocity.z==0&&!g->jumping&&!g->sprinting);
    assert(!memcmp(&g->repair,&repair,sizeof repair)&&!memcmp(&g->hunt_loot,&loot,sizeof loot));
    assert(!memcmp(&g->build_history,&history,sizeof history)&&!memcmp(&g->outfit,&outfit,sizeof outfit));
    assert(g->rng_state==rng&&pe_world_hash(&g->world)==world&&g->known_recipe_count==recipes&&pe_inventory_count(&g->inventory,554)==script);
    PeVec3 destination;assert(pe_adventure_hunt_reference(&destination));
    assert(g->player.x==destination.x-g->world.origin_x&&g->player.z==destination.z-g->world.origin_z);
    PeSandboxCollision *collision=pe_sandbox_collision_create();char error[256];assert(collision);
    assert(pe_game_collision_update(g,collision,error,sizeof error));
    assert(!pe_sandbox_collision_overlap(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));
    assert(pe_sandbox_collision_support(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,.1f,.65f).hit);
    pe_sandbox_collision_free(collision);roundtrip(g,path);
    /* Existing encounter credit is retained, while the host performs its
     * documented remaining-cohort reload. No corpse/repair/history is lost. */
    g->hunt_kills=3;g->player=base->player;g->hp=0;apply(g,PE_DEBUG_START_HUNT);
    assert(g->hunt_kills==3&&g->hunt_state==PE_HUNT_ENCOUNTER_PENDING);
    assert(!memcmp(&g->repair,&repair,sizeof repair)&&!memcmp(&g->hunt_loot,&loot,sizeof loot)&&!memcmp(&g->build_history,&history,sizeof history));
    roundtrip(g,path);
    for(unsigned state=PE_HUNT_RETURN_PENDING;state<=PE_HUNT_COMPLETE;state++){
        g->hunt_state=state;g->hunt_kills=4;reject(g,PE_DEBUG_START_HUNT);
    }
    release(g);
    /* Existing accepted travel uses its normal discovery transition. */
    g=copy(base);g->welcome_page=9;g->adventure_mission=PE_ADVENTURE_SUPPLIES;
    g->arrival_complete=g->supplies_claimed=g->annot_complete=true;g->medicine_state=PE_MEDICINE_COMPLETE;
    assert(pe_adventure_hunt_accept(g));apply(g,PE_DEBUG_START_HUNT);
    assert(g->hunt_state==PE_HUNT_ENCOUNTER_PENDING&&!g->hunt_kills);release(g);
}
static void failures(const PeGame *base){
    PeGame *g=copy(base);reject(g,PE_DEBUG_NONE);reject(g,(PeDebugAction)99);
    g->hp=NAN;reject(g,PE_DEBUG_COMBAT_SUPPLIES);g->hp=base->hp;
    g->welcome_page=10;reject(g,PE_DEBUG_START_HUNT);g->welcome_page=base->welcome_page;
    discard_knife(g);g->inventory.next_instance_id=UINT32_MAX;reject(g,PE_DEBUG_COMBAT_SUPPLIES);release(g);
    /* Equipment insertion would succeed; a later full consumable category
     * must roll back that newly allocated Knife and all earlier top-ups. */
    g=copy(base);discard_knife(g);int category=pe_item_def(925)->tab,start=pe_inventory_category_start(&g->inventory,category);
    const PeItemDef *juice=pe_item_def(916);assert(juice->tab==category);
    for(int i=start;i<start+PE_INVENTORY_CATEGORY_SLOTS;i++)g->inventory.slots[i]=(PeSlot){.item_id=916,.count=juice->stack_limit};
    assert(pe_inventory_state_valid(&g->inventory));reject(g,PE_DEBUG_COMBAT_SUPPLIES);reject(g,PE_DEBUG_START_HUNT);
    reject(g,PE_DEBUG_MEDICINE);release(g);
    g=copy(base);g->inventory.slots[slot(g,33)].durability=0;
    reject(g,PE_DEBUG_START_HUNT);release(g);
    /* Deliberately obstruct the exact source anchor through the full allowed
     * settle height; no replacement crop or lateral fabricated spawn. */
    g=copy(base);PeVec3 p;assert(pe_adventure_hunt_reference(&p));
    p.x-=g->world.origin_x;p.y-=g->world.origin_y;p.z-=g->world.origin_z;
    for(int y=-2;y<=8;y++)g->blocks[g->block_count++]=(PeBlock){(int)floorf(p.x),(int)floorf(p.y)+y,(int)floorf(p.z),281};
    reject(g,PE_DEBUG_START_HUNT);release(g);
    g=calloc(1,sizeof *g);assert(g&&pe_game_init_mode(g,72,PE_GAME_ADVENTURE));
    reject(g,PE_DEBUG_START_HUNT);release(g);
    g=calloc(1,sizeof *g);assert(g&&pe_game_init_mode(g,73,PE_GAME_LEGACY));
    reject(g,PE_DEBUG_MEDICINE);reject(g,PE_DEBUG_COMBAT_SUPPLIES);release(g);
    char error[32];assert(!pe_debug_apply(NULL,PE_DEBUG_MEDICINE,error,sizeof error)&&error[0]);
}
int main(void){
    char directory[]="/tmp/pe-debug-test-XXXXXX";assert(mkdtemp(directory));
    char path[256];assert(snprintf(path,sizeof path,"%s/save.pecs",directory)>0);
    PeGame *g=calloc(1,sizeof *g);char error[256];assert(g&&pe_game_init_mode(g,721,PE_GAME_ADVENTURE));
    if(!pe_settlement_start(g,error,sizeof error)){fprintf(stderr,"debug route: %s\n",error);abort();}
    assert(pe_settlement_is_route(&g->world));
    medicine(g,path);combat_supplies(g,path);start_hunt(g,path);failures(g);release(g);
    assert(!unlink(path)&&!rmdir(directory));
    puts("debug: idempotent top-ups, source-route hunt skip, ownership/kill preservation, save roundtrips and atomic failures pass");
    return 0;
}
