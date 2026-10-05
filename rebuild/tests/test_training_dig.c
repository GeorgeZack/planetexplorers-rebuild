#include "pe_training_dig.h"
#include "pe_training.h"
#include "pe_training_terrain.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL training dig %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
static void fixture(PeWorld*w,PeTrainingGathering*l,PeTrainingDig*s,PeTrainingShovel*t,float*stamina,bool male){
    memset(w,0,sizeof*w);w->source=1;w->revision=1;
    pe_world_set(w,20,5,20,(PeVoxel){255,1});
    *l=(PeTrainingGathering){.mission=751,.accepted=31,.shovel=2};
    pe_training_dig_init(s,male);*t=(PeTrainingShovel){1,3553};*stamina=100;
}
static PeTrainingDigInput input(void){return(PeTrainingDigInput){.ray_origin={20,5,18},.ray_direction={0,0,1},.player={20,4,18},.enabled=true,.held=true};}
static void timing(PeWorld*w,bool male){
    PeTrainingGathering l;PeTrainingDig s;PeTrainingShovel t;float stamina;
    fixture(w,&l,&s,&t,&stamina,male);PeTrainingDigUpdate u;PeTrainingDigInput in=input();
    CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u)&&u.started&&stamina==96);
    CHECK(s.stage==PE_DIG_ENTRY&&t.durability==3553&&!u.swung);
    unsigned tick=1;
    while(!u.swung&&tick<80){CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u));tick++;}
    CHECK(tick==(male?52u:49u)); /* Entry .6*.7/2 + authored loop event. */
    CHECK(u.swung&&!u.removed&&pe_world_get(w,20,5,20).volume==221);
    CHECK(t.durability==3548&&stamina==92&&s.swings==1&&l.dirt==0);
    in.held=false;
    for(unsigned i=0;i<200&&s.stage!=PE_DIG_IDLE;i++)CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u));
    CHECK(s.stage==PE_DIG_IDLE&&s.swings==1&&stamina==92); /* No charge during ending. */
    CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u)&&!s.stamina_lock);
}
static void damage_and_receipts(PeWorld*w){
    const uint8_t density[]={255,210,190,130,110};const unsigned expected[]={4,3,2,1,1};
    for(unsigned k=0;k<5;k++){
        PeTrainingGathering l;PeTrainingDig s;PeTrainingShovel t;float stamina;
        fixture(w,&l,&s,&t,&stamina,false);pe_world_set(w,20,5,20,(PeVoxel){density[k],1});
        if(density[k]<128)pe_world_set(w,20,4,20,(PeVoxel){255,1});
        PeTrainingDigUpdate u={0};PeTrainingDigInput in=input();
        for(unsigned i=0;i<600&&!u.removed;i++)CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u));
        CHECK(u.removed&&u.dirt_added==2&&l.dirt==2&&l.removed_count==1&&s.swings==expected[k]);
        CHECK(pe_world_get(w,20,5,20).volume==0&&t.durability==3553-5*(float)expected[k]);
    }
    PeTrainingGathering l;PeTrainingDig s;PeTrainingShovel t;float stamina;
    fixture(w,&l,&s,&t,&stamina,false);PeTrainingDigInput in=input();PeTrainingDigUpdate u;
    for(unsigned sample=0;sample<3;sample++){
        pe_world_set(w,20+(int)sample,5,20,(PeVoxel){130,1});
        in.ray_origin.x=in.player.x=(float)(20+sample);bool removed=false;
        for(unsigned i=0;i<200&&!removed;i++){CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u));removed=u.removed;}
        CHECK(removed&&l.dirt==(sample+1)*2&&l.removed_count==sample+1);
    }
    CHECK(l.complete&&l.dirt==6&&s.swings==3);
}
static void failure_and_targets(PeWorld*w){
    PeTrainingGathering l;PeTrainingDig s;PeTrainingShovel t;float stamina;
    fixture(w,&l,&s,&t,&stamina,false);PeTrainingDigUpdate u={0};PeTrainingDigInput in=input();
    PeTrainingDig old=s;PeTrainingGathering old_l=l;PeTrainingShovel old_t=t;uint64_t hash=pe_world_hash(w);
    in.ray_direction.x=NAN;
    CHECK(!pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u));
    CHECK(!memcmp(&s,&old,sizeof s)&&!memcmp(&l,&old_l,sizeof l)&&!memcmp(&t,&old_t,sizeof t)&&pe_world_hash(w)==hash&&stamina==100);
    in=input();in.player.z=17;CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u)&&!u.started&&stamina==100); /* strict3m */
    in=input();t.durability=0;CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u)&&!u.started);
    t.durability=3553;stamina=40;CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u)&&stamina==36&&fabsf(s.speed-.9f)<1e-6);
    pe_training_dig_cancel(&s);stamina=20;CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u)&&stamina==16&&fabsf(s.speed-.81f)<1e-6);
    pe_training_dig_cancel(&s);stamina=0;CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u)&&u.started&&stamina==0);
    /* First locked target survives retargeting before the first event. */
    in.ray_origin.x=in.player.x=23;
    for(unsigned i=0;i<100&&!u.swung;i++)CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u));
    CHECK(u.swung&&pe_world_get(w,20,5,20).volume==221);
    in.enabled=false;CHECK(pe_training_dig_step(&s,NULL,NULL,NULL,NULL,in,&u)&&s.stage==PE_DIG_IDLE);
    fixture(w,&l,&s,&t,&stamina,false);pe_world_set(w,20,5,20,(PeVoxel){130,1});l.dirt=PE_GATHERING_ITEM_LIMIT;
    in=input();bool blocked=false;hash=pe_world_hash(w);
    for(unsigned i=0;i<100&&!blocked;i++){CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u));blocked=u.capacity_blocked;}
    CHECK(blocked&&pe_world_hash(w)==hash&&l.dirt==PE_GATHERING_ITEM_LIMIT&&t.durability==3553&&stamina==96&&s.swings==0);
    l.dirt=0;
    for(unsigned i=0;i<100&&!u.removed;i++)CHECK(pe_training_dig_step(&s,w,&l,&t,&stamina,in,&u));
    CHECK(u.removed&&l.dirt==2&&s.swings==1);
}
static void training_bridge(PeWorld*w){
    PeTraining t={0};t.ready=t.grounded=true;t.hp=t.max_hp=500;t.stamina=100;
    t.tutorial=(PeTutorial){751,3,15,0};t.dialogue_index=3;
    t.gathering=(PeTrainingGathering){.mission=751,.accepted=31,.shovel=2,.axe=1};
    t.shovels[0]=(PeTrainingShovel){1,3553};t.shovels[1]=(PeTrainingShovel){2,3553};
    t.equipped_shovel=-1;t.axe_equipped=true;pe_training_dig_init(&t.dig,false);
    pe_training_terrain_init(w);
    CHECK(pe_world_set(w,60,25,60,(PeVoxel){255,1})); /* Unrelated sample survives cleanup. */
    PeTrainingDigUpdate u;
    PeTraining before=t;CHECK(!pe_training_equip_shovel(&t,0)&&!memcmp(&before,&t,sizeof t));
    CHECK(pe_training_open_inventory(&t)&&pe_training_equip_shovel(&t,0));
    CHECK(!t.axe_equipped&&t.equipped_shovel==0&&pe_training_inventory_count(&t,12)==1&&t.gathering.shovel==2);
    CHECK(!pe_training_set_shovel_held(&t,true));pe_training_close_inventory(&t);
    CHECK(pe_training_set_shovel_held(&t,true));t.position=(PeVec3){5,1.5f,9.5f};
    PeVec3 origin={5,5,11},direction={0,-1,0};
    CHECK(pe_training_mine_step(&t,w,origin,direction,true,true,&u)&&!u.started); /* Movement intent. */
    t.camera_mode=2;CHECK(pe_training_mine_step(&t,w,origin,direction,true,false,&u)&&!u.started);t.camera_mode=0;
    before=t;uint64_t hash=pe_world_hash(w);
    CHECK(!pe_training_mine_step(&t,w,origin,(PeVec3){NAN,0,0},true,false,&u));
    CHECK(!memcmp(&before,&t,sizeof t)&&hash==pe_world_hash(w));
    for(unsigned i=0;i<100&&!u.swung;i++)CHECK(pe_training_mine_step(&t,w,origin,direction,true,false,&u));
    CHECK(u.swung&&!u.removed&&t.shovels[0].durability==3548&&t.shovels[1].durability==3553);
    CHECK(pe_training_open_inventory(&t)&&pe_training_equip_shovel(&t,1));
    CHECK(t.dig.stage==PE_DIG_IDLE&&t.shovels[0].durability==3548&&t.shovels[1].durability==3553);
    pe_training_close_inventory(&t);CHECK(pe_training_set_shovel_held(&t,true));
    const int points[3][2]={{5,11},{6,11},{5,12}};
    for(unsigned sample=0;sample<3;sample++){
        origin=(PeVec3){(float)points[sample][0],5,(float)points[sample][1]};
        t.position=(PeVec3){origin.x,1.5f,origin.z-1.5f};bool removed=false;
        for(unsigned i=0;i<300&&!removed;i++){
            CHECK(pe_training_mine_step(&t,w,origin,direction,true,false,&u));removed=u.removed;
        }
        CHECK(removed);
        if(sample<2)CHECK(t.gathering.dirt==(sample+1)*2&&t.tutorial.mission_id==751);
    }
    CHECK(t.tutorial.mission_id==752&&t.gathering.mission==752&&t.gathering.dirt==9006);
    CHECK(t.gathering.shovel==2&&t.shovels[0].durability==3548&&t.shovels[1].durability==3528);
    CHECK(t.dig.stage==PE_DIG_IDLE&&!t.shovel_held&&pe_training_dialogue(&t)==4074);
    for(int x=0;x<PE_WORLD_X;x++)for(int y=0;y<PE_WORLD_Y;y++)for(int z=0;z<PE_WORLD_Z;z++)
        if(x!=60||y!=25||z!=60)CHECK(pe_world_get(w,x,y,z).volume==0);
    CHECK(pe_world_get(w,60,25,60).volume==255);
    CHECK(pe_training_advance_dialogue(&t,4074)&&pe_training_advance_dialogue(&t,300244)&&pe_training_advance_dialogue(&t,300245));
    before=t;hash=pe_world_hash(w);
    CHECK(pe_training_mine_step(&t,w,origin,direction,true,false,&u));
    CHECK(!memcmp(&t,&before,sizeof t)&&pe_world_hash(w)==hash&&t.gathering.dirt==9006);
}
int pe_test_training_dig(void){
    failures=0;PeWorld*w=malloc(sizeof*w);CHECK(w!=NULL);if(!w)return failures;
    timing(w,false);timing(w,true);damage_and_receipts(w);failure_and_targets(w);training_bridge(w);free(w);return failures;
}
