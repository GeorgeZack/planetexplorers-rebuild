#include "pe_passive_recovery.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"passive recovery:%d %s\n",__LINE__,#c);failures++;}}while(0)
static PePassiveRecovery ready(float started,float hurt){
    return (PePassiveRecovery){.created=started,.mounted_at=started,.last_hurt=hurt,
        .last_time=fmaxf(started,hurt),.evaluations=1,.frame=1,
        .initialized=true,.mounted=true,.initialization_checked=true};
}
int pe_test_passive_recovery(void){
    failures=0;PeAttributeClock clock={0};PeAttributeFrame frame;
    for(unsigned i=1;i<=24;i++){
        CHECK(pe_attribute_clock_step(&clock,.125,&frame));
        CHECK(frame.now==(float)i*.125f&&frame.frame==i&&frame.attributes_due==(i%8==0));
    }
    /* The source stores actual last updater frame, not a modulo phase. */
    clock.frame=40;CHECK(pe_attribute_clock_step(&clock,0,&frame)&&frame.attributes_due&&clock.attribute_frame==41);
    for(unsigned i=0;i<7;i++)CHECK(pe_attribute_clock_step(&clock,0,&frame)&&!frame.attributes_due);
    CHECK(pe_attribute_clock_step(&clock,0,&frame)&&frame.attributes_due);
    PeAttributeClock original_clock=clock;PeAttributeFrame original_frame=frame;
    CHECK(!pe_attribute_clock_step(&clock,NAN,&frame)&&!memcmp(&clock,&original_clock,sizeof clock)&&!memcmp(&frame,&original_frame,sizeof frame));
    CHECK(!pe_attribute_clock_step(&clock,-1,&frame));clock.frame=UINT64_MAX;CHECK(!pe_attribute_clock_step(&clock,0,&frame));

    float hp=100;PePassiveRecovery state=ready(0,0);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){3.999f,2,true},&hp,500)&&hp==100);
    CHECK(state.evaluations==2); /* Rejected conditional still consumes its tick. */
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){4,3,true},&hp,500)&&hp==105&&state.evaluations==3);
    /* A large time jump runs one overdue pulse, not elapsed-seconds healing. */
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){100,4,true},&hp,500)&&hp==110&&state.evaluations==4);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){100,5,false},&hp,500)&&hp==110);
    pe_passive_recovery_hurt(&state,hp);CHECK(state.last_hurt==100&&state.evaluations==4);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){103.999f,6,true},&hp,500)&&hp==110);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){104,7,true},&hp,500)&&hp==115);

    hp=164;state=ready(0,0);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){4,2,true},&hp,500)&&hp==169);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){5,3,true},&hp,500)&&hp==169);
    hp=165;state=ready(0,0);CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){4,2,true},&hp,500)&&hp==165);
    hp=200;state=ready(0,0);CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){4,2,true},&hp,1000)&&hp==210);
    /* Strict interval threshold independently of inclusive hurt threshold. */
    hp=100;state=ready(10,0);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){11,2,true},&hp,500)&&hp==100);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){nextafterf(11,INFINITY),3,true},&hp,500)&&hp==105);

    /* First deferred Exec may do initial plus one overdue evaluation. */
    hp=100;state=ready(0,0);state.evaluations=0;
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){10,2,true},&hp,500)&&hp==110&&state.evaluations==2);
    /* Fresh entity mounts immediately and is remounted at the delayed check. */
    hp=100;state=(PePassiveRecovery){0};
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){20,1,true},&hp,500)&&hp==105&&state.evaluations==1);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){20.499f,2,true},&hp,500)&&hp==105);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){20.5f,3,true},&hp,500)&&hp==110&&state.initialization_checked&&state.mounted_at==20.5f);
    /* Load restores attributes but has no initial buff until delayed check.
     * Last hurt is process time zero, never load time plus four seconds. */
    hp=100;state=(PePassiveRecovery){.loaded=true};
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){20,1,true},&hp,500)&&hp==100&&!state.mounted);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){20.499f,2,true},&hp,500)&&hp==100);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){20.5f,3,true},&hp,500)&&hp==105&&state.mounted);
    /* Same load in a newly started process still honors absolute lastHurt0. */
    hp=100;state=(PePassiveRecovery){.loaded=true};
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){0,1,true},&hp,500));
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){.5f,2,true},&hp,500)&&hp==100&&state.evaluations==1);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){4,3,true},&hp,500)&&hp==105);

    /* Death unmounts, and positive HP alone is not a revival action. */
    hp=0;pe_passive_recovery_hurt(&state,hp);CHECK(!state.mounted);
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){100,4,true},&hp,500)&&hp==0);
    hp=100;CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){101,5,true},&hp,500)&&hp==100&&!state.mounted);
    state=(PePassiveRecovery){.loaded=true};hp=0;
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){20,1,true},&hp,500));
    CHECK(pe_passive_recovery_frame(&state,(PeAttributeFrame){21,2,true},&hp,500)&&hp==0&&!state.mounted);
    /* Invalid/duplicate/out-of-order callbacks are atomic, including HP. */
    PePassiveRecovery before=state;
    CHECK(!pe_passive_recovery_frame(&state,(PeAttributeFrame){21,2,true},&hp,500)&&!memcmp(&before,&state,sizeof state)&&hp==0);
    CHECK(!pe_passive_recovery_frame(&state,(PeAttributeFrame){NAN,3,true},&hp,500));
    CHECK(!pe_passive_recovery_frame(&state,(PeAttributeFrame){20,3,true},&hp,500));
    CHECK(!pe_passive_recovery_frame(&state,(PeAttributeFrame){22,3,true},&hp,0));
    state=ready(0,0);state.evaluations=UINT64_MAX-1;before=state;hp=100;
    CHECK(!pe_passive_recovery_frame(&state,(PeAttributeFrame){1e30f,2,true},&hp,500)&&!memcmp(&before,&state,sizeof state)&&hp==100);
    return failures;
}
