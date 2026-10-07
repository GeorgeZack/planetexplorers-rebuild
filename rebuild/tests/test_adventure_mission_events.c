#include "pe_adventure_mission_events.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {int items[1200];unsigned calls,chain_advances;bool fail,missing_catalog;} Ledger;
static bool commit(void *context,const PeAdventureMissionEffects *effects){
    Ledger *ledger=context;assert(ledger&&effects->count<=PE_MISSION_EVENTS_EFFECT_CAPACITY);
    if(ledger->fail)return false;
    Ledger staged=*ledger;staged.calls++;
    for(unsigned i=0;i<effects->count;i++){
        PeAdventureMissionEffect e=effects->values[i];
        if(e.kind==PE_MISSION_EFFECT_ADVANCE_CHAIN)staged.chain_advances++;
        if(e.kind!=PE_MISSION_EFFECT_GRANT_ACCEPT&&e.kind!=PE_MISSION_EFFECT_GRANT_COMPLETE&&e.kind!=PE_MISSION_EFFECT_REMOVE_COMPLETE)continue;
        assert(e.value>0&&e.value<1200&&e.count>0);
        if(staged.missing_catalog&&e.value==490)return false;
        if(e.kind==PE_MISSION_EFFECT_REMOVE_COMPLETE){if(staged.items[e.value]<e.count)return false;staged.items[e.value]-=e.count;}
        else staged.items[e.value]+=e.count;
    }
    *ledger=staged;return true;
}
static PeAdventureMissionInput input(PeAdventureMissionInputKind kind,int mission,int value){return (PeAdventureMissionInput){kind,mission,value,true,true};}
static PeAdventureMissionEffects apply(PeAdventureMissionEvents *s,Ledger *l,PeAdventureMissionInputKind kind,int mission,int value){
    PeAdventureMissionEffects effects={0};assert(pe_adventure_mission_events_apply(s,input(kind,mission,value),commit,l,&effects));
    assert(pe_adventure_mission_events_valid(s));return effects;
}
static void reject(PeAdventureMissionEvents *s,Ledger *l,PeAdventureMissionInput in){
    PeAdventureMissionEvents old=*s;Ledger before=*l;PeAdventureMissionEffects out={.count=15},unchanged=out;
    assert(!pe_adventure_mission_events_apply(s,in,commit,l,&out));
    assert(!memcmp(s,&old,sizeof old)&&!memcmp(l,&before,sizeof before)&&!memcmp(&out,&unchanged,sizeof out));
}
static void offer_accept(PeAdventureMissionEvents *s,Ledger *l,int mission){
    const PeAdventureMissionEventSource *d=pe_adventure_mission_events_source(mission);assert(d);
    apply(s,l,PE_MISSION_EVENT_OFFER,mission,0);
    if(!d->opening_count)apply(s,l,PE_MISSION_EVENT_OPENING_ACK,mission,0);
    for(int i=0;i<d->opening_count;i++)apply(s,l,PE_MISSION_EVENT_OPENING_ACK,mission,d->opening[i]);
}
static void opening_and_plot(void){
    PeAdventureMissionEvents s;pe_adventure_mission_events_init(&s,false);Ledger ledger={0};
    assert(pe_adventure_mission_events_valid(&s)&&!pe_adventure_mission_events_eligible(&s,9077));
    reject(&s,&ledger,input(PE_MISSION_EVENT_OFFER,9077,0));
    pe_adventure_mission_events_init(&s,true);
    reject(&s,&ledger,input(PE_MISSION_EVENT_OFFER,9078,0));
    PeAdventureMissionInput unqualified=input(PE_MISSION_EVENT_OFFER,9077,0);unqualified.qualified=false;reject(&s,&ledger,unqualified);
    apply(&s,&ledger,PE_MISSION_EVENT_OFFER,9077,0);assert(s.offered==1&&!s.active&&!ledger.calls);
    reject(&s,&ledger,input(PE_MISSION_EVENT_OPENING_ACK,9077,4110));
    for(int talk=4107;talk<4110;talk++){apply(&s,&ledger,PE_MISSION_EVENT_OPENING_ACK,9077,talk);assert(!s.active);}
    PeAdventureMissionEvents resumed;assert(pe_adventure_mission_events_restore(&resumed,&s));
    unsigned calls=ledger.calls;PeAdventureMissionEffects accepted=apply(&resumed,&ledger,PE_MISSION_EVENT_OPENING_ACK,9077,4110);
    s=resumed;assert(s.active==1&&!s.offered&&accepted.count==1&&accepted.values[0].kind==PE_MISSION_EFFECT_ACCEPT&&ledger.calls==calls+1);
    reject(&s,&ledger,input(PE_MISSION_EVENT_OPENING_ACK,9077,4110));
    PeAdventureMissionEffects done=apply(&s,&ledger,PE_MISSION_EVENT_TARGET_READY,9077,7507);
    assert(s.completed==1&&!s.active&&!s.offered&&!s.queued_next&&done.count==1&&done.values[0].kind==PE_MISSION_EFFECT_COMPLETE);
    assert(pe_adventure_mission_events_eligible(&s,9078)&&!pe_adventure_mission_events_eligible(&s,9079));
    offer_accept(&s,&ledger,9078);apply(&s,&ledger,PE_MISSION_EVENT_TARGET_READY,9078,4504);
    offer_accept(&s,&ledger,9079);assert(s.plot5_pending&&(s.active&4)&&!(s.completed&4));
    PeAdventureMissionEvents queued=s;calls=ledger.calls;
    ledger.fail=true;reject(&s,&ledger,input(PE_MISSION_EVENT_PROCESS_PLOT,9079,5));ledger.fail=false;
    assert(s.plot5_pending&&!ledger.items[657]);
    PeAdventureMissionEffects plot=apply(&s,&ledger,PE_MISSION_EVENT_PROCESS_PLOT,9079,5);
    assert(plot.count==4&&plot.values[0].kind==PE_MISSION_EFFECT_ACCEPT&&plot.values[0].mission==9080);
    int ids[]={657,662,660};for(unsigned i=0;i<3;i++)assert(plot.values[i+1].kind==PE_MISSION_EFFECT_GRANT_ACCEPT&&plot.values[i+1].value==ids[i]&&ledger.items[ids[i]]==1);
    assert(!s.plot5_pending&&(s.active&4)&&(s.active&8)&&!(s.completed&4)&&(s.acceptance_granted&8));
    assert(ledger.calls==calls+1);
    reject(&s,&ledger,input(PE_MISSION_EVENT_PROCESS_PLOT,9079,5));
    PeAdventureMissionEvents duplicate_plot=s;duplicate_plot.plot5_pending=true;Ledger unchanged_ledger=ledger;
    PeAdventureMissionEffects no_replay=apply(&duplicate_plot,&ledger,PE_MISSION_EVENT_PROCESS_PLOT,9079,5);
    assert(!no_replay.count&&!duplicate_plot.plot5_pending&&!memcmp(&ledger,&unchanged_ledger,sizeof ledger));
    PeAdventureMissionEvents loaded={0};Ledger before=ledger;
    assert(pe_adventure_mission_events_restore(&loaded,&s)&&!memcmp(&ledger,&before,sizeof ledger));
    reject(&loaded,&ledger,input(PE_MISSION_EVENT_OPENING_ACK,9080,0));
    /* A queued record reload gives once when its source plot is actually processed. */
    Ledger separate={0};assert(pe_adventure_mission_events_restore(&resumed,&queued));
    apply(&resumed,&separate,PE_MISSION_EVENT_PROCESS_PLOT,9079,5);assert(separate.items[657]==1);
    /* Any of the three source ComMission use targets completes9080, but no
     * unqualified B-key placement event or foreign target can do so. */
    reject(&loaded,&ledger,input(PE_MISSION_EVENT_TARGET_READY,9080,5508));
    unqualified=input(PE_MISSION_EVENT_TARGET_READY,9080,5509);unqualified.qualified=false;reject(&loaded,&ledger,unqualified);
    done=apply(&loaded,&ledger,PE_MISSION_EVENT_TARGET_READY,9080,5510);
    assert((loaded.completed&8)&&loaded.ready_targets[3]==2&&(loaded.queued_next&128)&&!(loaded.active&128)&&!(loaded.offered&128));
    assert(done.count==2&&done.values[1].kind==PE_MISSION_EFFECT_NEXT_MISSION&&done.values[1].value==9084);
    for(unsigned i=0;i<3;i++)assert(ledger.items[ids[i]]==1);
    reject(&loaded,&ledger,input(PE_MISSION_EVENT_TARGET_READY,9080,5511));
    offer_accept(&loaded,&ledger,9084);assert(!(loaded.queued_next&128));
    done=apply(&loaded,&ledger,PE_MISSION_EVENT_TARGET_READY,9084,4506);
    assert(done.values[done.count-1].kind==PE_MISSION_EFFECT_NEXT_MISSION&&done.values[done.count-1].value==9114);
    assert(!pe_adventure_mission_events_source(9114));
    /* Repeatable choice shells remain exported but are not silently converted
     * into permanent one-shot missions. */
    assert(pe_adventure_mission_events_source(9081)->repeatable&&pe_adventure_mission_events_source(9081)->choices[0]==9082);
    reject(&s,&ledger,input(PE_MISSION_EVENT_OFFER,9081,0));reject(&s,&ledger,input(PE_MISSION_EVENT_OFFER,9083,0));
    PeAdventureMissionEvents invalid=s;invalid.acceptance_granted=0;PeAdventureMissionEvents old=loaded;
    assert(!pe_adventure_mission_events_restore(&loaded,&invalid)&&!memcmp(&loaded,&old,sizeof old));
    invalid=s;invalid.ready_targets[0]=128;assert(!pe_adventure_mission_events_valid(&invalid));
    pe_adventure_mission_events_init(&invalid,true);invalid.offered=1;invalid.opening_index[0]=4;
    assert(!pe_adventure_mission_events_valid(&invalid));
}
static void pigment_batch(void){
    PeAdventureMissionEvents s;pe_adventure_mission_events_init(&s,true);Ledger ledger={0};
    /* This input represents a future qualified source choice, not an enabled
     * offer in the game or a catalog-closure claim. Ledger is a test oracle. */
    offer_accept(&s,&ledger,9082);assert(ledger.items[597]==1&&(s.acceptance_granted&32));
    apply(&s,&ledger,PE_MISSION_EVENT_TARGET_READY,9082,2534);assert((s.active&32)&&!(s.completed&32));
    ledger.items[1038]=49;reject(&s,&ledger,input(PE_MISSION_EVENT_TURN_IN,9082,0));assert(!ledger.chain_advances);
    ledger.items[1038]=50;ledger.missing_catalog=true;reject(&s,&ledger,input(PE_MISSION_EVENT_TURN_IN,9082,0));
    assert(ledger.items[1038]==50&&!ledger.items[478]&&!ledger.chain_advances);
    ledger.missing_catalog=false;
    PeAdventureMissionInput not_ready=input(PE_MISSION_EVENT_TARGET_READY,9082,2534);not_ready.ready=false;
    PeAdventureMissionEffects effects;assert(pe_adventure_mission_events_apply(&s,not_ready,commit,&ledger,&effects));
    reject(&s,&ledger,input(PE_MISSION_EVENT_TURN_IN,9082,0));
    apply(&s,&ledger,PE_MISSION_EVENT_TARGET_READY,9082,2534);
    effects=apply(&s,&ledger,PE_MISSION_EVENT_TURN_IN,9082,0);
    assert(effects.count==11&&effects.values[0].kind==PE_MISSION_EFFECT_ADVANCE_CHAIN&&effects.values[1].kind==PE_MISSION_EFFECT_COMPLETE&&effects.values[2].kind==PE_MISSION_EFFECT_REMOVE_COMPLETE);
    int rewards[]={478,490,696,465,474,473,481};for(unsigned i=0;i<7;i++)assert(ledger.items[rewards[i]]==1&&effects.values[i+3].kind==PE_MISSION_EFFECT_GRANT_COMPLETE);
    assert(effects.values[10].kind==PE_MISSION_EFFECT_ENDING_DIALOGUE&&effects.values[10].value==4121);
    assert(!ledger.items[1038]&&ledger.chain_advances==1&&(s.completed&32)&&(s.ending_pending&32));
    reject(&s,&ledger,input(PE_MISSION_EVENT_TURN_IN,9082,0));
    PeAdventureMissionEvents reloaded;assert(pe_adventure_mission_events_restore(&reloaded,&s));Ledger before=ledger;
    effects=apply(&reloaded,&ledger,PE_MISSION_EVENT_ENDING_ACK,9082,4121);
    assert(!effects.count&&!reloaded.ending_pending&&!memcmp(&ledger,&before,sizeof ledger));
    reject(&reloaded,&ledger,input(PE_MISSION_EVENT_ENDING_ACK,9082,4121));
}
int main(void){opening_and_plot();pigment_batch();puts("Adventure typed mission events: PASS (disconnected; synthetic atomic callback)");return 0;}
