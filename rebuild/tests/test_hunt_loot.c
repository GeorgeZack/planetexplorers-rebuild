#define _POSIX_C_SOURCE 200809L
#include "pe_hunt_loot.h"
#include "pe_settlement.h"
#include <assert.h>
#include <errno.h>
#include <sys/wait.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void add_drop(PeGame *g,int item,int count,PeVec3 position){
    assert(g->hunt_loot.count<PE_HUNT_LOOT_CAPACITY);
    g->hunt_loot.drops[g->hunt_loot.count++]=(PeHuntLootDrop){
        .instance_id=g->inventory.next_instance_id++,.item_id=item,.count=count,
        .position=position,.created_seconds=g->game_seconds};
}
static PeGame *fixture(void){
    PeGame *g=calloc(1,sizeof *g);char error[256];assert(g);
    assert(pe_game_init_mode(g,722,PE_GAME_ADVENTURE));
    if(!pe_settlement_start(g,error,sizeof error)){fprintf(stderr,"loot route fixture: %s\n",error);abort();}
    assert(pe_settlement_is_route(&g->world)&&pe_hunt_loot_valid(g));
    return g;
}
static void rolls(PeGame *g){
    /* Fixed independent vectors distinguish ordered independent draws from a
     * weighted-choice draw, inclusive35, and repeated items kept separately. */
    const struct {uint32_t seed,after;unsigned count;int item[5],quantity[5];} cases[]={
        {1,1495045943u,4,{229,1011,1008,1026},{30,1,3,1}},
        {2,52840766u,5,{229,1011,1026,236,1008},{25,1,2,1,1}},
        {7,1695803691u,5,{229,1742,236,1008,1228},{26,1,2,1,1}},
        {12345,519671626u,5,{229,1026,1742,236,1008},{32,1,1,2,1}},
        {0x9e3779b9u,242805386u,4,{229,236,1011,1008},{27,2,1,1}},
    };
    for(unsigned c=0;c<sizeof cases/sizeof *cases;c++){
        pe_hunt_loot_clear(g);g->rng_state=cases[c].seed;
        uint32_t first=g->inventory.next_instance_id;PeInventory bag=g->inventory;
        assert(pe_hunt_loot_spawn(g,&g->player,1));
        assert(g->hunt_loot.count==cases[c].count&&g->rng_state==cases[c].after);
        assert(g->inventory.next_instance_id==first+cases[c].count);
        bag.next_instance_id=g->inventory.next_instance_id;assert(!memcmp(&bag,&g->inventory,sizeof bag));
        for(unsigned i=0;i<cases[c].count;i++){
            const PeHuntLootDrop *d=&g->hunt_loot.drops[i];
            assert(d->instance_id==first+i&&d->item_id==cases[c].item[i]&&d->count==cases[c].quantity[i]);
            assert(!memcmp(&d->position,&g->player,sizeof d->position)&&d->created_seconds==g->game_seconds);
        }
        assert(pe_hunt_loot_valid(g));
    }
    const uint32_t edge_seeds[]={13,8};const int edge_meat[]={22,34};
    for(unsigned i=0;i<2;i++){
        pe_hunt_loot_clear(g);g->rng_state=edge_seeds[i];
        assert(pe_hunt_loot_spawn(g,&g->player,1)&&g->hunt_loot.drops[0].item_id==229&&g->hunt_loot.drops[0].count==edge_meat[i]);
    }
    /* A batch and the same ordered deaths split across calls agree exactly;
     * same-item stacks retain their individual corpse locations and IDs. */
    PeGame *other=calloc(1,sizeof *other);assert(other);pe_hunt_loot_clear(g);g->rng_state=1;
    assert(pe_game_clone(other,g));PeVec3 p[3]={g->player,g->player,g->player};p[1].x+=1;p[2].z+=2;
    assert(pe_hunt_loot_spawn(g,p,3));for(unsigned i=0;i<3;i++)assert(pe_hunt_loot_spawn(other,p+i,1));
    assert(!memcmp(&g->hunt_loot,&other->hunt_loot,sizeof g->hunt_loot)&&g->rng_state==other->rng_state&&g->inventory.next_instance_id==other->inventory.next_instance_id);
    unsigned meat=0;for(unsigned i=0;i<g->hunt_loot.count;i++)meat+=g->hunt_loot.drops[i].item_id==229;assert(meat==3);
    pe_game_destroy(other);free(other);
}
static void rejection(PeGame *g){
    PeGame *before=malloc(sizeof *before);assert(before);PeVec3 p[2]={g->player,g->player};
    pe_hunt_loot_clear(g);g->rng_state=1;
    p[1].x=NAN;memcpy(before,g,sizeof *g);
    assert(!pe_hunt_loot_spawn(g,p,2)&&!memcmp(before,g,sizeof *g));
    p[1]=g->player;p[1].z=(float)pe_world_depth(&g->world);
    assert(!pe_hunt_loot_spawn(g,p,2)&&!memcmp(before,g,sizeof *g));
    assert(!pe_hunt_loot_spawn(g,NULL,1)&&!memcmp(before,g,sizeof *g));
    assert(!pe_hunt_loot_spawn(g,p,7)&&!memcmp(before,g,sizeof *g));
    for(unsigned i=0;i<PE_HUNT_LOOT_CAPACITY;i++)add_drop(g,229,22,g->player);
    assert(pe_hunt_loot_valid(g));memcpy(before,g,sizeof *g);
    assert(!pe_hunt_loot_spawn(g,p,1)&&!memcmp(before,g,sizeof *g));
    pe_hunt_loot_clear(g);p[1]=g->player;g->rng_state=1;
    for(unsigned i=0;i<60;i++)add_drop(g,229,22,g->player);
    memcpy(before,g,sizeof *g);
    assert(!pe_hunt_loot_spawn(g,p,2)&&!memcmp(before,g,sizeof *g)); /* second death exhausts capacity */
    pe_hunt_loot_clear(g);g->inventory.next_instance_id=UINT32_MAX;memcpy(before,g,sizeof *g);
    assert(!pe_hunt_loot_spawn(g,p,1)&&!memcmp(before,g,sizeof *g));
    g->inventory.next_instance_id=UINT32_MAX-2;memcpy(before,g,sizeof *g);
    assert(!pe_hunt_loot_spawn(g,p,1)&&!memcmp(before,g,sizeof *g)); /* late allocator overflow */
    g->inventory.next_instance_id=1000;add_drop(g,229,22,g->player);
    uint32_t id=g->hunt_loot.drops[0].instance_id;
    uint32_t *owners[]={&g->equipped_instance_id,&g->shield_instance_id,&g->quickslots[8].instance_id,
        &g->outfit.items[0].instance_id,&g->repair.item.instance_id,&g->deployed[0].instance_id,
        &g->town_buyback[0].slots[0].instance_id,&g->inventory.slots[1679].instance_id};
    for(unsigned i=0;i<sizeof owners/sizeof *owners;i++){
        uint32_t old=*owners[i];*owners[i]=id;assert(!pe_hunt_loot_valid(g));
        memcpy(before,g,sizeof *g);unsigned picked=41,expired=42;
        assert(!pe_hunt_loot_update(g,&picked,&expired)&&picked==41&&expired==42&&!memcmp(before,g,sizeof *g));
        *owners[i]=old;assert(pe_hunt_loot_valid(g));
    }
    pe_hunt_loot_clear(g);assert(pe_hunt_loot_valid(g));free(before);
}
static void pickup(PeGame *g){
    pe_hunt_loot_clear(g);g->game_seconds=100;g->hp=g->max_hp;
    PeVec3 origin=g->player,point=origin;point.y+=4;add_drop(g,229,22,point);
    int old=pe_inventory_count(&g->inventory,229);uint32_t counter=g->inventory.next_instance_id,rng=g->rng_state;
    unsigned picked=88,expired=99;
    assert(pe_hunt_loot_update(g,&picked,&expired)&&picked==0&&expired==0&&g->hunt_loot.count==1);
    g->hunt_loot.drops[0].position.y=nextafterf(point.y,origin.y);
    g->hp=0;assert(pe_hunt_loot_update(g,&picked,&expired)&&!picked&&g->hunt_loot.count==1);
    g->hp=g->max_hp;assert(pe_hunt_loot_update(g,&picked,&expired)&&picked==1&&!expired&&!g->hunt_loot.count);
    assert(pe_inventory_count(&g->inventory,229)==old+22&&g->inventory.next_instance_id==counter&&g->rng_state==rng);
    assert(pe_hunt_loot_valid(g));
    point=origin;point.x+=8;add_drop(g,229,22,point);g->hp=0;
    g->game_seconds=93700;assert(pe_hunt_loot_update(g,&picked,&expired)&&!picked&&!expired&&g->hunt_loot.count==1);
    g->game_seconds=nextafter(93700,INFINITY);
    assert(pe_hunt_loot_update(g,&picked,&expired)&&!picked&&expired==1&&!g->hunt_loot.count);
    add_drop(g,229,22,origin);g->game_seconds+=93601;g->hp=g->max_hp;
    assert(pe_hunt_loot_update(g,&picked,&expired)&&!picked&&expired==1&&pe_inventory_count(&g->inventory,229)==old+22);
    /* One uncollectable stack must not block another stack that can merge.
     * No partial stack debit or alias to the old world-item ID is allowed. */
    int start=pe_inventory_category_start(&g->inventory,pe_item_def(229)->tab);
    for(int i=start;i<start+PE_INVENTORY_CATEGORY_SLOTS;i++)g->inventory.slots[i]=(PeSlot){.item_id=279,.count=pe_item_def(279)->stack_limit};
    g->inventory.slots[start]=(PeSlot){.item_id=229,.count=9990};
    add_drop(g,229,20,origin);add_drop(g,229,5,origin);uint32_t retained=g->hunt_loot.drops[0].instance_id;
    assert(pe_hunt_loot_update(g,&picked,&expired)&&picked==1&&!expired&&g->hunt_loot.count==1);
    assert(g->hunt_loot.drops[0].instance_id==retained&&g->hunt_loot.drops[0].count==20&&g->inventory.slots[start].count==9995&&!g->inventory.slots[start].instance_id);
    assert(pe_hunt_loot_update(g,&picked,&expired)&&!picked&&g->hunt_loot.count==1);
    g->inventory.slots[start+1]=(PeSlot){0};
    assert(pe_hunt_loot_update(g,&picked,&expired)&&picked==1&&!g->hunt_loot.count&&pe_inventory_count(&g->inventory,229)==10015);
    assert(g->inventory.slots[start].count==9999&&g->inventory.slots[start+1].count==16&&!g->inventory.slots[start+1].instance_id&&pe_hunt_loot_valid(g));
}
static int loot_cases(void){
    PeGame *g=fixture();rolls(g);rejection(g);pickup(g);pe_game_destroy(g);free(g);
    puts("hunt loot: deterministic trials, atomic allocation, identity rejection, independent pickup and expiry pass");return 0;
}
/* Installed settlement geometry is process-immutable. Isolate its caches so
 * later missing/custom-asset tests can still configure their own roots.
 * Standalone sanitizer main bypasses fork for complete teardown/leak scanning. */
int pe_test_hunt_loot(void){
    fflush(NULL);pid_t pid=fork();if(pid<0)return 1;
    if(!pid)exit(loot_cases()?1:0);
    int status=0;pid_t waited;
    do{waited=waitpid(pid,&status,0);}while(waited<0&&errno==EINTR);
    return waited!=pid||!WIFEXITED(status)||WEXITSTATUS(status)!=0;
}
#ifdef PE_HUNT_LOOT_TEST_MAIN
int main(void){return loot_cases();}
#endif
