#include "pe_sword_motor.h"
#include <assert.h>
#include <math.h>
#include <string.h>

#define DT (1/60.f)
static bool near(float a,float b){return fabsf(a-b)<=2e-5f+fabsf(b)*2e-5f;}
/* One native tick in host order: consume, move, then request/animate/latch.
 * Returns the horizontal velocity the body moved with; *owned reports ownership. */
static PeVec3 tick(PeSwordMotor *m,bool active_before,const double *delta,PeVec3 *velocity,bool *owned){
    PeSwordMotorStep step;
    assert(pe_sword_motor_consume(m,active_before,*velocity,true,DT,&step));
    if(step.owns_motion)*velocity=step.velocity;
    if(owned)*owned=step.owns_motion;
    assert(pe_sword_motor_update(m,active_before,delta,*velocity,DT));
    return *velocity;
}
static void idle_and_two_tick_lag(void){
    PeSwordMotor m;memset(&m,0xA5,sizeof m);pe_sword_motor_reset(&m);
    PeVec3 v={0,0,5};bool owned=true;
    tick(&m,false,NULL,&v,&owned);assert(!owned&&v.z==5); /* no blade action: the walking step keeps the tick */
    /* Start tick S: the action begins during this tick's advance, so the body
     * still walked. Its first root delta is recorded for the next Update. */
    double d[3]={0,0,.05};
    tick(&m,false,d,&v,&owned);assert(!owned&&v.z==5);
    /* S+1: Move has ended and nothing is latched yet: source ground drag only. */
    d[2]=.10;tick(&m,true,d,&v,&owned);
    assert(owned&&near(v.z,5-1.5f*25*DT)&&v.x==0);
    /* S+2: the body takes the request built from the S delta (.05 m -> 3 m/s). */
    d[2]=.02;tick(&m,true,d,&v,&owned);assert(owned&&near(v.z,3));
    /* S+3: then the S+1 delta (.10 m -> 6 m/s), two ticks after its pose. */
    d[2]=0;tick(&m,true,d,&v,&owned);assert(owned&&near(v.z,6));
    tick(&m,true,d,&v,&owned);assert(near(v.z,1.2f));
    /* A zero delta is a zero request, not a missing one: desired velocity is
     * zero, so the body coasts under drag from 1.2 m/s. */
    tick(&m,true,d,&v,&owned);assert(owned&&near(v.z,1.2f-1.5f*1.44f*DT));
}
static void ten_metre_hold_and_twelve_correction(void){
    PeSwordMotor m;pe_sword_motor_reset(&m);PeVec3 v={0,0,5};bool owned;
    double d[3]={0,0,13.7/60};
    tick(&m,false,d,&v,NULL);tick(&m,true,d,&v,&owned);
    /* 13.7 m/s of root motion is held to MaxSubSpeed 10 by the controller. */
    tick(&m,true,d,&v,&owned);assert(owned&&near(v.z,10));
    tick(&m,true,d,&v,&owned);assert(near(v.z,10));
    assert(near(m.phy.applied_velocity.z,10));
    /* The 12-per-update correction comes first and measures against the velocity at Update. */
    pe_sword_motor_reset(&m);v=(PeVec3){0,0,-8};
    double e[3]={0,0,9.0/60};
    tick(&m,false,e,&v,NULL);tick(&m,true,e,&v,NULL);
    float coast=v.z; /* -8 after one tick of drag */
    assert(near(m.phy.applied_velocity.z,coast+12)&&coast+12<9);
    tick(&m,true,e,&v,NULL);assert(near(v.z,coast+12));
    /* Diagonal requests keep direction under the hold. */
    pe_sword_motor_reset(&m);v=(PeVec3){6,0,8};
    double g[3]={.18,0,.24}; /* 10.8, 14.4 -> 18 m/s */
    tick(&m,false,g,&v,NULL);tick(&m,true,g,&v,NULL);tick(&m,true,g,&v,NULL);
    assert(near(v.x,6)&&near(v.z,8));
}
static void end_cancel_and_release(void){
    PeSwordMotor m;pe_sword_motor_reset(&m);PeVec3 v={0,0,0};bool owned;
    double a[3]={0,0,.03},b[3]={0,0,.06},c[3]={0,0,.09};
    tick(&m,false,a,&v,NULL);          /* S: delta a */
    tick(&m,true,b,&v,NULL);           /* S+1: requests a; delta b */
    tick(&m,true,c,&v,&owned);         /* E: moves with a; requests b; terminal delta c; action ends in this advance */
    assert(owned&&near(v.z,1.8f));
    /* E+1: FixedUpdate still runs with the b request before Update cancels. */
    tick(&m,false,NULL,&v,&owned);assert(owned&&near(v.z,3.6f));
    assert(m.phy.applied_velocity.z==0&&m.phy.request_velocity.z==0&&!m.has_last_move&&!m.engaged);
    /* E+2: released. The terminal delta c (5.4 m/s) never reached the body. */
    tick(&m,false,NULL,&v,&owned);assert(!owned&&near(v.z,3.6f));
    /* EndImmediately path: the host resets the motor; nothing lingers. */
    tick(&m,false,a,&v,NULL);tick(&m,true,b,&v,NULL);
    assert(m.phy.applied_velocity.z!=0);
    pe_sword_motor_reset(&m);v=(PeVec3){0};
    tick(&m,false,NULL,&v,&owned);assert(!owned&&v.z==0);
    /* A combo stage boundary supplies deltas on every tick, so the latch never empties. */
    pe_sword_motor_reset(&m);v=(PeVec3){0};
    tick(&m,false,a,&v,NULL);
    for(int i=0;i<10;i++){tick(&m,true,a,&v,&owned);assert(owned);}
    assert(near(v.z,1.8f));
    /* Airborne mid-action: only source air drag while nothing is latched. */
    pe_sword_motor_reset(&m);v=(PeVec3){0,-3,5};PeSwordMotorStep step;
    assert(pe_sword_motor_consume(&m,true,v,false,DT,&step)&&step.owns_motion&&step.coasting);
    assert(near(step.velocity.z,5-.00600000005f*25*DT)&&step.velocity.y==0);
    /* Drag never reverses a slow body within one step. */
    pe_sword_motor_reset(&m);
    assert(pe_sword_motor_consume(&m,true,(PeVec3){0,0,60},true,.25f,&step)&&step.velocity.z==0);
}
static void invalid_input(void){
    PeSwordMotor m;pe_sword_motor_reset(&m);PeSwordMotor before=m;PeSwordMotorStep step,original;
    memset(&step,0x5A,sizeof step);original=step;double d[3]={0,0,.1},bad[3]={0,NAN,0};
    assert(!pe_sword_motor_consume(&m,true,(PeVec3){NAN,0,0},true,DT,&step));
    assert(!pe_sword_motor_consume(&m,true,(PeVec3){0},true,0,&step)&&!pe_sword_motor_consume(&m,true,(PeVec3){0},true,DT,NULL));
    assert(!pe_sword_motor_consume(NULL,true,(PeVec3){0},true,DT,&step));
    assert(!pe_sword_motor_update(&m,true,bad,(PeVec3){0},DT)&&!pe_sword_motor_update(&m,true,d,(PeVec3){0,0,INFINITY},DT));
    assert(!pe_sword_motor_update(&m,true,d,(PeVec3){0},0)&&!pe_sword_motor_update(NULL,true,d,(PeVec3){0},DT));
    assert(!memcmp(&m,&before,sizeof m)&&!memcmp(&step,&original,sizeof step));
    pe_sword_motor_reset(NULL);
}
int pe_test_sword_motor(void){
    idle_and_two_tick_lag();ten_metre_hold_and_twelve_correction();end_cancel_and_release();invalid_input();
    return 0;
}
