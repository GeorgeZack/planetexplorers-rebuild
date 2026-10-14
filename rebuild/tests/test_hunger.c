#include "pe_hunger.h"
#include "../generated/pe_hunger_source.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hunger:%d %s\n",__LINE__,#c);failures++;}}while(0)
#define NEAR(a,b) CHECK(fabsf((a)-(b))<1e-4f)
typedef struct {PePassiveRecovery life;PeHunger hunger;float hp;uint64_t frame;float now;PeHungerEvent event;} Fixture;
static bool step(Fixture *f,float dt,bool due,bool alive){
    Fixture next=*f;next.now+=dt;next.frame++;PeAttributeFrame time={next.now,next.frame,due};
    if(!pe_passive_recovery_frame(&next.life,time,&next.hp,500)||
       !pe_hunger_frame(&next.hunger,&f->life,&next.life,time,dt,alive,&next.event))return false;
    *f=next;return true;
}
/* A player whose initial buffs are mounted and already pulsed at time 0. */
static Fixture established(float hunger,bool starving){
    Fixture f={.life={.initialized=true,.mounted=true,.initialization_checked=true,.evaluations=1,.frame=1},.hp=500,.frame=1};
    CHECK(pe_hunger_loaded(&f.hunger,hunger,starving));f.hunger.initialized=true;f.hunger.drain_ticks=1;f.hunger.frame=1;
    return f;
}
/* Frames of dt until the condition changes; returns the frame count, or 0. */
static unsigned until_event(Fixture *f,float dt,unsigned limit,bool alive){
    for(unsigned i=1;i<=limit;i++){CHECK(step(f,dt,false,alive));if(f->event!=PE_HUNGER_NO_CHANGE)return i;}
    return 0;
}
int pe_test_hunger(void){
    failures=0;
    /* Source constants: 300 of 500, -0.0694 * 1 * 5 every 5 s. */
    CHECK(pe_hunger_source.hunger_max==500&&pe_hunger_source.initial_hunger==300&&pe_hunger_source.drain_interval_seconds==5);
    const float pulse=pe_hunger_source.recovery_speed*pe_hunger_source.reduction_percent*pe_hunger_source.drain_scale;
    NEAR(pulse,-.347f);

    /* Fresh: the drain runs at mount, again at the delayed remount, then strictly after each 5 s. */
    Fixture f={.hp=500};pe_hunger_fresh(&f.hunger);CHECK(f.hunger.hunger==300&&!f.hunger.starving);
    PeHungerMeal meal;CHECK(!pe_hunger_eat(&f.hunger,386,&meal));   /* nothing before the first frame */
    CHECK(step(&f,0,true,true));NEAR(f.hunger.hunger,300+pulse);CHECK(f.hunger.drain_ticks==1);
    CHECK(step(&f,.5f,true,true));NEAR(f.hunger.hunger,300+2*pulse);   /* CheckInitAttr remount */
    CHECK(step(&f,5,true,true));NEAR(f.hunger.hunger,300+2*pulse);      /* mount + 5, not strictly past */
    CHECK(step(&f,.01f,true,true));NEAR(f.hunger.hunger,300+3*pulse);
    /* Off-pass frames change nothing; one overdue pulse at most. */
    float before=f.hunger.hunger;CHECK(step(&f,30,false,true));CHECK(f.hunger.hunger==before);
    CHECK(step(&f,0,true,true));NEAR(f.hunger.hunger,before+pulse);

    /* Starvation starts on the check after a full second of frames, at hunger 0..50 inclusive. */
    f=established(50,false);CHECK(until_event(&f,.25f,20,true)==5&&f.event==PE_HUNGER_STARTED&&f.hunger.starving);
    PeHungerStarvation s=pe_hunger_starvation(&f.hunger);
    CHECK(s.stamina_reduce_percent==1&&s.stamina_recover_speed==-10&&s.stamina_recover_delay==2&&
          s.comfort_reduce_percent==1&&s.oxygen_reduce_percent==1);
    f=established(0,false);CHECK(until_event(&f,.1f,40,true)==11);
    /* Never for a dead player, and never above 50. */
    f=established(10,false);CHECK(until_event(&f,.25f,40,false)==0&&!f.hunger.starving);
    f=established(50.5f,false);CHECK(until_event(&f,.25f,40,true)==0);
    s=pe_hunger_starvation(&f.hunger);CHECK(s.stamina_recover_speed==0&&s.stamina_reduce_percent==0);

    /* It ends on the 0.1 s check once hunger reaches 51, and stays between 50 and 51. */
    f=established(20,true);
    CHECK(step(&f,.05f,true,true));CHECK(pe_hunger_eat(&f.hunger,384,&meal));   /* red mushroom soup +30 */
    CHECK(meal.item_id==384&&meal.hunger==30&&meal.nutrient==47&&meal.nutrient_gain==2);
    NEAR(f.hunger.raw,50);NEAR(f.hunger.hunger,20);   /* raw now, summed at the next pass */
    CHECK(until_event(&f,.05f,10,true)==0&&f.hunger.starving);
    f.hunger.food_cooldown=0;CHECK(pe_hunger_eat(&f.hunger,387,NULL));   /* puree +20 */
    CHECK(step(&f,.05f,true,true));NEAR(f.hunger.hunger,70);
    unsigned frames=until_event(&f,.05f,10,true);CHECK(frames>0&&frames<=3&&f.event==PE_HUNGER_ENDED&&!f.hunger.starving);
    CHECK(f.hunger.start_elapsed==.05f);   /* cleared, then this frame's time */
    /* Death does not end it. */
    f=established(10,true);CHECK(until_event(&f,.25f,20,false)==0&&f.hunger.starving);

    /* Food: the 4 s shared cooldown, the clamp at the next pass, only the supported foods. */
    f=established(490,false);CHECK(pe_hunger_eat(&f.hunger,386,&meal)&&meal.hunger==80&&meal.nutrient==46);
    PeHunger kept=f.hunger;CHECK(!pe_hunger_eat(&f.hunger,384,&meal));CHECK(!memcmp(&kept,&f.hunger,sizeof kept));
    CHECK(step(&f,0,true,true));CHECK(f.hunger.hunger==500&&f.hunger.raw==500);
    CHECK(step(&f,3.99f,false,true)&&!pe_hunger_eat(&f.hunger,384,NULL));
    CHECK(step(&f,.01f,false,true)&&pe_hunger_eat(&f.hunger,384,NULL));
    f.hunger.food_cooldown=0;CHECK(!pe_hunger_eat(&f.hunger,925,NULL)&&!pe_hunger_eat(&f.hunger,393,NULL));
    CHECK(pe_hunger_is_food(392)&&!pe_hunger_is_food(390)&&!pe_hunger_is_food(394)&&!pe_hunger_is_food(916));
    for(int i=0;i<PE_HUNGER_SOURCE_FOODS;i++)CHECK(pe_hunger_source_foods[i].hunger>0&&pe_hunger_source_foods[i].nutrient>=45&&pe_hunger_source_foods[i].nutrient<=49);
    /* Empty hunger stays at 0. */
    f=established(.1f,false);CHECK(step(&f,6,true,true));CHECK(f.hunger.hunger==0);

    /* Invalid input changes nothing. */
    f=established(100,false);kept=f.hunger;PeAttributeFrame bad={1,1,true};
    CHECK(!pe_hunger_frame(&f.hunger,&f.life,&f.life,bad,.1f,true,NULL));CHECK(!memcmp(&kept,&f.hunger,sizeof kept));
    Fixture g=f;CHECK(!step(&g,NAN,true,true));CHECK(!memcmp(&kept,&g.hunger,sizeof kept));
    CHECK(!pe_hunger_loaded(&g.hunger,501,false)&&!pe_hunger_loaded(&g.hunger,-1,false)&&!pe_hunger_loaded(NULL,1,false));
    CHECK(!pe_hunger_frame(NULL,&f.life,&f.life,bad,.1f,true,NULL));
    return failures?1:0;
}
