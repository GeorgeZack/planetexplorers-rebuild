#include "pe_repulsed.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>

static PeRepulsedStart start(float force){
    return (PeRepulsedStart){.force=force,.projected_direction={0,0,1},
        .position={2,4,6},.hit_position={4,6,8},.animator_present=true};
}
static PeRepulsedTick tick(float dt){
    return (PeRepulsedTick){.delta_time=dt,.context_present=true,.animator_present=true,
        .human_phy_present=true,.wentfly_bone_present=true};
}
static void profiles_and_relations(void){
    PeRepulsedProfile p;assert(pe_repulsed_profile(0,&p));
    /* Active female curves differ sharply from unused EntityPlayer curves. */
    assert(p.velocity==3.956972122192383f&&p.move_time==.20519250631332397f);
    assert(p.stop_time==.09598491340875626f&&p.wentfly_time==20);
    assert(pe_repulsed_profile(400,&p)&&p.velocity==9&&p.move_time==.3499999940395355f);
    assert(p.stop_time>.1f&&p.stop_time<.25f&&p.wentfly_time==20);
    assert(pe_repulsed_profile(750,&p)&&p.velocity==16&&p.move_time==.550000011920929f);
    assert(p.stop_time==.25f&&p.wentfly_time==20);
    assert(pe_repulsed_profile(700.04150390625f,&p)&&p.wentfly_time==.5258514285087585f);
    assert(pe_repulsed_profile(749.9514770507812f,&p)&&p.wentfly_time==.550000011920929f);
    assert(pe_repulsed_profile(FLT_MAX,&p)&&p.velocity==25&&p.move_time==1.5f&&p.wentfly_time==20);
    PeRepulsedRelation r=pe_repulsed_relation();
    assert(r.pause_actions&(UINT64_C(1)<<15)); /* Running shield is paused, retained. */
    assert(r.end_immediately_actions&(UINT64_C(1)<<14)); /* Sword attack ends immediately. */
    assert(pe_repulsed_source_admissible(0));
    for(unsigned i=0;i<56;i++)assert(pe_repulsed_source_admissible(UINT64_C(1)<<i)==!(r.forbidden_masks&(UINT64_C(1)<<i)));
    assert(!pe_repulsed_source_admissible(UINT64_C(1)<<56));
}
static void shield_and_repeated_hits(void){
    PeRepulsed a={0};PeRepulsedUpdate out;
    PeRepulsedStart in=start(400);in.shield_running=true;
    /* Pinned Unity5 Angle with the default old zero direction is 90 degrees. */
    /* Even a caller's modern zero-angle value cannot enable the first block. */
    in.previous_direction_angle=0;assert(pe_repulsed_begin(&a,in,&out));
    assert(!a.blocked&&a.force==400);
    assert(pe_repulsed_cancel(&a,tick(0),&out));
    /* Now the previous direction is retained. Source angle0 may qualify block
     * even when a new incoming direction would fail the cone. */
    in.previous_direction_angle=0;in.projected_direction=(PeVec3){1,0,0};
    assert(pe_repulsed_begin(&a,in,&out));
    assert(a.blocked&&a.force==200&&out.set_initial_block&&out.block_target==1);
    assert(out.position.x==3&&out.position.y==5&&out.position.z==7);
    assert(out.set_mask&&out.mask&&out.started&&!out.apply_move_request&&!out.apply_motor_force);
    assert(pe_repulsed_step(&a,tick(.02f),&out));PeRepulsed before=a;
    in.force=NAN;in.projected_direction=(PeVec3){NAN,0,0};
    assert(pe_repulsed_begin(&a,in,&out)&&out.repeat_ignored);
    assert(!memcmp(&a,&before,sizeof a)&&!out.started&&!out.set_mask);
    assert(pe_repulsed_cancel(&a,tick(0),&out));
    assert(!a.running&&a.direction.x==1&&a.elapsed==a.move_time);
    assert(out.ended&&out.cancel_move_request&&out.clear_desired_direction&&out.reset_anim_parameters);
    assert(out.continue_actions==pe_repulsed_relation().pause_actions);
    in=start(400);in.shield_running=true;in.previous_direction_angle=30;
    in.projected_direction=(PeVec3){1,0,0};assert(pe_repulsed_begin(&a,in,&out));
    assert(!a.blocked&&a.force==400&&a.direction.x==1);
    assert(pe_repulsed_cancel(&a,tick(0),&out));
    in.previous_direction_angle=nextafterf(30,-INFINITY);assert(pe_repulsed_begin(&a,in,&out)&&a.blocked);
}
static void ordering_and_cleanup(void){
    PeRepulsed a={0};PeRepulsedUpdate out;
    assert(pe_repulsed_begin(&a,start(400),&out));
    float stop=a.stop_time;
    assert(pe_repulsed_step(&a,tick(stop),&out));
    assert(out.apply_move_request&&fabsf(out.move_request.z-7.2f)<1e-5f&&!out.request_halt);
    assert(pe_repulsed_step(&a,tick(.001f),&out)&&!out.request_halt); /* Exactly stop time. */
    assert(pe_repulsed_step(&a,tick(.001f),&out)&&out.request_halt);
    assert(pe_repulsed_step(&a,tick(.001f),&out)&&!out.request_halt); /* Once only. */
    a.elapsed=a.move_time;assert(pe_repulsed_step(&a,tick(0),&out));
    assert(out.ended&&!out.apply_move_request&&!out.request_wentfly);
    assert(pe_repulsed_step(&a,tick(0),&out)&&!out.ended&&!out.cancel_move_request&&!out.continue_actions);

    assert(pe_repulsed_begin(&a,start(710),&out));
    a.elapsed=a.wentfly_time;PeRepulsedTick in=tick(0);
    assert(pe_repulsed_step(&a,in,&out)&&!out.request_wentfly); /* Strict >. */
    if(!a.running)assert(pe_repulsed_begin(&a,start(710),&out));
    a.elapsed=fmaxf(a.move_time,nextafterf(a.wentfly_time,INFINITY));
    assert(pe_repulsed_step(&a,in,&out)&&out.request_wentfly&&out.ended);
    assert(out.wentfly_direction.z==1&&out.wentfly_force==.800000011920929f*710);
    /* The Wentfly request ends Repulsed before any scheduler acceptance. */
    assert(!a.running&&!out.apply_move_request&&out.continue_actions);
    assert(pe_repulsed_begin(&a,start(710),&out));a.elapsed=1;
    in.wentfly_bone_present=false;assert(pe_repulsed_step(&a,in,&out)&&out.ended&&!out.request_wentfly);

    PeRepulsedStart s=start(400);s.move_motor_present=true;
    assert(pe_repulsed_begin(&a,s,&out)&&out.apply_motor_force&&out.motor_force.z==9);
    in=tick(.3f);in.move_motor_present=true;
    assert(pe_repulsed_step(&a,in,&out)&&out.blend_anim_parameters&&out.anim_blend_fraction==1);
    in.context_present=false;assert(pe_repulsed_step(&a,in,&out));
    assert(out.ended&&out.apply_motor_force&&out.motor_force.z==0&&out.cancel_move_request);
    assert(pe_repulsed_cancel(&a,in,&out)&&!out.ended);
}
static void invalid_and_component_requests(void){
    PeRepulsed a={0},before=a;PeRepulsedUpdate out;memset(&out,0xa5,sizeof out);
    PeRepulsedUpdate original=out;
    PeRepulsedStart in=start(NAN);
    assert(!pe_repulsed_begin(&a,in,&out)&&!memcmp(&a,&before,sizeof a)&&!memcmp(&out,&original,sizeof out));
    in=start(400);in.projected_direction.x=1;
    assert(!pe_repulsed_begin(&a,in,&out)&&!memcmp(&a,&before,sizeof a));
    in=start(400);in.projected_direction=(PeVec3){FLT_MIN,0,0};
    assert(!pe_repulsed_begin(&a,in,&out)&&!memcmp(&a,&before,sizeof a));
    in=start(400);in.position.x=in.hit_position.x=FLT_MAX;
    assert(!pe_repulsed_begin(&a,in,&out)&&!memcmp(&a,&before,sizeof a));
    in=start(400);in.projected_direction=(PeVec3){0};in.animator_present=false;
    assert(pe_repulsed_begin(&a,in,&out)&&!out.set_initial_block);
    PeRepulsedTick t=tick(.01f);t.human_phy_present=false;
    assert(pe_repulsed_step(&a,t,&out)&&!out.blend_anim_parameters&&!out.apply_move_request);
    before=a;original=out;t.delta_time=INFINITY;
    assert(!pe_repulsed_step(&a,t,&out)&&!memcmp(&a,&before,sizeof a)&&!memcmp(&out,&original,sizeof out));
    t=tick(0);t.animator_present=t.human_phy_present=false;
    assert(pe_repulsed_cancel(&a,t,&out)&&out.ended&&!out.reset_anim_parameters&&!out.cancel_move_request);
    PeRepulsedProfile p={1,2,3,4},saved=p;
    assert(!pe_repulsed_profile(-1,&p)&&!memcmp(&p,&saved,sizeof p));
    assert(!pe_repulsed_profile(400,NULL)&&!pe_repulsed_begin(NULL,start(400),&out));
    assert(!pe_repulsed_step(&a,t,NULL)&&!pe_repulsed_cancel(&a,t,NULL));
}
int pe_test_repulsed(void){
    profiles_and_relations();shield_and_repeated_hits();ordering_and_cleanup();invalid_and_component_requests();
    return 0;
}
