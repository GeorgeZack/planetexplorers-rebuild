#include "pe_player_locomotion.h"
#include <math.h>
#include <string.h>
static const char *names[]={"NormalIdle","NormalWF","NormalRF","Jump","Fall","Land"};
const char *pe_player_locomotion_name(PePlayerLocomotionClip clip){return clip>=0&&clip<PE_LOCOMOTION_COUNT?names[clip]:NULL;}
static bool durations_valid(const double*d){if(!d)return false;for(int i=0;i<PE_LOCOMOTION_COUNT;i++)if(!isfinite(d[i])||d[i]<.001||d[i]>120)return false;return true;}
static bool valid(const PePlayerLocomotion*s){return s&&s->ready&&durations_valid(s->duration)&&s->clip>=0&&s->clip<PE_LOCOMOTION_COUNT&&isfinite(s->seconds)&&s->seconds>=0&&s->seconds<=s->duration[s->clip]&&isfinite(s->air_time)&&s->air_time>=0&&s->air_time<=3600;}
bool pe_player_locomotion_init(PePlayerLocomotion*s,const double d[PE_LOCOMOTION_COUNT]){
    if(!s||!durations_valid(d))return false;
    PePlayerLocomotion n={0};memcpy(n.duration,d,sizeof n.duration);n.ready=true;*s=n;return true;
}
static bool cyclic(PePlayerLocomotionClip c){return c==PE_LOCOMOTION_IDLE||c==PE_LOCOMOTION_WALK||c==PE_LOCOMOTION_RUN||c==PE_LOCOMOTION_FALL;}
static void select_clip(PePlayerLocomotion*s,PePlayerLocomotionClip clip){
    if(s->clip==clip)return;
    /* A bounded presentation approximation: gait changes preserve foot phase,
     * while idle/air/landing state changes start their own authored timeline. */
    bool gait=(s->clip==PE_LOCOMOTION_WALK||s->clip==PE_LOCOMOTION_RUN)&&(clip==PE_LOCOMOTION_WALK||clip==PE_LOCOMOTION_RUN);
    s->seconds=gait?s->seconds/s->duration[s->clip]*s->duration[clip]:0;s->clip=clip;
}
bool pe_player_locomotion_step(PePlayerLocomotion*s,PePlayerLocomotionInput in,double dt){
    if(!valid(s)||!isfinite(dt)||dt<0||dt>1||!isfinite(in.velocity.x)||!isfinite(in.velocity.y)||!isfinite(in.velocity.z)||fabsf(in.velocity.x)>100||fabsf(in.velocity.y)>100||fabsf(in.velocity.z)>100)return false;
    if(dt==0)return true;
    PePlayerLocomotion n=*s;double speed=hypot(in.velocity.x,in.velocity.z);
    bool moving=speed>.05;
    PePlayerLocomotionClip ground=moving?(in.walk&&!in.sprint?PE_LOCOMOTION_WALK:PE_LOCOMOTION_RUN):PE_LOCOMOTION_IDLE;
    if(in.jump_started){n.airborne=true;n.air_time=0;select_clip(&n,PE_LOCOMOTION_JUMP);n.seconds=0;*s=n;return true;}
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
    /* No transition blend is represented. A newly selected state shows its
     * first frame; continuing states consume this update's authored delta. */
    if(n.clip==previous){
        double rate=1;
        if(n.clip==PE_LOCOMOTION_WALK)rate=fmin(2,fmax(.25,speed/2));
        if(n.clip==PE_LOCOMOTION_RUN)rate=fmin(2,fmax(.25,speed/5));
        double time=n.seconds+dt*rate;
        n.seconds=cyclic(n.clip)?fmod(time,n.duration[n.clip]):fmin(time,n.duration[n.clip]);
    }
    *s=n;return true;
}
