#include "adventure_test_helpers.h"
#include "pe_adventure_hunt.h"
#include "pe_adventure_medicine.h"
#include "pe_settlement.h"
#include <math.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT "."
#endif
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hunt:%d %s\n",__LINE__,#c);failures++;}}while(0)
static int hunt_tests(void){
    failures=0;
    PeGame *g=calloc(1,sizeof *g);PeWorld *world=calloc(1,sizeof *world);char error[256];
    if(!g||!world){free(g);free(world);return 1;}
    CHECK(!pe_adventure_hunt_state_valid(NULL));CHECK(!pe_adventure_hunt_available(NULL));CHECK(!pe_adventure_hunt_accept(NULL));CHECK(!pe_adventure_hunt_step(NULL));
    CHECK(pe_game_init_mode(g,7,PE_GAME_ADVENTURE));CHECK(pe_adventure_hunt_state_valid(g));CHECK(!pe_adventure_hunt_available(g));
    CHECK(pe_test_ack_welcome(g));CHECK(pe_game_arrival_complete(g));CHECK(pe_game_accept_supplies(g));g->annot_complete=true;
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id,.categorized=g->mode==PE_GAME_ADVENTURE};CHECK(pe_adventure_medicine_accept(g));CHECK(pe_inventory_add(&g->inventory,917,5));CHECK(pe_adventure_medicine_complete(g));
    CHECK(!pe_adventure_hunt_available(g));g->hunt_destination.x=1;CHECK(!pe_adventure_hunt_state_valid(g));g->hunt_destination.x=0;
    g->hunt_state=7;CHECK(!pe_adventure_hunt_state_valid(g));g->hunt_state=PE_HUNT_NONE;
    g->hunt_destination.y=NAN;CHECK(!pe_adventure_hunt_state_valid(g));g->hunt_destination.y=0;
    CHECK(!pe_adventure_hunt_reference(NULL));
    PeVec3 reference;
    if(!pe_adventure_hunt_reference(&reference)){
        PeInventory before=g->inventory;CHECK(!pe_adventure_hunt_accept(g));
        CHECK(g->hunt_state==PE_HUNT_NONE&&!memcmp(&before,&g->inventory,sizeof before));
        fprintf(stderr,"hunt: observed destination absent; positive arrival integration not tested\n");goto done;
    }
    /* Stored progression is independent of the currently imported world. */
    g->hunt_state=PE_HUNT_TRAVEL;g->hunt_destination=reference;CHECK(pe_adventure_hunt_state_valid(g));
    CHECK(!pe_adventure_hunt_step(g));g->hunt_destination.x+=1;CHECK(!pe_adventure_hunt_state_valid(g));g->hunt_destination=reference;
    g->medicine_state=PE_MEDICINE_ACTIVE;CHECK(!pe_adventure_hunt_state_valid(g));g->medicine_state=PE_MEDICINE_COMPLETE;
    g->hunt_state=PE_HUNT_NONE;g->hunt_destination=(PeVec3){0};
    const char *path=PE_SOURCE_ROOT "/rebuild/build/reference-assets/adventure-route/terrain.pevx";
    FILE *asset=fopen(path,"rb");if(!asset){fprintf(stderr,"hunt: route assets absent; positive arrival integration not tested\n");goto done;}fclose(asset);
    CHECK(pe_settlement_load_world(path,world,error,sizeof error));CHECK(pe_game_set_world(g,world,(PeVec3){34,28,284}));
    if(!pe_adventure_hunt_available(g)){
        fprintf(stderr,"hunt: observed destination outside route; acceptance correctly unavailable\n");
        CHECK(!pe_adventure_hunt_accept(g));CHECK(g->hunt_state==PE_HUNT_NONE);goto done;
    }
    g->hp=0;CHECK(!pe_adventure_hunt_available(g));CHECK(!pe_adventure_hunt_accept(g));g->hp=g->max_hp;
    PeInventory before=g->inventory;uint32_t money=g->money,rng=g->rng_state;
    bool accepted=pe_adventure_hunt_accept(g);if(!accepted)fprintf(stderr,"hunt acceptance: %s\n",g->message);CHECK(accepted);
    if(!accepted)goto done;
    CHECK(g->hunt_state==PE_HUNT_TRAVEL&&pe_adventure_hunt_state_valid(g));CHECK(!pe_adventure_hunt_accept(g));
    CHECK(!memcmp(&before,&g->inventory,sizeof before)&&g->money==money&&g->rng_state==rng);
    g->player=(PeVec3){reference.x-g->world.origin_x+15,reference.y-g->world.origin_y,reference.z-g->world.origin_z};
    CHECK(!pe_adventure_hunt_step(g));g->player.x-=.01f;g->player.y=-1-g->world.origin_y;CHECK(!pe_adventure_hunt_step(g));
    g->player.y=reference.y-g->world.origin_y;g->hp=NAN;CHECK(!pe_adventure_hunt_step(g));g->hp=g->max_hp;
    CHECK(pe_adventure_hunt_step(g));CHECK(g->hunt_state==PE_HUNT_ENCOUNTER_PENDING);CHECK(!pe_adventure_hunt_step(g));CHECK(!pe_adventure_hunt_accept(g));
    CHECK(!memcmp(&before,&g->inventory,sizeof before)&&g->money==money&&g->rng_state==rng);
    CHECK(g->medicine_state==PE_MEDICINE_COMPLETE&&g->adventure_mission==PE_ADVENTURE_SUPPLIES);
 done:
    pe_game_destroy(g);pe_world_destroy(world);free(g);free(world);return failures;
}

int pe_test_adventure_hunt(void){
    /* Real-route acceptance loads immutable process-global settlement geometry.
     * Isolate it so later synthetic asset-root tests do not inherit the cache. */
    pid_t pid=fork();
    if(pid<0){perror("hunt test fork");return 1;}
    if(pid==0)_exit(hunt_tests()?1:0);
    int status=0;pid_t waited;
    do{waited=waitpid(pid,&status,0);}while(waited<0&&errno==EINTR);
    if(waited!=pid){perror("hunt test waitpid");return 1;}
    if(!WIFEXITED(status)){fprintf(stderr,"hunt: test process terminated abnormally\n");return 1;}
    return WEXITSTATUS(status)!=0;
}
