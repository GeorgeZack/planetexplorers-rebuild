#include "pe_hare_damage.h"
#include "pe_combat.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

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
static unsigned active_bones(const PeHareAttack *a){
    unsigned bits=0;const PeHareAttackDef *d=pe_hare_attack_def(a->attack);
    if(a->active)for(unsigned i=0;i<d->window_count;i++)if(a->open_windows&(1u<<i))bits|=d->windows[i].bones;
    return bits;
}
static bool capsule_equal(PeHitboxCapsule a,PeHitboxCapsule b){
    return a.a.x==b.a.x&&a.a.y==b.a.y&&a.a.z==b.a.z&&a.b.x==b.b.x&&a.b.y==b.b.y&&a.b.z==b.b.z&&a.radius==b.radius;
}
static bool history_geometry_valid(const PeCapsuleHistory *h){
    PeCapsuleHistoryContact out;
    return pe_capsule_history_contact(h,h->current,&out)!=PE_CAPSULE_HISTORY_INVALID;
}
bool pe_hare_damage_history_step(PeHareDamageHistory *h,const PeHareAttack *a,PeHareDamagePose pose,PeVec3 center){
    if(!h||!action_valid(a)||!pose_valid(pose)||!isfinite(center.x)||!isfinite(center.y)||!isfinite(center.z))return false;
    PeHareDamageHistory next=*h;unsigned counts[2]={0};size_t parts=0;
    if(h->initialized&&((h->active_bones&~3u)||!pe_hare_attack_def(h->attack)||
        !isfinite(h->start)||h->start<0||!isfinite(h->normalized_time)||h->normalized_time<0))return false;
    bool same=h->initialized&&h->attack==a->attack&&h->start==a->last_melee_start;
    if(same&&a->normalized_time<h->normalized_time)return false;
    unsigned active=active_bones(a);
    char name[16];snprintf(name,sizeof name,"attack%u",a->attack);
    size_t clip=pe_actor_find_clip(pose.actor,name);const PeActorClip *info=pe_actor_clip(pose.actor,clip);
    const PeHareAttackDef *def=pe_hare_attack_def(a->attack);
    if(!info||info->loop||fabs(info->duration-def->clip_duration)>.00001)return false;
    for(size_t i=0;i<pe_hitboxes_count(pose.hitboxes);i++){
        const PeHitboxDefinition *b=pe_hitboxes_get(pose.hitboxes,i);
        if(b->kind!=PE_HITBOX_ATTACK||b->joint==PE_HITBOX_DISABLED||(b->group!=1&&b->group!=2))continue;
        if(parts>=PE_HARE_DAMAGE_PARTS)return false;
        PeHareDamageHistoryPart *p=&next.parts[parts];
        if(h->initialized&&(p->index!=i||p->bone!=b->group))return false;
        p->index=i;p->bone=b->group;counts[b->group-1]++;
        if(active&b->group){
            PeHitboxCapsule current;
            if(!posed(pose,i,clip,a->normalized_time*def->clip_duration,&current))return false;
            bool retained=same&&(h->active_bones&b->group);
            if(retained&&!history_geometry_valid(&p->capsule))return false;
            p->capsule.previous_a=retained?p->capsule.current.a:current.a;
            p->capsule.previous_b=retained?p->capsule.current.b:current.b;
            p->capsule.current=current;p->capsule.parent_center=center;
            if(!history_geometry_valid(&p->capsule))return false;
        }
        parts++;
    }
    if(parts!=PE_HARE_DAMAGE_PARTS||counts[0]!=2||counts[1]!=2)return false;
    next.initialized=true;next.active_bones=active;next.attack=a->attack;
    next.start=a->last_melee_start;next.normalized_time=a->normalized_time;
    *h=next;return true;
}
static bool history_matches(const PeHareDamageHistory *h,const PeHareAttack *a,PeHareDamagePose pose){
    if(!h->initialized||h->attack!=a->attack||h->start!=a->last_melee_start||
       h->normalized_time!=a->normalized_time||h->active_bones!=active_bones(a))return false;
    char name[16];snprintf(name,sizeof name,"attack%u",a->attack);size_t clip=pe_actor_find_clip(pose.actor,name);
    size_t n=0;unsigned counts[2]={0};
    for(size_t i=0;i<pe_hitboxes_count(pose.hitboxes);i++){
        const PeHitboxDefinition *b=pe_hitboxes_get(pose.hitboxes,i);
        if(b->kind!=PE_HITBOX_ATTACK||b->joint==PE_HITBOX_DISABLED||(b->group!=1&&b->group!=2))continue;
        if(n>=PE_HARE_DAMAGE_PARTS||h->parts[n].index!=i||h->parts[n].bone!=b->group)return false;
        if(h->active_bones&b->group){PeHitboxCapsule current;
            if(!history_geometry_valid(&h->parts[n].capsule)||
               !posed(pose,i,clip,a->normalized_time*pe_hare_attack_def(a->attack)->clip_duration,&current)||
               !capsule_equal(current,h->parts[n].capsule.current))return false;
        }
        counts[b->group-1]++;n++;
    }
    return n==PE_HARE_DAMAGE_PARTS&&counts[0]==2&&counts[1]==2;
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
    if(a->history&&!history_matches(a->history,action,a->pose))return PE_HARE_DAMAGE_INVALID;
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
            bool overlap;PeVec3 direction={0};
            if(a->history){
                const PeCapsuleHistory *history=NULL;
                for(unsigned k=0;k<PE_HARE_DAMAGE_PARTS;k++)if(a->history->parts[k].index==i)history=&a->history->parts[k].capsule;
                if(!history)return PE_HARE_DAMAGE_INVALID;
                PeCapsuleHistoryContact sampled;
                PeCapsuleHistoryStatus status=pe_capsule_history_contact(history,defence,&sampled);
                if(status==PE_CAPSULE_HISTORY_INVALID)return PE_HARE_DAMAGE_INVALID;
                overlap=status==PE_CAPSULE_HISTORY_HIT;
                if(overlap){contact=sampled.contact;direction=sampled.hit_direction;}
            }else{overlap=pe_hitbox_contact(attack_capsule,defence,&contact);if(overlap)direction=contact.normal;}
            if(overlap&&!found){
                found=true;out.target_id=t->entity_id;out.attack_index=i;out.defence_index=j;
                out.attack_form=at->form;out.defence_form=def->form;out.defence_material=def->material;
                out.contact=contact;out.hit_direction=direction;out.contact_scale=def->damage_scale;
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
