#include "pe_town_wallet.h"
#include "pe_game.h"
#include "pe_town_interaction.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"town wallet:%d %s\n",__LINE__,#c);failures++;}}while(0)
static void profiles(PeGame *g){
    const uint32_t ids[]={20002,20011,20014,20028,20031,20032,20033,20037};
    const uint32_t balances[]={553,584,587,2600,2075,2449,3434,3090};
    const uint32_t increases[]={342,260,288,1792,1181,1321,1623,1558};
    const uint32_t maximums[]={2000,2000,2000,10000,8000,8000,10000,10000};
    CHECK(PE_TOWN_WALLET_CAPACITY==8&&PE_DAY_SECONDS==93600);
    for(size_t i=0;i<PE_TOWN_WALLET_CAPACITY;i++){
        const PeTownWalletSource *s=pe_town_wallet_source_at(i);CHECK(s);if(!s)continue;
        CHECK(s->entity_id==ids[i]&&s->initial_balance==balances[i]&&s->daily_increase==increases[i]&&s->serialized_max==maximums[i]&&!s->initial_added);
        uint32_t value=UINT32_MAX;CHECK(pe_town_wallet_balance(g,ids[i],&value)&&value==balances[i]);
        CHECK(!g->town_wallet[i].added&&pe_town_shop_count(ids[i])>0);
    }
    CHECK(!pe_town_wallet_source_at(8)&&!pe_town_wallet_source_at(SIZE_MAX));
    uint32_t value=123;CHECK(!pe_town_wallet_balance(g,20018,&value)&&value==123);
    CHECK(!pe_town_wallet_balance(g,20002,NULL));
    PeTownWallet before[8];memcpy(before,g->town_wallet,sizeof before);
    CHECK(!pe_town_wallet_init(g)&&!memcmp(before,g->town_wallet,sizeof before));
}
static void clocks(PeGame *g){
    PeTownWallet initial[8];memcpy(initial,g->town_wallet,sizeof initial);
    g->game_seconds=nextafter(PE_DAY_SECONDS/2,0);CHECK(pe_town_wallet_step(g));
    CHECK(!memcmp(initial,g->town_wallet,sizeof initial));
    g->game_seconds=PE_DAY_SECONDS/2;CHECK(pe_town_wallet_step(g));
    for(size_t i=0;i<8;i++)CHECK(g->town_wallet[i].added&&g->town_wallet[i].balance==initial[i].balance+pe_town_wallet_source_at(i)->daily_increase);
    PeTownWallet afternoon[8];memcpy(afternoon,g->town_wallet,sizeof afternoon);
    CHECK(pe_town_wallet_step(g)&&!memcmp(afternoon,g->town_wallet,sizeof afternoon));
    /* Missing every intervening morning does not accumulate days or reset. */
    g->game_seconds=PE_DAY_SECONDS*800+.75*PE_DAY_SECONDS;
    CHECK(pe_town_wallet_step(g)&&!memcmp(afternoon,g->town_wallet,sizeof afternoon));
    g->game_seconds=PE_DAY_SECONDS*801;CHECK(pe_town_wallet_step(g));
    for(size_t i=0;i<8;i++)CHECK(!g->town_wallet[i].added&&g->town_wallet[i].balance==afternoon[i].balance);
    g->game_seconds+=PE_DAY_SECONDS/2;CHECK(pe_town_wallet_step(g));
    for(size_t i=0;i<8;i++)CHECK(g->town_wallet[i].added&&g->town_wallet[i].balance==afternoon[i].balance+pe_town_wallet_source_at(i)->daily_increase);
    /* Stored latch is authoritative at noon; dead players/shop closure do not
     * own NPC purse updates. No cap at the serialized maximum exists. */
    for(size_t i=0;i<8;i++)g->town_wallet[i]=(PeTownWallet){pe_town_wallet_source_at(i)->serialized_max+100,i%2!=0};
    memcpy(afternoon,g->town_wallet,sizeof afternoon);g->hp=0;
    CHECK(pe_town_wallet_step(g));
    for(size_t i=0;i<8;i++)CHECK(g->town_wallet[i].added&&g->town_wallet[i].balance==afternoon[i].balance+(i%2?0:pe_town_wallet_source_at(i)->daily_increase));
    /* Native seconds accept finite large values. At this diagnostic extreme,
     * divide/floor loses the fraction while fmod returns71200; require the
     * source expression, without claiming this is a representable Unity tick. */
    g->game_seconds=1e21;memcpy(afternoon,g->town_wallet,sizeof afternoon);
    CHECK(fmod(g->game_seconds,PE_DAY_SECONDS)>PE_DAY_SECONDS/2);
    CHECK(pe_town_wallet_step(g));
    for(size_t i=0;i<8;i++)CHECK(!g->town_wallet[i].added&&g->town_wallet[i].balance==afternoon[i].balance);
    g->hp=g->max_hp;
}
static void rejected(PeGame *g){
    g->game_seconds=PE_DAY_SECONDS/2;
    for(size_t i=0;i<8;i++)g->town_wallet[i]=(PeTownWallet){10,false};
    /* Last vendor overflows after seven provisional additions: none commit. */
    g->town_wallet[7].balance=INT32_MAX;PeTownWallet before[8];memcpy(before,g->town_wallet,sizeof before);
    CHECK(!pe_town_wallet_step(g)&&!memcmp(before,g->town_wallet,sizeof before));
    g->town_wallet[7].balance=(uint32_t)INT32_MAX-pe_town_wallet_source_at(7)->daily_increase;
    CHECK(pe_town_wallet_step(g)&&g->town_wallet[7].balance==INT32_MAX);
    memcpy(before,g->town_wallet,sizeof before);
    const double invalid[]={NAN,INFINITY,-1};
    for(size_t i=0;i<3;i++){g->game_seconds=invalid[i];CHECK(!pe_town_wallet_step(g)&&!memcmp(before,g->town_wallet,sizeof before));}
    g->game_seconds=0;g->town_wallet[7].balance=UINT32_MAX;memcpy(before,g->town_wallet,sizeof before);
    CHECK(!pe_town_wallet_state_valid(g)&&!pe_town_wallet_step(g)&&!memcmp(before,g->town_wallet,sizeof before));
    uint32_t out=77;CHECK(!pe_town_wallet_balance(g,20002,&out)&&out==77);
    CHECK(!pe_town_wallet_init(g)&&!memcmp(before,g->town_wallet,sizeof before));
    g->mode=(PeGameMode)2;CHECK(!pe_town_wallet_init(g)&&!pe_town_wallet_state_valid(g)&&!pe_town_wallet_step(g));
    g->mode=PE_GAME_LEGACY;CHECK(!pe_town_wallet_state_valid(g));
    memset(g->town_wallet,0,sizeof g->town_wallet);CHECK(pe_town_wallet_init(g)&&pe_town_wallet_state_valid(g));
    g->game_seconds=PE_DAY_SECONDS/2;CHECK(pe_town_wallet_step(g));
    for(size_t i=0;i<8;i++)CHECK(!g->town_wallet[i].balance&&!g->town_wallet[i].added);
    CHECK(pe_town_wallet_balance(g,20002,&out)&&out==0);
    g->town_wallet[0].added=true;CHECK(!pe_town_wallet_state_valid(g));g->town_wallet[0].added=false;
    CHECK(!pe_town_wallet_init(NULL)&&!pe_town_wallet_step(NULL)&&!pe_town_wallet_state_valid(NULL));
}
static void purchases(PeGame *g){
    g->mode=PE_GAME_ADVENTURE;CHECK(pe_town_wallet_init(g));g->game_seconds=PE_DAY_SECONDS;
    CHECK(pe_town_shop_open(g,20002));
    uint32_t start=g->town_wallet[0].balance;unsigned stock=pe_town_shop_stock(g,20002,1184);uint32_t money=g->money;
    PeTownWallet all[8];memcpy(all,g->town_wallet,sizeof all);
    CHECK(pe_town_shop_buy(g,20002,1184,1));
    CHECK(g->town_wallet[0].balance==start+158&&!g->town_wallet[0].added&&g->money==money-158&&pe_town_shop_stock(g,20002,1184)==stock-1);
    CHECK(!memcmp(all+1,g->town_wallet+1,7*sizeof all[0]));
    /* The purse may cross its serialized maximum on an ordinary purchase. */
    g->town_wallet[0].balance=1999;g->money=1000;CHECK(pe_town_shop_buy(g,20002,1184,1)&&g->town_wallet[0].balance==2157);
    g->town_wallet[0].balance=INT32_MAX-157;
    PeInventory inv=g->inventory;PeTownStock stocks[PE_TOWN_STOCK_CAPACITY];memcpy(stocks,g->town_stock,sizeof stocks);
    memcpy(all,g->town_wallet,sizeof all);money=g->money;
    CHECK(!pe_town_shop_buy(g,20002,1184,1));
    CHECK(!memcmp(&inv,&g->inventory,sizeof inv)&&!memcmp(stocks,g->town_stock,sizeof stocks)&&!memcmp(all,g->town_wallet,sizeof all)&&g->money==money);
    g->town_wallet[0].balance=INT32_MAX-158;
    CHECK(pe_town_shop_buy(g,20002,1184,1)&&g->town_wallet[0].balance==INT32_MAX);
    /* Full bag rejects before wallet/stock/player debit; preserve instance IDs. */
    memset(g->inventory.slots,0,sizeof g->inventory.slots);CHECK(pe_inventory_add(&g->inventory,877,PE_INVENTORY_CATEGORY_SLOTS));
    CHECK(pe_town_shop_open(g,20011));inv=g->inventory;memcpy(stocks,g->town_stock,sizeof stocks);memcpy(all,g->town_wallet,sizeof all);money=g->money;
    CHECK(!pe_town_shop_buy(g,20011,338,1));
    CHECK(!memcmp(&inv,&g->inventory,sizeof inv)&&!memcmp(stocks,g->town_stock,sizeof stocks)&&!memcmp(all,g->town_wallet,sizeof all)&&g->money==money);
    memset(g->inventory.slots,0,sizeof g->inventory.slots);g->money=0;inv=g->inventory;
    CHECK(!pe_town_shop_buy(g,20011,338,1)&&!memcmp(&inv,&g->inventory,sizeof inv)&&!memcmp(all,g->town_wallet,sizeof all));
    CHECK(!pe_town_shop_buy(g,20011,1184,1)&&!pe_town_shop_buy(g,20002,1184,0)&&!pe_town_shop_buy(g,20002,1184,UINT32_MAX));
}
int pe_test_town_wallet(void){
    failures=0;PeGame *g=calloc(1,sizeof *g);if(!g)return 1;
    if(!pe_game_init_mode(g,42,PE_GAME_ADVENTURE)){free(g);return 1;}
    CHECK(pe_town_wallet_state_valid(g));profiles(g);clocks(g);rejected(g);purchases(g);
    pe_game_destroy(g);free(g);return failures;
}
