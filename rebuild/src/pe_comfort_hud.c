#include "pe_comfort_hud.h"
#include "../generated/pe_comfort_hud_source.h"
#include <math.h>
#include <stddef.h>
/* Explicit float boundaries, as in pe_attack_playback's polynomial; do not fuse
 * multiply/add or expand this into a differently rounded basis polynomial. */
static float add(float a,float b){volatile float v=a+b;return v;}
static float sub(float a,float b){volatile float v=a-b;return v;}
static float mul(float a,float b){volatile float v=a*b;return v;}
static float divide(float a,float b){volatile float v=a/b;return v;}

bool pe_comfort_hud_curve(float t,float *value){
    if(!value||!isfinite(t))return false;
    const PeComfortHudKey *k=pe_comfort_hud_source_keys,*last=k+PE_COMFORT_HUD_SOURCE_KEYS-1;
    /* Serialized pre/post infinity 2: clamp to the end keys. */
    if(t<=k[0].time){*value=k[0].value;return true;}
    if(t>=last->time){*value=last->value;return true;}
    /* Start-inclusive segments: an interior key's exact time selects the next segment. */
    for(unsigned i=1;i<PE_COMFORT_HUD_SOURCE_KEYS;i++)if(t<k[i].time){
        const PeComfortHudKey *a=&k[i-1],*b=&k[i];
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
void pe_comfort_hud_reset(PeComfortHud *h){
    if(!h)return;
    *h=(PeComfortHud){.last_comfort=pe_comfort_hud_source.initial_last_comfort,.alpha=pe_comfort_hud_source.initial_alpha,
        .slider=pe_comfort_hud_source.initial_slider,.face=PE_COMFORT_FACE3};
}
static bool valid(const PeComfortHud *h){
    return h&&isfinite(h->last_comfort)&&isfinite(h->curve_time)&&h->curve_time>=0&&isfinite(h->alpha)&&
        isfinite(h->slider)&&h->slider>=0&&h->slider<=1&&h->face>=PE_COMFORT_FACE1&&h->face<=PE_COMFORT_FACE3;
}
bool pe_comfort_hud_frame(PeComfortHud *h,float comfort,float comfort_max,float dt){
    if(!valid(h)||!isfinite(comfort)||!isfinite(comfort_max)||!isfinite(dt)||dt<0)return false;
    PeComfortHud n=*h;
    /* mComfort_4.sliderValue = maxValue <= 0 ? 0 : curValue / maxValue. */
    float slider=comfort_max<=0?0:comfort/comfort_max;
    if(!isfinite(slider))return false;
    n.slider=fminf(1,fmaxf(0,slider));
    /* The fall check: previous frame only, never an accumulated drop. */
    if(comfort>n.last_comfort)n.last_comfort=comfort;
    else if(n.last_comfort-comfort>pe_comfort_hud_source.fall_value){n.playing=true;n.last_comfort=comfort;n.curve_time=0;}
    else n.last_comfort=comfort;
    if(n.playing){
        n.curve_time+=dt;
        float alpha;if(!isfinite(n.curve_time)||!pe_comfort_hud_curve(n.curve_time,&alpha))return false;
        n.alpha=alpha;
        if(n.alpha>=1)n.playing=false;
    }
    n.face=n.slider<pe_comfort_hud_source.face2_below?PE_COMFORT_FACE3:
           n.slider<pe_comfort_hud_source.face1_from?PE_COMFORT_FACE2:PE_COMFORT_FACE1;
    *h=n;return true;
}
const char *pe_comfort_hud_face_sprite(PeComfortFace face){
    static const char *names[]={"face1","face2","face3"};
    return face>=PE_COMFORT_FACE1&&face<=PE_COMFORT_FACE3?names[face-PE_COMFORT_FACE1]:NULL;
}
