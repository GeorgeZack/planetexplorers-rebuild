#include "pe_hare_pounce.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"pounce:%d %s\n",__LINE__,#c);failures++;}}while(0)
static PeHarePounceInput input(float now){return (PeHarePounceInput){.now=now,.gravity=9.81f,.sqr_distance_logic=9,.position={0,1,0},.target_position={0,1,4},.forward={0,0,1},.direction_xz={0,0,4},.selected=true,.target_present=true,.in_scope=true};}
static void admission(void){
    PeHarePounce s;PeHarePounceRequests r;pe_hare_pounce_init(&s);
    const PeHarePounceDef*d=pe_hare_pounce_def();CHECK(d&&d->skill_id==30100053&&d->trigger_id==10250&&d->weight==.5f&&d->cooldown==5&&d->skill_duration==.967f);
    CHECK(!pe_hare_pounce_ready_cd(&s,5,true)&&pe_hare_pounce_ready_cd(&s,nextafterf(5,INFINITY),true));CHECK(!pe_hare_pounce_ready_cd(&s,6,false));
    PeHarePounceInput i=input(6);i.sqr_distance_logic=4;CHECK(pe_hare_pounce_ready_geometry(i));i.sqr_distance_logic=nextafterf(4,0);CHECK(!pe_hare_pounce_ready_geometry(i));
    i.sqr_distance_logic=25;CHECK(pe_hare_pounce_ready_geometry(i));i.sqr_distance_logic=nextafterf(25,INFINITY);CHECK(!pe_hare_pounce_ready_geometry(i));
    i=input(6);i.in_scope=false;CHECK(!pe_hare_pounce_ready_geometry(i));i.in_scope=true;i.blocked=true;CHECK(!pe_hare_pounce_ready_geometry(i));i.blocked=false;i.target_present=false;CHECK(!pe_hare_pounce_ready_geometry(i));
    i=input(6);i.selected=false;PeHarePounce before=s;CHECK(pe_hare_pounce_begin(&s,i,&r)&&r.status==PE_HARE_POUNCE_FAILURE&&!memcmp(&before,&s,sizeof s));
    i.selected=true;CHECK(pe_hare_pounce_begin(&s,i,&r)&&r.status==PE_HARE_POUNCE_RUNNING&&r.gravity_set&&r.gravity==0&&r.clear_movement_effect&&r.stop_move&&!r.start_skill&&s.last_cooldown==0);
    i.now=7;CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.start_skill&&r.skill_id==30100053&&r.animator_set&&r.animator_attack5&&!r.move_direction_set&&!r.speed_set&&!r.stop_move);
    CHECK(s.last_cooldown==i.now&&s.start_time==i.now);CHECK(!pe_hare_pounce_ready_cd(&s,i.now+5,true)&&pe_hare_pounce_ready_cd(&s,nextafterf(i.now+5,INFINITY),true));
    CHECK(pe_hare_pounce_reset(&s,&r)&&r.gravity_set&&r.gravity==9.81f&&r.speed_set&&r.speed==0&&r.animator_set&&!r.animator_attack5&&r.move_direction_set&&r.face_direction_set&&!r.start_skill);
    CHECK(!s.active&&s.last_cooldown==i.now);CHECK(pe_hare_pounce_reset(&s,&r)&&!r.gravity_set);
}
static void boundaries(void){
    PeHarePounce s;PeHarePounceRequests r;PeHarePounceInput i=input(0);pe_hare_pounce_init(&s);
    /* Init consumes an already accepted selection, rather than re-running CD.
     * Zero epoch makes float subtraction boundaries exactly representable. */
    CHECK(pe_hare_pounce_begin(&s,i,&r)&&pe_hare_pounce_tick(&s,i,&r)&&r.start_skill);
    bool independent_skill_running=true;
    i.now=nextafterf(.267f,0);CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.stop_move&&!r.speed_set&&!r.move_direction_set);
    i.now=.267f;CHECK(pe_hare_pounce_tick(&s,i,&r)&&!r.stop_move&&r.speed_set&&r.move_direction_set&&r.face_direction_set);
    float speed=5/(.6f-.267f);CHECK(s.speed==speed&&r.speed==speed&&r.move_direction.z==1&&r.face_direction.z==4);
    i.now=nextafterf(.267f,INFINITY);i.target_position.x=100;i.direction_xz=(PeVec3){100,0,4};i.forward=(PeVec3){1,0,0};i.in_scope=false;
    CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.speed==speed&&r.move_direction.x==1&&r.face_direction.x==100); /* Face latch; speed only once. */
    i.now=.6f;CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.status==PE_HARE_POUNCE_RUNNING&&r.speed_set&&r.speed==speed);
    i.now=nextafterf(.6f,INFINITY);CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.status==PE_HARE_POUNCE_SUCCESS&&r.stop_move&&r.animator_set&&!r.animator_attack5&&r.move_direction_set&&r.face_direction_set&&r.pursue_run&&r.pursuit_target.x==100&&!r.speed_set&&!r.start_skill);
    CHECK(i.now<pe_hare_pounce_def()->stop_time&&independent_skill_running);
    /* The normally unreachable stopped tail is preserved, not a mandatory wait. */
    i.now=.967f;CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.status==PE_HARE_POUNCE_RUNNING&&!r.pursue_run);
    i.now=nextafterf(.967f,INFINITY);CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.status==PE_HARE_POUNCE_SUCCESS);
    CHECK(pe_hare_pounce_reset(&s,&r)&&!r.gravity_set&&!r.start_skill&&independent_skill_running); /* Original rotate_time=0 Reset guard. */
    const PeHarePounceDef*d=pe_hare_pounce_def();CHECK(!pe_hare_pounce_window(nextafterf(d->window_start,0))&&pe_hare_pounce_window(d->window_start));
    CHECK(pe_hare_pounce_window(d->window_end)&&!pe_hare_pounce_window(nextafterf(d->window_end,INFINITY))&&!pe_hare_pounce_window(NAN));
}
static void timeout_and_loss(void){
    PeHarePounce s;PeHarePounceRequests r;pe_hare_pounce_init(&s);PeHarePounceInput i=input(8);
    CHECK(pe_hare_pounce_begin(&s,i,&r));i.in_scope=false;
    CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.face_direction_set&&r.face_direction.z==4&&!r.start_skill);
    i.now=13;CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.status==PE_HARE_POUNCE_RUNNING);
    i.now=nextafterf(13,INFINITY);CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.status==PE_HARE_POUNCE_FAILURE&&s.active&&s.last_cooldown==0&&!r.gravity_set);
    CHECK(pe_hare_pounce_reset(&s,&r)&&r.gravity_set&&r.gravity==9.81f);
    i=input(14);CHECK(pe_hare_pounce_begin(&s,i,&r));i.target_present=false;
    CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.status==PE_HARE_POUNCE_FAILURE&&!r.start_skill&&!r.gravity_set&&!r.face_direction_set&&s.active);
    CHECK(pe_hare_pounce_reset(&s,&r)&&r.gravity_set);
    /* Alignment succeeds even after5s if the target is in scope on that tick. */
    i=input(15);CHECK(pe_hare_pounce_begin(&s,i,&r));i.now=21;CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.start_skill&&r.status==PE_HARE_POUNCE_RUNNING);
    i.now=21.1f;i.target_present=false;CHECK(pe_hare_pounce_tick(&s,i,&r)&&r.status==PE_HARE_POUNCE_FAILURE&&!r.start_skill);CHECK(pe_hare_pounce_reset(&s,&r)&&!r.start_skill&&r.gravity_set);
}
static void invalid(void){
    PeHarePounce s;PeHarePounceRequests r={.skill_id=77},saved=r;pe_hare_pounce_init(&s);PeHarePounceInput i=input(6);PeHarePounce before=s;
    i.now=NAN;CHECK(!pe_hare_pounce_begin(&s,i,&r)&&!memcmp(&s,&before,sizeof s)&&!memcmp(&r,&saved,sizeof r));
    i=input(6);CHECK(pe_hare_pounce_begin(&s,i,&r));before=s;saved=r;i.now=5;CHECK(!pe_hare_pounce_tick(&s,i,&r)&&!memcmp(&s,&before,sizeof s)&&!memcmp(&r,&saved,sizeof r));
    i.now=6;i.forward.x=INFINITY;CHECK(!pe_hare_pounce_tick(&s,i,&r)&&!memcmp(&s,&before,sizeof s));
    i=input(6);CHECK(pe_hare_pounce_tick(&s,i,&r));i.now=6.4f;i.target_position.x=FLT_MAX;before=s;saved=r;
    CHECK(!pe_hare_pounce_tick(&s,i,&r)&&!memcmp(&s,&before,sizeof s)&&!memcmp(&r,&saved,sizeof r));
    CHECK(!pe_hare_pounce_tick(&s,input(6.4f),NULL));CHECK(!pe_hare_pounce_begin(&s,input(7),&r));
    s.speed=NAN;saved=r;CHECK(!pe_hare_pounce_reset(&s,&r)&&!memcmp(&r,&saved,sizeof r));CHECK(!pe_hare_pounce_ready_cd(NULL,7,true));
}
int pe_test_hare_pounce(void){failures=0;admission();boundaries();timeout_and_loss();invalid();return failures;}
