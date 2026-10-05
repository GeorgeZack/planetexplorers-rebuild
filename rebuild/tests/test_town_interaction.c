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
    CHECK(pe_town_shop_open(g,20002));CHECK(pe_town_shop_stock(g,1184)==3);
    uint32_t money=g->money;CHECK(pe_town_shop_buy(g,20002,1184,1));
    CHECK(g->money==money-158&&pe_inventory_count(&g->inventory,12)==1&&pe_town_shop_stock(g,1184)==2);
    CHECK(!pe_town_shop_buy(g,20014,1184,1));CHECK(!pe_town_shop_buy(g,20002,1184,0));CHECK(!pe_town_shop_buy(g,20002,1184,UINT32_MAX));
    CHECK(pe_town_shop_buy(g,20002,1184,2));CHECK(pe_town_shop_stock(g,1184)==0);
    money=g->money;PeInventory inv=g->inventory;CHECK(!pe_town_shop_buy(g,20002,1185,1));CHECK(g->money==money&&!memcmp(&inv,&g->inventory,sizeof inv));
    double created=g->game_seconds;g->game_seconds=created+46800;CHECK(pe_town_shop_open(g,20002));CHECK(!pe_town_shop_stock(g,1184));
    g->game_seconds+=.01;CHECK(pe_town_shop_open(g,20002));CHECK(pe_town_shop_stock(g,1184)==3);
    /* Nonempty rows reset timestamps on opening even before refresh interval. */
    CHECK(g->town_stock[4].create_time==g->game_seconds);
    g->money=100000;memset(&g->inventory,0,sizeof g->inventory);g->inventory.next_instance_id=1;
    CHECK(pe_inventory_add(&g->inventory,12,32));inv=g->inventory;money=g->money;
    CHECK(!pe_town_shop_buy(g,20002,1184,1));CHECK(g->money==money&&pe_town_shop_stock(g,1184)==3&&!memcmp(&inv,&g->inventory,sizeof inv));
    g->town_stock[0].remaining=4;CHECK(!pe_town_state_valid(g));g->town_stock[0].remaining=3;
    g->town_stock[0].create_time=NAN;CHECK(!pe_town_state_valid(g));g->town_stock[0].create_time=g->game_seconds;
    CHECK(pe_test_ack_welcome(g));CHECK(pe_game_arrival_complete(g));
    memset(&g->inventory,0,sizeof g->inventory);g->inventory.next_instance_id=1;CHECK(pe_game_accept_supplies(g));
    g->world.source=1;g->world.origin_x=-300;g->world.origin_y=16;g->world.origin_z=100;
    g->player=(PeVec3){31,9,34};CHECK(!pe_adventure_annot_step(g));g->player.x=30.99f;g->player.y=-16.01f;CHECK(!pe_adventure_annot_step(g));g->player.y=9;
    g->supplies_claimed=false;CHECK(!pe_adventure_annot_step(g));g->supplies_claimed=true;
    inv=g->inventory;money=g->money;CHECK(pe_adventure_annot_step(g));CHECK(g->annot_complete);CHECK(!pe_adventure_annot_step(g));CHECK(g->money==money&&!memcmp(&inv,&g->inventory,sizeof inv));
    PeAdventureDialogue d;pe_adventure_dialogue_init(&d);CHECK(pe_adventure_dialogue_open(&d,g,3,20018,true));CHECK(pe_adventure_dialogue_current(&d,g)->talk_id==4095);
    CHECK(pe_adventure_dialogue_choose_supply(&d,g));CHECK(pe_adventure_dialogue_current(&d,g)->talk_id==4096);CHECK(pe_adventure_dialogue_advance(&d,g)==PE_ADVENTURE_DIALOGUE_ACCEPTED);CHECK(!memcmp(&inv,&g->inventory,sizeof inv));
    pe_world_destroy(&g->world);memset(g,0,sizeof *g);
    CHECK(pe_game_init_mode(g,77,PE_GAME_ADVENTURE));CHECK(pe_test_ack_welcome(g));CHECK(pe_game_arrival_complete(g));CHECK(pe_game_accept_supplies(g));
    CHECK(pe_town_shop_open(g,20011));CHECK(pe_town_shop_buy(g,20011,338,1));g->annot_complete=true;
    char path[128],error[256];snprintf(path,sizeof path,"/tmp/pe-town-%ld.pesave",(long)getpid());
    PeGame *loaded=calloc(1,sizeof *loaded);CHECK(loaded);
    bool saved=pe_game_save(g,path,error,sizeof error);if(!saved)fprintf(stderr,"town save: %s\n",error);CHECK(saved);
    if(saved&&loaded){CHECK(pe_game_load(loaded,path,error,sizeof error));CHECK(loaded->annot_complete&&loaded->money==g->money&&pe_town_shop_stock(loaded,338)==39);CHECK(!memcmp(g->town_stock,loaded->town_stock,sizeof g->town_stock));}
    unlink(path);if(loaded){pe_world_destroy(&loaded->world);free(loaded);}pe_world_destroy(&g->world);free(g);return failures;
}
