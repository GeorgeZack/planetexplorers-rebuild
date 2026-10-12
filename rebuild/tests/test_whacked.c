#include "pe_whacked.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static bool near(float a,float b){return fabsf(a-b)<=1e-5f;}
static PeWhackedTick tick(float dt,float style){
    return (PeWhackedTick){.delta_time=dt,.animator_present=true,.move_style=style};
}
static void relation_and_selection(void){
    PeWhackedRelation r=pe_whacked_relation();
    assert(r.forbidden_masks&(UINT64_C(1)<<47)); /* Whacked forbids its own mask */
    assert(r.forbidden_masks&(UINT64_C(1)<<6));  /* and Repulsed */
    assert(!(r.forbidden_masks&(UINT64_C(1)<<1))); /* but not InAir, unlike Repulsed */
    assert(r.pause_actions&(UINT64_C(1)<<15));   /* running shield is paused */
    assert(r.end_immediately_actions&(UINT64_C(1)<<14)); /* sword attack ends */
    assert(pe_whacked_source_admissible(0)&&!pe_whacked_source_admissible(UINT64_C(1)<<47));
    assert(!pe_whacked_source_admissible(UINT64_C(1)<<9)&&pe_whacked_source_admissible(UINT64_C(1)<<1));
    assert(!pe_whacked_source_admissible(UINT64_C(1)<<56));
    PeWhackedUpdate s;
    assert(pe_whacked_selection(0,&s)&&s.clip_a==139&&s.weight_b==0&&s.state_length==.933333635f);
    assert(pe_whacked_selection(1,&s)&&s.clip_a==157&&s.clip_b==188&&s.weight_b==0); /* Sword/knife style */
    assert(s.state_length==.93334198f);
    /* MoveStyle is smoothed, so a style change mid-way blends two leaves. */
    assert(pe_whacked_selection(.25f,&s)&&s.clip_a==139&&s.clip_b==157&&near(s.weight_b,.25f));
    assert(pe_whacked_selection(7,&s)&&s.clip_a==139); /* Carry reuses the idle clip */
    assert(pe_whacked_selection(10,&s)&&s.clip_b==116&&s.weight_b==1);
    assert(pe_whacked_selection(13,&s)&&s.clip_b==116&&s.weight_b==1); /* styles past Gloves clamp */
    assert(pe_whacked_selection(-2,&s)&&s.clip_a==139&&s.weight_b==0);
}
static void lifecycle(void){
    PeWhacked a={0};PeWhackedUpdate u;
    assert(pe_whacked_begin(&a,true,&u)&&u.started&&u.set_mask&&u.mask&&u.set_trigger_be_hit);
    assert(a.running&&a.mask&&a.presenting&&a.normalized_time==0);
    /* A second hit while running changes nothing. */
    PeWhacked before=a;
    assert(pe_whacked_begin(&a,true,&u)&&u.repeat_ignored&&!u.started&&!memcmp(&a,&before,sizeof a));
    /* 30 Hz: the idle clip is 28 samples long; the exit starts at .9. */
    int frames=0;bool exiting=false;
    for(;;){
        assert(pe_whacked_step(&a,tick(1/30.f,0),&u));frames++;
        if(u.ended)break;
        assert(u.presenting&&u.clip_a==139&&!u.hit_anim_end&&u.normalized_time<1);
        if(u.exit_blend>0){exiting=true;assert(u.normalized_time>.9f);}
        else assert(!exiting&&u.normalized_time<=.9f+1e-6f);
        assert(frames<40);
    }
    assert(exiting&&frames==29); /* .9333336 s needs 29 steps of 1/30 s to reach 1 */
    assert(u.hit_anim_end&&u.set_mask&&!u.mask&&u.set_trigger_reset_full_body&&u.reset_trigger_be_hit);
    assert(u.exit_blend==1&&near(u.normalized_time,1)&&!u.presenting);
    assert(u.continue_actions==pe_whacked_relation().pause_actions);
    assert(!a.running&&!a.mask&&!a.presenting&&a.normalized_time==0);
    assert(pe_whacked_step(&a,tick(1/30.f,0),&u)&&!u.ended&&!u.hit_anim_end); /* idle afterwards */
    /* The total is the state length, independent of step size. */
    assert(pe_whacked_begin(&a,true,&u));float elapsed=0;
    do{assert(pe_whacked_step(&a,tick(.001f,1),&u));elapsed+=.001f;}while(!u.ended);
    assert(fabsf(elapsed-.93334198f)<.0015f&&u.clip_a==157);
    /* A later admitted hit restarts from zero. */
    assert(pe_whacked_begin(&a,true,&u)&&u.started&&a.normalized_time==0);
    assert(pe_whacked_step(&a,tick(.5f,1),&u)&&!u.ended&&u.exit_blend==0);
    /* Manager cleanup. */
    assert(pe_whacked_cancel(&a,true,&u)&&u.ended&&!u.hit_anim_end&&u.set_trigger_reset_full_body&&!a.mask);
    assert(pe_whacked_cancel(&a,true,&u)&&!u.ended);
}
static void missing_animator(void){
    PeWhacked a={0};PeWhackedUpdate u;
    /* DoAction still sets the mask; Update then reports completion and the
     * manager drops the action without EndImmediately, leaving the mask set. */
    assert(pe_whacked_begin(&a,false,&u)&&u.set_mask&&u.mask&&!u.set_trigger_be_hit&&!a.presenting);
    PeWhackedTick t=tick(1/60.f,0);t.animator_present=false;
    assert(pe_whacked_step(&a,t,&u)&&u.ended&&!u.set_mask&&!u.hit_anim_end&&!u.set_trigger_reset_full_body);
    assert(!a.running&&a.mask&&u.continue_actions==pe_whacked_relation().pause_actions);
    /* Losing the animator mid-reaction takes the same path. */
    assert(pe_whacked_cancel(&a,false,&u)&&!u.ended); /* not running: nothing to end */
    a=(PeWhacked){0};
    assert(pe_whacked_begin(&a,true,&u)&&pe_whacked_step(&a,tick(.1f,0),&u)&&u.presenting);
    assert(pe_whacked_step(&a,t,&u)&&u.ended&&a.mask&&!a.presenting);
    /* An animator arriving after DoAction never received the trigger: no clock runs. */
    a=(PeWhacked){0};
    assert(pe_whacked_begin(&a,false,&u));
    for(int i=0;i<200;i++)assert(pe_whacked_step(&a,tick(1/30.f,0),&u)&&!u.ended&&!u.presenting&&u.clip_a==0);
    assert(a.running&&pe_whacked_cancel(&a,true,&u)&&u.ended&&!a.mask);
}
static void invalid_input(void){
    PeWhacked a={0},before;PeWhackedUpdate u,original;
    assert(pe_whacked_begin(&a,true,&u));before=a;memset(&u,0x5A,sizeof u);original=u;
    assert(!pe_whacked_step(&a,tick(-1,0),&u)&&!pe_whacked_step(&a,tick(NAN,0),&u));
    assert(!pe_whacked_step(&a,tick(.1f,INFINITY),&u)&&!pe_whacked_step(&a,tick(.1f,0),NULL));
    assert(!pe_whacked_step(NULL,tick(.1f,0),&u)&&!pe_whacked_begin(NULL,true,&u)&&!pe_whacked_begin(&a,true,NULL));
    assert(!pe_whacked_cancel(NULL,true,&u)&&!pe_whacked_cancel(&a,true,NULL)&&!pe_whacked_selection(NAN,&u));
    assert(!memcmp(&a,&before,sizeof a)&&!memcmp(&u,&original,sizeof u));
    PeWhacked bad={.presenting=true}; /* presenting without a running action */
    assert(!pe_whacked_step(&bad,tick(.1f,0),&u)&&!memcmp(&u,&original,sizeof u));
    bad=(PeWhacked){.running=true,.normalized_time=NAN};assert(!pe_whacked_begin(&bad,true,&u));
}
int pe_test_whacked(void){
    relation_and_selection();lifecycle();missing_animator();invalid_input();
    return 0;
}
