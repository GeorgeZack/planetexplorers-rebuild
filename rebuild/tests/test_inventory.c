#include "pe_game.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL inventory %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
static int first(const PeInventory *p,int id){for(int i=0;i<pe_inventory_capacity(p);i++)if(p->slots[i].item_id==id)return i;return -1;}
static void category_capacity(void){
    PeInventory p={.categorized=true},before;
    CHECK(pe_inventory_capacity(&p)==1680&&pe_inventory_category_capacity(&p,3)==420&&pe_inventory_category_start(&p,3)==1260);
    int max=pe_item_def(PE_ITEM_WOOD)->stack_limit;
    CHECK(pe_inventory_add(&p,PE_ITEM_WOOD,max*420));CHECK(first(&p,PE_ITEM_WOOD)==840);
    before=p;CHECK(!pe_inventory_add(&p,PE_ITEM_STONE,1)&&memcmp(&p,&before,sizeof p)==0);
    CHECK(pe_inventory_add(&p,916,7)&&pe_inventory_add(&p,12,1));CHECK(first(&p,916)==0&&first(&p,12)==420);
    CHECK(pe_inventory_count(&p,PE_ITEM_WOOD)==max*420);
    CHECK(pe_inventory_remove(&p,PE_ITEM_WOOD,max*420));CHECK(pe_inventory_count(&p,PE_ITEM_WOOD)==0);
    CHECK(pe_inventory_state_valid(&p));
    memset(&p,0,sizeof p);CHECK(pe_inventory_capacity(&p)==32);
    CHECK(pe_inventory_add(&p,PE_ITEM_WOOD,max*32));before=p;
    CHECK(!pe_inventory_add(&p,916,1)&&memcmp(&p,&before,sizeof p)==0);
}
static void migration(void){
    PeInventory p={0},before;
    CHECK(pe_inventory_add(&p,12,1)&&pe_inventory_add(&p,916,20)&&pe_inventory_add(&p,PE_ITEM_WOOD,99)&&pe_inventory_add(&p,19,1));
    p.slots[0].durability=1;PeSlot a=p.slots[0],b=p.slots[3];uint32_t next=p.next_instance_id;
    CHECK(pe_inventory_enable_categories(&p));CHECK(p.categorized&&p.next_instance_id==next);
    CHECK(memcmp(&p.slots[420],&a,sizeof a)==0&&memcmp(&p.slots[421],&b,sizeof b)==0);
    CHECK(p.slots[0].item_id==916&&p.slots[840].item_id==PE_ITEM_WOOD);before=p;
    CHECK(pe_inventory_enable_categories(&p)&&memcmp(&p,&before,sizeof p)==0);
    p.slots[420].instance_id=p.slots[421].instance_id;CHECK(!pe_inventory_state_valid(&p));
    p=before;p.slots[1260]=p.slots[840];CHECK(!pe_inventory_state_valid(&p));
    memset(&p,0,sizeof p);p.slots[32]=(PeSlot){.item_id=916,.count=1};before=p;
    CHECK(!pe_inventory_enable_categories(&p)&&memcmp(&p,&before,sizeof p)==0);
}
static void editing(void){
    PeInventory p={.categorized=true},before;int destination=-1;
    CHECK(pe_inventory_add(&p,PE_ITEM_WOOD,20));CHECK(pe_inventory_split(&p,840,7,&destination)&&destination==841);
    CHECK(p.slots[840].count==13&&p.slots[841].count==7&&pe_inventory_count(&p,PE_ITEM_WOOD)==20);
    before=p;destination=-99;CHECK(!pe_inventory_split(&p,840,13,&destination)&&destination==-99&&memcmp(&p,&before,sizeof p)==0);
    CHECK(!pe_inventory_split(&p,840,0,NULL));CHECK(!pe_inventory_split(&p,-1,1,NULL));
    CHECK(pe_inventory_move(&p,840,841)&&p.slots[840].count==7&&p.slots[841].count==13); /* Source drag swaps. */
    CHECK(pe_inventory_move(&p,841,900)&&!p.slots[841].count&&p.slots[900].count==13);
    before=p;CHECK(!pe_inventory_move(&p,900,0)&&memcmp(&p,&before,sizeof p)==0);
    CHECK(pe_inventory_merge(&p,900,840)&&p.slots[840].count==20&&!p.slots[900].count);
    int max=pe_item_def(PE_ITEM_WOOD)->stack_limit;p.slots[840].count=max-2;p.slots[841]=(PeSlot){.item_id=PE_ITEM_WOOD,.count=7};
    CHECK(pe_inventory_merge(&p,841,840)&&p.slots[840].count==max&&p.slots[841].count==5);
    before=p;CHECK(!pe_inventory_merge(&p,841,840)&&memcmp(&p,&before,sizeof p)==0);
    for(int i=842;i<1260;i++)p.slots[i]=(PeSlot){.item_id=PE_ITEM_STONE,.count=1};
    before=p;destination=-1;
    CHECK(!pe_inventory_split(&p,841,1,&destination)&&destination==-1&&memcmp(&p,&before,sizeof p)==0);
    CHECK(pe_inventory_add(&p,12,1));before=p;CHECK(!pe_inventory_split(&p,420,1,NULL)&&memcmp(&p,&before,sizeof p)==0);
}
static void sorting(void){
    PeInventory p={.categorized=true};int max=pe_item_def(PE_ITEM_WOOD)->stack_limit;
    p.slots[845]=(PeSlot){.item_id=PE_ITEM_STONE,.count=8};p.slots[880]=(PeSlot){.item_id=PE_ITEM_WOOD,.count=max-2};p.slots[1200]=(PeSlot){.item_id=PE_ITEM_WOOD,.count=7};
    CHECK(pe_inventory_sort(&p,2));CHECK(p.slots[840].item_id==PE_ITEM_WOOD&&p.slots[840].count==5);
    CHECK(p.slots[841].item_id==PE_ITEM_WOOD&&p.slots[841].count==max&&p.slots[842].item_id==PE_ITEM_STONE&&!p.slots[843].count);
    CHECK(pe_inventory_add(&p,12,2)&&pe_inventory_add(&p,33,1));PeSlot old=p.slots[420];p.slots[420].durability=old.durability=0;
    CHECK(pe_inventory_sort(&p,1));CHECK(p.slots[420].item_id==33&&p.slots[421].instance_id==old.instance_id&&p.slots[421].durability==0);
    CHECK(pe_inventory_state_valid(&p));PeInventory before=p;
    CHECK(!pe_inventory_sort(&p,4)&&memcmp(&p,&before,sizeof p)==0);
    memset(&p,0,sizeof p);CHECK(pe_inventory_add(&p,12,1)&&pe_inventory_add(&p,PE_ITEM_STONE,5)&&pe_inventory_add(&p,PE_ITEM_WOOD,7));old=p.slots[0];
    CHECK(pe_inventory_sort(&p,2)&&memcmp(&old,&p.slots[0],sizeof old)==0&&p.slots[1].item_id==PE_ITEM_WOOD);
}
static void exact_transfer(void){
    PeInventory p={.categorized=true};PeSlot s={12,1,77,2};int index=-1;
    CHECK(pe_inventory_insert(&p,s,&index)&&index==420&&p.next_instance_id==78&&p.slots[index].durability==2);
    PeInventory before=p;CHECK(!pe_inventory_insert(&p,s,NULL)&&memcmp(&p,&before,sizeof p)==0);
    CHECK(pe_inventory_first_empty(&p,12)==421);s.instance_id=UINT32_MAX;CHECK(!pe_inventory_insert(&p,s,NULL));
    s.instance_id=78;s.durability=NAN;CHECK(!pe_inventory_insert(&p,s,NULL));
    p.next_instance_id=UINT32_MAX;before=p;CHECK(!pe_inventory_add(&p,12,1)&&memcmp(&p,&before,sizeof p)==0);
    CHECK(!pe_inventory_state_valid(NULL)&&pe_inventory_capacity(NULL)==0&&pe_inventory_count(NULL,1)==0);
}
static void game_identity(void){
    PeGame *g=calloc(1,sizeof *g);CHECK(g);if(!g)return;
    g->mode=PE_GAME_ADVENTURE;g->hp=100;g->inventory.categorized=true;
    CHECK(pe_inventory_add(&g->inventory,12,1)&&pe_inventory_add(&g->inventory,916,20));
    CHECK(pe_game_equip_slot(g,420)&&pe_game_quickslot_assign(g,0,420)&&pe_game_quickslot_assign(g,1,0));
    uint32_t id=g->equipped_instance_id;CHECK(pe_inventory_move(&g->inventory,420,839));
    CHECK(pe_game_equipped_tool(g)&&pe_game_equipped_tool(g)->instance_id==id&&g->quickslots[0].instance_id==id);
    CHECK(pe_inventory_split(&g->inventory,0,10,NULL)&&pe_inventory_sort(&g->inventory,0)&&g->quickslots[1].item_id==916&&g->quickslots[1].instance_id==0);
    CHECK(pe_inventory_sort(&g->inventory,1)&&pe_game_equipped_tool(g)->instance_id==id);
    free(g);
}
int pe_test_inventory(void){failures=0;category_capacity();migration();editing();sorting();exact_transfer();game_identity();return failures;}
