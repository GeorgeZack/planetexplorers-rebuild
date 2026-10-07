#include "pe_camp_repair.h"
#include "../generated/pe_camp_repair_source.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"camp repair:%d %s\n",__LINE__,#x);failures++;}}while(0)
static bool unbound(const void *context,uint32_t id){return id!=*(const uint32_t *)context;}
static int find(const PeInventory *p,int id){for(int i=0;i<pe_inventory_capacity(p);i++)if(p->slots[i].item_id==id)return i;return -1;}
static bool unchanged(const PeCampRepair *r,const PeCampRepair *old,const PeInventory *p,const PeInventory *bag){return !memcmp(r,old,sizeof *r)&&!memcmp(p,bag,sizeof *p);}
int pe_test_camp_repair(void){
    failures=0;
    PeInventory bag={.categorized=true};uint32_t denied=0;
    PeCampRepairAccess access={&bag,unbound,&denied};PeCampRepair repair={0};
    CHECK(pe_camp_repair_valid(&repair,&access));
    /* These exclusions have empty material lists or unsupported LifeLimit,
     * creation, energy-only and non-instance ownership components. */
    const int unsupported[]={10,33,67,1527,365,370,1473,1474,1475,228,279,925};
    for(unsigned i=0;i<sizeof unsupported/sizeof *unsupported;i++){
        PeSlot item={unsupported[i],1,1,0};PeCampRepairCost quote={.count=7},before=quote;
        CHECK(!pe_camp_repair_item_supported(&item));CHECK(!pe_camp_repair_quote(&item,&quote));CHECK(!memcmp(&quote,&before,sizeof quote));
    }
    CHECK(!pe_camp_repair_item_supported(NULL));
    unsigned supported=0;
    for(unsigned i=0;i<PE_CAMP_REPAIR_SOURCE_ITEM_COUNT;i++){
        const PeCampRepairItemSource *source=&pe_camp_repair_items[i];PeSlot item={source->item_id,1,1,0};PeCampRepairCost quote;
        if(!pe_camp_repair_item_supported(&item))continue;
        supported++;CHECK(pe_camp_repair_quote(&item,&quote));CHECK(quote.count==source->material_count);
        for(unsigned j=0;j<quote.count;j++)CHECK(quote.materials[j].item_id==source->materials[j].item_id&&quote.materials[j].count==source->materials[j].count);
        item.durability=source->durability_max;CHECK(pe_camp_repair_quote(&item,&quote));
        for(unsigned j=0;j<quote.count;j++)CHECK(quote.materials[j].count==0);
    }
    CHECK(supported==63); /* Current core supports these raw, unstrengthened values. */
    CHECK(pe_inventory_add(&bag,1,1));int index=find(&bag,1);CHECK(index>=0);
    bag.slots[index].durability=2369*.5f;PeSlot sword=bag.slots[index];PeCampRepairCost quote;
    CHECK(pe_camp_repair_quote(&sword,&quote)&&quote.count==1&&quote.materials[0].item_id==279&&quote.materials[0].count==2);
    PeSlot tiny=sword;tiny.durability=nextafterf(2369,0);CHECK(pe_camp_repair_quote(&tiny,&quote)&&quote.materials[0].count==1);
    PeSlot bad=sword;bad.durability=NAN;CHECK(!pe_camp_repair_item_supported(&bad));bad=sword;bad.durability=-1;CHECK(!pe_camp_repair_item_supported(&bad));bad=sword;bad.durability=2370;CHECK(!pe_camp_repair_item_supported(&bad));bad=sword;bad.count=2;CHECK(!pe_camp_repair_item_supported(&bad));bad=sword;bad.instance_id=0;CHECK(!pe_camp_repair_item_supported(&bad));
    PeInventory previous=bag;PeCampRepair old=repair;
    denied=sword.instance_id;CHECK(!pe_camp_repair_insert(&repair,&access,index,&sword)&&unchanged(&repair,&old,&bag,&previous));denied=0;
    PeSlot stale=sword;stale.durability--;
    CHECK(!pe_camp_repair_insert(&repair,&access,index,&stale)&&unchanged(&repair,&old,&bag,&previous));
    PeCampRepairAccess no_guard={.inventory=&bag};CHECK(!pe_camp_repair_insert(&repair,&no_guard,index,&sword));
    CHECK(pe_camp_repair_insert(&repair,&access,index,&sword));CHECK(!pe_inventory_count(&bag,1)&&repair.item.instance_id==sword.instance_id&&repair.item.durability==sword.durability&&pe_camp_repair_valid(&repair,&access));
    CHECK(pe_inventory_add(&bag,279,1));previous=bag;old=repair;
    CHECK(!pe_camp_repair_start(&repair,&access)&&unchanged(&repair,&old,&bag,&previous));
    CHECK(pe_inventory_add(&bag,279,3));CHECK(pe_camp_repair_start(&repair,&access));CHECK(pe_inventory_count(&bag,279)==2&&repair.running&&repair.remaining_ticks==6000000&&pe_camp_repair_seconds(&repair)==60);
    previous=bag;old=repair;int destination=-8;
    CHECK(!pe_camp_repair_start(&repair,&access)&&!pe_camp_repair_take(&repair,&access,&destination)&&destination==-8&&unchanged(&repair,&old,&bag,&previous));
    CHECK(pe_inventory_add(&bag,34,1));int shield_index=find(&bag,34);PeSlot shield=bag.slots[shield_index];old=repair;previous=bag;
    CHECK(!pe_camp_repair_insert(&repair,&access,shield_index,&shield)&&unchanged(&repair,&old,&bag,&previous));
    bool completed=true;
    CHECK(pe_camp_repair_update(&repair,&access,0,true,&completed)&&!completed&&repair.remaining_ticks==6000000);
    CHECK(pe_camp_repair_update(&repair,&access,.000001f,true,&completed)&&repair.remaining_ticks==6000000);
    CHECK(pe_camp_repair_update(&repair,&access,.000001f,true,&completed)&&repair.remaining_ticks==6000000);
    CHECK(pe_camp_repair_update(&repair,&access,60,false,&completed)&&!completed&&repair.remaining_ticks==6000000);
    const float invalid_dt[]={-1,NAN,INFINITY,FLT_MAX,0x1p63f/100000};
    for(unsigned i=0;i<sizeof invalid_dt/sizeof *invalid_dt;i++){
        old=repair;previous=bag;completed=true;
        CHECK(!pe_camp_repair_update(&repair,&access,invalid_dt[i],true,&completed)&&completed&&unchanged(&repair,&old,&bag,&previous));
    }
    CHECK(pe_camp_repair_update(&repair,&access,7.25f,true,&completed)&&repair.remaining_ticks==5275000&&!completed);
    CHECK(pe_camp_repair_cancel(&repair,&access)&&!repair.running&&!repair.remaining_ticks&&repair.item.durability==sword.durability&&pe_inventory_count(&bag,279)==2);
    CHECK(!pe_camp_repair_cancel(&repair,&access));
    CHECK(pe_camp_repair_start(&repair,&access)&&!pe_inventory_count(&bag,279));
    /* Sixty seconds of float60Hz frames lose .024s to per-frame truncation. */
    for(int i=0;i<3600;i++)CHECK(pe_camp_repair_update(&repair,&access,1.0f/60.0f,true,&completed)&&!completed);
    CHECK(repair.remaining_ticks==2400&&repair.item.durability==sword.durability);
    /* Restore the detached value with an independently restored matching bag. */
    PeCampRepair snapshot=repair,restored={0};CHECK(pe_camp_repair_restore(&restored,&snapshot,&access));CHECK(!memcmp(&snapshot,&restored,sizeof snapshot));
    old=restored;CHECK(!pe_camp_repair_restore(&restored,&snapshot,&access)&&!memcmp(&old,&restored,sizeof old));
    PeCampRepair empty={0},corrupt=snapshot;
    corrupt.remaining_ticks=-1;CHECK(!pe_camp_repair_restore(&empty,&corrupt,&access)&&!empty.item.count);
    corrupt=snapshot;corrupt.remaining_ticks=6000001;CHECK(!pe_camp_repair_restore(&empty,&corrupt,&access));
    corrupt=snapshot;corrupt.running=false;CHECK(!pe_camp_repair_restore(&empty,&corrupt,&access));
    corrupt=snapshot;corrupt.item.durability=2369;CHECK(!pe_camp_repair_restore(&empty,&corrupt,&access));
    corrupt=snapshot;corrupt.item.durability=NAN;CHECK(!pe_camp_repair_restore(&empty,&corrupt,&access));
    denied=sword.instance_id;CHECK(!pe_camp_repair_restore(&empty,&snapshot,&access));old=repair;completed=true;
    CHECK(!pe_camp_repair_update(&repair,&access,1,true,&completed)&&completed&&!memcmp(&old,&repair,sizeof old));denied=0;
    PeInventory collision=bag;CHECK(pe_inventory_insert(&collision,snapshot.item,NULL));PeCampRepairAccess conflict={&collision,unbound,&denied};
    CHECK(!pe_camp_repair_restore(&empty,&snapshot,&conflict));
    collision=bag;collision.next_instance_id=sword.instance_id;CHECK(!pe_camp_repair_restore(&empty,&snapshot,&conflict));
    CHECK(pe_camp_repair_update(&repair,&access,.024f,true,&completed)&&completed&&!repair.running&&!repair.remaining_ticks&&repair.item.durability==2369);
    old=repair;CHECK(pe_camp_repair_update(&repair,&access,100,true,&completed)&&!completed&&!memcmp(&old,&repair,sizeof old));CHECK(!pe_camp_repair_start(&repair,&access));
    CHECK(pe_camp_repair_take(&repair,&access,&destination)&&destination>=0&&!repair.item.count);CHECK(bag.slots[destination].instance_id==sword.instance_id&&bag.slots[destination].durability==2369);
    /* Fully broken instances remain repairable; same-category swap succeeds
     * even when there is no empty equipment slot. Take-back must roll back. */
    bag=(PeInventory){.categorized=true};repair=(PeCampRepair){0};CHECK(pe_inventory_add(&bag,1,1));index=find(&bag,1);bag.slots[index].durability=0;sword=bag.slots[index];
    CHECK(pe_camp_repair_insert(&repair,&access,index,&sword));CHECK(pe_inventory_add(&bag,34,PE_INVENTORY_CATEGORY_SLOTS));shield_index=find(&bag,34);shield=bag.slots[shield_index];
    old=repair;previous=bag;destination=-8;CHECK(!pe_camp_repair_take(&repair,&access,&destination)&&destination==-8&&unchanged(&repair,&old,&bag,&previous));
    CHECK(pe_camp_repair_insert(&repair,&access,shield_index,&shield)&&repair.item.instance_id==shield.instance_id&&bag.slots[shield_index].instance_id==sword.instance_id&&bag.slots[shield_index].durability==0);
    CHECK(pe_camp_repair_insert(&repair,&access,shield_index,&sword)&&repair.item.instance_id==sword.instance_id);
    CHECK(pe_inventory_add(&bag,279,4));CHECK(pe_camp_repair_start(&repair,&access)&&!pe_inventory_count(&bag,279));CHECK(pe_camp_repair_update(&repair,&access,60,true,&completed)&&completed&&repair.item.durability==2369);
    old=repair;previous=bag;CHECK(!pe_camp_repair_take(&repair,&access,NULL)&&unchanged(&repair,&old,&bag,&previous));
    CHECK(pe_inventory_remove(&bag,34,1));CHECK(pe_camp_repair_take(&repair,&access,NULL)&&pe_inventory_state_valid(&bag));
    return failures;
}
#ifdef PE_CAMP_REPAIR_TEST_MAIN
int main(void){return pe_test_camp_repair();}
#endif
