#include "pe_hare_perception.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hare perception:%d: %s\n",__LINE__,#c);failures++;}}while(0)
typedef struct { bool fail,blocked; unsigned calls; uint32_t mask; PeVec3 origin,target; } Query;
static bool block(const void *ctx,PeVec3 origin,PeVec3 target,uint32_t mask,bool *out){
    Query *q=(Query *)ctx;q->calls++;q->mask=mask;q->origin=origin;q->target=target;
    if(q->fail)return false;
    *out=q->blocked;return true;
}
static PeHarePerceptionTarget player(void){return(PeHarePerceptionTarget){true,true,true,false,{0,0,10},{0,1,10},.5f};}
static PeHareVisionPose sensor(void){return(PeHareVisionPose){{0,1,0},{0,0,1}};}
static PeHareCollectionGate gates(void){return(PeHareCollectionGate){true,true,true,true,true,false,true};}
static PeHarePermissionInput relations(void){return(PeHarePermissionInput){.adventure=true,.force_conflict=true,.initial_threat=5,.damage_value=1};}
static void vision_due(PeHarePerception *s,PeHareVisionPose p,PeHarePerceptionTarget t,PeHarePerceptionQueries *q){s->vision_updates=19;CHECK(pe_hare_perception_vision(s,(PeHareVisionConfig){20,60},p,t,q));}
static void hearing_due(PeHarePerception *s,PeHarePerceptionTarget t,PeHarePerceptionQueries *q,bool ignore){s->hearing_updates=15;CHECK(pe_hare_perception_hearing(s,(PeHareHearingConfig){15,ignore},(PeVec3){0,1,0},t,q));}
int pe_test_hare_perception(void){
    failures=0;PeHarePerception s;CHECK(pe_hare_perception_init(&s,100,1));PeHarePerception old=s;
    CHECK(!pe_hare_perception_init(&s,0,1));CHECK(!memcmp(&s,&old,sizeof(s)));CHECK(!pe_hare_perception_init(&s,1,1));
    Query query={0};PeHarePerceptionQueries q={&query,block};PeHareVisionPose p=sensor();PeHarePerceptionTarget t=player();
    for(int i=0;i<19;i++){CHECK(pe_hare_perception_vision(&s,(PeHareVisionConfig){20,60},p,t,&q));CHECK(!s.visible&&query.calls==0);}
    CHECK(pe_hare_perception_vision(&s,(PeHareVisionConfig){20,60},p,t,&q));CHECK(s.visible&&query.calls==1);
    CHECK(query.mask==PE_HARE_PERCEPTION_BLOCK_MASK&&(query.mask&(1u<<14))&&(query.mask&(1u<<21)));
    CHECK(query.origin.y==1&&query.target.y==t.center.y&&query.target.z==t.center.z);
    t.present=false;for(int i=0;i<19;i++){CHECK(pe_hare_perception_vision(&s,(PeHareVisionConfig){20,60},p,t,&q));CHECK(s.visible);}
    CHECK(pe_hare_perception_vision(&s,(PeHareVisionConfig){20,60},p,t,&q));CHECK(!s.visible&&query.calls==1);
    t=player();t.position=(PeVec3){0,100,20.5f};t.center=(PeVec3){0,1,20.5f};vision_due(&s,p,t,&q);CHECK(s.visible);
    t.position.z=nextafterf(20.5f,INFINITY);vision_due(&s,p,t,&q);CHECK(!s.visible);t.position.z=20.5f;
    t.center.y=100;vision_due(&s,p,t,&q);CHECK(!s.visible); /* Cone uses3D center, radius ignoresY. */
    t=player();t.center=(PeVec3){10,1,0};s.vision_updates=19;
    CHECK(pe_hare_perception_vision(&s,(PeHareVisionConfig){20,90},p,t,&q));CHECK(!s.visible);
    t.center.z=.01f;s.vision_updates=19;CHECK(pe_hare_perception_vision(&s,(PeHareVisionConfig){20,90},p,t,&q));CHECK(s.visible);
    p.axis=(PeVec3){1,0,0};t.center=(PeVec3){10,1,0};vision_due(&s,p,t,&q);CHECK(s.visible);p=sensor();
    t=player();query.blocked=true;vision_due(&s,p,t,&q);CHECK(!s.visible);hearing_due(&s,t,&q,false);CHECK(!s.heard);
    hearing_due(&s,t,NULL,true);CHECK(s.heard);query.blocked=false;
    t.is_snake=true;unsigned calls=query.calls;vision_due(&s,p,t,&q);CHECK(!s.visible&&query.calls==calls+1);
    hearing_due(&s,t,&q,false);CHECK(s.heard);t.alive=false;vision_due(&s,p,t,&q);CHECK(!s.visible);t.is_snake=false;vision_due(&s,p,t,&q);CHECK(s.visible);
    PeHareAcquisition out;s.target_updates=14;CHECK(pe_hare_perception_collect(&s,t,gates(),relations(),&out));CHECK(out.collection_ran&&!out.collection_candidate);
    t=player();t.position.z=15.5f;hearing_due(&s,t,&q,false);CHECK(s.heard);t.position.z=nextafterf(15.5f,INFINITY);hearing_due(&s,t,&q,false);CHECK(!s.heard);
    CHECK(pe_hare_perception_init(&s,100,1));t=player();
    for(int i=0;i<15;i++){CHECK(pe_hare_perception_hearing(&s,(PeHareHearingConfig){15,false},p.position,t,&q));CHECK(!s.heard);}
    CHECK(pe_hare_perception_hearing(&s,(PeHareHearingConfig){15,false},p.position,t,&q));CHECK(s.heard);
    /* Components have independent frame counters and cached membership. */
    CHECK(pe_hare_perception_init(&s,100,1));t=player();t.position.z=t.center.z=18;
    for(int frame=1;frame<=45;frame++){
        if(frame==31)t.position.z=t.center.z=100;
        CHECK(pe_hare_perception_vision(&s,(PeHareVisionConfig){20,60},p,t,&q));
        CHECK(pe_hare_perception_hearing(&s,(PeHareHearingConfig){15,false},p.position,t,&q));
        CHECK(pe_hare_perception_collect(&s,t,gates(),relations(),&out));
        CHECK(out.tick_due==(frame%15==0));CHECK(out.collection_candidate==(frame==30));
    }
    CHECK(pe_hare_perception_init(&s,100,1));s.visible=true;s.vision_updates=59;s.target_updates=59;PeHarePerception other=s;
    CHECK(pe_hare_perception_collect(&s,t,gates(),relations(),&out));CHECK(out.collection_candidate);
    CHECK(pe_hare_perception_vision(&other,(PeHareVisionConfig){20,60},p,t,&q));CHECK(pe_hare_perception_collect(&other,t,gates(),relations(),&out));CHECK(!out.collection_candidate);
    /* New acquisition and continuing damage permission are separate gates. */
    PeHarePermissionInput rel=relations();PeHarePermissions permission;
    CHECK(pe_hare_perception_permissions(rel,&permission));CHECK(permission.can_attack&&permission.can_damage);
    rel.forward=(PeHareReputation){true,4};CHECK(pe_hare_perception_permissions(rel,&permission));CHECK(!permission.can_attack&&permission.can_damage);
    rel.forward.level=5;rel.adventure=false;CHECK(pe_hare_perception_permissions(rel,&permission));CHECK(!permission.can_attack&&!permission.can_damage);
    rel.adventure=true;CHECK(pe_hare_perception_permissions(rel,&permission));CHECK(!permission.can_attack&&permission.can_damage);
    rel=relations();rel.reverse=(PeHareReputation){true,4};CHECK(pe_hare_perception_permissions(rel,&permission));CHECK(!permission.can_attack&&permission.can_damage);
    rel=relations();rel.initial_threat=-5;rel.damage_value=-1;CHECK(pe_hare_perception_permissions(rel,&permission));CHECK(permission.can_attack&&permission.can_damage);
    rel.initial_threat=1.192092896e-7f;CHECK(pe_hare_perception_permissions(rel,&permission));CHECK(!permission.can_attack&&permission.can_damage);
    rel=relations();rel.damage_value=0;CHECK(pe_hare_perception_permissions(rel,&permission));CHECK(permission.can_attack&&!permission.can_damage);
    rel.force_conflict=false;CHECK(pe_hare_perception_permissions(rel,&permission));CHECK(!permission.can_attack&&!permission.can_damage);
    t=player();s.visible=true;s.heard=false;PeHareCollectionGate g=gates();
    for(int blocked=0;blocked<7;blocked++){
        g=gates();
        if(blocked==0)g.self_has_view=false;
        if(blocked==1)g.self_alive=false;
        if(blocked==2)g.local_controller=false;
        if(blocked==3)g.scan=false;
        if(blocked==4)g.can_search=false;
        if(blocked==5)g.already_enemy=true;
        if(blocked==6)g.carrier_allowed=false;
        s.target_updates=14;CHECK(pe_hare_perception_collect(&s,t,g,relations(),&out));CHECK(!out.collection_candidate);
    }
    /* Sensor callback failure cannot partially advance a counter/cache. */
    s.vision_updates=19;old=s;query.fail=true;CHECK(!pe_hare_perception_vision(&s,(PeHareVisionConfig){20,60},p,t,&q));CHECK(!memcmp(&s,&old,sizeof(s)));
    s.hearing_updates=15;old=s;CHECK(!pe_hare_perception_hearing(&s,(PeHareHearingConfig){15,false},p.position,t,&q));CHECK(!memcmp(&s,&old,sizeof(s)));query.fail=false;
    t.center.x=NAN;old=s;CHECK(!pe_hare_perception_vision(&s,(PeHareVisionConfig){20,60},p,t,&q));CHECK(!memcmp(&s,&old,sizeof(s)));t=player();
    PeHareAcquisition saved=out;rel=relations();rel.initial_threat=NAN;old=s;CHECK(!pe_hare_perception_collect(&s,t,gates(),rel,&out));CHECK(!memcmp(&s,&old,sizeof(s))&&!memcmp(&out,&saved,sizeof(out)));
    return failures;
}
