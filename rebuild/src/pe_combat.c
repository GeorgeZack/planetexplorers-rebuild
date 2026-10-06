#include "pe_combat.h"
#include "../generated/pe_starter_sword_shield_source.h"
#include <float.h>
#include <math.h>
#include <string.h>

/* Exact float-decoded authored clip events from Unity5.2.4f1 installed
 * sharedassets1.assets SHA256 d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67
 * FemaleAnimator2(779), MaleAnimator override(780). Reproduce with
 * tools/inventory_player_animation.py; see docs/audit/PLAYER_ANIMATION.md.
 * Tables exclude audiovisual callbacks; order of mechanical events is kept. */
static const PeCombatClip clips[2][2][4] = {
    {
        {
            {231, {{0.0687236562371254, PE_COMBAT_HIT_START}, {0.15694579482078552, PE_COMBAT_COMBO_START}, {0.29946303367614746, PE_COMBAT_HIT_END}, {0.451433390378952, PE_COMBAT_COMBO_END}, {0.7091259360313416, PE_COMBAT_ACTION_END}}},
            {240, {{0.17313574254512787, PE_COMBAT_HIT_START}, {0.2717646062374115, PE_COMBAT_COMBO_START}, {0.4647010564804077, PE_COMBAT_HIT_END}, {0.6226184368133545, PE_COMBAT_COMBO_END}, {0.8369432091712952, PE_COMBAT_ACTION_END}}},
            {247, {{0.1389591544866562, PE_COMBAT_HIT_START}, {0.26057448983192444, PE_COMBAT_COMBO_START}, {0.42548567056655884, PE_COMBAT_HIT_END}, {0.5481649041175842, PE_COMBAT_COMBO_END}, {0.8732097148895264, PE_COMBAT_ACTION_END}}},
            {254, {{0.0385129339993, PE_COMBAT_HIT_START}, {0.09853022545576096, PE_COMBAT_COMBO_START}, {0.2535538673400879, PE_COMBAT_HIT_END}, {0.3644658923149109, PE_COMBAT_COMBO_END}, {0.5349509716033936, PE_COMBAT_ACTION_END}}},
        },
        {
            {499, {{0.05357366427779198, PE_COMBAT_HIT_START}, {0.15608230233192444, PE_COMBAT_COMBO_START}, {0.3297732472419739, PE_COMBAT_HIT_END}, {0.4939122796058655, PE_COMBAT_COMBO_END}, {0.791520893573761, PE_COMBAT_ACTION_END}}},
            {504, {{0.10482795536518097, PE_COMBAT_HIT_START}, {0.22237010300159454, PE_COMBAT_COMBO_START}, {0.43721404671669006, PE_COMBAT_HIT_END}, {0.6368725895881653, PE_COMBAT_COMBO_END}, {0.8786963224411011, PE_COMBAT_ACTION_END}}},
            {509, {{0.10924293100833893, PE_COMBAT_HIT_START}, {0.2359457015991211, PE_COMBAT_COMBO_START}, {0.41786473989486694, PE_COMBAT_HIT_END}, {0.540788471698761, PE_COMBAT_COMBO_END}, {0.8842633366584778, PE_COMBAT_ACTION_END}}},
            {515, {{0.058486443012952805, PE_COMBAT_HIT_START}, {0.0700094923377037, PE_COMBAT_COMBO_START}, {0.21739012002944946, PE_COMBAT_HIT_END}, {0.4034821391105652, PE_COMBAT_COMBO_END}, {0.5355193614959717, PE_COMBAT_ACTION_END}}},
        },
    },
    {
        {
            {179, {{0.08961795270442963, PE_COMBAT_COMBO_START}, {0.12368965893983841, PE_COMBAT_HIT_START}, {0.34291690587997437, PE_COMBAT_HIT_END}, {0.3587287962436676, PE_COMBAT_COMBO_END}, {0.9103504419326782, PE_COMBAT_ACTION_END}}},
            {192, {{0.08349296450614929, PE_COMBAT_COMBO_START}, {0.1307704895734787, PE_COMBAT_HIT_START}, {0.34284707903862, PE_COMBAT_HIT_END}, {0.34323036670684814, PE_COMBAT_COMBO_END}, {1.0103625059127808, PE_COMBAT_ACTION_END}}},
            {203, {{0.0702059343457222, PE_COMBAT_COMBO_START}, {0.12909388542175293, PE_COMBAT_HIT_START}, {0.359584242105484, PE_COMBAT_COMBO_END}, {0.398490846157074, PE_COMBAT_HIT_END}, {0.660395622253418, PE_COMBAT_ACTION_END}}},
            {213, {{0.3477517068386078, PE_COMBAT_COMBO_START}, {0.41856271028518677, PE_COMBAT_HIT_START}, {0.6489682793617249, PE_COMBAT_COMBO_END}, {0.7323139309883118, PE_COMBAT_HIT_END}, {1.2319550514221191, PE_COMBAT_ACTION_END}}},
        },
        {
            {419, {{0.17273004353046417, PE_COMBAT_COMBO_START}, {0.230301633477211, PE_COMBAT_HIT_START}, {0.39256414771080017, PE_COMBAT_HIT_END}, {0.5254658460617065, PE_COMBAT_COMBO_END}, {0.9630920886993408, PE_COMBAT_ACTION_END}}},
            {424, {{0.18844181299209595, PE_COMBAT_COMBO_START}, {0.23729650676250458, PE_COMBAT_HIT_START}, {0.42821964621543884, PE_COMBAT_COMBO_END}, {0.5635973215103149, PE_COMBAT_HIT_END}, {0.9552344083786011, PE_COMBAT_ACTION_END}}},
            {428, {{0.17757020890712738, PE_COMBAT_COMBO_START}, {0.2256174385547638, PE_COMBAT_HIT_START}, {0.3471084535121918, PE_COMBAT_HIT_END}, {0.543795108795166, PE_COMBAT_COMBO_END}, {1.104954719543457, PE_COMBAT_ACTION_END}}},
            {433, {{0.3687412142753601, PE_COMBAT_COMBO_START}, {0.4269886314868927, PE_COMBAT_HIT_START}, {0.6558063626289368, PE_COMBAT_HIT_END}, {0.7154244780540466, PE_COMBAT_COMBO_END}, {1.1928558349609375, PE_COMBAT_ACTION_END}}},
        },
    },
};

/* FemaleAnimator2(779), RunSwordAttack408, same installed asset hash above.
 * Exact float-decoded events: reference_root_motion_bake.py --inventory and
 * build/root-motion-bake/running-knife-pose-plan.json. Clip length is
 * 1.8333396911621094; EndAction deliberately ends sooner. Skill20110014 then
 * continues through the ordinary female TP Attack7..9 guides. */
static const PeCombatClip running_female={408,{
    {0.2968580424785614,PE_COMBAT_HIT_START},
    {0.4202437996864319,PE_COMBAT_COMBO_START},
    {0.7091204524040222,PE_COMBAT_HIT_END},
    {0.7872685194015503,PE_COMBAT_COMBO_END},
    {1.1680725812911987,PE_COMBAT_ACTION_END}}};

const PeCombatClip *pe_combat_clip(PeCombatGender gender,PeCombatView view,unsigned stage){
    if(gender<PE_COMBAT_FEMALE||gender>PE_COMBAT_MALE||view<PE_COMBAT_THIRD_PERSON||view>PE_COMBAT_FIRST_PERSON||stage>=4)return NULL;
    return &clips[gender][view][stage];
}
const PeCombatClip *pe_combat_variant_clip(PeCombatGender gender,PeCombatView view,PeCombatVariant variant,unsigned stage){
    if(variant==PE_COMBAT_STATIONARY)return pe_combat_clip(gender,view,stage);
    if(variant!=PE_COMBAT_RUNNING||gender!=PE_COMBAT_FEMALE||view!=PE_COMBAT_THIRD_PERSON||stage>=4)return NULL;
    return stage?pe_combat_clip(gender,view,stage):&running_female;
}
const char *pe_combat_pose_name(const PeCombat *a){
    static const char *const names[]={"Attack6","Attack7","Attack8","Attack9"};
    if(!a||!pe_combat_variant_clip(a->gender,a->view,a->variant,a->stage))return NULL;
    return a->variant==PE_COMBAT_RUNNING&&a->stage==0?"RunSwordAttack":names[a->stage];
}
void pe_combat_init(PeCombat *a){if(a)memset(a,0,sizeof *a);}
void pe_combat_stop(PeCombat *a){
    if(!a)return;
    a->active=false;a->contact_open=false;a->combo_window=false;
    a->combo_queued=false;a->pending_close=false;
}
static bool stamina_valid(float stamina){return isfinite(stamina)&&stamina>=0;}
static float charge(float stamina,float reduction){
    return (float)fmax(0,(double)stamina-5.0*(double)reduction);
}
bool pe_combat_begin(PeCombat *a,PeCombatGender gender,PeCombatView view,float reduction,float *stamina){
    return pe_combat_begin_variant(a,gender,view,PE_COMBAT_STATIONARY,reduction,stamina);
}
bool pe_combat_begin_variant(PeCombat *a,PeCombatGender gender,PeCombatView view,PeCombatVariant variant,float reduction,float *stamina){
    if(!a||a->active||!stamina||!stamina_valid(*stamina)||!isfinite(reduction)||reduction<0||!pe_combat_variant_clip(gender,view,variant,0))return false;
    PeCombat next={0};next.active=true;next.gender=gender;next.view=view;next.variant=variant;next.stamina_reduce_percent=reduction;
    float remaining=charge(*stamina,reduction);*a=next;*stamina=remaining;return true;
}
float pe_combat_animation_speed(float stamina){
    return pe_combat_item_animation_speed(33,stamina);
}
float pe_combat_item_animation_speed(int item_id,float stamina){
    if(!stamina_valid(stamina))return 0;
    float speed;
    if(item_id==33)speed=1.37f;
    else if(item_id==pe_starter_sword_source.item_id)speed=pe_starter_sword_source.animation_speed;
    else return 0;
    if(stamina<=50)speed*=.9f;
    if(stamina<=20)speed*=.9f;
    return fminf(1.5f,fmaxf(.5f,speed));
}
double pe_combat_stationary_clip_rate(PeCombatView view,double animator_speed){
    if(view<PE_COMBAT_THIRD_PERSON||view>PE_COMBAT_FIRST_PERSON||
       !isfinite(animator_speed)||animator_speed<0)return 0;
    return animator_speed*(view==PE_COMBAT_FIRST_PERSON?.65:1.0);
}
bool pe_combat_queue_combo(PeCombat *a){
    if(!a||!a->active||!a->combo_window||a->pending_close||a->combo_queued||a->stage>=3)return false;
    a->combo_queued=true;return true;
}
static bool close_window(PeCombat *a,float *stamina){
    a->contact_open=false;a->pending_close=false;a->combo_window=false;
    if(!a->combo_queued||a->stage>=3)return false;
    a->stage++;a->event_index=0;a->target_count=0;a->combo_queued=false;
    a->clip_time=0;a->wall_time=0;
    *stamina=charge(*stamina,a->stamina_reduce_percent);return true;
}
PeCombatAdvance pe_combat_advance(PeCombat *a,double clip_delta,double wall_delta,float *stamina){
    PeCombatInterval interval;return pe_combat_advance_interval(a,clip_delta,wall_delta,stamina,&interval);
}
PeCombatAdvance pe_combat_advance_interval(PeCombat *a,double clip_delta,double wall_delta,float *stamina,PeCombatInterval *interval){
    if(!a||!stamina||!interval||!stamina_valid(*stamina)||!isfinite(clip_delta)||!isfinite(wall_delta)||clip_delta<0||wall_delta<0)return PE_COMBAT_INVALID;
    if(!a->active){*interval=(PeCombatInterval){0};return PE_COMBAT_IDLE;}
    const PeCombatClip *clip=pe_combat_variant_clip(a->gender,a->view,a->variant,a->stage);
    if(!clip||a->event_index>5||a->target_count>PE_COMBAT_TARGET_CAPACITY||
       a->update_index==UINT64_MAX||a->opened_update>a->update_index||!isfinite(a->clip_time)||!isfinite(a->wall_time)||
       a->clip_time<0||a->wall_time<0||!isfinite(a->stamina_reduce_percent)||a->stamina_reduce_percent<0||
       !isfinite(a->clip_time+clip_delta)||!isfinite(a->wall_time+wall_delta))return PE_COMBAT_INVALID;
    PeCombat next=*a;float remaining=*stamina;PeCombatAdvance result=PE_COMBAT_ADVANCED;
    PeCombatInterval span={.valid=true,.variant=a->variant,.gender=a->gender,.view=a->view,.stage=a->stage,
        .from=a->clip_time,.to=a->clip_time+clip_delta};
    next.update_index++;next.clip_time+=clip_delta;next.wall_time+=wall_delta;
    if(next.pending_close&&next.update_index-next.opened_update>=2&&close_window(&next,&remaining)){
        result=PE_COMBAT_NEXT_STAGE;span.to=span.from;
    }else{
        while(next.active&&next.event_index<5&&clip->events[next.event_index].time<=next.clip_time){
            PeCombatEvent authored=clip->events[next.event_index++];PeCombatEventType event=authored.type;
            switch(event){
            case PE_COMBAT_HIT_START:next.contact_open=true;next.opened_update=next.update_index;break;
            case PE_COMBAT_HIT_END:next.combo_window=false;next.pending_close=true;break;
            case PE_COMBAT_COMBO_START:next.combo_window=true;break;
            case PE_COMBAT_COMBO_END:next.combo_window=false;break;
            case PE_COMBAT_ACTION_END:
                /* Action_Sword.OnAnimEvent ignores stale EndAction after a
                 * combo until scaled Time.time-lastAttackTime reaches .3 game seconds.
                 * wall_time is the legacy API name for that scaled action age. */
                if(next.wall_time>=.3){pe_combat_stop(&next);result=PE_COMBAT_ENDED;span.to=fmax(span.from,authored.time);}
                break;
            }
            if(next.pending_close&&next.update_index-next.opened_update>=2){
                if(close_window(&next,&remaining)){result=PE_COMBAT_NEXT_STAGE;span.to=fmax(span.from,authored.time);break;}
            }
        }
    }
    *a=next;*stamina=remaining;*interval=span;return result;
}
bool pe_combat_record_contact(PeCombat *a,uint64_t target){
    /* SkInst.ExecEventsFromCol297-309 stores target entity once per guide;
     * the next skill stage clears the list at377. */
    if(!a||!a->active||!a->contact_open||!target||a->target_count>=PE_COMBAT_TARGET_CAPACITY)return false;
    for(unsigned i=0;i<a->target_count;i++)if(a->targets[i]==target)return false;
    a->targets[a->target_count++]=target;return true;
}
bool pe_combat_damage(float attack,float defense,bool shield_block,float protection,bool critical,float scale,float *damage){
    if(!damage||!isfinite(attack)||!isfinite(defense)||!isfinite(protection)||!isfinite(scale)||
       attack<0||defense<0||protection<0||protection>1)return false;
    double a=attack,value=a>0?a*a/((double)defense+a):0;
    if(shield_block)value*=1.0-(double)protection;
    if(critical)value*=2;
    /* PEMath.cs Linux build epsilon; other platforms use Single.Epsilon. */
    if(scale>1.192092896e-7f)value*=scale;
    if(!isfinite(value)||value>FLT_MAX)return false;
    *damage=(float)value;return true;
}
