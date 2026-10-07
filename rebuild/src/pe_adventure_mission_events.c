#include "pe_adventure_mission_events.h"
#include "../generated/pe_adventure_mission_events_source.h"
#include <string.h>
_Static_assert(sizeof pe_adventure_mission_event_sources/sizeof pe_adventure_mission_event_sources[0]==PE_MISSION_EVENTS_COUNT,"Mission source bounds");
static int index_of(int mission){return mission>=9077&&mission<=9084?mission-9077:-1;}
static uint8_t bit(int index){return (uint8_t)(1u<<(unsigned)index);}
const PeAdventureMissionEventSource *pe_adventure_mission_events_source(int mission){
    int i=index_of(mission);return i>=0?&pe_adventure_mission_event_sources[i]:NULL;
}
static bool supported(const PeAdventureMissionEventSource *d){return d&&!d->repeatable&&d->target_count>0;}
static bool complete(const PeAdventureMissionEvents *s,int mission){
    int i=index_of(mission);return mission==9127?s->prior9127_complete:i>=0&&(s->completed&bit(i));
}
static bool prerequisite(const PeAdventureMissionEvents *s,const PeAdventureMissionEventSource *d){return !d->prerequisite||complete(s,d->prerequisite);}
static bool target_ready(const PeAdventureMissionEvents *s,int i){
    const PeAdventureMissionEventSource *d=&pe_adventure_mission_event_sources[i];
    return d->any_target?s->ready_targets[i]!=0:s->ready_targets[i]==(1u<<(unsigned)d->target_count)-1u;
}
void pe_adventure_mission_events_init(PeAdventureMissionEvents *s,bool prior){if(s)*s=(PeAdventureMissionEvents){.prior9127_complete=prior};}
bool pe_adventure_mission_events_valid(const PeAdventureMissionEvents *s){
    if(!s||(s->offered&s->active)||(s->offered&s->completed)||(s->active&s->completed))return false;
    for(int i=0;i<PE_MISSION_EVENTS_COUNT;i++){
        const PeAdventureMissionEventSource *d=&pe_adventure_mission_event_sources[i];uint8_t b=bit(i);
        unsigned state=(s->offered|s->active|s->completed)&b;
        if(!supported(d)){
            if(state||((s->acceptance_granted|s->ending_pending|s->queued_next|s->chain_advanced)&b)||s->opening_index[i]||s->ready_targets[i])return false;
            continue;
        }
        if(state&&!prerequisite(s,d))return false;
        if(s->opening_index[i]>(unsigned)d->opening_count)return false;
        if((s->offered&b)&&d->opening_count&&s->opening_index[i]>=(unsigned)d->opening_count)return false;
        if(!(s->active&b)&&!(s->completed&b)&&s->ready_targets[i])return false;
        if(!state&&s->opening_index[i])return false;
        if((s->active|s->completed)&b){
            if(s->opening_index[i]!=(unsigned)d->opening_count)return false;
            if(d->grant_count&&!(s->acceptance_granted&b))return false;
        }
        if((s->acceptance_granted&b)&&(!d->grant_count||!((s->active|s->completed)&b)))return false;
        if((s->ending_pending&b)&&(!d->ending||!(s->completed&b)))return false;
        if((s->chain_advanced&b)&&(!d->increase_chain||!(s->completed&b)))return false;
        if((s->completed&b)&&d->increase_chain&&!(s->chain_advanced&b))return false;
        if(s->ready_targets[i]&~((1u<<(unsigned)d->target_count)-1u))return false;
        if((s->completed&b)&&!target_ready(s,i))return false;
        if((s->queued_next&b)&&((s->active|s->completed)&b))return false;
    }
    if(s->plot5_pending&&!((s->active|s->completed)&bit(index_of(9079))))return false;
    /* Only the bounded9080->9084 NextQuest is represented internally. */
    if(s->queued_next&~bit(index_of(9084)))return false;
    if(s->queued_next&&!complete(s,9080))return false;
    return true;
}
bool pe_adventure_mission_events_restore(PeAdventureMissionEvents *s,const PeAdventureMissionEvents *snapshot){
    if(!s||!pe_adventure_mission_events_valid(snapshot))return false;
    *s=*snapshot;return true;
}
bool pe_adventure_mission_events_eligible(const PeAdventureMissionEvents *s,int mission){
    const PeAdventureMissionEventSource *d=pe_adventure_mission_events_source(mission);int i=index_of(mission);
    if(!pe_adventure_mission_events_valid(s)||!supported(d)||!prerequisite(s,d)||((s->active|s->completed)&bit(i)))return false;
    for(unsigned k=0;k<2;k++)if(d->mutex[k]){
        int j=index_of(d->mutex[k]);if(j<0||((s->active|s->completed)&bit(j)))return false;
    }
    return true;
}
static bool emit(PeAdventureMissionEffects *e,PeAdventureMissionEffectKind kind,int mission,int value,int count){
    if(e->count>=PE_MISSION_EVENTS_EFFECT_CAPACITY)return false;
    e->values[e->count++]=(PeAdventureMissionEffect){kind,mission,value,count};return true;
}
static bool accept(PeAdventureMissionEvents *s,int i,PeAdventureMissionEffects *e){
    const PeAdventureMissionEventSource *d=&pe_adventure_mission_event_sources[i];uint8_t b=bit(i);
    if(!pe_adventure_mission_events_eligible(s,d->mission))return false;
    s->offered&=(uint8_t)~b;s->active|=b;s->queued_next&=(uint8_t)~b;s->opening_index[i]=(uint8_t)d->opening_count;
    if(!emit(e,PE_MISSION_EFFECT_ACCEPT,d->mission,0,0))return false;
    if(d->accept_plot){
        if(d->accept_plot!=5)return false;
        s->plot5_pending=true;if(!emit(e,PE_MISSION_EFFECT_ACCEPT_PLOT,d->mission,d->accept_plot,0))return false;
    }
    if(!(s->acceptance_granted&b)){
        for(unsigned k=0;k<d->grant_count;k++)if(!emit(e,PE_MISSION_EFFECT_GRANT_ACCEPT,d->mission,d->grants[k].item_id,d->grants[k].count))return false;
        if(d->grant_count)s->acceptance_granted|=b;
    }
    return true;
}
static bool finish(PeAdventureMissionEvents *s,int i,PeAdventureMissionEffects *e){
    const PeAdventureMissionEventSource *d=&pe_adventure_mission_event_sources[i];uint8_t b=bit(i);
    if(!(s->active&b)||!target_ready(s,i))return false;
    if(d->increase_chain){s->chain_advanced|=b;if(!emit(e,PE_MISSION_EFFECT_ADVANCE_CHAIN,d->mission,1,0))return false;}
    s->active&=(uint8_t)~b;s->completed|=b;
    if(!emit(e,PE_MISSION_EFFECT_COMPLETE,d->mission,0,0))return false;
    for(unsigned k=0;k<d->remove_count;k++)if(!emit(e,PE_MISSION_EFFECT_REMOVE_COMPLETE,d->mission,d->removes[k].item_id,d->removes[k].count))return false;
    for(unsigned k=0;k<d->reward_count;k++)if(!emit(e,PE_MISSION_EFFECT_GRANT_COMPLETE,d->mission,d->rewards[k].item_id,d->rewards[k].count))return false;
    if(d->complete_plot&&!emit(e,PE_MISSION_EFFECT_COMPLETE_PLOT,d->mission,d->complete_plot,0))return false;
    if(d->ending){s->ending_pending|=b;if(!emit(e,PE_MISSION_EFFECT_ENDING_DIALOGUE,d->mission,d->ending,0))return false;}
    else if(d->next){
        int next=index_of(d->next);
        if(next<0||!((s->active|s->completed)&bit(next))){
            if(next>=0)s->queued_next|=bit(next);
            if(!emit(e,PE_MISSION_EFFECT_NEXT_MISSION,d->mission,d->next,0))return false;
        }
    }
    return true;
}
bool pe_adventure_mission_events_apply(PeAdventureMissionEvents *state,PeAdventureMissionInput input,
    PeAdventureMissionCommit commit,void *context,PeAdventureMissionEffects *out){
    if(!out||!pe_adventure_mission_events_valid(state)||!input.qualified)return false;
    int i=index_of(input.mission);const PeAdventureMissionEventSource *d=pe_adventure_mission_events_source(input.mission);
    if(i<0||!supported(d))return false;
    PeAdventureMissionEvents s=*state;PeAdventureMissionEffects effects={0};uint8_t b=bit(i);
    switch(input.kind){
    case PE_MISSION_EVENT_OFFER:
        if(input.value||!pe_adventure_mission_events_eligible(&s,input.mission)||(s.offered&b))return false;
        s.offered|=b;break;
    case PE_MISSION_EVENT_OPENING_ACK:
        if(!(s.offered&b))return false;
        if(!d->opening_count){if(input.value||!accept(&s,i,&effects))return false;}
        else{
            if(s.opening_index[i]>=(unsigned)d->opening_count||input.value!=d->opening[s.opening_index[i]])return false;
            if(s.opening_index[i]+1u==(unsigned)d->opening_count){if(!accept(&s,i,&effects))return false;}
            else s.opening_index[i]++;
        }
        break;
    case PE_MISSION_EVENT_TARGET_READY:{
        if(!(s.active&b))return false;
        int target=-1;for(int j=0;j<d->target_count;j++)if(d->targets[j]==input.value)target=j;
        if(target<0)return false;
        uint8_t mask=bit(target);
        if(input.ready)s.ready_targets[i]|=mask;else s.ready_targets[i]&=(uint8_t)~mask;
        if(input.ready&&target_ready(&s,i)&&d->auto_reply&&!finish(&s,i,&effects))return false;
        break;}
    case PE_MISSION_EVENT_TURN_IN:
        if(input.value||d->auto_reply||!finish(&s,i,&effects))return false;
        break;
    case PE_MISSION_EVENT_PROCESS_PLOT:
        if(input.mission!=9079||input.value!=5||!s.plot5_pending)return false;
        s.plot5_pending=false;
        if(!((s.active|s.completed)&bit(index_of(9080)))&&!accept(&s,index_of(9080),&effects))return false;
        break;
    case PE_MISSION_EVENT_ENDING_ACK:
        if(!(s.ending_pending&b)||input.value!=d->ending)return false;
        s.ending_pending&=(uint8_t)~b;break;
    default:return false;
    }
    if(!pe_adventure_mission_events_valid(&s))return false;
    if(effects.count&&(!commit||!commit(context,&effects)))return false;
    *state=s;*out=effects;return true;
}
