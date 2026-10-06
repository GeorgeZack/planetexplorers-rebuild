#include "pe_town_defense_clock.h"
#include "../generated/pe_adventure_town_defense_source.h"
#include <math.h>
#include <stddef.h>
static unsigned prep_seconds(unsigned wave){
    for(size_t i=0;i<sizeof pe_town_defense_source_groups/sizeof pe_town_defense_source_groups[0];i++)
        if(pe_town_defense_source_groups[i].wave==(int)wave)
            return (unsigned)pe_town_defense_source_groups[i].delay_seconds;
    return 0;
}
void pe_town_defense_clock_init(PeTownDefenseClock*c){
    if(c)*c=(PeTownDefenseClock){PE_TOWN_DEFENSE_INITIAL_PREP,
        PE_TOWN_DEFENSE_INITIAL_SECONDS+prep_seconds(0),1};
}
bool pe_town_defense_clock_valid(const PeTownDefenseClock*c){
    if(!c||!isfinite(c->wait_remaining))return false;
    if(c->phase==PE_TOWN_DEFENSE_CLOCK_FINISHED)
        return c->seconds_remaining==0&&c->wait_remaining==0;
    if(c->wait_remaining<=0||c->wait_remaining>1)return false;
    switch(c->phase){
    case PE_TOWN_DEFENSE_INITIAL_PREP:
        return c->seconds_remaining<=PE_TOWN_DEFENSE_INITIAL_SECONDS+prep_seconds(0);
    case PE_TOWN_DEFENSE_COOLDOWN:
        return c->seconds_remaining>0&&c->seconds_remaining<=PE_TOWN_DEFENSE_COOLDOWN_SECONDS;
    case PE_TOWN_DEFENSE_SECOND_PREP:return c->seconds_remaining<=prep_seconds(1);
    default:return false;
    }
}
bool pe_town_defense_clock_ready(PeTownDefenseClock*c){
    if(!pe_town_defense_clock_valid(c)||(c->phase!=PE_TOWN_DEFENSE_INITIAL_PREP&&
       c->phase!=PE_TOWN_DEFENSE_SECOND_PREP))return false;
    c->seconds_remaining=0;return true;
}
bool pe_town_defense_clock_step(PeTownDefenseClock*c,double dt,bool paused,PeTownDefenseClockEvent*out){
    if(!out||!pe_town_defense_clock_valid(c)||!isfinite(dt)||dt<0||dt>1)return false;
    PeTownDefenseClock next=*c;PeTownDefenseClockEvent event={-1};
    if(!paused&&dt>0&&next.phase!=PE_TOWN_DEFENSE_CLOCK_FINISHED){
        next.wait_remaining-=dt;
        if(next.wait_remaining<=0){
            next.wait_remaining=1;
            if(next.seconds_remaining)next.seconds_remaining--;
            if(!next.seconds_remaining){
                if(next.phase==PE_TOWN_DEFENSE_COOLDOWN){
                    next.phase=PE_TOWN_DEFENSE_SECOND_PREP;
                    next.seconds_remaining=prep_seconds(1);
                }else if(next.phase==PE_TOWN_DEFENSE_INITIAL_PREP){
                    event.wave=0;next.phase=PE_TOWN_DEFENSE_COOLDOWN;
                    next.seconds_remaining=PE_TOWN_DEFENSE_COOLDOWN_SECONDS;
                }else{
                    event.wave=1;next.phase=PE_TOWN_DEFENSE_CLOCK_FINISHED;next.wait_remaining=0;
                }
            }
        }
    }
    *c=next;*out=event;return true;
}
unsigned pe_town_defense_clock_prep(const PeTownDefenseClock*c){
    if(!pe_town_defense_clock_valid(c))return 0;
    return c->phase==PE_TOWN_DEFENSE_INITIAL_PREP||c->phase==PE_TOWN_DEFENSE_SECOND_PREP?c->seconds_remaining:0;
}
unsigned pe_town_defense_clock_waves_remaining(const PeTownDefenseClock*c){
    if(!pe_town_defense_clock_valid(c)||c->phase==PE_TOWN_DEFENSE_CLOCK_FINISHED)return 0;
    return c->phase==PE_TOWN_DEFENSE_INITIAL_PREP?2:1;
}
