#include "pe_hit_ik.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>
static bool near(float a,float b){return fabsf(a-b)<2e-5f;}
static PeHitIkStart hit(unsigned id){return (PeHitIkStart){.transform_id=id,.direction={0,0,1},.weight=1,.duration=.6f,.weight_scale=1,.controller_present=true,.solver_present=true};}
static PeHitIkTick tick(float dt){return (PeHitIkTick){.delta_time=dt,.master_weight=1,.weight_scale=1,.controller_present=true,.solver_present=true,.enabled=true};}
static void profile(void){
    PeHitIkProfile p;
    assert(pe_hit_ik_profile(50,0,&p)&&near(p.weight,.7f)&&near(p.duration,.4f));
    assert(pe_hit_ik_profile(200,0,&p)&&near(p.weight,1.1f)&&near(p.duration,.6f));
    assert(pe_hit_ik_profile(398.03656f,0,&p)&&near(p.weight,1.4f));
    assert(pe_hit_ik_profile(400,0,&p)&&near(p.weight,.5f)&&near(p.duration,.85f)); /* deliberate sharp source drop */
    assert(pe_hit_ik_profile(5000,0,&p)&&near(p.weight,.9f)&&near(p.duration,1.1f));
    assert(pe_hit_ik_profile(200,.1f,&p)&&near(p.randomized_force,220));
    assert(pe_hit_ik_profile(200,-.1f,&p)&&near(p.randomized_force,180));
    assert(pe_hit_ik_profile(0,0,&p)&&p.duration>0&&p.duration<.001f);
}
static void routing_and_restart(void){
    const unsigned ids[]={400284,400148,400264,400166,400144,400282,400142,400246,400290,400200,400218,400162,400254,400150,400278,400236};
    PeHitIk a={0};PeHitIkUpdate u;
    for(unsigned i=0;i<16;i++){
        a=(PeHitIk){0};assert(pe_hit_ik_begin(&a,hit(ids[i]),&u));
        assert(u.matched_parts==(i==13?2u:1u));
        assert(u.active_parts==u.matched_parts);
    }
    a=(PeHitIk){0};assert(pe_hit_ik_begin(&a,hit(400284),&u)&&pe_hit_ik_step(&a,tick(.2f),&u));
    PeHitIkStart h=hit(400148);h.direction=(PeVec3){2,1,0};h.root_position.x=4;
    assert(pe_hit_ik_begin(&a,h,&u)&&u.active_parts==2&&near(a.parts[0].remaining,.4f));
    h=hit(400284);h.direction.z=-2;h.root_position.z=3;
    assert(pe_hit_ik_begin(&a,h,&u)&&u.active_parts==2&&near(a.parts[0].remaining,.6f)&&a.parts[0].direction.z==-2&&a.parts[0].root_start.z==3);
    PeHitIk before=a;assert(pe_hit_ik_begin(&a,hit(999),&u)&&u.matched_parts==0&&!memcmp(&a,&before,sizeof a));
    h=hit(400284);h.solver_present=false;assert(pe_hit_ik_begin(&a,h,&u)&&u.matched_parts==0&&!memcmp(&a,&before,sizeof a));
    h=hit(400284);h.controller_present=false;assert(pe_hit_ik_begin(&a,h,&u)&&u.matched_parts==0);
    h=hit(400284);h.duration=0;assert(pe_hit_ik_begin(&a,h,&u)&&u.active_parts==1);
}
static void offsets(void){
    PeHitIk a={0},b={0};PeHitIkUpdate x,y;PeHitIkStart h=hit(400284);
    assert(pe_hit_ik_begin(&a,h,&x));h.weight_scale=.5f;assert(pe_hit_ik_begin(&b,h,&y));
    assert(pe_hit_ik_step(&a,tick(.2f),&x));PeHitIkTick t=tick(.2f);t.weight_scale=.5f;
    assert(pe_hit_ik_step(&b,t,&y));
    for(unsigned i=0;i<9;i++)assert(x.offsets[i].x==0&&x.offsets[i].y==0&&x.offsets[i].z==0); /* old elapsed 0 */
    PeHitIk still=a,moved=a;
    assert(pe_hit_ik_step(&a,tick(.05f),&x)&&pe_hit_ik_step(&b,t,&y));
    for(unsigned i=0;i<9;i++)assert(near(y.offsets[i].z,x.offsets[i].z*.25f)&&near(y.offsets[i].y,x.offsets[i].y*.25f)); /* twice weightScale */
    assert(x.offsets[0].z!=0&&x.offsets[8].z!=0);
    t=tick(.05f);t.root_position=(PeVec3){1,2,3};assert(pe_hit_ik_step(&moved,t,&y)&&pe_hit_ik_step(&still,tick(.05f),&x));
    /* Head pins only body and right foot. Root displacement is subtracted in world space. */
    for(unsigned i=0;i<9;i++){
        bool pin=i==0||i==8;
        assert(near(y.offsets[i].x-x.offsets[i].x,pin?-1:0));
        assert(near(y.offsets[i].y-x.offsets[i].y,pin?-2:0));
        assert(near(y.offsets[i].z-x.offsets[i].z,pin?-3:0));
    }
    /* The left foot starts LFoot plus Foot; their separately authored links add. */
    PeHitIk both={0};assert(pe_hit_ik_begin(&both,hit(400150),&x)&&pe_hit_ik_step(&both,tick(.2f),&x));
    PeHitIk left=both,right=both;left.parts[15].remaining=0;right.parts[13].remaining=0;
    PeHitIkUpdate l,r;assert(pe_hit_ik_step(&both,tick(.1f),&x)&&pe_hit_ik_step(&left,tick(.1f),&l)&&pe_hit_ik_step(&right,tick(.1f),&r));
    for(unsigned i=0;i<9;i++)assert(near(x.offsets[i].z,l.offsets[i].z+r.offsets[i].z)&&near(x.offsets[i].y,l.offsets[i].y+r.offsets[i].y));
    /* Modifier skip paths do not consume time, and master >1 clamps. */
    PeHitIk saved=both;
    t=tick(0);assert(pe_hit_ik_step(&both,t,&x)&&!memcmp(&both,&saved,sizeof both));
    t=tick(.1f);t.master_weight=0;assert(pe_hit_ik_step(&both,t,&x)&&!memcmp(&both,&saved,sizeof both));
    t=tick(.1f);t.enabled=false;assert(pe_hit_ik_step(&both,t,&x)&&!memcmp(&both,&saved,sizeof both));
    t=tick(.1f);t.solver_present=false;assert(pe_hit_ik_step(&both,t,&x)&&!memcmp(&both,&saved,sizeof both));
    t=tick(.1f);t.controller_present=false;assert(pe_hit_ik_step(&both,t,&x)&&!memcmp(&both,&saved,sizeof both));
    a=saved;b=saved;t=tick(.1f);t.master_weight=3;
    assert(pe_hit_ik_step(&a,t,&x)&&pe_hit_ik_step(&b,tick(.1f),&y));assert(!memcmp(x.offsets,y.offsets,sizeof x.offsets));
    assert(pe_hit_ik_step(&a,tick(2),&x)&&x.active_parts==0&&pe_hit_ik_step(&a,tick(.1f),&x));
    for(unsigned i=0;i<9;i++)assert(x.offsets[i].z==0&&x.offsets[i].y==0);
}
static void invalid(void){
    PeHitIk a={0};PeHitIkUpdate u;assert(pe_hit_ik_begin(&a,hit(400284),&u));PeHitIk before=a;
    memset(&u,0x5a,sizeof u);PeHitIkUpdate original=u;
    PeHitIkTick t=tick(NAN);assert(!pe_hit_ik_step(&a,t,&u));t=tick(-1);assert(!pe_hit_ik_step(&a,t,&u));
    t=tick(.1f);t.root_position.x=INFINITY;assert(!pe_hit_ik_step(&a,t,&u));
    t=tick(.1f);t.weight_scale=NAN;assert(!pe_hit_ik_step(&a,t,&u));
    PeHitIkStart h=hit(400284);h.duration=-1;assert(!pe_hit_ik_begin(&a,h,&u));
    h=hit(400284);h.weight=FLT_MAX;h.weight_scale=2;assert(!pe_hit_ik_begin(&a,h,&u));
    assert(!pe_hit_ik_step(NULL,tick(.1f),&u)&&!pe_hit_ik_begin(NULL,hit(1),&u)&&!pe_hit_ik_step(&a,tick(1),NULL));
    assert(!memcmp(&a,&before,sizeof a)&&!memcmp(&u,&original,sizeof u));
    PeHitIkProfile p={1,2,3},saved=p;assert(!pe_hit_ik_profile(1,.11f,&p)&&!pe_hit_ik_profile(NAN,0,&p)&&!pe_hit_ik_profile(-1,0,&p)&&!pe_hit_ik_profile(FLT_MAX,.1f,&p));assert(!memcmp(&p,&saved,sizeof p));
}
int pe_test_hit_ik(void){profile();routing_and_restart();offsets();invalid();return 0;}
