#include "pe_hare_attack.h"
#include <math.h>
#include <string.h>

/* Exact exported hare-manifest combat mapping; source bundle SHA256
 * 94409ad1bf9570048a52c42671986907e2a5ca131f55f73737e5b7d7cff76c35
 * Controller4131577587453613724; PEAnimatorAttackTrigger/EX source semantics.
 * BTMelee BTAttack.cs474..710; skill own/shared cooldowns from skMain. */
static const PeHareAttackDef definitions[5]={
    {30100003,10100,1.0f,1.0,1,1,{{0.3720000088214874,0.6420000195503235,1,true,false},{0,0,0,false,false}}},
    {30100004,10100,1.0f,1.0000009536743164,1,1,{{0.3720000088214874,0.6420000195503235,2,true,false},{0,0,0,false,false}}},
    {30100008,10100,1.5f,1.5000004768371582,2,2,{{0.23999999463558197,0.3400000035762787,1,false,true},{0.5,0.6600000262260437,2,false,true}}},
    {30100046,10400,1.0f,1.0,1,1,{{0.2409999966621399,0.6309999823570251,1,true,false},{0,0,0,false,false}}},
    {30100047,10250,1.0666f,1.066667079925537,1,1,{{0.4189999997615814,0.699999988079071,3,true,false},{0,0,0,false,false}}},
};
static const PeHareAttackBone bones[2]={
    {"Bip01 R Forearm",INT64_C(-4777932767781160561),35,UINT32_C(923816839)},
    {"Bip01 L Forearm",INT64_C(5847284281709126597),13,UINT32_C(633050727)},
};

void pe_hare_attack_init(PeHareAttack *a){if(a){memset(a,0,sizeof *a);a->initialized=true;}}
const PeHareAttackDef *pe_hare_attack_def(unsigned attack){return attack<5?&definitions[attack]:NULL;}
const PeHareAttackBone *pe_hare_attack_bone(unsigned bone){return bone==1?&bones[0]:bone==2?&bones[1]:NULL;}
uint8_t pe_hare_attack_candidates(const PeHareAttack *a,double now,bool eligible,uint8_t runnable){
    if(!a||!a->initialized||a->active||!eligible||(runnable&~31u)||!isfinite(now)||now<0||
       !isfinite(a->last_melee_start)||a->last_melee_start<0||now-a->last_melee_start<=1||
       !isfinite(a->shared_ready)||now<a->shared_ready)return 0;
    uint8_t result=0;
    for(unsigned i=0;i<5;i++)if((runnable&(1u<<i))&&isfinite(a->skill_ready[i])&&now>=a->skill_ready[i])result|=(uint8_t)(1u<<i);
    return result;
}
bool pe_hare_attack_begin(PeHareAttack *a,double now,bool eligible,uint8_t runnable,unsigned rank){
    uint8_t candidates=pe_hare_attack_candidates(a,now,eligible,runnable);unsigned selected=5;
    for(unsigned i=0;i<5;i++)if(candidates&(1u<<i)){if(rank==0){selected=i;break;}rank--;}
    if(selected==5)return false;
    double own_ready=now+definitions[selected].skill_cooldown,shared_ready=now+1;
    if(!isfinite(own_ready)||!isfinite(shared_ready))return false;
    a->active=true;a->attack=selected;a->last_melee_start=now;a->shared_ready=shared_ready;
    a->skill_ready[selected]=own_ready;a->normalized_time=0;a->open_windows=0;a->ended_windows=0;
    memset(a->target_count,0,sizeof a->target_count);memset(a->targets,0,sizeof a->targets);return true;
}
bool pe_hare_attack_update(PeHareAttack *a,double time,PeHareAttackEvents *events){
    if(!a||!events||!a->initialized||!a->active||a->attack>=5||!isfinite(time)||time<0||
       !isfinite(a->normalized_time)||a->normalized_time<0||time<a->normalized_time)return false;
    const PeHareAttackDef *d=&definitions[a->attack];PeHareAttack next=*a;PeHareAttackEvents out={0};
    next.normalized_time=time;uint8_t open=0;
    for(unsigned i=0;i<d->window_count;i++){
        const PeHareAttackWindow *w=&d->windows[i];uint8_t bit=(uint8_t)(1u<<i);
        if(w->emits_monster_end){
            if(!(next.ended_windows&bit)){
                /* EX checks >=end before >=start. A skipped interval emits
                 * end but never briefly opens a fictitious contact window. */
                if(time>=w->end){next.ended_windows|=bit;out.monster_end_windows|=bit;}
                else if(time>=w->start)open|=bit;
            }
        }else if(time>=w->start&&(w->end_inclusive?time<=w->end:time<w->end))open|=bit;
        if(open&bit)out.active_bones|=w->bones;
    }
    out.opened_windows=(uint8_t)(open&~next.open_windows);
    out.closed_windows=(uint8_t)(next.open_windows&~open);
    out.open_windows=open;next.open_windows=open;*a=next;*events=out;return true;
}
void pe_hare_attack_end(PeHareAttack *a){if(a){a->active=false;a->open_windows=0;}}
bool pe_hare_attack_record_contact(PeHareAttack *a,unsigned window,unsigned guide,uint64_t target){
    if(!a||!a->initialized||!a->active||a->attack>=5||!target)return false;
    const PeHareAttackDef *d=&definitions[a->attack];
    if(window>=d->window_count||guide>=d->guide_count||!(a->open_windows&(1u<<window))||
       a->target_count[guide]>=PE_HARE_ATTACK_TARGET_CAPACITY)return false;
    for(unsigned i=0;i<a->target_count[guide];i++)if(a->targets[guide][i]==target)return false;
    a->targets[guide][a->target_count[guide]++]=target;return true;
}
