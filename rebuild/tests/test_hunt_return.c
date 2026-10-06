#include "adventure_test_helpers.h"
#include "pe_adventure_hunt.h"
#include "pe_adventure_medicine.h"
#include <math.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hunt return:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static void at(PeGame*g,PeVec3 source){g->player=(PeVec3){source.x-g->world.origin_x,source.y-g->world.origin_y,source.z-g->world.origin_z};}
int pe_test_hunt_return(void){
    failures=0;PeGame*g=calloc(1,sizeof *g);if(!g)return 1;
    CHECK(pe_game_init_mode(g,54,PE_GAME_ADVENTURE));CHECK(pe_test_ack_welcome(g));CHECK(pe_game_arrival_complete(g));CHECK(pe_game_accept_supplies(g));g->annot_complete=true;
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};CHECK(pe_adventure_medicine_accept(g));CHECK(pe_inventory_add(&g->inventory,917,5));CHECK(pe_adventure_medicine_complete(g));
    /* Explicit state/empty-world fixture isolates the return transaction. It
     * supplies no runtime kill shortcut and establishes no route playability. */
    g->hunt_state=PE_HUNT_RETURN_PENDING;g->hunt_kills=4;CHECK(pe_adventure_hunt_reference(&g->hunt_destination));
    PeVec3 destination;CHECK(pe_adventure_hunt_return_reference(&destination));CHECK(!pe_adventure_hunt_return_reference(NULL));
    CHECK(destination.x==-266&&destination.y==25&&destination.z==136);
    CHECK(!pe_adventure_hunt_return_step(NULL)&&!pe_adventure_hunt_return_talk(NULL)&&!pe_adventure_hunt_return_advance(NULL,4105));
    at(g,destination);CHECK(!pe_adventure_hunt_return_step(g)); /* unsupported world */
    pe_world_destroy(&g->world);CHECK(pe_world_resize(&g->world,416,64,416));g->world.source=1;g->world.origin_x=-640;g->world.origin_z=64;
    at(g,destination);g->player.x+=5;CHECK(!pe_adventure_hunt_return_step(g));
    at(g,destination);g->player.x+=3;g->player.z+=4;
    CHECK(!pe_adventure_hunt_return_step(g)); /* Exact diagonal radius5 is outside. */
    g->player.z+=.01f;CHECK(!pe_adventure_hunt_return_step(g));
    at(g,destination);g->player.x+=5;
    g->player.x-=.01f;g->player.y=-.01f;CHECK(!pe_adventure_hunt_return_step(g));
    g->player.y=0;g->hp=0;CHECK(!pe_adventure_hunt_return_step(g));g->hp=g->max_hp;
    g->player.z=NAN;CHECK(!pe_adventure_hunt_return_step(g));at(g,destination);
    g->hunt_kills=3;CHECK(!pe_adventure_hunt_return_step(g)&&!pe_adventure_hunt_state_valid(g));g->hunt_kills=4;
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};
    /* Four source nonstackable scripts need four slots. Three spare slots must
     * roll back even though three earlier rewards could fit. */
    for(unsigned i=0;i<PE_INVENTORY_SLOTS-3;i++)CHECK(pe_inventory_add(&g->inventory,593,1));
    PeInventory before=g->inventory;uint32_t money=g->money,rng=g->rng_state;
    CHECK(!pe_adventure_hunt_return_step(g)&&g->hunt_state==PE_HUNT_RETURN_PENDING&&!memcmp(&g->inventory,&before,sizeof before));
    CHECK(g->money==money&&g->rng_state==rng&&!pe_adventure_hunt_return_talk(g));
    CHECK(pe_inventory_remove(&g->inventory,593,1));int parachutes=pe_inventory_count(&g->inventory,593);
    at(g,destination);g->player.x+=3;g->player.z+=3.99f;g->player.y=0;
    CHECK(pe_adventure_hunt_return_step(g)&&g->hunt_state==PE_HUNT_RETURN_TALK1&&g->hunt_kills==4);
    CHECK(pe_inventory_count(&g->inventory,592)==1&&pe_inventory_count(&g->inventory,593)==parachutes+1&&pe_inventory_count(&g->inventory,594)==1&&pe_inventory_count(&g->inventory,595)==1);
    CHECK(pe_adventure_hunt_return_talk(g)==4105&&g->money==money&&g->rng_state==rng);
    before=g->inventory;CHECK(!pe_adventure_hunt_return_step(g)&&!pe_adventure_hunt_return_advance(g,4106));
    CHECK(g->hunt_state==PE_HUNT_RETURN_TALK1&&!memcmp(&before,&g->inventory,sizeof before));
    CHECK(pe_adventure_hunt_return_advance(g,4105)&&pe_adventure_hunt_return_talk(g)==4106);
    CHECK(!pe_adventure_hunt_return_advance(g,4105));
    g->hp=0;CHECK(!pe_adventure_hunt_return_talk(g)&&!pe_adventure_hunt_return_advance(g,4106));g->hp=g->max_hp;
    CHECK(pe_adventure_hunt_return_advance(g,4106)&&g->hunt_state==PE_HUNT_COMPLETE&&!pe_adventure_hunt_return_talk(g));
    CHECK(!pe_adventure_hunt_return_advance(g,4106)&&!pe_adventure_hunt_return_step(g));
    CHECK(!memcmp(&before,&g->inventory,sizeof before)&&g->hunt_kills==4&&pe_adventure_hunt_state_valid(g));
    g->hunt_state=7;CHECK(!pe_adventure_hunt_state_valid(g));
    pe_game_destroy(g);free(g);return failures;
}
