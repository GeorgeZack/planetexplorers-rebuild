#include "pe_halt_host.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned failures;
#define CHECK(v) do{if(!(v)){fprintf(stderr,"halt_host:%d %s\n",__LINE__,#v);failures++;}}while(0)
static const float dt=1/60.f;
static PeHaltHostInput standing(void){
    return (PeHaltHostInput){.delta_time=dt,.move_admissible=true,.grounded=true,.alive=true,
        .left_foot={-.1f,0,.2f},.right_foot={.1f,0,-.2f}};
}
static PeHaltHostInput moving(float speed){PeHaltHostInput in=standing();in.move_requested=true;in.velocity=(PeVec3){0,0,speed};return in;}
static PeHaltHostInput released(float speed){PeHaltHostInput in=standing();in.velocity=(PeVec3){0,0,speed};return in;}
/* Runs until the mask clears; returns the steps it was held, the release step included. */
static unsigned hold(PeHaltHost *h,PeHaltHostInput in,float *largest){
    PeHaltHostOutput out={.block_move=true};unsigned steps=1;
    while(steps<200){
        CHECK(pe_halt_host_step(h,in,&out));
        for(unsigned i=0;i<h->offset_count;i++){PeVec3 o=h->offsets[i].offset;float m=sqrtf(o.x*o.x+o.y*o.y+o.z*o.z);if(largest&&m>*largest)*largest=m;}
        if(!out.block_move)break;
        CHECK(out.block_jump&&!h->moving);steps++;
    }
    CHECK(out.ended&&!out.block_jump&&!h->action.running);return steps;
}
int pe_test_halt_host(void){
    failures=0;PeHaltHost h={0};PeHaltHostOutput out;
    /* Standing still, or releasing without having moved, requests nothing. */
    CHECK(pe_halt_host_step(&h,standing(),&out)&&!out.started&&!out.block_move&&!out.block_jump&&!h.moving);
    /* A walk stop holds the mask for about 0.22 s and moves nothing. */
    CHECK(pe_halt_host_step(&h,moving(2),&out)&&h.moving&&!out.block_move);
    CHECK(pe_halt_host_step(&h,released(2),&out)&&out.started&&out.block_move&&out.block_jump&&!h.moving);
    CHECK(h.offset_count==PE_HALT_LINKS&&h.action.weight==0);
    float largest=0;unsigned steps=hold(&h,moving(2),&largest);
    CHECK(steps>=12&&steps<=15&&largest==0);
    /* Movement held through the lock starts on the first free step, and a second release stops again. */
    CHECK(pe_halt_host_step(&h,moving(2),&out)&&h.moving&&!out.block_move&&h.offset_count==0);
    CHECK(pe_halt_host_step(&h,released(2),&out)&&out.started);hold(&h,standing(),NULL);
    CHECK(pe_halt_host_step(&h,standing(),&out)&&!out.started&&!out.block_move);
    /* A sprint stop lasts 0.9 s; its first pass adds nothing and later ones reach decimetres. */
    CHECK(pe_halt_host_step(&h,moving(8),&out)&&pe_halt_host_step(&h,released(8),&out)&&out.started&&h.action.weight==1);
    for(unsigned i=0;i<h.offset_count;i++)CHECK(h.offsets[i].offset.x==0&&h.offsets[i].offset.y==0&&h.offsets[i].offset.z==0);
    largest=0;steps=hold(&h,standing(),&largest);CHECK(steps>=53&&steps<=56&&largest>.3f);
    CHECK(pe_halt_host_step(&h,standing(),&out)&&h.offset_count==0);
    /* A run stop: 0.5 s and a small weight. */
    CHECK(pe_halt_host_step(&h,moving(5),&out)&&pe_halt_host_step(&h,released(5),&out)&&out.started&&h.action.weight>.1f&&h.action.weight<.25f);
    steps=hold(&h,standing(),NULL);CHECK(steps>=29&&steps<=32);
    /* The stepping foot follows the feet: right when the left foot leads against the motion. */
    PeHaltHostInput back=released(5);back.velocity=(PeVec3){0,0,-5};
    CHECK(pe_halt_host_step(&h,moving(5),&out)&&pe_halt_host_step(&h,back,&out)&&h.action.right_foot);hold(&h,standing(),NULL);
    CHECK(pe_halt_host_step(&h,moving(5),&out)&&pe_halt_host_step(&h,released(5),&out)&&!h.action.right_foot);hold(&h,standing(),NULL);
    /* Refused in the air, in water or with equipment drawn; the release still ends Move. */
    for(int kind=0;kind<3;kind++){
        PeHaltHostInput in=released(8);if(kind==0)in.grounded=in.move_admissible=false;if(kind==1)in.in_water=true;if(kind==2)in.equipment_held=true;
        CHECK(pe_halt_host_step(&h,moving(8),&out)&&h.moving);
        CHECK(pe_halt_host_step(&h,in,&out)&&!out.started&&!out.block_move&&!h.moving&&!h.action.running);
        CHECK(pe_halt_host_step(&h,standing(),&out)&&!out.started);
    }
    /* Losing the ground or an action mask ends Move immediately: a later release requests nothing. */
    PeHaltHostInput air=moving(8);air.grounded=air.move_admissible=false;
    CHECK(pe_halt_host_step(&h,moving(8),&out)&&pe_halt_host_step(&h,air,&out)&&!h.moving&&!out.started);
    CHECK(pe_halt_host_step(&h,released(8),&out)&&!out.started);
    PeHaltHostInput masked=moving(8);masked.move_admissible=false;
    CHECK(pe_halt_host_step(&h,moving(8),&out)&&pe_halt_host_step(&h,masked,&out)&&!h.moving&&!out.started);
    /* A jump ends Move without a Halt, even with the release in the same step. */
    PeHaltHostInput jump=released(8);jump.jump_requested=true;
    CHECK(pe_halt_host_step(&h,moving(8),&out)&&pe_halt_host_step(&h,jump,&out)&&!out.started&&!out.block_jump&&!h.moving);
    /* During a Halt a jump is refused and does not disturb it. */
    CHECK(pe_halt_host_step(&h,moving(8),&out)&&pe_halt_host_step(&h,released(8),&out)&&out.started);
    PeHalt before=h.action;jump=standing();jump.jump_requested=jump.move_requested=true;
    CHECK(pe_halt_host_step(&h,jump,&out)&&out.block_jump&&out.block_move&&h.action.running&&h.action.remaining<before.remaining);
    /* Death ends it at once and clears the offsets. */
    PeHaltHostInput dead=standing();dead.alive=false;
    CHECK(pe_halt_host_step(&h,dead,&out)&&out.ended&&!out.block_move&&!h.action.running&&!h.action.mask&&h.offset_count==0);
    CHECK(pe_halt_host_step(&h,dead,&out)&&!out.ended);
    dead.move_requested=true;CHECK(pe_halt_host_step(&h,dead,&out)&&!h.moving);
    /* Reset drops a running Halt and the Move state. */
    CHECK(pe_halt_host_step(&h,moving(8),&out)&&pe_halt_host_step(&h,released(8),&out)&&out.started);
    pe_halt_host_reset(&h);CHECK(!h.action.running&&!h.action.mask&&!h.moving&&h.offset_count==0);pe_halt_host_reset(NULL);
    CHECK(pe_halt_host_step(&h,standing(),&out)&&!out.block_move&&!out.ended);
    /* Invalid input changes nothing. */
    CHECK(pe_halt_host_step(&h,moving(8),&out));PeHaltHost kept=h;PeHaltHostOutput same=out;PeHaltHostInput bad=released(8);
    bad.delta_time=0;CHECK(!pe_halt_host_step(&h,bad,&out));bad.delta_time=NAN;CHECK(!pe_halt_host_step(&h,bad,&out));
    bad=released(8);bad.velocity.x=NAN;CHECK(!pe_halt_host_step(&h,bad,&out));bad=released(8);bad.left_foot.y=INFINITY;CHECK(!pe_halt_host_step(&h,bad,&out));
    CHECK(!pe_halt_host_step(NULL,released(8),&out)&&!pe_halt_host_step(&h,released(8),NULL));
    CHECK(!memcmp(&kept,&h,sizeof h)&&!memcmp(&same,&out,sizeof out));
    return failures?1:0;
}
