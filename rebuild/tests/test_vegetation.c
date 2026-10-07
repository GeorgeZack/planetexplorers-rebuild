#include "pe_vegetation.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"vegetation:%d %s\n",__LINE__,#c);failures++;}}while(0)
static void fresh(PeGame *g){
    pe_game_destroy(g);CHECK(pe_game_init_mode(g,832,PE_GAME_ADVENTURE));
    for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++)
        pe_world_set(&g->world,x,y,z,(PeVoxel){y<10?255:0,y<10?PE_MAT_DIRT:PE_MAT_AIR});
    g->player=(PeVec3){32,9.6f,32};g->velocity=(PeVec3){0};g->block_count=0;g->cell_count=0;g->tree_count=0;
    memset(g->blocks,0,sizeof g->blocks);memset(g->cells,0,sizeof g->cells);memset(g->trees,0,sizeof g->trees);
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};g->equipped_instance_id=0;g->shield_instance_id=0;
    memset(g->quickslots,0,sizeof g->quickslots);memset(&g->craft,0,sizeof g->craft);
}
static bool unchanged(const PeGame *g,const PeGame *before){
    size_t off=offsetof(PeGame,message),after=off+sizeof g->message;
    return !memcmp(g,before,off)&&!memcmp((const char *)g+after,(const char *)before+after,sizeof *g-after);
}
static void update(const PeGame *g,PeSandboxCollision *c){char error[160];CHECK(pe_game_collision_update(g,c,error,sizeof error));}
static void one(PeGame *g,uint32_t proto){
    g->vegetation=(PeVegetation){.installed=true,.count=1,.plants={{proto,{33.5f,9.503f,32},false}}};
}
static void reject_harvest(PeGame *g,PeGame *before,PeSandboxCollision *c,unsigned i){
    memcpy(before,g,sizeof *g);CHECK(!pe_vegetation_can_harvest(g,i,c)&&!memcmp(g,before,sizeof *g));
    CHECK(!pe_vegetation_harvest(g,i,c)&&unchanged(g,before));
}
static void planting(PeGame *g,PeGame *before,PeGame *copy){
    fresh(g);CHECK(pe_vegetation_state_valid(g));uint32_t rng=g->rng_state;
    PeInventory bag=g->inventory;CHECK(pe_game_clone(copy,g));
    CHECK(pe_vegetation_install_fixture(g,g->player));
    CHECK(g->vegetation.installed&&g->vegetation.count==8&&g->tree_count==2&&pe_vegetation_state_valid(g));
    CHECK(g->rng_state==rng&&!memcmp(&bag,&g->inventory,sizeof bag));
    unsigned herbs=0,decorations=0;
    for(unsigned i=0;i<g->vegetation.count;i++){
        const PePlantSource *s=pe_vegetation_source(g->vegetation.plants[i].prototype_id);CHECK(s);
        if(s&&s->harvestable)herbs++;else decorations++;
        CHECK(fabsf(g->vegetation.plants[i].position.y-9.5f)<.02f);
    }
    CHECK(herbs==6&&decorations==2);
    for(int i=0;i<2;i++)CHECK(g->trees[i].remaining==8&&g->trees[i].health==255&&fabsf(g->trees[i].position.y-9.5f)<.02f);
    CHECK(pe_vegetation_install_fixture(copy,copy->player));
    CHECK(!memcmp(&copy->vegetation,&g->vegetation,sizeof g->vegetation)&&!memcmp(copy->trees,g->trees,sizeof g->trees));
    g->vegetation.plants[0].depleted=true;g->trees[0].remaining=0;g->trees[0].health=0;
    memcpy(before,g,sizeof *g);CHECK(pe_vegetation_install_fixture(g,(PeVec3){20,9.5f,20})&&!memcmp(before,g,sizeof *g));
    CHECK(pe_game_clone(copy,g)&&copy->vegetation.plants[0].depleted&&copy->trees[0].health==0);
    copy->vegetation.plants[0].depleted=false;CHECK(g->vegetation.plants[0].depleted);
    PeWorld *world=calloc(1,sizeof *world);CHECK(world);
    if(world){CHECK(pe_world_clone(world,&copy->world));CHECK(pe_game_set_world(copy,world,copy->player));CHECK(!copy->vegetation.installed&&!copy->vegetation.count&&copy->tree_count==0);pe_world_destroy(world);free(world);}
    /* Append rather than replacing old trees; existing cut damage is retained. */
    fresh(g);g->trees[0]=(PeTree){{45,9.5f,45},7,123};g->tree_count=1;PeTree prior=g->trees[0];
    CHECK(pe_vegetation_install_fixture(g,g->player)&&g->tree_count==3&&!memcmp(&prior,&g->trees[0],sizeof prior));
    PeGame *moved=calloc(1,sizeof *moved);CHECK(moved);
    if(moved){pe_game_move(moved,g);CHECK(moved->vegetation.installed&&moved->tree_count==3&&!g->vegetation.installed);pe_game_destroy(moved);free(moved);}
    fresh(g);g->tree_count=15;memcpy(before,g,sizeof *g);CHECK(!pe_vegetation_install_fixture(g,g->player)&&unchanged(g,before));
    fresh(g);memcpy(before,g,sizeof *g);CHECK(!pe_vegetation_install_fixture(g,(PeVec3){NAN,0,0})&&!memcmp(before,g,sizeof *g));
    CHECK(!pe_vegetation_install_fixture(g,(PeVec3){10000,0,10000})&&unchanged(g,before));
    /* No terrain means no supported patch; none of ten provisional objects commit. */
    memset(pe_world_data(&g->world),0,pe_world_voxel_count(&g->world)*sizeof(PeVoxel));g->world.revision++;
    memcpy(before,g,sizeof *g);CHECK(!pe_vegetation_install_fixture(g,g->player)&&unchanged(g,before));
}
static void harvest(PeGame *g,PeGame *before,PeSandboxCollision *c){
    const uint32_t prototypes[]={15,37,40};const int ids[]={238,235,239};const unsigned counts[]={2,3,4};
    for(unsigned i=0;i<3;i++){
        fresh(g);one(g,prototypes[i]);update(g,c);uint32_t rng=g->rng_state,gathered=g->gathered;
        memcpy(before,g,sizeof *g);CHECK(pe_vegetation_can_harvest(g,0,c)&&!memcmp(before,g,sizeof *g));
        CHECK(pe_vegetation_harvest(g,0,c)&&g->vegetation.plants[0].depleted);
        CHECK(pe_inventory_count(&g->inventory,ids[i])==(int)counts[i]&&g->rng_state==rng&&g->gathered==gathered+1);
        CHECK(pe_inventory_count(&g->inventory,917)==0); /* No tutorial/quest grants. */
        reject_harvest(g,before,c,0);
    }
    fresh(g);one(g,15);update(g,c);g->gathered=UINT32_MAX;
    CHECK(pe_vegetation_harvest(g,0,c)&&g->gathered==UINT32_MAX);
    fresh(g);one(g,146);update(g,c);reject_harvest(g,before,c,0);
    one(g,15);g->hp=0;reject_harvest(g,before,c,0);g->hp=NAN;reject_harvest(g,before,c,0);g->hp=500;
    reject_harvest(g,before,c,1);reject_harvest(g,before,NULL,0);
    /* Inclusive2m XZ; source vertical-cell filter remains independently active. */
    g->vegetation.plants[0].position.x=34;CHECK(pe_vegetation_can_harvest(g,0,c));
    g->vegetation.plants[0].position.x=nextafterf(34,INFINITY);reject_harvest(g,before,c,0);
    g->vegetation.plants[0].position.x=33.5f;g->vegetation.plants[0].position.y=13;reject_harvest(g,before,c,0);
    g->vegetation.plants[0].position.y=9.503f;
    /* Current geometry ray obstruction is checked again at transaction time. */
    g->blocks[0]=(PeBlock){33,9,31,PE_ITEM_FENCE};g->blocks[1]=(PeBlock){33,10,31,PE_ITEM_FENCE};g->block_count=2;
    g->vegetation.plants[0].position.z=31.5f;update(g,c);reject_harvest(g,before,c,0);
    g->block_count=0;memset(g->blocks,0,sizeof g->blocks);update(g,c);CHECK(pe_vegetation_can_harvest(g,0,c));
    g->trees[0]=(PeTree){{33.1f,9.5f,31.6f},8,255};g->tree_count=1;update(g,c);reject_harvest(g,before,c,0);
    g->tree_count=0;memset(g->trees,0,sizeof g->trees);update(g,c);
    /* Capacity failure does not deplete, count an acquisition, or consume RNG. */
    CHECK(pe_inventory_add(&g->inventory,12,32));reject_harvest(g,before,c,0);
    g->inventory.slots[31]=(PeSlot){0};CHECK(pe_vegetation_harvest(g,0,c));
    CHECK(pe_inventory_count(&g->inventory,238)==2);
}
static void picking(PeGame *g,PeSandboxCollision *c){
    fresh(g);one(g,15);update(g,c);const PePlantSource *s=pe_vegetation_source(15);CHECK(s);if(!s)return;
    PeVec3 target={33.5f+(s->minimum.x+s->maximum.x)*.5f,9.503f+(s->minimum.y+s->maximum.y)*.5f,32+(s->minimum.z+s->maximum.z)*.5f};
    PeVec3 origin={30,target.y,target.z},direction={5,0,0};unsigned index=99;float distance=123;
    CHECK(pe_vegetation_pick(g,origin,direction,INFINITY,&index,&distance)&&index==0&&distance>2&&distance<4);
    float first=distance;index=99;distance=123;
    CHECK(!pe_vegetation_pick(g,origin,direction,first-.01f,&index,&distance)&&index==99&&distance==123);
    CHECK(pe_vegetation_pick(g,origin,direction,first,&index,&distance));
    /* Nearest reachable render AABB wins, independent of saved array order. */
    g->vegetation.plants[1]=g->vegetation.plants[0];g->vegetation.plants[1].position.x=32.75f;g->vegetation.count=2;
    CHECK(pe_vegetation_pick(g,origin,direction,INFINITY,&index,&distance)&&index==1);
    g->vegetation.plants[1].depleted=true;
    CHECK(pe_vegetation_pick(g,origin,direction,INFINITY,&index,&distance)&&index==0);
    g->vegetation.plants[0].prototype_id=146;index=99;distance=123;
    CHECK(!pe_vegetation_pick(g,origin,direction,INFINITY,&index,&distance)&&index==99&&distance==123);
    one(g,15);CHECK(!pe_vegetation_pick(g,origin,(PeVec3){0},INFINITY,&index,&distance));
    CHECK(!pe_vegetation_pick(g,origin,direction,NAN,&index,&distance));
    CHECK(!pe_vegetation_pick(g,(PeVec3){NAN,0,0},direction,INFINITY,&index,&distance));
    CHECK(!pe_vegetation_pick(g,origin,direction,INFINITY,NULL,&distance));
    g->hp=0;CHECK(!pe_vegetation_pick(g,origin,direction,INFINITY,&index,&distance));
}
static void invalid(PeGame *g,PeGame *before,PeSandboxCollision *c){
    fresh(g);one(g,15);update(g,c);PeVegetation good=g->vegetation;
#define BAD(change) do{change;CHECK(!pe_vegetation_state_valid(g));reject_harvest(g,before,c,0);g->vegetation=good;}while(0)
    BAD(g->vegetation.count=33);BAD(g->vegetation.installed=false);BAD(g->vegetation.count=0);
    BAD(g->vegetation.plants[0].prototype_id=99999);BAD(g->vegetation.plants[0].position.x=NAN);
    BAD(g->vegetation.plants[0].position.y=-1);BAD(g->vegetation.plants[0].position.z=63);
    BAD(g->vegetation.plants[31].prototype_id=15);BAD(g->vegetation.plants[31].position.y=NAN);
    BAD(g->vegetation.plants[31].position.x=-0.0f);BAD(g->vegetation.plants[31].position.y=-0.0f);BAD(g->vegetation.plants[31].position.z=-0.0f);
    BAD(g->vegetation.plants[0].prototype_id=146;g->vegetation.plants[0].depleted=true);
    BAD(g->vegetation.count=2;g->vegetation.plants[1]=g->vegetation.plants[0]);
#undef BAD
    g->mode=PE_GAME_LEGACY;CHECK(!pe_vegetation_state_valid(g));reject_harvest(g,before,c,0);
    pe_vegetation_clear(g);CHECK(pe_vegetation_state_valid(g)&&!g->vegetation.installed);
    CHECK(!pe_vegetation_source(99999)&&!pe_vegetation_state_valid(NULL));
    CHECK(!pe_vegetation_install_fixture(NULL,(PeVec3){0})&&!pe_vegetation_harvest(NULL,0,c));
    pe_vegetation_clear(NULL);
}
int pe_test_vegetation(void){
    failures=0;PeGame *g=calloc(1,sizeof *g),*before=malloc(sizeof *before),*copy=calloc(1,sizeof *copy);
    PeSandboxCollision *c=pe_sandbox_collision_create();CHECK(g&&before&&copy&&c);
    if(g&&before&&copy&&c){planting(g,before,copy);harvest(g,before,c);picking(g,c);invalid(g,before,c);}
    pe_sandbox_collision_free(c);pe_game_destroy(g);free(g);free(before);pe_game_destroy(copy);free(copy);return failures;
}
