#include "pe_attack_playback.h"
#include "../generated/pe_attack_playback_source.h"
#include <math.h>
static bool valid(const PeAttackPlayback *s){
    return s&&isfinite(s->time)&&s->time>=0&&isfinite(s->last_contact)&&
        s->last_contact>=0&&s->last_contact<=s->time&&isfinite(s->frame_contact)&&
        s->frame_contact>=0&&s->frame_contact<=s->last_contact&&
        (s->initialized||(s->time==0&&s->last_contact==0&&s->frame_contact==0));
}
bool pe_attack_playback_sample(PeAttackPlayback *s,float now,int item,float stamina,PeAttackPlaybackSample *out){
    if(!out||!valid(s)||!isfinite(now)||now<s->time)return false;
    float speed=pe_combat_item_animation_speed(item,stamina);
    if(speed<=0)return false;
    float frame_contact=(!s->initialized||now>s->time)?s->last_contact:s->frame_contact;
    PeAttackPlaybackSample result={.base_speed=speed,.elapsed=now-frame_contact};
    result.curve_active=result.elapsed<pe_attack_playback_source.duration_seconds;
    if(result.curve_active)result.curve_time=result.elapsed/pe_attack_playback_source.duration_seconds;
    s->time=now;s->frame_contact=frame_contact;s->initialized=true;*out=result;return true;
}
bool pe_attack_playback_contact(PeAttackPlayback *s){
    if(!valid(s)||!s->initialized)return false;
    s->last_contact=s->time;return true;
}
/* Explicit float boundaries match the native SSE polynomial path; do not fuse
 * multiply/add or expand this into a differently rounded basis polynomial. */
static float add(float a,float b){volatile float v=a+b;return v;}
static float sub(float a,float b){volatile float v=a-b;return v;}
static float mul(float a,float b){volatile float v=a*b;return v;}
static float divide(float a,float b){volatile float v=a/b;return v;}
bool pe_attack_playback_curve(float t,float *value){
    if(!value||!isfinite(t)||t<0||t>1)return false;
    if(t<=pe_attack_playback_keys[0].time){*value=pe_attack_playback_keys[0].value;return true;}
    const PeAttackPlaybackKey *last=&pe_attack_playback_keys[PE_ATTACK_PLAYBACK_KEY_COUNT-1];
    if(t>=last->time){*value=last->value;return true;}
    for(unsigned i=1;i<PE_ATTACK_PLAYBACK_KEY_COUNT;i++)if(t<pe_attack_playback_keys[i].time){
        const PeAttackPlaybackKey *a=&pe_attack_playback_keys[i-1],*b=&pe_attack_playback_keys[i];
        float h=fmaxf(sub(b->time,a->time),1e-4f),delta=sub(b->value,a->value);
        float left=mul(h,a->out_slope),right=mul(h,b->in_slope),inv=divide(1,mul(h,h));
        float cubic=divide(mul(sub(sub(add(left,right),delta),delta),inv),h);
        float quadratic=mul(sub(sub(sub(add(add(delta,delta),delta),left),left),right),inv);
        float x=sub(t,a->time);
        float result=add(mul(add(mul(add(mul(cubic,x),quadratic),x),a->out_slope),x),a->value);
        if(!isfinite(result))return false;
        *value=result;return true;
    }
    return false;
}
bool pe_attack_playback_speed(PeAttackPlaybackSample s,float curve,float *speed){
    if(!speed||!isfinite(s.base_speed)||s.base_speed<pe_attack_playback_source.minimum_base_speed||
       s.base_speed>pe_attack_playback_source.maximum_base_speed||!isfinite(s.elapsed)||s.elapsed<0||
       s.curve_active!=(s.elapsed<pe_attack_playback_source.duration_seconds)||
       (s.curve_active&&(!isfinite(s.curve_time)||s.curve_time!=s.elapsed/pe_attack_playback_source.duration_seconds)))return false;
    float value=s.curve_active?s.base_speed*curve:s.base_speed;
    if(!isfinite(value)||value<0)return false; /* Native forward-clock bound. */
    *speed=value;return true;
}
