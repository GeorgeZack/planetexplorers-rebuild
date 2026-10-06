#include "pe_player_locomotion.h"
#include <math.h>
#include <string.h>
static const char *names[]={"NormalIdle","NormalWF","NormalRF","Jump","Fall","Land",
    "NormalWL","NormalWR","NormalWB","NormalRL","NormalRR","NormalRB"};
const char *pe_player_locomotion_name(PePlayerLocomotionClip clip){return clip>=0&&clip<PE_LOCOMOTION_COUNT?names[clip]:NULL;}
static bool durations_valid(const double*d){if(!d)return false;for(int i=0;i<PE_LOCOMOTION_COUNT;i++)if(!isfinite(d[i])||d[i]<.001||d[i]>120)return false;return true;}
static bool gait(PePlayerLocomotionClip c);
static bool valid(const PePlayerLocomotion*s){
    if(!s||!s->ready||!durations_valid(s->duration)||s->clip<0||s->clip>=PE_LOCOMOTION_COUNT||
       !isfinite(s->seconds)||s->seconds<0||s->seconds>s->duration[s->clip]||
       !isfinite(s->air_time)||s->air_time<0||s->air_time>3600||!gait(s->blend_gait))return false;
    double sum=0;
    for(int i=0;i<PE_LOCOMOTION_COUNT;i++){
        double w=s->blend.weights[i],t=s->blend.seconds[i];
        if(!isfinite(w)||w<0||w>1||!isfinite(t)||t<0||t>s->duration[i])return false;
        sum+=w;
    }
    return fabs(sum-1)<1e-8;
}
bool pe_player_locomotion_init(PePlayerLocomotion*s,const double d[PE_LOCOMOTION_COUNT]){
    if(!s||!durations_valid(d))return false;
    PePlayerLocomotion n={0};memcpy(n.duration,d,sizeof n.duration);n.ready=true;n.blend.weights[PE_LOCOMOTION_IDLE]=1;n.blend_gait=PE_LOCOMOTION_RUN;*s=n;return true;
}
static bool walking(PePlayerLocomotionClip c){return c==PE_LOCOMOTION_WALK||(c>=PE_LOCOMOTION_WALK_LEFT&&c<=PE_LOCOMOTION_WALK_BACK);}
static bool running(PePlayerLocomotionClip c){return c==PE_LOCOMOTION_RUN||(c>=PE_LOCOMOTION_RUN_LEFT&&c<=PE_LOCOMOTION_RUN_BACK);}
static bool gait(PePlayerLocomotionClip c){return walking(c)||running(c);}
static bool cyclic(PePlayerLocomotionClip c){return c==PE_LOCOMOTION_IDLE||gait(c)||c==PE_LOCOMOTION_FALL;}
static double clip_rate(PePlayerLocomotionClip clip,double speed){
    if(walking(clip))return fmin(2,fmax(.25,speed/2));
    if(running(clip))return fmin(2,fmax(.25,speed/5));
    return 1;
}
static double advance_time(double time,double dt,double rate,double duration,bool loop){
    double next=time+dt*rate;return loop?fmod(next,duration):fmin(next,duration);
}
static void blend_step(PePlayerLocomotion *next,const PePlayerLocomotion *old,double speed,double dt,bool restart){
    if(gait(next->clip))next->blend_gait=next->clip;
    PePlayerLocomotionClip driver=next->blend_gait;
    bool gait_active=false;
    for(int i=0;i<PE_LOCOMOTION_COUNT;i++)if(gait((PePlayerLocomotionClip)i)&&old->blend.weights[i]>0)gait_active=true;
    /* All gait channels share phase, including a direction currently fading
     * out. Reversing cannot reset a still-visible contributor's foot cycle. */
    double phase=gait_active?fmod(old->blend.seconds[driver]/old->duration[driver]+
                       dt*clip_rate(driver,speed)/old->duration[driver],1):0;
    double decay=exp(-dt/PE_LOCOMOTION_BLEND_TAU),total=0;
    for(int i=0;i<PE_LOCOMOTION_COUNT;i++){
        PePlayerLocomotionClip clip=(PePlayerLocomotionClip)i;
        if(gait(clip))next->blend.seconds[i]=phase*next->duration[i];
        else if(next->clip==clip&&!cyclic(clip)&&(next->clip!=old->clip||restart))
            next->blend.seconds[i]=0; /* New Jump/Land must replay after a prior completed one-shot. */
        else if(old->blend.weights[i]>0)
            next->blend.seconds[i]=advance_time(old->blend.seconds[i],dt,1,next->duration[i],cyclic(clip));
        else if(next->clip==clip)next->blend.seconds[i]=0;
        next->blend.weights[i]=old->blend.weights[i]*decay+(next->clip==clip?1-decay:0);
        total+=next->blend.weights[i];
    }
    for(int i=0;i<PE_LOCOMOTION_COUNT;i++)next->blend.weights[i]/=total;
}
bool pe_player_locomotion_snapshot(const PePlayerLocomotion *state,PePlayerLocomotionPose *pose){
    if(!pose||!valid(state))return false;
    PePlayerLocomotionPose result=state->blend;*pose=result;return true;
}
static PePlayerLocomotionClip ground_clip(PePlayerLocomotionInput in){
    bool walk=in.walk&&!in.sprint;
    if(in.directional){
        if(fabsf(in.local_right)>fabsf(in.local_forward))
            return in.local_right<0?(walk?PE_LOCOMOTION_WALK_LEFT:PE_LOCOMOTION_RUN_LEFT):(walk?PE_LOCOMOTION_WALK_RIGHT:PE_LOCOMOTION_RUN_RIGHT);
        if(in.local_forward<0)return walk?PE_LOCOMOTION_WALK_BACK:PE_LOCOMOTION_RUN_BACK;
    }
    return walk?PE_LOCOMOTION_WALK:PE_LOCOMOTION_RUN;
}
static void select_clip(PePlayerLocomotion*s,PePlayerLocomotionClip clip){
    if(s->clip==clip)return;
    /* A bounded presentation approximation: gait changes preserve foot phase,
     * while idle/air/landing state changes start their own authored timeline. */
    bool preserve=gait(s->clip)&&gait(clip);
    s->seconds=preserve?s->seconds/s->duration[s->clip]*s->duration[clip]:0;s->clip=clip;
}
bool pe_player_locomotion_step(PePlayerLocomotion*s,PePlayerLocomotionInput in,double dt){
    if(!valid(s)||!isfinite(dt)||dt<0||dt>1||!isfinite(in.velocity.x)||!isfinite(in.velocity.y)||!isfinite(in.velocity.z)||fabsf(in.velocity.x)>100||fabsf(in.velocity.y)>100||fabsf(in.velocity.z)>100)return false;
    if(in.directional&&(!isfinite(in.local_right)||!isfinite(in.local_forward)||fabsf(in.local_right)>100||fabsf(in.local_forward)>100))return false;
    if(dt==0)return true;
    PePlayerLocomotion n=*s;double speed=hypot(in.velocity.x,in.velocity.z);
    bool moving=speed>.05;
    PePlayerLocomotionClip ground=moving?ground_clip(in):PE_LOCOMOTION_IDLE;
    if(in.jump_started){n.airborne=true;n.air_time=0;select_clip(&n,PE_LOCOMOTION_JUMP);n.seconds=0;blend_step(&n,s,speed,dt,true);*s=n;return true;}
    /* Ground-contact update time is not additional ledge air time. Jump's
     * source ground grace still needs its action clock while grounded. */
    if(!in.grounded||(n.airborne&&n.clip==PE_LOCOMOTION_JUMP))n.air_time=fmin(3600,n.air_time+dt);
    if(!in.grounded)n.airborne=true;
    PePlayerLocomotionClip previous=n.clip;
    if(n.airborne){
        if(in.grounded&&(n.clip!=PE_LOCOMOTION_JUMP||n.air_time>.1)){
            bool landed=n.clip==PE_LOCOMOTION_JUMP||n.clip==PE_LOCOMOTION_FALL||n.air_time>.3;
            n.airborne=false;n.air_time=0;select_clip(&n,landed&&!moving?PE_LOCOMOTION_LAND:ground);
        }else if(n.clip==PE_LOCOMOTION_JUMP){
            if(n.seconds+dt>=.8500000238418579*n.duration[PE_LOCOMOTION_JUMP])select_clip(&n,PE_LOCOMOTION_FALL);
        }else if(n.air_time>.3)select_clip(&n,PE_LOCOMOTION_FALL);
    }else if(n.clip==PE_LOCOMOTION_LAND){
        if(moving||n.seconds+dt>=.699999988079071*n.duration[PE_LOCOMOTION_LAND])select_clip(&n,ground);
    }else select_clip(&n,ground);
    /* Retain the legacy selected-clock interface. Continuous presentation
     * clocks below are independent, including during repeated selection flips. */
    if(n.clip==previous){
        double rate=clip_rate(n.clip,speed);
        double time=n.seconds+dt*rate;
        n.seconds=cyclic(n.clip)?fmod(time,n.duration[n.clip]):fmin(time,n.duration[n.clip]);
    }
    blend_step(&n,s,speed,dt,false);*s=n;return true;
}
