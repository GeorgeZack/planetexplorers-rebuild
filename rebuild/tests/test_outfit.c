#include "pe_game.h"
#include "pe_outfit.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int slot(const PeGame *g,int id){
    for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].item_id==id&&g->inventory.slots[i].count)return i;
    return -1;
}
static void reject_init(PeGame *g){
    unsigned char *before=malloc(sizeof *g);assert(before);memcpy(before,g,sizeof *g);
    assert(!pe_outfit_init(g)&&!memcmp(before,g,sizeof *g));free(before);
}
static void identities(void){
    PeGame *g=calloc(1,sizeof *g);assert(g);g->mode=PE_GAME_ADVENTURE;g->inventory.next_instance_id=5;
    g->inventory.slots[0]=(PeSlot){33,1,1,100};g->equipped_instance_id=1;
    g->quickslots[0]=(PeQuickslot){33,1};
    g->deployed_count=1;g->deployed[0]=(PeDeployable){.item_id=10,.instance_id=4,.durability=1200};
    PeInventory bag=g->inventory;
    assert(pe_outfit_init(g)&&pe_outfit_state_valid(g));
    assert(g->outfit.items[0].item_id==95&&g->outfit.items[1].item_id==131&&g->outfit.items[2].item_id==192);
    assert(g->outfit.items[0].durability==2156&&g->outfit.items[1].durability==2156&&g->outfit.items[2].durability==862);
    assert(g->inventory.next_instance_id==8&&g->equipped_instance_id==1);
    assert(!memcmp(g->inventory.slots,bag.slots,sizeof bag.slots));
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)assert(g->outfit.items[i].instance_id==5+i);
    reject_init(g);
    PeOutfit intact=g->outfit;
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++){
        uint32_t id=intact.items[i].instance_id;
        g->inventory.slots[3].instance_id=id;assert(!pe_outfit_state_valid(g));g->inventory.slots[3].instance_id=0;
        g->equipped_instance_id=id;assert(!pe_outfit_state_valid(g));g->equipped_instance_id=1;
        g->quickslots[8]=(PeQuickslot){intact.items[i].item_id,id};assert(!pe_outfit_state_valid(g));g->quickslots[8]=(PeQuickslot){0};
        g->deployed[63].instance_id=id;assert(!pe_outfit_state_valid(g));g->deployed[63].instance_id=0;
        g->outfit.items[i].instance_id=8;assert(!pe_outfit_state_valid(g));g->outfit=intact;
        g->outfit.items[i].durability=0;assert(pe_outfit_state_valid(g));g->outfit=intact;
    }
    g->outfit.items[1].instance_id=g->outfit.items[0].instance_id;assert(!pe_outfit_valid(&g->outfit));g->outfit=intact;
    g->outfit.items[0].item_id=113;assert(!pe_outfit_state_valid(g));g->outfit=intact;
    g->mode=PE_GAME_LEGACY;assert(!pe_outfit_state_valid(g));g->outfit=(PeOutfit){0};assert(pe_outfit_state_valid(g)&&pe_outfit_init(g));
    g->mode=PE_GAME_ADVENTURE;assert(!pe_outfit_state_valid(g));
    g->inventory.next_instance_id=0;reject_init(g);
    g->inventory.next_instance_id=4;reject_init(g); /* Deployed identity4 cannot be repaired. */
    g->inventory.next_instance_id=1;reject_init(g); /* Bag/held/quickslot identity1. */
    g->inventory.next_instance_id=8;g->quickslots[8].instance_id=8;reject_init(g);g->quickslots[8].instance_id=0;
    g->deployed[63].instance_id=8;reject_init(g);g->deployed[63].instance_id=0;
    g->equipped_instance_id=8;reject_init(g);g->equipped_instance_id=1;
    g->inventory.next_instance_id=UINT32_MAX-2;reject_init(g);
    g->inventory.next_instance_id=UINT32_MAX-PE_OUTFIT_SLOTS;
    assert(pe_outfit_init(g)&&g->inventory.next_instance_id==UINT32_MAX&&pe_outfit_state_valid(g));
    assert(g->outfit.items[2].instance_id==UINT32_MAX-1);
    free(g);
}
static void wear(void){
    PeGame *g=calloc(1,sizeof *g);assert(g);g->mode=PE_GAME_ADVENTURE;g->inventory.next_instance_id=1;
    assert(pe_outfit_init(g));PeOutfit intact=g->outfit;
    assert(pe_outfit_defense(&intact)==12&&pe_player_defaults.defense==0);
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)g->outfit.items[i].durability=(float)(10*(i+1));
    assert(pe_outfit_wear(&g->outfit,100)&&pe_outfit_defense(&g->outfit)==5);
    assert(g->outfit.items[0].durability==0&&g->outfit.items[1].durability==10&&g->outfit.items[2].durability==20);
    assert(pe_outfit_wear(&g->outfit,100)&&pe_outfit_defense(&g->outfit)==1);
    assert(pe_outfit_wear(&g->outfit,100)&&pe_outfit_defense(&g->outfit)==0&&pe_outfit_state_valid(g));
    PeOutfit exhausted=g->outfit;
    assert(pe_outfit_wear(&g->outfit,-350)&&!memcmp(&g->outfit,&exhausted,sizeof exhausted));
    g->outfit=intact;g->hp=1; /* Wear uses calculated damage, never remaining HP. */
    assert(pe_outfit_wear(&g->outfit,5000)&&g->outfit.items[0].durability==1656&&g->outfit.items[2].durability==362);
    assert(pe_outfit_wear(&g->outfit,FLT_MAX)&&pe_outfit_defense(&g->outfit)==0);
    for(unsigned k=0;k<4;k++){
        g->outfit=intact;
        if(k==0)g->outfit.items[2].durability=NAN;
        if(k==1)g->outfit.items[2].durability=INFINITY;
        if(k==2)g->outfit.items[2].durability=-1;
        if(k==3)g->outfit.items[2].durability=863;
        PeOutfit before=g->outfit;
        assert(!pe_outfit_valid(&g->outfit)&&isnan(pe_outfit_defense(&g->outfit)));
        assert(!pe_outfit_wear(&g->outfit,30)&&!memcmp(&before,&g->outfit,sizeof before));
    }
    g->outfit=intact;
    assert(!pe_outfit_wear(&g->outfit,NAN)&&!memcmp(&intact,&g->outfit,sizeof intact));
    assert(!pe_outfit_wear(&g->outfit,INFINITY)&&!memcmp(&intact,&g->outfit,sizeof intact));
    assert(pe_outfit_wear(&g->outfit,-FLT_MAX)&&!memcmp(&intact,&g->outfit,sizeof intact));
    PeOutfit zero={0};assert(pe_outfit_valid(&zero)&&pe_outfit_defense(&zero)==0&&pe_outfit_wear(&zero,100));
    g->outfit=zero;g->outfit.items[0]=intact.items[0];assert(!pe_outfit_valid(&g->outfit));
    assert(!pe_outfit_valid(NULL)&&!pe_outfit_wear(NULL,1)&&isnan(pe_outfit_defense(NULL)));
    assert(!pe_outfit_init(NULL)&&!pe_outfit_state_valid(NULL));free(g);
}
static void initialization_and_healing(void){
    PeGame *g=calloc(1,sizeof *g),*copy=calloc(1,sizeof *copy);assert(g&&copy);
    assert(pe_game_init_mode(g,87,PE_GAME_LEGACY)&&pe_outfit_state_valid(g)&&pe_outfit_defense(&g->outfit)==0);
    pe_game_destroy(g);assert(pe_game_init_mode(g,87,PE_GAME_ADVENTURE)&&pe_outfit_state_valid(g));
    int knife=slot(g,33);assert(knife>=0&&g->inventory.slots[knife].instance_id==1);
    uint32_t maximum=0;for(unsigned i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].instance_id>maximum)maximum=g->inventory.slots[i].instance_id;
    assert(g->outfit.items[0].instance_id==maximum+1&&g->inventory.next_instance_id==maximum+4);
    assert(pe_outfit_wear(&g->outfit,130));PeOutfit worn=g->outfit;
    assert(pe_inventory_add(&g->inventory,925,1));g->hp=10;
    assert(pe_game_use_slot(g,slot(g,925))&&g->hp==360&&!memcmp(&worn,&g->outfit,sizeof worn));
    assert(pe_inventory_add(&g->inventory,916,1));
    assert(pe_game_use_slot(g,slot(g,916))&&!memcmp(&worn,&g->outfit,sizeof worn));
    assert(pe_game_clone(copy,g)&&pe_outfit_state_valid(copy)&&!memcmp(&copy->outfit,&worn,sizeof worn));
    assert(pe_outfit_wear(&copy->outfit,100)&&!memcmp(&g->outfit,&worn,sizeof worn));
    assert(pe_inventory_add(&g->inventory,33,1)&&pe_outfit_state_valid(g));
    pe_game_destroy(g);pe_game_destroy(copy);free(g);free(copy);
}
int pe_test_outfit(void){identities();wear();initialization_and_healing();puts("starter outfit: exact defense/wear, independent identity, atomic validation and healing ownership passed");return 0;}
