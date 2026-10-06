#include "pe_hare_damage.h"
#include "pe_combat.h"
#include <math.h>
#include <stdio.h>

static bool pose_valid(PeHareDamagePose p){
    return p.actor&&p.hitboxes&&pe_hitboxes_count(p.hitboxes)&&
        isfinite(p.position.x)&&isfinite(p.position.y)&&isfinite(p.position.z)&&
        fabsf(p.position.x)<=1000000&&fabsf(p.position.y)<=1000000&&fabsf(p.position.z)<=1000000&&
        isfinite(p.yaw)&&isfinite(p.scale)&&p.scale>0&&p.scale<=1000;
}
static bool action_valid(const PeHareAttack *a){
    if(!a||!a->initialized||!pe_hare_attack_def(a->attack)||!isfinite(a->normalized_time)||a->normalized_time<0||
       !isfinite(a->last_melee_start)||a->last_melee_start<0||!isfinite(a->shared_ready)||a->shared_ready<0)return false;
    for(unsigned i=0;i<PE_HARE_ATTACK_COUNT;i++)if(!isfinite(a->skill_ready[i])||a->skill_ready[i]<0)return false;
    for(unsigned i=0;i<2;i++){
        if(a->target_count[i]>PE_HARE_ATTACK_TARGET_CAPACITY)return false;
        for(unsigned j=0;j<a->target_count[i];j++){
            if(!a->targets[i][j])return false;
            for(unsigned k=0;k<j;k++)if(a->targets[i][j]==a->targets[i][k])return false;
        }
    }
    return true;
}
static bool posed(PeHareDamagePose p,size_t index,size_t clip,double seconds,PeHitboxCapsule *out){
    return pe_hitbox_pose(p.hitboxes,p.actor,index,clip,seconds,p.position,p.yaw,p.scale,out);
}
static bool target_pose_valid(const PeHareDamageTarget *t){
    if(t->blend_count>PE_ACTOR_BLEND_MAX)return false;
    if(t->blend_count)return pe_actor_blend_valid(t->pose.actor,t->blend,t->blend_count);
    return isfinite(t->seconds)&&t->seconds>=0&&
        (t->clip==PE_ACTOR_BIND_POSE||pe_actor_clip(t->pose.actor,t->clip));
}
static bool target_posed(const PeHareDamageTarget *t,size_t index,PeHitboxCapsule *out){
    if(t->blend_count)return pe_hitbox_pose_blend(t->pose.hitboxes,t->pose.actor,index,t->blend,t->blend_count,
        t->pose.position,t->pose.yaw,t->pose.scale,out);
    return posed(t->pose,index,t->clip,t->seconds,out);
}
PeHareDamageStatus pe_hare_damage_resolve(PeHareAttack *action,const PeHareDamageAttack *a,
    const PeHareDamageTarget *t,float *hp,PeHareDamageResult *result){
    float damage;
    if(!action_valid(action)||!a||!t||!hp||!result||!a->entity_id||!t->entity_id||
       !pose_valid(a->pose)||!pose_valid(t->pose)||!isfinite(*hp)||*hp<0||
       !isfinite(t->max_hp)||t->max_hp<=0||*hp>t->max_hp||!target_pose_valid(t)||
       (a->bone!=PE_HARE_RIGHT_FOREARM&&a->bone!=PE_HARE_LEFT_FOREARM)||
       !pe_combat_damage(a->attack,t->defense,t->shield_block,t->shield_protection,a->critical,1,&damage))return PE_HARE_DAMAGE_INVALID;
    const PeHareAttackDef *d=pe_hare_attack_def(action->attack);
    if(a->window>=d->window_count||a->guide>=d->guide_count)return PE_HARE_DAMAGE_INVALID;
    if(!action->active||!a->eligible||!t->eligible||*hp==0||a->entity_id==t->entity_id)return PE_HARE_DAMAGE_IGNORED;
    PeHareAttack next=*action;PeHareAttackEvents events;
    if(!pe_hare_attack_update(&next,next.normalized_time,&events))return PE_HARE_DAMAGE_INVALID;
    if(!(events.open_windows&(1u<<a->window))||!(d->windows[a->window].bones&a->bone)||
       !pe_hare_attack_record_contact(&next,a->window,a->guide,t->entity_id))return PE_HARE_DAMAGE_IGNORED;
    char name[16];snprintf(name,sizeof(name),"attack%u",action->attack);
    size_t clip=pe_actor_find_clip(a->pose.actor,name);const PeActorClip *info=pe_actor_clip(a->pose.actor,clip);
    if(!info||info->loop||fabs(info->duration-d->clip_duration)>.00001)return PE_HARE_DAMAGE_INVALID;
    double seconds=action->normalized_time*d->clip_duration;
    if(!isfinite(seconds))return PE_HARE_DAMAGE_INVALID;
    PeHareDamageResult out={0};bool found=false,have_attack=false,have_defence=false;
    for(size_t i=0;i<pe_hitboxes_count(a->pose.hitboxes);i++){
        const PeHitboxDefinition *at=pe_hitboxes_get(a->pose.hitboxes,i);
        if(at->kind!=PE_HITBOX_ATTACK||at->joint==PE_HITBOX_DISABLED||at->group!=a->bone)continue;
        PeHitboxCapsule attack_capsule;have_attack=true;
        if(!posed(a->pose,i,clip,seconds,&attack_capsule))return PE_HARE_DAMAGE_INVALID;
        for(size_t j=0;j<pe_hitboxes_count(t->pose.hitboxes);j++){
            const PeHitboxDefinition *def=pe_hitboxes_get(t->pose.hitboxes,j);
            if(def->kind!=PE_HITBOX_DEFENCE||def->joint==PE_HITBOX_DISABLED)continue;
            PeHitboxCapsule defence;PeHitboxContact contact;have_defence=true;
            if(!target_posed(t,j,&defence))return PE_HARE_DAMAGE_INVALID;
            if(pe_hitbox_contact(attack_capsule,defence,&contact)&&!found){
                found=true;out.target_id=t->entity_id;out.attack_index=i;out.defence_index=j;
                out.attack_form=at->form;out.defence_form=def->form;out.defence_material=def->material;
                out.contact=contact;out.contact_scale=def->damage_scale;
            }
        }
    }
    if(!have_attack||!have_defence)return PE_HARE_DAMAGE_INVALID;
    if(!found)return PE_HARE_DAMAGE_MISS;
    /* skTriggerEvent10100/10400/10250 share the knife numeric HP expression;
     * rigid forces100/400/250 and experience .1 remain outside this helper. */
    if(!pe_combat_damage(a->attack,t->defense,t->shield_block,t->shield_protection,a->critical,out.contact_scale,&out.damage))return PE_HARE_DAMAGE_INVALID;
    out.hp_before=*hp;out.hp_after=fmaxf(0,*hp-out.damage);out.applied_damage=out.hp_before-out.hp_after;
    out.died=out.hp_before>0&&out.hp_after==0;
    *action=next;*hp=out.hp_after;*result=out;return PE_HARE_DAMAGE_HIT;
}
