#define _POSIX_C_SOURCE 200809L
#include "pe_game.h"
#include "pe_town_interaction.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"bandage:%d %s\n",__LINE__,#x);failures++;}}while(0)
static int find(const PeGame *g,int id){for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==id)return i;return -1;}
static void advance(PeGame *g,unsigned ticks){for(unsigned i=0;i<ticks;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);}
int pe_test_bandage(void){
    failures=0;char path[1024],error[256];snprintf(path,sizeof path,"%s/pe-bandage-XXXXXX",getenv("TMPDIR")?getenv("TMPDIR"):"/tmp");int fd=mkstemp(path);if(fd>=0)close(fd);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded),*snapshot=malloc(sizeof *snapshot);
    CHECK(fd>=0&&g&&loaded&&snapshot);if(fd<0||!g||!loaded||!snapshot)goto done;
    CHECK(pe_instant_heal_count==2&&pe_instant_heal_def(924)&&pe_instant_heal_def(925));
    CHECK(pe_instant_heal_def(924)->heal==200&&pe_instant_heal_def(925)->heal==350&&pe_instant_heal_def(924)->cooldown_seconds==10);
    CHECK(pe_game_item_is_healing_consumable(916)&&pe_game_item_is_healing_consumable(924)&&pe_game_item_is_healing_consumable(925));
    const int unsupported[]={0,-1,917,926,927,928,929,930,931,932,933,937,1479};
    for(unsigned i=0;i<sizeof unsupported/sizeof *unsupported;i++)CHECK(!pe_game_item_is_healing_consumable(unsupported[i])&&!pe_instant_heal_def(unsupported[i]));
    CHECK(pe_game_init_mode(g,924,PE_GAME_ADVENTURE));CHECK(!pe_inventory_count(&g->inventory,924));g->money=1000;
    /* Existing Annot shop and formula owners supply this item without grants. */
    CHECK(pe_town_shop_open(g,20028));CHECK(pe_town_shop_buy(g,20028,1407,3));CHECK(g->money==961&&pe_town_shop_stock(g,20028,1407)==57);
    CHECK(pe_town_shop_buy(g,20028,341,1));int book=find(g,878);CHECK(book>=0);if(book<0)goto done;
    CHECK(pe_game_use_slot(g,book)&&pe_game_recipe_known(g,146)&&pe_game_recipe_known(g,391)&&pe_game_recipe_known(g,394)&&!pe_inventory_count(&g->inventory,878));
    CHECK(pe_town_shop_buy(g,20028,208,1)&&pe_town_shop_buy(g,20028,212,1)&&pe_town_shop_buy(g,20028,207,2));CHECK(g->money==942);
    CHECK(pe_game_start_craft(g,146,1));pe_game_step(g,(PeInput){0},.25f);CHECK(pe_inventory_count(&g->inventory,924)==3);
    for(unsigned i=0;i<100&&g->craft.recipe_id;i++)pe_game_step(g,(PeInput){0},.1f);
    CHECK(!g->craft.recipe_id&&pe_inventory_count(&g->inventory,924)==4);
    CHECK(!pe_inventory_count(&g->inventory,235)&&!pe_inventory_count(&g->inventory,239)&&!pe_inventory_count(&g->inventory,234));
    CHECK(pe_town_shop_buy(g,20028,1408,2));int bandage=find(g,924),medpack=find(g,925),juice=find(g,916);CHECK(bandage>=0&&medpack>=0&&juice>=0);if(bandage<0||medpack<0||juice<0)goto done;
    CHECK(pe_game_quickslot_assign(g,0,bandage)&&pe_game_quickslot_assign(g,1,bandage)&&pe_game_quickslot_assign(g,2,medpack)&&pe_game_quickslot_assign(g,3,juice));
    g->hp=100;CHECK(pe_game_quickslot_use(g,0)&&g->hp==300&&g->medpack_cooldown==10&&pe_inventory_count(&g->inventory,924)==3);
    CHECK(pe_game_item_cooldown(g,924)==10&&pe_game_item_cooldown(g,925)==10&&pe_game_item_cooldown(g,916)==0&&pe_game_item_cooldown(g,937)==0&&pe_game_item_cooldown(NULL,924)==0);
    memcpy(snapshot,g,sizeof *g);CHECK(!pe_game_quickslot_use(g,1)&&!pe_game_quickslot_use(g,2)&&!memcmp(snapshot,g,sizeof *g));
    advance(g,30);float med=g->medpack_cooldown;CHECK(med>9.49f&&med<9.51f);
    CHECK(pe_game_quickslot_use(g,3)&&g->hp==330&&g->juice_buff_active&&g->juice_cooldown==10&&g->medpack_cooldown==med);
    CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->medpack_cooldown==med&&loaded->juice_cooldown==10&&loaded->juice_buff_active&&loaded->juice_buff_elapsed==g->juice_buff_elapsed&&loaded->juice_buff_tick==g->juice_buff_tick);
    CHECK(!pe_game_quickslot_use(loaded,0)&&!pe_game_quickslot_use(loaded,2));advance(loaded,569);
    CHECK(pe_game_item_cooldown(loaded,924)>0&&!pe_game_quickslot_use(loaded,2));advance(loaded,1);CHECK(pe_game_item_cooldown(loaded,924)==0&&pe_game_item_cooldown(loaded,925)==0);
    double jc=loaded->juice_cooldown,je=loaded->juice_buff_elapsed;unsigned jt=loaded->juice_buff_tick;bool ja=loaded->juice_buff_active;
    loaded->hp=100;CHECK(pe_game_quickslot_use(loaded,2)&&loaded->hp==450&&loaded->medpack_cooldown==10);
    CHECK(loaded->juice_cooldown==jc&&loaded->juice_buff_elapsed==je&&loaded->juice_buff_tick==jt&&loaded->juice_buff_active==ja);
    memcpy(snapshot,loaded,sizeof *loaded);CHECK(!pe_game_quickslot_use(loaded,0)&&!memcmp(snapshot,loaded,sizeof *loaded));
    CHECK(pe_game_save(loaded,path,error,sizeof error)&&pe_game_load(g,path,error,sizeof error));CHECK(g->medpack_cooldown==10&&!pe_game_quickslot_use(g,0));
    advance(g,600);g->hp=490;CHECK(pe_game_quickslot_use(g,0)&&g->hp==500&&pe_inventory_count(&g->inventory,924)==2);
    advance(g,600);CHECK(pe_game_quickslot_use(g,1)&&g->hp==500&&pe_inventory_count(&g->inventory,924)==1); /* Full HP allowed. */
    advance(g,600);CHECK(pe_game_quickslot_use(g,0)&&!pe_inventory_count(&g->inventory,924)&&g->quickslots[0].item_id==924&&g->quickslots[1].item_id==924);
    memcpy(snapshot,g,sizeof *g);CHECK(!pe_game_quickslot_use(g,1)&&!memcmp(snapshot,g,sizeof *g));
    CHECK(pe_inventory_add(&g->inventory,924,1));bandage=find(g,924);CHECK(bandage>=0);if(bandage<0)goto done;
    g->medpack_cooldown=0;g->hp=0;memcpy(snapshot,g,sizeof *g);CHECK(!pe_game_use_slot(g,bandage)&&!memcmp(snapshot,g,sizeof *g));
    const float bad_hp[]={NAN,INFINITY,-1,501};for(unsigned i=0;i<sizeof bad_hp/sizeof *bad_hp;i++){g->hp=bad_hp[i];memcpy(snapshot,g,sizeof *g);CHECK(!pe_game_use_slot(g,bandage)&&!memcmp(snapshot,g,sizeof *g));}
    g->hp=100;const float bad_max[]={0,-1,NAN,INFINITY};for(unsigned i=0;i<sizeof bad_max/sizeof *bad_max;i++){g->max_hp=bad_max[i];memcpy(snapshot,g,sizeof *g);CHECK(!pe_game_use_slot(g,bandage)&&!memcmp(snapshot,g,sizeof *g));}g->max_hp=500;
    const float bad_timer[]={-1,NAN,INFINITY,11};for(unsigned i=0;i<sizeof bad_timer/sizeof *bad_timer;i++){g->medpack_cooldown=bad_timer[i];memcpy(snapshot,g,sizeof *g);CHECK(!pe_game_use_slot(g,bandage)&&!memcmp(snapshot,g,sizeof *g));}g->medpack_cooldown=0;
    PeSlot original=g->inventory.slots[bandage];const PeSlot malformed[]={ {924,-1,0,0},{924,101,0,0},{924,1,1,0},{924,1,0,NAN} };
    for(unsigned i=0;i<sizeof malformed/sizeof *malformed;i++){g->inventory.slots[bandage]=malformed[i];memcpy(snapshot,g,sizeof *g);CHECK(!pe_game_use_slot(g,bandage)&&!memcmp(snapshot,g,sizeof *g));}g->inventory.slots[bandage]=original;
    CHECK(pe_game_use_slot(g,bandage)&&g->hp==300&&g->medpack_cooldown==10);
    memcpy(snapshot,g,sizeof *g);pe_game_step(g,(PeInput){0},NAN);pe_game_step(g,(PeInput){0},-.1f);pe_game_step(g,(PeInput){0},0);CHECK(!memcmp(snapshot,g,sizeof *g));
    FILE *f=fopen(path,"rb");CHECK(f);if(f){unsigned char header[8];CHECK(fread(header,1,8,f)==8&&header[4]==21&&header[5]==0);fclose(f);} /* Bandage reuses the saved group9 timer; verify the current envelope. */
 done:
    if(fd>=0)unlink(path);
    pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);free(snapshot);return failures;
}
#ifdef PE_BANDAGE_TEST_MAIN
int main(void){return pe_test_bandage();}
#endif
