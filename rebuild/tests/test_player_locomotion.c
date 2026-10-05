#include "pe_player_locomotion.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"player locomotion:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static bool near(double a,double b){return fabs(a-b)<1e-6;}
int pe_test_player_locomotion(void){
    failures=0;double d[]={2,1,.6666666667,1,.6666666667,1};PePlayerLocomotion s,before;PePlayerLocomotionInput in={.grounded=true};
    CHECK(pe_player_locomotion_init(&s,d));CHECK(!strcmp(pe_player_locomotion_name(s.clip),"NormalIdle"));
    CHECK(pe_player_locomotion_step(&s,in,.25));CHECK(near(s.seconds,.25));
    in.velocity.z=2;in.walk=true;CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_WALK&&s.seconds==0);
    CHECK(pe_player_locomotion_step(&s,in,.25));CHECK(near(s.seconds,.25));
    in.walk=false;in.velocity.z=5;CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_RUN&&near(s.seconds/d[s.clip],.25));
    double time=s.seconds;in.sprint=true;in.velocity.z=8;CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(near(s.seconds,time+.16));
    in.velocity=(PeVec3){0};CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_IDLE&&s.seconds==0);
    in.jump_started=true;in.grounded=false;in.velocity.y=7;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(s.clip==PE_LOCOMOTION_JUMP&&s.seconds==0);
    in.jump_started=false;CHECK(pe_player_locomotion_step(&s,in,.84));CHECK(s.clip==PE_LOCOMOTION_JUMP&&near(s.seconds,.84));
    in.velocity.y=-5;CHECK(pe_player_locomotion_step(&s,in,.02));CHECK(s.clip==PE_LOCOMOTION_FALL&&s.seconds==0);
    CHECK(pe_player_locomotion_step(&s,in,1));CHECK(s.clip==PE_LOCOMOTION_FALL&&s.seconds<d[PE_LOCOMOTION_FALL]);
    in.grounded=true;in.velocity.y=0;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(s.clip==PE_LOCOMOTION_LAND&&s.seconds==0);
    CHECK(pe_player_locomotion_step(&s,in,.69));CHECK(s.clip==PE_LOCOMOTION_LAND);
    CHECK(pe_player_locomotion_step(&s,in,.02));CHECK(s.clip==PE_LOCOMOTION_IDLE);
    /* Short ground-probe gaps don't replay Land; ledge falls wait strictly>.3. */
    in.grounded=false;CHECK(pe_player_locomotion_step(&s,in,.2));CHECK(s.clip==PE_LOCOMOTION_IDLE);
    in.grounded=true;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(s.clip==PE_LOCOMOTION_IDLE&&!s.airborne);
    in.grounded=false;CHECK(pe_player_locomotion_step(&s,in,.3));CHECK(s.clip==PE_LOCOMOTION_IDLE);
    in.grounded=true;CHECK(pe_player_locomotion_step(&s,in,.5));CHECK(s.clip==PE_LOCOMOTION_IDLE&&!s.airborne);
    in.grounded=false;CHECK(pe_player_locomotion_step(&s,in,.3));CHECK(s.clip==PE_LOCOMOTION_IDLE);
    CHECK(pe_player_locomotion_step(&s,in,.001));CHECK(s.clip==PE_LOCOMOTION_FALL);
    in.grounded=true;in.velocity.x=5;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(s.clip==PE_LOCOMOTION_RUN);
    /* Jump ground grace, immediate movement exit from land, clocks bound under
     * long sessions, and invalid-state/input atomicity. */
    in.velocity=(PeVec3){0};in.jump_started=true;CHECK(pe_player_locomotion_step(&s,in,.01));in.jump_started=false;
    CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_JUMP);
    CHECK(pe_player_locomotion_step(&s,in,.001));CHECK(s.clip==PE_LOCOMOTION_LAND);
    in.velocity.x=2;in.walk=true;in.sprint=false;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(s.clip==PE_LOCOMOTION_WALK);
    for(int i=0;i<10000;i++){CHECK(pe_player_locomotion_step(&s,in,.1));}
    CHECK(s.seconds>=0&&s.seconds<d[s.clip]);
    before=s;CHECK(!pe_player_locomotion_step(&s,in,NAN));CHECK(!pe_player_locomotion_step(&s,in,-1));CHECK(!pe_player_locomotion_step(&s,in,2));
    in.velocity.x=NAN;CHECK(!pe_player_locomotion_step(&s,in,.1));CHECK(!memcmp(&s,&before,sizeof s));
    d[0]=0;CHECK(!pe_player_locomotion_init(&s,d));CHECK(!memcmp(&s,&before,sizeof s));
    CHECK(!pe_player_locomotion_name(PE_LOCOMOTION_COUNT));return failures;
}
