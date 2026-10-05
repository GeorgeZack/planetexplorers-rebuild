#include "pe_combat.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL combat %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
static double event_time(const PeCombatClip *clip,PeCombatEventType type){
    for(unsigned i=0;i<5;i++)if(clip->events[i].type==type)return clip->events[i].time;
    return -1;
}
static PeCombatAdvance to_time(PeCombat *a,double time,float *stamina){
    double delta=time-a->clip_time;return pe_combat_advance(a,delta,delta,stamina);
}
static void window_tests(void){
    /* Exact source event equality opens contact; early calls cannot hit.
     * Run all gender/view schedules, including different event ordering. */
    for(int gender=0;gender<2;gender++)for(int view=0;view<2;view++){
        PeCombat a;pe_combat_init(&a);float stamina=100;
        CHECK(pe_combat_begin(&a,(PeCombatGender)gender,(PeCombatView)view,1,&stamina));
        CHECK(stamina==95&&!a.contact_open&&!pe_combat_queue_combo(&a));
        CHECK(!pe_combat_record_contact(&a,9));
        const PeCombatClip *clip=pe_combat_clip(a.gender,a.view,0);
        double start=event_time(clip,PE_COMBAT_HIT_START),end=event_time(clip,PE_COMBAT_HIT_END);
        CHECK(to_time(&a,start-0.000001,&stamina)==PE_COMBAT_ADVANCED);
        CHECK(!a.contact_open&&!pe_combat_record_contact(&a,9));
        CHECK(to_time(&a,start,&stamina)==PE_COMBAT_ADVANCED&&a.contact_open);
        CHECK(pe_combat_record_contact(&a,9)&&!pe_combat_record_contact(&a,9));
        CHECK(pe_combat_record_contact(&a,10)&&!pe_combat_record_contact(&a,0));
        CHECK(to_time(&a,end,&stamina)==PE_COMBAT_ADVANCED);
        CHECK(a.pending_close&&a.contact_open); /* Only one update since opening. */
        CHECK(!pe_combat_queue_combo(&a));
        CHECK(pe_combat_advance(&a,0,0,&stamina)==PE_COMBAT_ADVANCED);
        CHECK(!a.pending_close&&!a.contact_open&&!pe_combat_record_contact(&a,11));
        CHECK(to_time(&a,event_time(clip,PE_COMBAT_ACTION_END),&stamina)==PE_COMBAT_ENDED);
        CHECK(!a.active&&stamina==95);
        CHECK(pe_combat_advance(&a,1,1,&stamina)==PE_COMBAT_IDLE);
    }
    /* Same authored time, different gender/view means different hit state. */
    PeCombat male,female;pe_combat_init(&male);pe_combat_init(&female);float m=100,f=100;
    CHECK(pe_combat_begin(&male,PE_COMBAT_MALE,PE_COMBAT_THIRD_PERSON,1,&m));
    CHECK(pe_combat_begin(&female,PE_COMBAT_FEMALE,PE_COMBAT_THIRD_PERSON,1,&f));
    CHECK(to_time(&male,.1,&m)==PE_COMBAT_ADVANCED&&to_time(&female,.1,&f)==PE_COMBAT_ADVANCED);
    CHECK(!male.contact_open&&female.contact_open);
    CHECK(pe_combat_clip(PE_COMBAT_MALE,PE_COMBAT_FIRST_PERSON,0)->path_id==419);
    CHECK(pe_combat_clip(PE_COMBAT_FEMALE,PE_COMBAT_THIRD_PERSON,0)->path_id==231);
}
static void combo_tests(void){
    for(int gender=0;gender<2;gender++)for(int view=0;view<2;view++){
        PeCombat a;pe_combat_init(&a);float stamina=100;
        CHECK(pe_combat_begin(&a,(PeCombatGender)gender,(PeCombatView)view,1,&stamina));
        for(unsigned stage=0;stage<4;stage++){
            const PeCombatClip *clip=pe_combat_clip(a.gender,a.view,stage);
            CHECK(a.stage==stage&&a.event_index==0&&a.target_count==0);
            bool queued=false,contact=false;PeCombatAdvance result=PE_COMBAT_ADVANCED;
            for(unsigned frame=0;frame<2000&&a.active&&a.stage==stage;frame++){
                result=pe_combat_advance(&a,1.0/600,1.0/600,&stamina);
                CHECK(result!=PE_COMBAT_INVALID);
                if(a.stage!=stage||!a.active)break;
                if(a.contact_open&&!contact){CHECK(pe_combat_record_contact(&a,42));contact=true;}
                if(a.contact_open&&contact)CHECK(!pe_combat_record_contact(&a,42));
                if(a.combo_window&&!queued){
                    if(stage<3){CHECK(pe_combat_queue_combo(&a));queued=true;CHECK(!pe_combat_queue_combo(&a));}
                    else CHECK(!pe_combat_queue_combo(&a));
                }
            }
            CHECK(clip!=NULL&&contact);
            if(stage<3){CHECK(queued&&result==PE_COMBAT_NEXT_STAGE&&a.stage==stage+1&&a.clip_time==0&&a.wall_time==0);}
            else CHECK(result==PE_COMBAT_ENDED&&!a.active);
        }
        CHECK(stamina==80); /* Exactly four charges, including three combos. */
    }
    /* Zero stamina still permits action/combo; it only changes animation speed. */
    PeCombat a;pe_combat_init(&a);float stamina=2;
    CHECK(pe_combat_begin(&a,PE_COMBAT_MALE,PE_COMBAT_THIRD_PERSON,1,&stamina)&&stamina==0);
    CHECK(to_time(&a,.1,&stamina)==PE_COMBAT_ADVANCED&&pe_combat_queue_combo(&a));
    CHECK(to_time(&a,.2,&stamina)==PE_COMBAT_ADVANCED);
    CHECK(to_time(&a,.35,&stamina)==PE_COMBAT_ADVANCED);
    CHECK(pe_combat_advance(&a,0,0,&stamina)==PE_COMBAT_NEXT_STAGE&&stamina==0);
    CHECK(fabsf(pe_combat_animation_speed(51)-1.37f)<.000001f);
    CHECK(fabsf(pe_combat_animation_speed(50)-1.233f)<.000001f);
    CHECK(fabsf(pe_combat_animation_speed(20)-1.1097f)<.000001f);
    CHECK(pe_combat_animation_speed(0)==pe_combat_animation_speed(20));
    pe_combat_stop(&a);stamina=10;
    CHECK(pe_combat_begin(&a,PE_COMBAT_FEMALE,PE_COMBAT_FIRST_PERSON,.5f,&stamina)&&stamina==7.5f);
}
static void guards_tests(void){
    PeCombat a;pe_combat_init(&a);float stamina=100;
    CHECK(!pe_combat_begin(&a,(PeCombatGender)9,PE_COMBAT_THIRD_PERSON,1,&stamina)&&stamina==100);
    CHECK(!pe_combat_begin(&a,PE_COMBAT_MALE,(PeCombatView)-1,1,&stamina));
    CHECK(!pe_combat_begin(&a,PE_COMBAT_MALE,PE_COMBAT_THIRD_PERSON,-1,&stamina));
    CHECK(pe_combat_begin(&a,PE_COMBAT_MALE,PE_COMBAT_THIRD_PERSON,1,&stamina));
    PeCombat before=a;CHECK(!pe_combat_begin(&a,PE_COMBAT_MALE,PE_COMBAT_THIRD_PERSON,1,&stamina));
    CHECK(!memcmp(&before,&a,sizeof a)&&stamina==95);
    const double bad[]={-1,NAN,INFINITY,-INFINITY};
    for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){
        CHECK(pe_combat_advance(&a,bad[i],.01,&stamina)==PE_COMBAT_INVALID);
        CHECK(pe_combat_advance(&a,.01,bad[i],&stamina)==PE_COMBAT_INVALID);
        CHECK(!memcmp(&before,&a,sizeof a)&&stamina==95);
    }
    stamina=NAN;CHECK(pe_combat_advance(&a,0,0,&stamina)==PE_COMBAT_INVALID&&!memcmp(&before,&a,sizeof a));stamina=95;
    a.opened_update=a.update_index+1;PeCombat malformed=a;
    CHECK(pe_combat_advance(&a,0,0,&stamina)==PE_COMBAT_INVALID&&!memcmp(&a,&malformed,sizeof a)&&stamina==95);
    a=before;
    CHECK(to_time(&a,.2,&stamina)==PE_COMBAT_ADVANCED);
    for(uint64_t target=1;target<=PE_COMBAT_TARGET_CAPACITY;target++)CHECK(pe_combat_record_contact(&a,target));
    CHECK(!pe_combat_record_contact(&a,999)&&a.target_count==PE_COMBAT_TARGET_CAPACITY);
    pe_combat_stop(&a);CHECK(!a.contact_open&&!pe_combat_record_contact(&a,1000));
    CHECK(pe_combat_begin(&a,PE_COMBAT_MALE,PE_COMBAT_THIRD_PERSON,1,&stamina));
    /* The two clocks are independent; EndAction under .3 wall seconds is a
     * stale callback and ignored, exactly as source. Host may stop explicitly. */
    CHECK(pe_combat_advance(&a,2,.1,&stamina)==PE_COMBAT_ADVANCED&&a.active);
    CHECK(pe_combat_advance(&a,0,.3,&stamina)==PE_COMBAT_ADVANCED&&a.active);
    pe_combat_stop(&a);
    CHECK(pe_combat_animation_speed(NAN)==0&&pe_combat_animation_speed(-1)==0);
    CHECK(pe_combat_clip(PE_COMBAT_MALE,PE_COMBAT_FIRST_PERSON,4)==NULL);
    CHECK(pe_combat_advance(NULL,0,0,&stamina)==PE_COMBAT_INVALID);
    CHECK(!pe_combat_record_contact(NULL,1)&&!pe_combat_queue_combo(NULL));
    pe_combat_init(NULL);pe_combat_stop(NULL);
}
static void damage_tests(void){
    float d=-1;CHECK(pe_combat_damage(19,29,false,0,false,1,&d));CHECK(fabsf(d-361.0f/48)<.000001f);
    float body=d;CHECK(pe_combat_damage(19,29,false,0,false,2,&d)&&d==body*2);
    CHECK(pe_combat_damage(19,29,false,0,true,2,&d)&&d==body*4);
    CHECK(pe_combat_damage(19,29,true,.5f,false,1,&d)&&d==body*.5f);
    CHECK(pe_combat_damage(19,29,false,.5f,false,1,&d)&&d==body);
    CHECK(pe_combat_damage(19,29,true,1,false,1,&d)&&d==0);
    CHECK(pe_combat_damage(0,0,false,0,false,1,&d)&&d==0);
    CHECK(pe_combat_damage(19,29,false,0,false,0,&d)&&d==body);
    CHECK(pe_combat_damage(19,29,false,0,false,-1,&d)&&d==body);
    CHECK(pe_combat_damage(19,29,false,0,false,1e-8f,&d)&&d==body);
    CHECK(pe_combat_damage(19,29,false,0,false,.1f,&d)&&fabsf(d-body*.1f)<.000001f);
    d=123;CHECK(!pe_combat_damage(NAN,29,false,0,false,1,&d)&&d==123);
    CHECK(!pe_combat_damage(19,-1,false,0,false,1,&d)&&d==123);
    CHECK(!pe_combat_damage(19,29,false,2,false,1,&d)&&d==123);
    CHECK(!pe_combat_damage(19,29,false,0,false,INFINITY,&d)&&d==123);
    CHECK(!pe_combat_damage(FLT_MAX,0,false,0,true,2,&d)&&d==123);
    CHECK(!pe_combat_damage(19,29,false,0,false,1,NULL));
}
static void calibrated_clock_tests(void){
    /* Measured female stable endpoints: TP1.37 vs FP .8905 clip sec/wall sec.
     * The same real elapsed time therefore crosses different authored events. */
    PeCombat tp,fp;pe_combat_init(&tp);pe_combat_init(&fp);float a=100,b=100;
    CHECK(pe_combat_begin(&tp,PE_COMBAT_FEMALE,PE_COMBAT_THIRD_PERSON,1,&a));
    CHECK(pe_combat_begin(&fp,PE_COMBAT_FEMALE,PE_COMBAT_FIRST_PERSON,1,&b));
    for(int i=0;i<30;i++){
        CHECK(pe_combat_advance(&tp,pe_combat_stationary_clip_rate(tp.view,1.37)/120,1.0/120,&a)!=PE_COMBAT_INVALID);
        CHECK(pe_combat_advance(&fp,pe_combat_stationary_clip_rate(fp.view,1.37)/120,1.0/120,&b)!=PE_COMBAT_INVALID);
    }
    CHECK(tp.active&&!tp.contact_open&&fp.active&&fp.contact_open);
    for(int i=30;i<66;i++){
        pe_combat_advance(&tp,pe_combat_stationary_clip_rate(tp.view,1.37)/120,1.0/120,&a);
        pe_combat_advance(&fp,pe_combat_stationary_clip_rate(fp.view,1.37)/120,1.0/120,&b);
    }
    CHECK(!tp.active&&fp.active&&!fp.contact_open&&a==95&&b==95);
    CHECK(fabs(pe_combat_stationary_clip_rate(PE_COMBAT_FIRST_PERSON,1.233)-.80145)<1e-9);
    CHECK(pe_combat_stationary_clip_rate((PeCombatView)-1,1.37)==0);
    CHECK(pe_combat_stationary_clip_rate((PeCombatView)2,1.37)==0);
    CHECK(pe_combat_stationary_clip_rate(PE_COMBAT_FIRST_PERSON,NAN)==0);
    CHECK(pe_combat_stationary_clip_rate(PE_COMBAT_FIRST_PERSON,INFINITY)==0);
    CHECK(pe_combat_stationary_clip_rate(PE_COMBAT_FIRST_PERSON,-1)==0);
    CHECK(pe_combat_stationary_clip_rate(PE_COMBAT_FIRST_PERSON,0)==0);
}
int pe_test_combat(void){failures=0;window_tests();combo_tests();guards_tests();damage_tests();calibrated_clock_tests();return failures;}
