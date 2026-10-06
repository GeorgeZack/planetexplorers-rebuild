#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_town_interaction.h"
#include "pe_adventure_dialogue.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"town:%d %s\n",__LINE__,#c);failures++;}}while(0)
int pe_test_town_interaction(void){
    PeGame *g=calloc(1,sizeof *g);if(!g)return 1;
    CHECK(pe_game_init_mode(g,42,PE_GAME_ADVENTURE));
    CHECK(pe_town_state_valid(g));CHECK(pe_town_shop_count(20002)==8);CHECK(pe_town_shop_count(20014)==2);CHECK(pe_town_shop_count(20011)==7);CHECK(!pe_town_shop_count(20016));
    CHECK(!pe_town_shop_open(g,20016));CHECK(!pe_town_shop_buy(g,20002,1184,1));
    CHECK(pe_town_shop_open(g,20002));CHECK(pe_town_shop_stock(g,20002,1184)==3);
    uint32_t money=g->money;CHECK(pe_town_shop_buy(g,20002,1184,1));
    CHECK(g->money==money-158&&pe_inventory_count(&g->inventory,12)==1&&pe_town_shop_stock(g,20002,1184)==2);
    CHECK(!pe_town_shop_buy(g,20014,1184,1));CHECK(!pe_town_shop_buy(g,20002,1184,0));CHECK(!pe_town_shop_buy(g,20002,1184,UINT32_MAX));
    CHECK(pe_town_shop_buy(g,20002,1184,2));CHECK(pe_town_shop_stock(g,20002,1184)==0);
    money=g->money;PeInventory inv=g->inventory;CHECK(!pe_town_shop_buy(g,20002,1185,1));CHECK(g->money==money&&!memcmp(&inv,&g->inventory,sizeof inv));
    double created=g->game_seconds;g->game_seconds=created+46800;CHECK(pe_town_shop_open(g,20002));CHECK(!pe_town_shop_stock(g,20002,1184));
    g->game_seconds+=.01;CHECK(pe_town_shop_open(g,20002));CHECK(pe_town_shop_stock(g,20002,1184)==3);
    /* Nonempty rows reset timestamps on opening even before refresh interval. */
    CHECK(g->town_stock[4].create_time==g->game_seconds);
    g->money=100000;memset(g->inventory.slots,0,sizeof g->inventory.slots);
    CHECK(pe_inventory_add(&g->inventory,12,PE_INVENTORY_SLOTS));inv=g->inventory;money=g->money;
    CHECK(!pe_town_shop_buy(g,20002,1184,1));CHECK(g->money==money&&pe_town_shop_stock(g,20002,1184)==3&&!memcmp(&inv,&g->inventory,sizeof inv));
    g->town_stock[0].remaining=4;CHECK(!pe_town_state_valid(g));g->town_stock[0].remaining=3;
    g->town_stock[0].create_time=NAN;CHECK(!pe_town_state_valid(g));g->town_stock[0].create_time=g->game_seconds;
    /* All five observed Annot vendors resolve independently. Buy the exact
     * first medicine formula ingredients through their source offer rows. */
    CHECK(pe_town_shop_count(20028)==57&&pe_town_shop_count(20031)==31);
    CHECK(pe_town_shop_count(20032)==79&&pe_town_shop_count(20033)==58&&pe_town_shop_count(20037)==42);
    CHECK(!pe_town_shop_count(20018)&&!pe_town_shop_count(20019)&&!pe_town_shop_count(20027));
    CHECK(pe_town_shop_offer_at(16)->entity_id==20011&&pe_town_shop_offer_at(17)->entity_id==20028);
    CHECK(!pe_town_shop_offer_at(PE_TOWN_STOCK_CAPACITY));
    memset(g->inventory.slots,0,sizeof g->inventory.slots);g->money=1000;
    CHECK(pe_town_shop_open(g,20028));
    const int stores[]={212,208,214,211,206},items[]={239,235,241,238,233};
    const unsigned counts[]={5,10,10,5,5};
    for(size_t j=0;j<5;j++){
        unsigned available=pe_town_shop_stock(g,20028,stores[j]);CHECK(available>=counts[j]);
        CHECK(!pe_town_shop_stock(g,20031,stores[j]));CHECK(!pe_town_shop_buy(g,20031,stores[j],1));
        CHECK(pe_town_shop_buy(g,20028,stores[j],counts[j]));
        CHECK(pe_town_shop_stock(g,20028,stores[j])==available-counts[j]);
        CHECK(pe_inventory_count(&g->inventory,items[j])==(int)counts[j]);
    }
    CHECK(g->money==895);
    CHECK(pe_inventory_add(&g->inventory,877,1));
    int script=-1;for(int j=0;j<PE_INVENTORY_SLOTS;j++)if(g->inventory.slots[j].item_id==877)script=j;
    CHECK(script>=0&&pe_game_use_slot(g,script));CHECK(!pe_inventory_count(&g->inventory,877));
    CHECK(pe_game_recipe_known(g,337)&&pe_game_recipe_known(g,388)&&pe_game_recipe_known(g,390));
    CHECK(pe_test_ack_welcome(g));CHECK(pe_game_arrival_complete(g));
    memset(g->inventory.slots,0,sizeof g->inventory.slots);CHECK(pe_game_accept_supplies(g));
    g->world.source=1;g->world.origin_x=-300;g->world.origin_y=16;g->world.origin_z=100;
    g->player=(PeVec3){31,9,34};CHECK(!pe_adventure_annot_step(g));g->player.x=30.99f;g->player.y=-16.01f;CHECK(!pe_adventure_annot_step(g));g->player.y=9;
    g->supplies_claimed=false;CHECK(!pe_adventure_annot_step(g));g->supplies_claimed=true;
    inv=g->inventory;money=g->money;CHECK(pe_adventure_annot_step(g));CHECK(g->annot_complete);CHECK(!pe_adventure_annot_step(g));CHECK(g->money==money&&!memcmp(&inv,&g->inventory,sizeof inv));
    PeAdventureDialogue d;pe_adventure_dialogue_init(&d);CHECK(pe_adventure_dialogue_open(&d,g,3,20018,true));CHECK(pe_adventure_dialogue_current(&d,g)->talk_id==4095);
    CHECK(pe_adventure_dialogue_choose_supply(&d,g));CHECK(pe_adventure_dialogue_current(&d,g)->talk_id==4096);CHECK(pe_adventure_dialogue_advance(&d,g)==PE_ADVENTURE_DIALOGUE_ADVANCED);CHECK(d.stage==PE_ADVENTURE_DIALOGUE_CHOICE);CHECK(!memcmp(&inv,&g->inventory,sizeof inv));
    pe_world_destroy(&g->world);memset(g,0,sizeof *g);
    CHECK(pe_game_init_mode(g,77,PE_GAME_ADVENTURE));CHECK(pe_test_ack_welcome(g));CHECK(pe_game_arrival_complete(g));CHECK(pe_game_accept_supplies(g));
    CHECK(pe_town_shop_open(g,20011));CHECK(pe_town_shop_buy(g,20011,338,1));g->annot_complete=true;CHECK(pe_town_shop_open(g,20028));CHECK(pe_town_shop_buy(g,20028,206,1));
    char path[128],error[256];snprintf(path,sizeof path,"/tmp/pe-town-%ld.pesave",(long)getpid());
    PeGame *loaded=calloc(1,sizeof *loaded);CHECK(loaded);
    bool saved=pe_game_save(g,path,error,sizeof error);if(!saved)fprintf(stderr,"town save: %s\n",error);CHECK(saved);
    if(saved&&loaded){CHECK(pe_game_load(loaded,path,error,sizeof error));CHECK(loaded->annot_complete&&loaded->money==g->money&&pe_town_shop_stock(loaded,20011,338)==39);CHECK(!memcmp(g->town_stock,loaded->town_stock,sizeof g->town_stock));}
    unlink(path);if(loaded){pe_world_destroy(&loaded->world);free(loaded);}pe_world_destroy(&g->world);free(g);return failures;
}
