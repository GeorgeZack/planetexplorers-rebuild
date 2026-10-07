#include "pe_town_trade.h"
#include "pe_town_wallet.h"
#include "pe_shield.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"town trade:%d %s\n",__LINE__,#c);failures++;}}while(0)
static void fresh(PeGame *g){
    pe_game_destroy(g);CHECK(pe_game_init_mode(g,772,PE_GAME_ADVENTURE));
    memset(g->inventory.slots,0,sizeof g->inventory.slots);
    memset(g->quickslots,0,sizeof g->quickslots);g->equipped_instance_id=0;g->shield_instance_id=0;
    g->money=1000;g->town_wallet[0].balance=1000;
}
static PeSlot item(int id,uint32_t instance){
    const PeItemDef *d=pe_item_def(id);CHECK(d);if(!d)return (PeSlot){0};
    return (PeSlot){id,1,d->stack_limit==1?instance:0,d->stack_limit==1?(float)d->durability_max:0};
}
static int add(PeGame *g,int id,int count){
    CHECK(pe_inventory_add(&g->inventory,id,count));
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==id)return i;
    CHECK(false);return 0;
}
static bool same_except_message(const PeGame *g,const PeGame *before){
    size_t offset=offsetof(PeGame,message),after=offset+sizeof g->message;
    return !memcmp(g,before,offset)&&!memcmp((const char *)g+after,(const char *)before+after,sizeof *g-after);
}
static void reject_sell(PeGame *g,PeGame *copy,uint32_t entity,int slot,unsigned count){
    memcpy(copy,g,sizeof *copy);
    CHECK(!pe_town_trade_can_sell(g,entity,slot,count)&&!memcmp(copy,g,sizeof *g));
    CHECK(!pe_town_trade_sell(g,entity,slot,count)&&same_except_message(g,copy));
}
static void reject_back(PeGame *g,PeGame *copy,uint32_t entity,size_t row,unsigned count){
    memcpy(copy,g,sizeof *copy);
    CHECK(!pe_town_trade_can_buyback(g,entity,row,count)&&!memcmp(copy,g,sizeof *g));
    CHECK(!pe_town_trade_buyback(g,entity,row,count)&&same_except_message(g,copy));
}
static void prices(void){
    PeSlot s=item(12,7);uint32_t unit=123,base=456;
    CHECK(pe_town_trade_price(&s,&unit,&base)&&unit==42&&base==42);
    s.durability*=.5f;CHECK(pe_town_trade_price(&s,&unit,&base)&&unit==25&&base==42);
    s.durability=0;CHECK(pe_town_trade_price(&s,&unit,&base)&&unit==8&&base==42);
    s=item(33,7);s.durability*=.5f;CHECK(pe_town_trade_price(&s,&unit,&base)&&unit==8&&base==14);
    s=item(10,7);s.durability=0;CHECK(pe_town_trade_price(&s,&unit,&base)&&unit==56&&base==56);
    s=item(925,0);s.count=3;CHECK(pe_town_trade_price(&s,&unit,&base)&&unit==0&&base==0);
    s=item(365,7);unit=123;base=456;CHECK(!pe_town_trade_price(&s,&unit,&base)&&unit==123&&base==456);
    s=item(370,7);CHECK(!pe_town_trade_price(&s,NULL,NULL));
    s=item(12,7);s.durability=NAN;CHECK(!pe_town_trade_price(&s,&unit,&base)&&unit==123&&base==456);
    s=item(12,7);s.count=2;CHECK(!pe_town_trade_price(&s,NULL,NULL));
    s=item(12,0);CHECK(!pe_town_trade_price(&s,NULL,NULL));
    s=item(12,7);s.durability=INFINITY;CHECK(!pe_town_trade_price(&s,NULL,NULL));
    s=item(12,7);s.durability=-1;CHECK(!pe_town_trade_price(&s,NULL,NULL));
    s=item(12,7);s.durability++;CHECK(!pe_town_trade_price(&s,NULL,NULL));
    s=item(279,0);s.durability=1;CHECK(!pe_town_trade_price(&s,NULL,NULL));
    s=(PeSlot){123456,1,0,0};CHECK(!pe_town_trade_price(&s,NULL,NULL));
    CHECK(!pe_town_trade_price(NULL,NULL,NULL));
}
static void durable(PeGame *g,PeGame *before){
    fresh(g);int slot=add(g,12,1);PeSlot sold=g->inventory.slots[slot];sold.durability*=.5f;g->inventory.slots[slot]=sold;
    g->quickslots[0]=(PeQuickslot){12,sold.instance_id};g->quickslots[8]=g->quickslots[0];
    uint32_t next=g->inventory.next_instance_id;memcpy(before,g,sizeof *g);
    CHECK(pe_town_trade_can_sell(g,20002,slot,1)&&!memcmp(g,before,sizeof *g));
    CHECK(pe_town_trade_sell(g,20002,slot,1)&&!g->inventory.slots[slot].count);
    CHECK(g->money==1025&&g->town_wallet[0].balance==975&&g->inventory.next_instance_id==next);
    CHECK(!g->quickslots[0].item_id&&!g->quickslots[8].instance_id);
    const PeSlot *row=pe_town_trade_at(g,20002,0);CHECK(row&&memcmp(row,&sold,sizeof sold)==0);
    CHECK(pe_town_trade_count(g,20002)==1&&pe_town_trade_count(g,20011)==0&&pe_town_trade_state_valid(g));
    CHECK(!pe_town_trade_at(g,20011,0)&&!pe_town_trade_at(g,20002,1));
    reject_back(g,before,20011,0,1);reject_back(g,before,20002,0,2);
    CHECK(pe_town_trade_buyback(g,20002,0,1));
    CHECK(g->money==1000&&g->town_wallet[0].balance==1017); /*42credit,25charge*/
    CHECK(!memcmp(&g->inventory.slots[slot],&sold,sizeof sold)&&g->inventory.next_instance_id==next);
    CHECK(!g->quickslots[0].item_id&&!g->quickslots[8].item_id&&!pe_town_trade_count(g,20002));
    CHECK(pe_town_trade_state_valid(g));
    /* Exhausted instances remain intact and cost floor(base*.2). */
    g->inventory.slots[slot].durability=0;
    CHECK(pe_town_trade_sell(g,20002,slot,1)&&g->money==1008);
    CHECK(pe_town_trade_buyback(g,20002,0,1)&&g->inventory.slots[slot].durability==0&&g->money==1000);
}
static void stacks(PeGame *g,PeGame *before){
    fresh(g);int slot=add(g,279,12);uint32_t unit=0,base=0;CHECK(pe_town_trade_price(&g->inventory.slots[slot],&unit,&base));
    g->quickslots[0]=(PeQuickslot){279,0};uint32_t next=g->inventory.next_instance_id;
    CHECK(pe_town_trade_sell(g,20002,slot,3)&&g->inventory.slots[slot].count==9);
    CHECK(pe_town_trade_sell(g,20002,slot,4)&&g->inventory.slots[slot].count==5);
    CHECK(pe_town_trade_count(g,20002)==2&&g->town_buyback[0].slots[0].count==3&&g->town_buyback[0].slots[1].count==4);
    CHECK(g->money==1000+7*unit&&g->town_wallet[0].balance==1000-7*unit);
    CHECK(pe_town_trade_buyback(g,20002,0,2)&&g->inventory.slots[slot].count==7);
    CHECK(g->town_buyback[0].slots[0].count==1&&g->town_buyback[0].slots[1].count==4);
    CHECK(pe_town_trade_buyback(g,20002,0,1)&&g->town_buyback[0].count==1&&g->town_buyback[0].slots[0].count==4);
    CHECK(!g->town_buyback[0].slots[1].item_id&&g->inventory.next_instance_id==next);
    CHECK(pe_town_trade_sell(g,20002,slot,8)&&!g->inventory.slots[slot].count&&g->quickslots[0].item_id==279);
    CHECK(pe_town_trade_buyback(g,20002,1,8)&&g->inventory.slots[slot].count==8&&g->quickslots[0].item_id==279);
    /* Even free consumables transfer ownership and occupy a buyback row. */
    int med=add(g,925,3);g->town_wallet[0].balance=0;g->money=0;
    CHECK(pe_town_trade_sell(g,20002,med,3)&&g->money==0&&g->town_wallet[0].balance==0);
    CHECK(pe_town_trade_buyback(g,20002,1,3)&&pe_inventory_count(&g->inventory,925)==3);
    reject_sell(g,before,20002,slot,0);reject_sell(g,before,20002,slot,9);
    reject_sell(g,before,99999,slot,1);reject_sell(g,before,20002,-1,1);reject_sell(g,before,20002,32,1);
    reject_back(g,before,20002,SIZE_MAX,1);reject_back(g,before,20002,0,0);
    CHECK(!pe_town_trade_can_sell(NULL,20002,0,1)&&!pe_town_trade_sell(NULL,20002,0,1));
    CHECK(!pe_town_trade_can_buyback(NULL,20002,0,1)&&!pe_town_trade_buyback(NULL,20002,0,1));
    CHECK(!pe_town_trade_count(NULL,20002)&&!pe_town_trade_at(NULL,20002,0)&&!pe_town_trade_state_valid(NULL));
}
static void admission(PeGame *g,PeGame *before){
    fresh(g);int slot=add(g,12,1);uint32_t id=g->inventory.slots[slot].instance_id;
    g->equipped_instance_id=id;reject_sell(g,before,20002,slot,1);g->equipped_instance_id=0;
    int shield=add(g,34,1);g->shield_instance_id=g->inventory.slots[shield].instance_id;
    reject_sell(g,before,20002,shield,1);g->shield_instance_id=0;
    int unsupported=add(g,365,1);reject_sell(g,before,20002,unsupported,1);
    g->town_wallet[0].balance=41;reject_sell(g,before,20002,slot,1);g->town_wallet[0].balance=1000;
    g->money=INT32_MAX-41;reject_sell(g,before,20002,slot,1);
    g->money=INT32_MAX-42;CHECK(pe_town_trade_sell(g,20002,slot,1)&&g->money==INT32_MAX);
    g->money=0;reject_back(g,before,20002,0,1);g->money=42;
    g->town_wallet[0].balance=INT32_MAX-41;reject_back(g,before,20002,0,1);
    g->town_wallet[0].balance=INT32_MAX-42;CHECK(pe_town_trade_buyback(g,20002,0,1)&&g->town_wallet[0].balance==INT32_MAX);
    g->money=UINT32_MAX;reject_sell(g,before,20002,slot,1);g->money=0;
    g->hp=0;reject_sell(g,before,20002,slot,1);g->hp=NAN;reject_sell(g,before,20002,slot,1);g->hp=500;
    g->mode=PE_GAME_LEGACY;reject_sell(g,before,20002,slot,1);g->mode=PE_GAME_ADVENTURE;
    g->inventory.slots[slot].durability=NAN;reject_sell(g,before,20002,shield,1);
}
static void capacity(PeGame *g,PeGame *before){
    fresh(g);int slot=add(g,12,1);g->inventory.slots[slot].durability*=.5f;
    CHECK(pe_town_trade_sell(g,20002,slot,1));uint32_t id=g->town_buyback[0].slots[0].instance_id;
    CHECK(pe_inventory_add(&g->inventory,12,PE_INVENTORY_CATEGORY_SLOTS));reject_back(g,before,20002,0,1);
    g->inventory.slots[839]=(PeSlot){0};CHECK(pe_town_trade_buyback(g,20002,0,1)&&g->inventory.slots[839].instance_id==id);
    fresh(g);slot=add(g,279,40);
    for(unsigned i=0;i<PE_TOWN_BUYBACK_CAPACITY;i++)CHECK(pe_town_trade_sell(g,20002,slot,1));
    CHECK(g->town_buyback[0].count==32&&g->inventory.slots[slot].count==8);
    reject_sell(g,before,20002,slot,1);
    CHECK(pe_town_trade_sell(g,20011,slot,1)&&pe_town_trade_count(g,20011)==1);
    CHECK(pe_town_trade_buyback(g,20002,31,1)&&g->town_buyback[0].count==31);
    CHECK(pe_town_trade_sell(g,20002,slot,1)&&g->town_buyback[0].count==32);
    /* Stack buyback must preflight a partial merge plus a missing free slot. */
    fresh(g);slot=add(g,279,5);CHECK(pe_town_trade_sell(g,20002,slot,5));
    int max=pe_item_def(279)->stack_limit;CHECK(pe_inventory_add(&g->inventory,279,max-1));
    CHECK(pe_inventory_add(&g->inventory,280,pe_item_def(280)->stack_limit*(PE_INVENTORY_CATEGORY_SLOTS-1)));reject_back(g,before,20002,0,2);
    CHECK(pe_town_trade_buyback(g,20002,0,1)&&g->inventory.slots[840].count==max);
}
static void malformed(PeGame *g,PeGame *before){
    fresh(g);int slot=add(g,12,1);CHECK(pe_town_trade_sell(g,20002,slot,1));
    PeSlot sold=g->town_buyback[0].slots[0];
#define BAD_STATE(change,restore) do{change;CHECK(!pe_town_trade_state_valid(g));reject_back(g,before,20002,0,1);restore;CHECK(pe_town_trade_state_valid(g));}while(0)
    BAD_STATE(g->town_buyback[0].count=33,g->town_buyback[0].count=1);
    BAD_STATE(g->town_buyback[1].slots[0]=sold,g->town_buyback[1].slots[0]=(PeSlot){0});
    BAD_STATE(g->town_buyback[1].count=1;g->town_buyback[1].slots[0]=sold,g->town_buyback[1]=(PeTownBuyback){0});
    BAD_STATE(g->inventory.slots[0]=sold,g->inventory.slots[0]=(PeSlot){0});
    BAD_STATE(g->quickslots[0]=((PeQuickslot){12,sold.instance_id}),g->quickslots[0]=(PeQuickslot){0});
    uint32_t outfit=g->outfit.items[0].instance_id;
    BAD_STATE(g->outfit.items[0].instance_id=sold.instance_id,g->outfit.items[0].instance_id=outfit);
    BAD_STATE(g->deployed[63].instance_id=sold.instance_id,g->deployed[63].instance_id=0);
    BAD_STATE(g->equipped_instance_id=sold.instance_id,g->equipped_instance_id=0);
    BAD_STATE(g->shield_instance_id=sold.instance_id,g->shield_instance_id=0);
    uint32_t next=g->inventory.next_instance_id;
    BAD_STATE(g->inventory.next_instance_id=sold.instance_id,g->inventory.next_instance_id=next);
    BAD_STATE(g->town_buyback[0].slots[0].durability=NAN,g->town_buyback[0].slots[0]=sold);
    BAD_STATE(g->town_buyback[0].slots[31].durability=NAN,g->town_buyback[0].slots[31]=(PeSlot){0});
    BAD_STATE(g->town_buyback[0].slots[0].count=0,g->town_buyback[0].slots[0]=sold);
    BAD_STATE(g->mode=PE_GAME_LEGACY,g->mode=PE_GAME_ADVENTURE);
#undef BAD_STATE
    /* Malformed unrelated inventory state cannot be laundered through trades. */
    int bag=add(g,19,1);PeSlot original=g->inventory.slots[bag];
    g->inventory.slots[1]=original;reject_back(g,before,20002,0,1);g->inventory.slots[1]=(PeSlot){0};
    g->quickslots[0]=(PeQuickslot){12,UINT32_MAX};reject_back(g,before,20002,0,1);g->quickslots[0]=(PeQuickslot){0};
    g->inventory.slots[31].item_id=12;reject_back(g,before,20002,0,1);g->inventory.slots[31]=(PeSlot){0};
    g->town_wallet[7].balance=UINT32_MAX;reject_back(g,before,20002,0,1);
    PeGame *empty=calloc(1,sizeof *empty);CHECK(empty);
    if(empty){CHECK(pe_town_trade_state_valid(empty));empty->mode=PE_GAME_ADVENTURE;CHECK(pe_town_trade_state_valid(empty));free(empty);}
}
int pe_test_town_trade(void){
    failures=0;PeGame *g=calloc(1,sizeof *g),*before=malloc(sizeof *before);CHECK(g&&before);
    prices();if(g&&before){durable(g,before);stacks(g,before);admission(g,before);capacity(g,before);malformed(g,before);}
    pe_game_destroy(g);free(g);free(before);return failures;
}
