#include "pe_training_gathering.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"gathering:%d: %s\n",__LINE__,#x);failures++;}}while(0)
static const PeGatheringInput cut={true,true,true,true,true};
static void complete_herbs(PeTrainingGathering*s){
    CHECK(pe_training_gathering_enter(s,719));
    for(unsigned i=0;i<5;i++){
        CHECK(pe_training_gathering_begin_herb(s,i,pe_training_gathering_herb_position(i)));
        CHECK(pe_training_gathering_step(s,2,(PeGatheringInput){0}));
    }
    CHECK(s->herbs_depleted==5&&s->complete&&s->wood==0&&s->dirt==0);
}
static void until_mining(PeTrainingGathering*s){
    pe_training_gathering_init(s);complete_herbs(s);
    CHECK(pe_training_gathering_enter(s,748));CHECK(s->axe==1&&s->tree_hp==7);
    CHECK(pe_training_gathering_step(s,.01,cut));CHECK(pe_training_gathering_step(s,20,cut));
    CHECK(s->tree_hp==0&&s->wood==1&&s->complete);
    CHECK(pe_training_gathering_enter(s,757));CHECK(s->script==1);
    CHECK(pe_training_gathering_learn_script(s));CHECK(s->script==0&&s->formula1&&s->complete);
    CHECK(!pe_training_gathering_learn_script(s));CHECK(pe_training_gathering_enter(s,721));CHECK(s->wood==21);
    CHECK(pe_training_gathering_start_craft(s));CHECK(s->wood==11&&s->shovel==0);
    CHECK(!pe_training_gathering_start_craft(s));CHECK(!pe_training_gathering_enter(s,751));
    CHECK(pe_training_gathering_step(s,2.999,(PeGatheringInput){0}));CHECK(s->shovel==0&&!s->complete);
    CHECK(pe_training_gathering_step(s,.0011,(PeGatheringInput){0}));CHECK(s->shovel==1&&s->complete);
    CHECK(pe_training_gathering_enter(s,751));CHECK(s->shovel==2&&s->wood==11);
}
int pe_test_training_gathering(void){
    failures=0;PeTrainingGathering s,before;
    pe_training_gathering_init(&s);CHECK(!pe_training_gathering_enter(&s,748));CHECK(!pe_training_gathering_enter(&s,746));
    CHECK(pe_training_gathering_enter(&s,719));before=s;CHECK(!pe_training_gathering_enter(&s,719));CHECK(!memcmp(&s,&before,sizeof s));
    PeVec3 p=pe_training_gathering_herb_position(0);p.y+=3;CHECK(!pe_training_gathering_begin_herb(&s,0,p));
    p=pe_training_gathering_herb_position(0);p.y+=2.64f;CHECK(pe_training_gathering_begin_herb(&s,0,p));
    CHECK(pe_training_gathering_movement_locked(&s));CHECK(!pe_training_gathering_begin_herb(&s,0,p));
    CHECK(pe_training_gathering_step(&s,1.099,(PeGatheringInput){0}));CHECK(!s.herb_dead[0]);
    CHECK(pe_training_gathering_step(&s,.0011,(PeGatheringInput){0}));CHECK(s.herb_dead[0]&&s.herbs_depleted==1&&s.herb_active[0]);
    CHECK(!pe_training_gathering_movement_locked(&s));CHECK(pe_training_gathering_step(&s,1,(PeGatheringInput){0}));CHECK(!s.herb_active[0]);
    before=s;CHECK(!pe_training_gathering_begin_herb(&s,0,p));CHECK(!pe_training_gathering_begin_herb(&s,5,p));CHECK(!pe_training_gathering_step(&s,NAN,cut));CHECK(!pe_training_gathering_step(&s,-1,cut));CHECK(!memcmp(&s,&before,sizeof s));
    for(unsigned i=1;i<5;i++){
        CHECK(pe_training_gathering_begin_herb(&s,i,pe_training_gathering_herb_position(i)));
        if(i==1){CHECK(pe_training_gathering_step(&s,1.1,(PeGatheringInput){0}));CHECK(s.herb_dead[i]&&s.herb_active[i]);}
        CHECK(pe_training_gathering_step(&s,60,(PeGatheringInput){0}));
    }
    CHECK(s.complete&&pe_training_gathering_next(&s)==748&&s.herbs_depleted==5);
    CHECK(pe_training_gathering_enter(&s,748));before=s;CHECK(!pe_training_gathering_enter(&s,748));CHECK(!memcmp(&s,&before,sizeof s));
    PeTrainingGathering released=s;PeGatheringInput release=cut;release.cut_held=false;
    CHECK(pe_training_gathering_step(&released,.01,cut));CHECK(pe_training_gathering_step(&released,3,release));CHECK(released.tree_hp==6&&released.tree_stage==PE_TREE_IDLE);
    released=s;CHECK(pe_training_gathering_step(&released,.01,cut));CHECK(pe_training_gathering_step(&released,1.21,cut));
    CHECK(pe_training_gathering_step(&released,.1,release));CHECK(pe_training_gathering_step(&released,.5,cut));
    CHECK(released.tree_stage==PE_TREE_EXIT&&released.tree_hp==6&&released.tree_release);
    CHECK(pe_training_gathering_step(&released,1,cut));CHECK(released.tree_stage==PE_TREE_IDLE);
    PeGatheringInput input=cut;input.tree_raycast_hit=false;CHECK(pe_training_gathering_step(&s,2,input));CHECK(s.tree_stage==PE_TREE_IDLE);
    input=cut;input.animator_do_nothing=false;CHECK(pe_training_gathering_step(&s,2,input));CHECK(s.tree_stage==PE_TREE_WAIT&&s.tree_hp==7);
    input.cut_held=false;CHECK(pe_training_gathering_step(&s,.1,input));CHECK(s.tree_release&&s.tree_stage==PE_TREE_WAIT);
    CHECK(pe_training_gathering_step(&s,.01,cut));CHECK(s.tree_stage==PE_TREE_ENTRY&&s.tree_time==0&&!s.tree_release);
    CHECK(pe_training_gathering_step(&s,.6,cut));CHECK(s.tree_stage==PE_TREE_ENTRY&&s.tree_hp==7);
    CHECK(pe_training_gathering_step(&s,.6,cut));CHECK(s.tree_hp==6);
    input=cut;input.cut_held=false;CHECK(pe_training_gathering_step(&s,2,input));CHECK(s.tree_stage==PE_TREE_IDLE&&s.tree_hp==6&&s.wood==0);
    CHECK(pe_training_gathering_step(&s,.01,cut));input=cut;input.axe_present=false;
    CHECK(pe_training_gathering_step(&s,2,input));CHECK(s.tree_stage==PE_TREE_IDLE&&s.tree_hp==6);
    CHECK(pe_training_gathering_step(&s,.01,cut));CHECK(pe_training_gathering_step(&s,20,cut));CHECK(s.tree_hp==0&&s.wood==1&&s.complete);
    CHECK(pe_training_gathering_step(&s,20,cut));CHECK(s.wood==1);
    until_mining(&s);
    PeWorld*old=calloc(1,sizeof *old),*now=calloc(1,sizeof *now);CHECK(old&&now);
    if(old&&now){
        old->source=now->source=1;old->origin_x=now->origin_x=-2;old->revision=1;now->revision=2;
        for(int x=0;x<4;x++)old->voxels[x]=(PeVoxel){110,1};
        before=s;CHECK(!pe_training_gathering_removed_voxel(&s,old,old,0,0,0));
        now->origin_x++;CHECK(!pe_training_gathering_removed_voxel(&s,old,now,0,0,0));now->origin_x--;
        now->revision=old->revision;CHECK(!pe_training_gathering_removed_voxel(&s,old,now,0,0,0));now->revision++;
        old->voxels[0].volume=0;CHECK(!pe_training_gathering_removed_voxel(&s,old,now,0,0,0));old->voxels[0].volume=110;
        old->voxels[0].material=2;CHECK(!pe_training_gathering_removed_voxel(&s,old,now,0,0,0));old->voxels[0].material=1;
        now->voxels[0].volume=1;CHECK(!pe_training_gathering_removed_voxel(&s,old,now,0,0,0));now->voxels[0].volume=0;
        CHECK(!pe_training_gathering_removed_voxel(&s,old,now,-1,0,0));CHECK(!memcmp(&s,&before,sizeof s));
        CHECK(pe_training_gathering_removed_voxel(&s,old,now,0,0,0));CHECK(s.dirt==2&&!s.complete);
        before=s;CHECK(!pe_training_gathering_removed_voxel(&s,old,now,0,0,0));CHECK(!memcmp(&s,&before,sizeof s));
        CHECK(pe_training_gathering_removed_voxel(&s,old,now,1,0,0));CHECK(s.dirt==4&&!s.complete);
        CHECK(pe_training_gathering_removed_voxel(&s,old,now,2,0,0));CHECK(s.dirt==6&&s.complete);
        CHECK(!pe_training_gathering_removed_voxel(&s,old,now,3,0,0));CHECK(old->voxels[0].volume==110&&now->voxels[0].volume==0);
        CHECK(pe_training_gathering_enter(&s,752));CHECK(s.dirt==9006);before=s;CHECK(!pe_training_gathering_enter(&s,752));CHECK(!memcmp(&s,&before,sizeof s));
        CHECK(pe_training_gathering_enter_build_mode(&s));CHECK(pe_training_gathering_next(&s)==753);CHECK(!pe_training_gathering_enter_build_mode(&s));
    }
    free(old);free(now);
    /* Grant capacity failure is atomic. Acquisition during a craft reserves
     * the pending output slot, so work cannot silently lose the produced item. */
    pe_training_gathering_init(&s);complete_herbs(&s);s.axe=PE_GATHERING_ITEM_LIMIT;before=s;
    CHECK(!pe_training_gathering_enter(&s,748));CHECK(!memcmp(&s,&before,sizeof s));
    until_mining(&s);CHECK(!pe_training_gathering_acquire(&s,268,UINT32_MAX));CHECK(!pe_training_gathering_acquire(&s,999,1));
    pe_training_gathering_init(&s);complete_herbs(&s);CHECK(pe_training_gathering_enter(&s,748));s.tree_hp=1;s.wood=PE_GATHERING_ITEM_LIMIT;
    CHECK(pe_training_gathering_step(&s,.01,cut));CHECK(pe_training_gathering_step(&s,20,cut));CHECK(s.tree_hp==1&&!s.complete&&s.wood==PE_GATHERING_ITEM_LIMIT);
    released=s;CHECK(pe_training_gathering_step(&released,2,release));
    CHECK(released.tree_stage==PE_TREE_IDLE&&!pe_training_gathering_movement_locked(&released));
    CHECK(released.tree_hp==1&&released.wood==PE_GATHERING_ITEM_LIMIT&&!released.complete);
    s.wood--;CHECK(pe_training_gathering_step(&s,1.1,cut));CHECK(s.tree_hp==0&&s.complete&&s.wood==PE_GATHERING_ITEM_LIMIT);
    CHECK(pe_training_gathering_enter(&s,757));s.formula1=true;before=s;
    CHECK(!pe_training_gathering_learn_script(&s));CHECK(!memcmp(&s,&before,sizeof s));
    s.formula1=false;CHECK(pe_training_gathering_learn_script(&s));s.wood=1;
    CHECK(pe_training_gathering_enter(&s,721));CHECK(pe_training_gathering_start_craft(&s));
    before=s;CHECK(!pe_training_gathering_acquire(&s,12,PE_GATHERING_ITEM_LIMIT));CHECK(!memcmp(&s,&before,sizeof s));
    CHECK(pe_training_gathering_acquire(&s,12,PE_GATHERING_ITEM_LIMIT-1));CHECK(s.complete&&s.crafting);
    CHECK(!pe_training_gathering_enter(&s,751));CHECK(pe_training_gathering_step(&s,3,(PeGatheringInput){0}));
    CHECK(s.shovel==PE_GATHERING_ITEM_LIMIT&&s.wood==11&&!s.crafting);
    before=s;CHECK(!pe_training_gathering_enter(&s,751));CHECK(!memcmp(&s,&before,sizeof s));
    CHECK(!pe_training_gathering_start_craft(&s));
    return failures;
}
