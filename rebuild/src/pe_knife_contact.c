#include "pe_knife_contact.h"
#include <math.h>

static bool point_valid(PeVec3 p){
    return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z)&&fabsf(p.x)<=1000000&&fabsf(p.y)<=1000000&&fabsf(p.z)<=1000000;
}
static bool weapon_valid(PeHitboxCapsule c){return point_valid(c.a)&&point_valid(c.b)&&isfinite(c.radius)&&c.radius>0&&c.radius<=1000000;}
static bool same_point(PeVec3 a,PeVec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
static bool action_valid(const PeCombat *a){
    if(!a||a->target_count>PE_COMBAT_TARGET_CAPACITY||!isfinite(a->clip_time)||a->clip_time<0||!isfinite(a->wall_time)||a->wall_time<0||
       !isfinite(a->stamina_reduce_percent)||a->stamina_reduce_percent<0||a->opened_update>a->update_index||a->event_index>5||
       !pe_combat_variant_clip(a->gender,a->view,a->variant,a->stage))return false;
    for(unsigned i=0;i<a->target_count;i++){
        if(!a->targets[i])return false;
        for(unsigned j=0;j<i;j++)if(a->targets[i]==a->targets[j])return false;
    }
    return true;
}
PeKnifeContactStatus pe_knife_contact_resolve(PeCombat *action,const PeKnifeContactAttack *attack,
                                             PeKnifeContactTarget *target,PeKnifeContactResult *result){
    float base_damage;
    if(!action_valid(action)||!attack||!target||!result||!attack->entity_id||!target->entity_id||!weapon_valid(attack->weapon)||
       !target->actor||!target->hitboxes||!pe_hitboxes_count(target->hitboxes)||
       (target->clip!=PE_ACTOR_BIND_POSE&&!pe_actor_clip(target->actor,target->clip))||
       !isfinite(target->seconds)||target->seconds<0||!point_valid(target->position)||!isfinite(target->yaw)||
       !isfinite(target->scale)||target->scale<=0||target->scale>1000||!isfinite(target->hp)||target->hp<0||
       !isfinite(target->max_hp)||target->max_hp<=0||target->hp>target->max_hp||
       !pe_combat_damage(attack->attack,target->defense,target->shield_block,target->shield_protection,attack->critical,1,&base_damage))return PE_KNIFE_CONTACT_INVALID;
    if(attack->history&&(!same_point(attack->weapon.a,attack->history->current.a)||
       !same_point(attack->weapon.b,attack->history->current.b)||attack->weapon.radius!=attack->history->current.radius))return PE_KNIFE_CONTACT_INVALID;
    if(!action->active||!action->contact_open||!attack->eligible||!target->eligible||target->hp==0||
       attack->entity_id==target->entity_id||action->target_count==PE_COMBAT_TARGET_CAPACITY)return PE_KNIFE_CONTACT_IGNORED;
    for(unsigned i=0;i<action->target_count;i++)if(action->targets[i]==target->entity_id)return PE_KNIFE_CONTACT_IGNORED;
    PeKnifeContactResult next={0};bool found=false;
    for(size_t i=0;i<pe_hitboxes_count(target->hitboxes);i++){
        const PeHitboxDefinition *definition=pe_hitboxes_get(target->hitboxes,i);
        if(definition->kind!=PE_HITBOX_DEFENCE||definition->joint==PE_HITBOX_DISABLED)continue;
        PeHitboxCapsule capsule;PeHitboxContact contact;
        if(!pe_hitbox_pose(target->hitboxes,target->actor,i,target->clip,target->seconds,target->position,target->yaw,target->scale,&capsule))return PE_KNIFE_CONTACT_INVALID;
        PeCapsuleHistoryContact history={0};bool hit;
        if(attack->history){
            PeCapsuleHistoryStatus status=pe_capsule_history_contact(attack->history,capsule,&history);
            if(status==PE_CAPSULE_HISTORY_INVALID)return PE_KNIFE_CONTACT_INVALID;
            hit=status==PE_CAPSULE_HISTORY_HIT;
            if(hit)contact=history.contact;
        }else hit=pe_hitbox_contact(attack->weapon,capsule,&contact);
        if(hit&&!found){
            /* PEAttackTrigger.CheckHit194..231: every successful capsule
             * collision sets result.distance=0, so strict > keeps first. */
            found=true;next.target_id=target->entity_id;next.defence_index=i;next.contact=contact;
            next.historical=attack->history!=NULL;next.history_contact=history;
            next.defence_form=definition->form;next.defence_material=definition->material;next.contact_scale=definition->damage_scale;
        }
    }
    if(!found)return PE_KNIFE_CONTACT_MISS;
    if(!pe_combat_damage(attack->attack,target->defense,target->shield_block,target->shield_protection,attack->critical,next.contact_scale,&next.damage))return PE_KNIFE_CONTACT_INVALID;
    next.hp_before=target->hp;next.hp_after=fmaxf(0,target->hp-next.damage);
    next.applied_damage=next.hp_before-next.hp_after;next.died=next.hp_before>0&&next.hp_after==0;
    PeCombat committed=*action;
    if(!pe_combat_record_contact(&committed,target->entity_id))return PE_KNIFE_CONTACT_IGNORED;
    *action=committed;target->hp=next.hp_after;*result=next;return PE_KNIFE_CONTACT_HIT;
}
