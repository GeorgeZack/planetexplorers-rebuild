#include "pe_halt.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static bool near(float a,float b){return fabsf(a-b)<=2e-5f+fabsf(b)*2e-5f;}
static PeHaltStart start(float vx,float vz){
    /* Standing feet: left at -x, right at +x. */
    return (PeHaltStart){.effect_present=true,.motor_present=true,.velocity={vx,-3,vz},
        .left_foot={-.1f,0,0},.right_foot={.1f,0,0}};
}
static PeHaltTick tick(float dt){
    return (PeHaltTick){.delta_time=dt,.effect_present=true,.motor_present=true,.effect_enabled=true,.master_weight=1};
}
static const PeHaltOffset *find(const PeHaltUpdate *u,unsigned effector){
    for(unsigned i=0;i<u->offset_count;i++)if(u->offsets[i].effector==effector)return &u->offsets[i];
    return NULL;
}
static void relation_and_profile(void){
    PeHaltRelation r=pe_halt_relation();
    assert(r.forbidden_masks&(UINT64_C(1)<<1));   /* InAir */
    assert(r.forbidden_masks&(UINT64_C(1)<<13));  /* HoldEquip: no stop effect with a held tool */
    assert(!(r.forbidden_masks&(UINT64_C(1)<<16)));/* the Halt mask does not forbid Halt itself */
    assert(r.end_immediately_actions==((UINT64_C(1)<<1)|(UINT64_C(1)<<2)|(UINT64_C(1)<<3))); /* Move, Sprint, Rotate */
    assert(pe_halt_source_admissible(0)&&!pe_halt_source_admissible(UINT64_C(1)<<1)&&!pe_halt_source_admissible(UINT64_C(1)<<56));
    float w,t;
    assert(pe_halt_profile(0,&w,&t)&&w==0&&t==.200000048f);
    assert(pe_halt_profile(2,&w,&t)&&w==0&&t>.2f&&t<.5f);     /* a walk stop still holds Halt, with no visible effect */
    assert(pe_halt_profile(4.5f,&w,&t)&&w==0);
    assert(pe_halt_profile(5,&w,&t)&&w>0&&w<.3f&&near(t,.50000006f));
    assert(pe_halt_profile(8,&w,&t)&&w==1&&t==.899999976f);
    assert(pe_halt_profile(30,&w,&t)&&w==1&&t==.899999976f);  /* clamped beyond the last key */
    float w2;assert(pe_halt_profile(6.5f,&w2,&t)&&w2>.3f&&w2<1&&t>.5f&&t<.9f);
}
static void foot_choice_and_lifecycle(void){
    PeHalt a={0};PeHaltUpdate u;
    /* Moving toward +x: right-to-left vector points -x, more than 90 degrees from the motion: right foot. */
    assert(pe_halt_begin(&a,start(8,0),&u)&&u.started&&u.set_mask&&u.mask);
    assert(a.right_foot&&a.effect_running&&near(a.direction.x,1)&&a.direction.y==0&&a.weight==1&&a.remaining==.899999976f);
    PeHalt before=a;
    assert(pe_halt_begin(&a,start(0,8),&u)&&u.repeat_ignored&&!memcmp(&a,&before,sizeof a));
    /* First frame: elapsed 0, so the crossfade is 0 and every offset is zero. */
    assert(pe_halt_step(&a,tick(1/60.f),&u)&&!u.ended&&u.offset_count==PE_HALT_LINKS&&u.progress==0);
    for(unsigned i=0;i<u.offset_count;i++)assert(u.offsets[i].offset.x==0&&u.offsets[i].offset.y==0&&u.offsets[i].offset.z==0);
    /* The planted right foot (effector 8) is pushed along the motion; mid-effect ~.58 m at full weight. */
    unsigned frames=1;float peak=0,body_low=0;
    for(;;){
        assert(pe_halt_step(&a,tick(1/60.f),&u));
        if(u.ended)break;
        frames++;assert(u.offset_count==PE_HALT_LINKS&&find(&u,8)&&find(&u,7)&&find(&u,0));
        peak=fmaxf(peak,find(&u,8)->offset.x);body_low=fminf(body_low,find(&u,0)->offset.y);
        assert(find(&u,8)->offset.z==0&&frames<80);
    }
    assert(frames==55);                 /* .9 s at 60 Hz: 54 offset frames reach zero, the 55th Update still sees it running */
    assert(peak>.55f&&peak<.6f&&body_low<-.4f&&body_low>-.5f);
    assert(u.set_mask&&!u.mask&&u.offset_count==0&&!a.running&&!a.mask&&!a.effect_running);
    assert(pe_halt_step(&a,tick(1/60.f),&u)&&!u.ended&&u.offset_count==0);
    /* Moving toward -x: left foot. Its fade is .01 s, the right foot's .3 s. */
    assert(pe_halt_begin(&a,start(-8,0),&u)&&!a.right_foot&&near(a.direction.x,-1));
    assert(pe_halt_step(&a,tick(1/60.f),&u)&&pe_halt_step(&a,tick(1/60.f),&u));
    assert(find(&u,7)&&find(&u,7)->offset.x<0); /* left foot offset follows the motion direction */
    assert(pe_halt_cancel(&a,&u)&&u.ended&&!a.mask&&!a.effect_running&&a.remaining==0);
    assert(pe_halt_cancel(&a,&u)&&!u.ended);
    /* Straight ahead with level feet is exactly 90 degrees, and a standing stop has no direction: both left. */
    assert(pe_halt_begin(&a,start(0,8),&u)&&!a.right_foot&&pe_halt_cancel(&a,&u));
    assert(pe_halt_begin(&a,start(0,0),&u)&&!a.right_foot&&a.direction.x==0&&a.weight==0&&a.remaining==.200000048f);
    /* A standing or walking stop still holds the mask for its duration, with zero offsets. */
    frames=0;
    do{assert(pe_halt_step(&a,tick(1/60.f),&u));frames++;
       for(unsigned i=0;i<u.offset_count;i++)assert(u.offsets[i].offset.x==0&&u.offsets[i].offset.y==0);}while(!u.ended);
    assert(frames==14); /* .2 s: 13 offset frames (the float sum crosses on the 13th), ended on the 14th Update */
}
static void missing_parts_and_weight(void){
    PeHalt a={0};PeHaltUpdate u;PeHaltStart s=start(8,0);
    /* No effect or no motor: the mask is set, no effect starts, and the first Update ends it. */
    s.effect_present=false;assert(pe_halt_begin(&a,s,&u)&&a.mask&&!a.effect_running);
    assert(pe_halt_step(&a,tick(1/60.f),&u)&&u.ended&&u.offset_count==0);
    s=start(8,0);s.motor_present=false;assert(pe_halt_begin(&a,s,&u)&&!a.effect_running);
    assert(pe_halt_step(&a,tick(1/60.f),&u)&&u.ended);
    /* Master weight 0 ends the action at once but the effect's time is not consumed. */
    assert(pe_halt_begin(&a,start(8,0),&u));PeHaltTick t=tick(1/60.f);t.master_weight=0;
    assert(pe_halt_step(&a,t,&u)&&u.ended&&u.offset_count==0&&a.effect_running&&a.remaining==.899999976f);
    /* The lingering effect keeps writing offsets once the weight returns, with no action held. */
    assert(pe_halt_step(&a,tick(.1f),&u)&&pe_halt_step(&a,tick(.1f),&u)&&!u.ended&&u.offset_count==PE_HALT_LINKS&&!a.running);
    assert(pe_halt_cancel(&a,&u)&&!a.effect_running);
    /* Disabled component or zero delta: action ends / no time passes. */
    assert(pe_halt_begin(&a,start(8,0),&u));t=tick(0);
    assert(pe_halt_step(&a,t,&u)&&!u.ended&&u.offset_count==0&&a.remaining==.899999976f);
    t=tick(1/60.f);t.effect_enabled=false;assert(pe_halt_step(&a,t,&u)&&u.ended&&a.remaining==.899999976f);
    assert(pe_halt_cancel(&a,&u));
    /* Half master weight halves the offsets; above 1 is clamped. */
    PeHalt b={0};PeHaltUpdate full,half,over;
    assert(pe_halt_begin(&a,start(8,0),&u)&&pe_halt_begin(&b,start(8,0),&u));
    for(int i=0;i<20;i++){assert(pe_halt_step(&a,tick(1/60.f),&full));t=tick(1/60.f);t.master_weight=.5f;assert(pe_halt_step(&b,t,&half));}
    assert(near(find(&half,8)->offset.x,find(&full,8)->offset.x*.5f));
    PeHalt c={0};assert(pe_halt_begin(&c,start(8,0),&u));
    for(int i=0;i<20;i++){t=tick(1/60.f);t.master_weight=3;assert(pe_halt_step(&c,t,&over));}
    assert(near(find(&over,8)->offset.x,find(&full,8)->offset.x));
}
static void invalid_input(void){
    PeHalt a={0},before;PeHaltUpdate u,original;
    assert(pe_halt_begin(&a,start(8,0),&u));before=a;memset(&u,0x5A,sizeof u);original=u;
    assert(!pe_halt_step(&a,tick(-1),&u)&&!pe_halt_step(&a,tick(NAN),&u)&&!pe_halt_step(&a,tick(.1f),NULL));
    PeHaltTick t=tick(.1f);t.master_weight=NAN;assert(!pe_halt_step(&a,t,&u)&&!pe_halt_step(NULL,tick(.1f),&u));
    PeHaltStart s=start(NAN,0);PeHalt idle={0};
    assert(!pe_halt_begin(&idle,s,&u)&&!pe_halt_begin(NULL,start(1,0),&u)&&!pe_halt_begin(&idle,start(1,0),NULL));
    s=start(1,0);s.left_foot.y=INFINITY;assert(!pe_halt_begin(&idle,s,&u)&&!idle.running);
    assert(!pe_halt_cancel(NULL,&u)&&!pe_halt_cancel(&a,NULL));
    float w=7,time=8;assert(!pe_halt_profile(-1,&w,&time)&&!pe_halt_profile(NAN,&w,&time)&&!pe_halt_profile(1,NULL,&time)&&w==7&&time==8);
    assert(!memcmp(&a,&before,sizeof a)&&!memcmp(&u,&original,sizeof u));
    PeHalt bad={.remaining=2,.step_time=1};assert(!pe_halt_step(&bad,tick(.1f),&u));
}
int pe_test_halt(void){
    relation_and_profile();foot_choice_and_lifecycle();missing_parts_and_weight();invalid_input();
    return 0;
}
