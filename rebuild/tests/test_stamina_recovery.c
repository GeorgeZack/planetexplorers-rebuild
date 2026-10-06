#include "pe_stamina_recovery.h"
#include "../generated/pe_stamina_recovery_source.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"stamina recovery:%d %s\n",__LINE__,#c);failures++;}}while(0)
typedef struct {PePassiveRecovery life;PeStaminaRecovery stamina;PeStaminaVitals vitals;float hp;} Fixture;
static Fixture fresh(bool loaded){return (Fixture){.life={.loaded=loaded},.vitals={0,100,40,0,500},.hp=500};}
static bool step(Fixture *f,float now,uint64_t frame,bool due){
    Fixture next=*f;PeAttributeFrame time={now,frame,due};
    if(!pe_passive_recovery_frame(&next.life,time,&next.hp,500)||
       !pe_stamina_recovery_frame(&next.stamina,&f->life,&next.life,time,&next.vitals))return false;
    *f=next;return true;
}
static Fixture established(float now){
    Fixture f=fresh(false);
    f.life=(PePassiveRecovery){.initialized=true,.mounted=true,.initialization_checked=true,.created=now,.mounted_at=now,.last_time=now,.evaluations=1,.frame=1};
    f.stamina=(PeStaminaRecovery){.initialized=true,.stamina_ticks=1,.comfort_ticks=1,.delay_ticks=1,.frame=1};return f;
}
int pe_test_stamina_recovery(void){
    failures=0;Fixture f=fresh(false);
    CHECK(step(&f,0,1,true));CHECK(f.vitals.stamina==0&&fabsf(f.vitals.comfort-499.306f)<.0001f);
    CHECK(f.stamina.stamina_ticks==1&&f.stamina.comfort_ticks==1&&f.stamina.delay_ticks==1);
    CHECK(fabsf(f.stamina.summed_delay-.002776f)<.00001f);
    CHECK(step(&f,.499f,2,true)&&f.vitals.stamina==0);
    CHECK(step(&f,.5f,3,true)&&f.vitals.stamina==40); /* Fresh remount executes again. */
    CHECK(fabsf(f.vitals.comfort-498.612f)<.0001f&&f.stamina.comfort_ticks==1);
    CHECK(step(&f,1.5f,4,true)&&f.vitals.stamina==40); /* Strict mount+1. */
    CHECK(step(&f,nextafterf(1.5f,INFINITY),5,true)&&f.vitals.stamina==80);
    CHECK(step(&f,10,6,true)&&f.vitals.stamina==100&&f.stamina.stamina_ticks==3); /* One overdue only. */
    CHECK(fabsf(f.vitals.comfort-497.918f)<.0001f&&f.stamina.comfort_ticks==2);

    f=established(10);f.stamina.last_consumption=10.9f;
    CHECK(step(&f,11,2,true)&&f.vitals.stamina==0); /* No scheduled pulse yet. */
    CHECK(step(&f,nextafterf(11,INFINITY),3,true)&&f.vitals.stamina==40);
    /* Strict consumption equality with independently overdue pulse. */
    f=established(0);f.stamina.last_consumption=9.9f;
    CHECK(step(&f,10,2,true)&&f.vitals.stamina==0&&f.stamina.stamina_ticks==2);
    CHECK(step(&f,nextafterf(10,INFINITY),3,true)&&f.vitals.stamina==40);
    /* Action activity refreshes the timestamp even at zero energy. */
    pe_stamina_recovery_consume(&f.stamina,f.life.last_time);
    CHECK(step(&f,10.05f,4,true)&&f.vitals.stamina==40&&f.stamina.stamina_ticks==4);
    CHECK(step(&f,10.2f,5,true)&&f.vitals.stamina==80);

    /* Source uses the old summed delay, then new raw comfort, then clamps8. */
    f=fresh(true);f.vitals.comfort=0;
    CHECK(step(&f,20,1,true)&&f.vitals.stamina==0&&f.stamina.summed_delay==0);
    f.stamina.last_consumption=20;
    CHECK(step(&f,20.5f,2,true)&&f.vitals.stamina==40&&f.vitals.comfort==0);
    CHECK(f.stamina.summed_delay>2&&fabsf(f.stamina.summed_delay-2.002776f)<.00001f);
    CHECK(step(&f,21.6f,3,true)&&f.vitals.stamina==40&&f.stamina.summed_delay==2);
    CHECK(step(&f,22.6f,4,true)&&f.vitals.stamina==80);
    /* Changing comfort does not itself dirty delay6 between delay pulses. */
    float previous=f.stamina.summed_delay;f.vitals.comfort=500;
    CHECK(step(&f,22.7f,5,true)&&f.stamina.summed_delay==previous);
    CHECK(step(&f,23.6f,6,true)&&f.stamina.summed_delay==0);
    /* Independent5s comfort boundary. */
    f=established(10);
    CHECK(step(&f,15,2,true)&&f.vitals.comfort==500);
    CHECK(step(&f,nextafterf(15,INFINITY),3,true)&&fabsf(f.vitals.comfort-499.306f)<.0001f);
    /* Deferred first pass can evaluate initial+one periodic for each buff. */
    f=established(10);f.stamina.stamina_ticks=f.stamina.comfort_ticks=f.stamina.delay_ticks=0;
    CHECK(step(&f,20,2,true)&&f.vitals.stamina==80&&fabsf(f.vitals.comfort-498.612f)<.0001f);
    CHECK(f.stamina.stamina_ticks==2&&f.stamina.comfort_ticks==2&&f.stamina.delay_ticks==2);
    /* Maximum and raw base delay are separate from derived comfort delay. */
    f=established(0);f.vitals.stamina=95;f.vitals.base_delay=1;f.stamina.summed_delay=1;f.vitals.comfort=250;
    CHECK(step(&f,4,2,true)&&f.vitals.stamina==100&&f.vitals.base_delay==1&&f.stamina.summed_delay==2);
    /* Death stops all three buffs; no inferred revival from a positive stat. */
    float comfort=f.vitals.comfort;f.hp=0;
    CHECK(step(&f,20,3,true)&&f.vitals.comfort==comfort&&f.stamina.stamina_ticks==0&&f.stamina.summed_delay==f.vitals.base_delay);
    f.hp=500;f.vitals.stamina=0;CHECK(step(&f,30,4,true)&&f.vitals.stamina==0&&f.vitals.comfort==comfort);
    Fixture before=f;
    CHECK(!step(&f,31,4,true)&&!memcmp(&f,&before,sizeof f));
    CHECK(!step(&f,NAN,5,true)&&!memcmp(&f,&before,sizeof f));
    f.vitals.comfort=-1;before=f;CHECK(!step(&f,31,5,true)&&!memcmp(&f,&before,sizeof f));
    f.vitals.comfort=501;before=f;CHECK(!step(&f,31,5,true)&&!memcmp(&f,&before,sizeof f));
    f=established(0);f.stamina.stamina_ticks=UINT64_MAX-2;before=f;
    CHECK(!step(&f,1e30f,2,true)&&!memcmp(&f,&before,sizeof f));
    return failures;
}
