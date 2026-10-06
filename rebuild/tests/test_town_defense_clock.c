#include "pe_town_defense_clock.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int step(PeTownDefenseClock*c,double dt,bool paused){
    PeTownDefenseClockEvent e={99};assert(pe_town_defense_clock_step(c,dt,paused,&e));
    assert(pe_town_defense_clock_valid(c));return e.wave;
}
int main(void){
    PeTownDefenseClock c;pe_town_defense_clock_init(&c);
    assert(pe_town_defense_clock_prep(&c)==30&&pe_town_defense_clock_waves_remaining(&c)==2);
    /* Source coroutine oracle:30 one-second waits,10 separate cooldown waits,
     * then20 prep waits. No dependence on first-wave survival or random counts. */
    for(int second=1;second<30;second++){assert(step(&c,1,false)==-1);assert(pe_town_defense_clock_prep(&c)==30u-(unsigned)second);}
    assert(step(&c,1,false)==0&&c.phase==PE_TOWN_DEFENSE_COOLDOWN);
    assert(pe_town_defense_clock_prep(&c)==0&&pe_town_defense_clock_waves_remaining(&c)==1);
    for(int second=1;second<10;second++){assert(step(&c,1,false)==-1);assert(pe_town_defense_clock_prep(&c)==0);}
    assert(step(&c,1,false)==-1&&c.phase==PE_TOWN_DEFENSE_SECOND_PREP&&pe_town_defense_clock_prep(&c)==20);
    for(int second=1;second<20;second++)assert(step(&c,1,false)==-1);
    assert(step(&c,1,false)==1&&c.phase==PE_TOWN_DEFENSE_CLOCK_FINISHED);
    assert(pe_town_defense_clock_prep(&c)==0&&pe_town_defense_clock_waves_remaining(&c)==0);
    PeTownDefenseClock frozen=c;assert(!pe_town_defense_clock_ready(&c));
    for(int i=0;i<100;i++)assert(step(&c,1,false)==-1);
    assert(!memcmp(&c,&frozen,sizeof c));
    /* Ready affects prep but must wait for the outstanding wake. */
    pe_town_defense_clock_init(&c);assert(step(&c,.25,false)==-1);
    assert(pe_town_defense_clock_ready(&c)&&c.wait_remaining==.75&&pe_town_defense_clock_prep(&c)==0);
    assert(pe_town_defense_clock_ready(&c));frozen=c;
    assert(step(&c,1,true)==-1&&step(&c,0,false)==-1&&!memcmp(&c,&frozen,sizeof c));
    assert(step(&c,.5,false)==-1&&step(&c,.25,false)==0);
    frozen=c;assert(!pe_town_defense_clock_ready(&c)&&!memcmp(&c,&frozen,sizeof c));
    for(int i=0;i<10;i++){assert(!pe_town_defense_clock_ready(&c));assert(step(&c,1,false)==-1);}
    assert(c.phase==PE_TOWN_DEFENSE_SECOND_PREP&&pe_town_defense_clock_prep(&c)==20);
    assert(step(&c,.5,false)==-1&&pe_town_defense_clock_ready(&c));
    assert(step(&c,.25,false)==-1&&step(&c,.25,false)==1);
    /* Scheduling overshoot is not accumulated into the next wait: at.75+.5s
     * exactly one source wake occurs and the next full1s wait begins now. */
    pe_town_defense_clock_init(&c);assert(step(&c,.75,false)==-1);
    assert(step(&c,.5,false)==-1&&c.seconds_remaining==29&&c.wait_remaining==1);
    assert(step(&c,.75,false)==-1&&c.seconds_remaining==29&&c.wait_remaining==.25);
    frozen=c;for(int i=0;i<100;i++)assert(step(&c,.5,true)==-1);
    assert(!memcmp(&c,&frozen,sizeof c));assert(step(&c,.25,false)==-1&&c.seconds_remaining==28);
    /* Invalid input never clobbers either state or event. */
    const double invalid[]={NAN,INFINITY,-INFINITY,-.1,1.01};PeTownDefenseClockEvent e={42};
    frozen=c;
    for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++){
        assert(!pe_town_defense_clock_step(&c,invalid[i],false,&e));
        assert(!memcmp(&c,&frozen,sizeof c)&&e.wave==42);
    }
    assert(!pe_town_defense_clock_step(NULL,1,false,&e)&&e.wave==42);
    assert(!pe_town_defense_clock_step(&c,1,false,NULL)&&!memcmp(&c,&frozen,sizeof c));
    c.phase=(PeTownDefenseClockPhase)99;frozen=c;
    assert(!pe_town_defense_clock_step(&c,1,false,&e)&&!pe_town_defense_clock_ready(&c)&&!memcmp(&c,&frozen,sizeof c));
    pe_town_defense_clock_init(&c);c.wait_remaining=0;assert(!pe_town_defense_clock_valid(&c));
    pe_town_defense_clock_init(&c);c.seconds_remaining=31;assert(!pe_town_defense_clock_valid(&c));
    c=(PeTownDefenseClock){PE_TOWN_DEFENSE_COOLDOWN,0,1};assert(!pe_town_defense_clock_valid(&c));
    c=(PeTownDefenseClock){PE_TOWN_DEFENSE_CLOCK_FINISHED,0,1};assert(!pe_town_defense_clock_valid(&c));
    pe_town_defense_clock_init(NULL);assert(!pe_town_defense_clock_ready(NULL));
    assert(!pe_town_defense_clock_prep(NULL)&&!pe_town_defense_clock_waves_remaining(NULL));
    puts("town defense clock: source30+10+20, deferred Ready, pause, no catch-up, events and invalid-state checks passed");
    return 0;
}
