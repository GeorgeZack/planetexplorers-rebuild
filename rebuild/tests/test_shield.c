#include "pe_shield.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"shield:%d %s\n",__LINE__,#c);failures++;}}while(0)
static int slot(const PeGame*g,int item){for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].count&&g->inventory.slots[i].item_id==item)return i;return -1;}
int pe_test_shield(void){
    failures=0;PeGame*g=calloc(1,sizeof *g);unsigned char*snapshot=malloc(sizeof *g);CHECK(g&&snapshot);if(!g||!snapshot)goto done;
    CHECK(pe_game_init_mode(g,104,PE_GAME_ADVENTURE));CHECK(!g->shield_instance_id&&pe_shield_state_valid(g));
    CHECK(pe_inventory_add(&g->inventory,1,1)&&pe_inventory_add(&g->inventory,34,2));int sword=slot(g,1),shield=slot(g,34);CHECK(sword>=0&&shield>=0);if(sword<0||shield<0)goto done;
    CHECK(pe_game_equip_slot(g,sword));uint32_t primary=g->equipped_instance_id;
    CHECK(pe_game_quickslot_assign(g,0,shield));PeInventory before=g->inventory;
    CHECK(!pe_game_use_slot(g,shield)); /* Public UI route stays disabled until visuals qualify. */
    CHECK(pe_shield_equip(g,shield)&&pe_shield_equipped(g)==&g->inventory.slots[shield]);
    CHECK(g->equipped_instance_id==primary&&!memcmp(&before,&g->inventory,sizeof before));
    CHECK(pe_shield_defense(pe_shield_equipped(g))==9&&fabsf(pe_shield_protection(pe_shield_equipped(g))-.22f)<1e-7f);
    CHECK(pe_shield_defense(NULL)==0&&pe_shield_protection(NULL)==0);
    uint32_t identity=g->shield_instance_id;CHECK(pe_shield_equip(g,shield)&&g->shield_instance_id==identity); /* Idempotent reference selection. */
    while(pe_inventory_add(&g->inventory,19,1)){}before=g->inventory;
    CHECK(pe_shield_unequip(g)&&!g->shield_instance_id&&pe_shield_equip(g,shield));
    CHECK(!memcmp(&before,&g->inventory,sizeof before)&&g->equipped_instance_id==primary); /* Full bag never needs a transfer. */
    int second=-1;for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(i!=shield&&g->inventory.slots[i].item_id==34){second=i;break;}
    CHECK(second>=0);if(second<0)goto done;
    g->inventory.slots[second].durability=1234.5f;before=g->inventory;
    CHECK(pe_shield_equip(g,second)&&pe_shield_equipped(g)->durability==1234.5f&&!memcmp(&before,&g->inventory,sizeof before));
    CHECK(pe_shield_equip(g,shield));PeSlot staged=*pe_shield_equipped(g);
    CHECK(pe_shield_wear(&staged,250)&&staged.durability==4287&&pe_shield_equipped(g)->durability==4312); /* No mutation before transaction commit. */
    g->inventory.slots[shield]=staged;CHECK(pe_shield_equipped(g)->durability==4287);
    CHECK(pe_shield_wear(&g->inventory.slots[shield],50000)&&pe_shield_equipped(g)->durability==0);
    CHECK(g->shield_instance_id==identity&&pe_shield_defense(pe_shield_equipped(g))==0&&pe_shield_protection(pe_shield_equipped(g))==0);
    CHECK(pe_shield_unequip(g)&&pe_shield_equip(g,shield)); /* Exhausted ownership is valid; effects stay absent. */
    staged=g->inventory.slots[shield];CHECK(pe_shield_wear(&staged,-100)&&staged.durability==0);
    const float bad[]={NAN,INFINITY,-INFINITY};
    for(unsigned i=0;i<3;i++){PeSlot old=staged;CHECK(!pe_shield_wear(&staged,bad[i])&&!memcmp(&staged,&old,sizeof old));}
    for(unsigned i=0;i<8;i++){
        PeSlot malformed=staged;
        if(i==0)malformed.durability=NAN;
        if(i==1)malformed.durability=INFINITY;
        if(i==2)malformed.durability=-.01f;
        if(i==3)malformed.durability=4312.5f;
        if(i==4)malformed.instance_id=0;
        if(i==5)malformed.count=0;
        if(i==6)malformed.count=2;
        if(i==7)malformed.item_id=1;
        PeSlot old=malformed;
        CHECK(!pe_shield_slot_valid(&malformed)&&isnan(pe_shield_defense(&malformed))&&isnan(pe_shield_protection(&malformed)));
        CHECK(!pe_shield_wear(&malformed,100)&&!memcmp(&malformed,&old,sizeof old));
    }
    /* Invalid selection never partially clears/replaces the valid reference. */
    const int invalid[]={-1,PE_INVENTORY_SLOTS,sword};
    for(unsigned i=0;i<3;i++){memcpy(snapshot,g,sizeof *g);CHECK(!pe_shield_equip(g,invalid[i])&&!memcmp(snapshot,g,sizeof *g));}
    g->hp=0;memcpy(snapshot,g,sizeof *g);CHECK(!pe_shield_equip(g,second)&&!memcmp(snapshot,g,sizeof *g));g->hp=500;
    uint32_t old=g->inventory.next_instance_id;g->inventory.next_instance_id=identity;CHECK(!pe_shield_state_valid(g));g->inventory.next_instance_id=old;
    g->equipped_instance_id=identity;CHECK(!pe_shield_state_valid(g));g->equipped_instance_id=primary;
    old=g->outfit.items[0].instance_id;g->outfit.items[0].instance_id=identity;CHECK(!pe_shield_state_valid(g));g->outfit.items[0].instance_id=old;
    g->deployed[0].instance_id=identity;CHECK(!pe_shield_state_valid(g));g->deployed[0].instance_id=0;
    PeQuickslot q=g->quickslots[0];g->quickslots[0].item_id=1;CHECK(!pe_shield_state_valid(g));g->quickslots[0]=q;CHECK(pe_shield_state_valid(g));
    old=g->inventory.slots[second].instance_id;g->inventory.slots[second].instance_id=identity;CHECK(!pe_shield_state_valid(g));g->inventory.slots[second].instance_id=old;
    g->mode=PE_GAME_LEGACY;CHECK(!pe_shield_state_valid(g)&&!pe_shield_equip(g,second));g->mode=PE_GAME_ADVENTURE;
    /* Consumption/removal clears only the missing reference, not primary gear.
     * An existing malformed slot is not silently repaired by cleanup. */
    g->inventory.slots[shield].durability=NAN;pe_shield_clear_missing(g);CHECK(g->shield_instance_id==identity);
    g->inventory.slots[shield].durability=0;CHECK(pe_inventory_remove(&g->inventory,34,1));pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    CHECK(!g->shield_instance_id&&g->equipped_instance_id==primary&&pe_shield_state_valid(g));
 done:pe_game_destroy(g);free(g);free(snapshot);return failures;
}
