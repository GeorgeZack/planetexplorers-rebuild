#include "pe_town_defense_spawn.h"
#include "pe_town_defense_clock.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {unsigned calls,reals;bool upper,east;int fail_at,bad;char kind[64];} Tape;
static bool integer(void *context,int low,int high,int *out){
    Tape *t=context;assert(t->calls<sizeof t->kind);t->kind[t->calls++]='I';
    assert((low==8&&high==10)||(low==2&&high==3));
    if(t->fail_at>0&&(int)t->calls==t->fail_at)return false;
    *out=t->bad==1?high:t->bad==2?low-1:t->upper?high-1:low;return true;
}
static bool real(void *context,float low,float high,float *out){
    Tape *t=context;assert(t->calls<sizeof t->kind);t->kind[t->calls++]='R';
    assert((low==0&&high==360)||(low==20&&high==80));
    if(t->fail_at>0&&(int)t->calls==t->fail_at)return false;
    unsigned pair=t->reals/2;
    *out=low==0?90*(float)(pair%5):20+15*(float)(pair%5);t->reals++;
    if(t->east)*out=low==0?90:80;
    if(t->bad==3)*out=NAN;
    if(t->bad==4)*out=high+1;
    return true;
}
static PeTownDefenseDraws draws(Tape *t){return (PeTownDefenseDraws){t,integer,real};}
static bool near(float a,float b){return fabsf(a-b)<.0001f;}
static PeTownDefenseWavePlan plan(unsigned wave,bool upper){
    Tape t={.upper=upper};PeTownDefenseWavePlan p={0};
    assert(pe_town_defense_plan_wave(wave,(PeVec3){-284,27,134},(PeVec3){0,0,1},draws(&t),&p));
    assert(pe_town_defense_plan_valid(&p)&&p.count==(upper?11u:10u));
    unsigned first=upper?9:8;assert(t.calls==p.count*2+2&&t.kind[0]=='I'&&t.kind[1+first*2]=='I');
    for(unsigned i=0;i<t.calls;i++)if(i!=0&&i!=1+first*2)assert(t.kind[i]=='R');
    for(unsigned i=0;i<p.count;i++){
        assert(p.requests[i].anchor.y==0);
        assert(p.requests[i].prototype==(i<first?(wave?7:13):4));
        float dx=p.requests[i].anchor.x-p.center.x,dz=p.requests[i].anchor.z-p.center.z;
        assert(near(hypotf(dx,dz),p.requests[i].radius));
    }
    return p;
}
static void planner_tests(void){
    PeTownDefenseWavePlan p=plan(0,true),minimum=plan(1,false);assert(minimum.count==10);
    assert(near(p.requests[0].anchor.x,-284)&&near(p.requests[0].anchor.z,154));
    assert(near(p.requests[1].anchor.x,-249)&&near(p.requests[1].anchor.z,134));
    assert(near(p.requests[2].anchor.x,-284)&&near(p.requests[2].anchor.z,84));
    assert(near(p.requests[3].anchor.x,-349)&&near(p.requests[3].anchor.z,134));
    assert(near(p.requests[4].anchor.z,214)&&near(p.requests[4].radius,80));
    /* The third sampled radius is exactly50 (uniform radius midpoint), not
     * sqrt((20^2+80^2)/2). No route-fit clamping of the south annulus. */
    Tape outside={.east=true};PeTownDefenseWavePlan uncovered;
    assert(pe_town_defense_plan_wave(0,p.center,p.forward,draws(&outside),&uncovered));
    assert(near(uncovered.requests[0].anchor.x,-204));
    Tape t={0};PeTownDefenseWavePlan turned;
    assert(pe_town_defense_plan_wave(0,p.center,(PeVec3){10,999,0},draws(&t),&turned));
    assert(near(turned.requests[0].anchor.x,-264)&&near(turned.requests[0].anchor.z,134));
    assert(near(turned.requests[1].anchor.x,-284)&&near(turned.requests[1].anchor.z,99));
    PeTownDefenseWavePlan before=p;
    for(int bad=1;bad<=4;bad++){
        t=(Tape){.bad=bad};assert(!pe_town_defense_plan_wave(0,p.center,p.forward,draws(&t),&p));assert(!memcmp(&p,&before,sizeof p));
    }
    for(int at=1;at<=22;at++){
        t=(Tape){.fail_at=at};assert(!pe_town_defense_plan_wave(0,p.center,p.forward,draws(&t),&p));assert(!memcmp(&p,&before,sizeof p));
    }
    t=(Tape){0};assert(!pe_town_defense_plan_wave(2,p.center,p.forward,draws(&t),&p));assert(!t.calls);
    assert(!pe_town_defense_plan_wave(0,p.center,(PeVec3){0,1,0},draws(&t),&p)&&!t.calls);
    assert(!pe_town_defense_plan_wave(0,(PeVec3){NAN,0,0},p.forward,draws(&t),&p)&&!t.calls);
    assert(!pe_town_defense_plan_wave(0,p.center,p.forward,(PeTownDefenseDraws){0},&p));
    p=before;p.requests[0].anchor.x+=1;assert(!pe_town_defense_plan_valid(&p));
    p=minimum;p.requests[10].radius=20;assert(!pe_town_defense_plan_valid(&p));
}
static void lifecycle_tests(void){
    PeTownDefenseWavePlan p0=plan(0,true),p1=plan(1,true);
    PeTownDefenseSpawn s={0};assert(!pe_town_defense_spawn_valid(&s));
    assert(pe_town_defense_spawn_init(&s,55,p0.center,p0.forward)&&pe_town_defense_spawn_valid(&s));
    PeTownDefenseSpawn before=s;
    assert(!pe_town_defense_spawn_init(&s,0,p0.center,p0.forward)&&!memcmp(&s,&before,sizeof s));
    assert(!pe_town_defense_spawn_admit(&s,&p1)&&!memcmp(&s,&before,sizeof s));
    assert(pe_town_defense_spawn_admit(&s,&p0)&&s.count==11&&s.waves_admitted==1&&!s.deaths);
    before=s;assert(!pe_town_defense_spawn_admit(&s,&p0)&&!memcmp(&s,&before,sizeof s));
    /* The second wave is admitted even though no first-wave agent exists yet. */
    assert(pe_town_defense_spawn_admit(&s,&p1)&&s.count==22&&s.waves_admitted==2);
    before=s;
    PeVec3 position=s.members[0].request.anchor;position.y=28;
    assert(!pe_town_defense_spawn_created(&s,1,100,13,position,false)&&!memcmp(&s,&before,sizeof s));
    assert(pe_town_defense_spawn_death(&s,55,100)==PE_TOWN_DEFENSE_DEATH_REJECTED&&!memcmp(&s,&before,sizeof s));
    position.x+=.01f;assert(!pe_town_defense_spawn_created(&s,1,100,13,position,true));position.x=s.members[0].request.anchor.x;
    assert(!pe_town_defense_spawn_created(&s,1,100,7,position,true)&&!memcmp(&s,&before,sizeof s));
    assert(pe_town_defense_spawn_created(&s,1,100,13,position,true));
    assert(s.members[0].default_player_id==8&&s.members[0].camp_id==26&&s.members[0].state==PE_TOWN_DEFENSE_AGENT_CREATED);
    before=s;assert(!pe_town_defense_spawn_created(&s,1,101,13,position,true)&&!memcmp(&s,&before,sizeof s));
    position=s.members[1].request.anchor;position.y=28;
    assert(!pe_town_defense_spawn_created(&s,2,100,13,position,true)&&!memcmp(&s,&before,sizeof s));
    assert(!pe_town_defense_spawn_created(&s,2,55,13,position,true));
    for(unsigned i=1;i<18;i++){
        position=s.members[i].request.anchor;position.y=28+(float)i;
        assert(pe_town_defense_spawn_created(&s,i+1,100+i,s.members[i].request.prototype,position,true));
    }
    before=s;
    assert(pe_town_defense_spawn_death(&s,54,100)==PE_TOWN_DEFENSE_DEATH_REJECTED);
    assert(pe_town_defense_spawn_death(&s,55,900)==PE_TOWN_DEFENSE_DEATH_REJECTED&&!memcmp(&s,&before,sizeof s));
    for(unsigned i=0;i<15;i++){
        assert(pe_town_defense_spawn_death(&s,55,100+i)==(i==14?PE_TOWN_DEFENSE_DEATH_COMPLETED:PE_TOWN_DEFENSE_DEATH_CREDITED));
        assert(s.deaths==i+1&&pe_town_defense_spawn_valid(&s));before=s;
        assert(pe_town_defense_spawn_death(&s,55,100+i)==PE_TOWN_DEFENSE_DEATH_REJECTED&&!memcmp(&s,&before,sizeof s));
    }
    assert(s.complete&&s.count==22&&s.members[15].state==PE_TOWN_DEFENSE_AGENT_CREATED&&s.members[18].state==PE_TOWN_DEFENSE_AGENT_PENDING);
    assert(pe_town_defense_spawn_death(&s,55,115)==PE_TOWN_DEFENSE_DEATH_REJECTED&&!memcmp(&s,&before,sizeof s));
    position=s.members[18].request.anchor;position.y=30;
    assert(!pe_town_defense_spawn_created(&s,19,999,s.members[18].request.prototype,position,true)&&!memcmp(&s,&before,sizeof s));
    PeTownDefenseSpawn invalid=s;invalid.members[16].entity_id=invalid.members[15].entity_id;assert(!pe_town_defense_spawn_valid(&invalid));
    invalid=s;invalid.deaths=14;assert(!pe_town_defense_spawn_valid(&invalid));
    invalid=s;invalid.members[21].request.prototype=13;assert(!pe_town_defense_spawn_valid(&invalid));
}
static void clock_integration(void){
    PeTownDefenseClock clock;pe_town_defense_clock_init(&clock);
    PeTownDefenseSpawn owner;assert(pe_town_defense_spawn_init(&owner,1000,(PeVec3){-284,27,134},(PeVec3){0,0,1}));
    unsigned events=0;
    for(int second=0;second<61;second++){
        PeTownDefenseClockEvent event;assert(pe_town_defense_clock_step(&clock,1,false,&event));
        if(event.wave>=0){
            PeTownDefenseWavePlan wave=plan((unsigned)event.wave,false);
            assert(pe_town_defense_spawn_admit(&owner,&wave));events++;
        }
    }
    assert(events==2&&owner.count==20&&!owner.complete&&!owner.deaths);
    assert(pe_town_defense_clock_valid(&clock)&&pe_town_defense_clock_waves_remaining(&clock)==0);
    /* Scheduling finished, all20 agents pending: neither actual spawn nor a
     * completed defense follows from the two clock requests. */
    for(unsigned i=0;i<owner.count;i++)assert(owner.members[i].state==PE_TOWN_DEFENSE_AGENT_PENDING);
}
int main(void){planner_tests();lifecycle_tests();clock_integration();puts("Town Defense planner/member owner: PASS (disconnected; no graphics/assets)");return 0;}
