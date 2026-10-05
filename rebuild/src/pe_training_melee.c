#include "pe_training_melee.h"
#include <math.h>
#include <string.h>
static const char *attack_names[]={"Attack6","Attack7","Attack8","Attack9"};
static bool scalar(float value,float max){return isfinite(value)&&value>=0&&value<=max;}
static bool point(PeVec3 p,float bound){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z)&&fabsf(p.x)<=bound&&fabsf(p.y)<=bound&&fabsf(p.z)<=bound;}
static bool gate_valid(PeTrainingMeleeGate gate){return point(gate.velocity,100);}
static bool gate_allowed(PeTrainingMeleeGate g){return g.alive&&g.knife_owned&&g.knife_equipped&&g.knife_drawn&&g.grounded&&g.third_person&&g.action_allowed&&!g.move_requested&&hypot(g.velocity.x,g.velocity.z)<=.05;}
static bool setup_valid(const PeTrainingMeleeSetup*s){
    return s&&s->body&&s->drawn_knife&&s->target_actor&&s->drawn_capsule&&s->target_defence&&
        s->player_id&&s->target_id&&s->player_id!=s->target_id&&pe_combat_clip(s->gender,PE_COMBAT_THIRD_PERSON,0)&&
        scalar(s->attack,1000000)&&scalar(s->stamina_reduce_percent,1000)&&scalar(s->target_defense,1000000)&&
        scalar(s->target_max_hp,1000000)&&s->target_max_hp>0&&scalar(s->target_hp,s->target_max_hp);
}
static bool valid(const PeTrainingMelee*h){
    if(!h||!h->ready||!setup_valid(&h->setup)||!scalar(h->target_hp,h->setup.target_max_hp)||h->action.stage>=4||h->action.target_count>PE_COMBAT_TARGET_CAPACITY)return false;
    if(h->action.active&&(h->action.gender!=h->setup.gender||h->action.view!=PE_COMBAT_THIRD_PERSON))return false;
    return true;
}
static bool clocks_equal(const PeActorClip*a,const PeActorClip*b){
    return a&&b&&!strcmp(a->name,b->name)&&a->fps==b->fps&&a->frames==b->frames&&a->duration==b->duration&&a->loop==b->loop&&a->terminal_at_duration==b->terminal_at_duration;
}
static bool target_pose_valid(const PeTrainingMelee*h,size_t clip,double seconds,PeVec3 position,float yaw,float scale){
    unsigned enabled=0;
    for(size_t i=0;i<pe_hitboxes_count(h->setup.target_defence);i++){
        const PeHitboxDefinition*d=pe_hitboxes_get(h->setup.target_defence,i);
        if(d->kind!=PE_HITBOX_DEFENCE||d->joint==PE_HITBOX_DISABLED)continue;
        PeHitboxCapsule capsule;
        if(!pe_hitbox_pose(h->setup.target_defence,h->setup.target_actor,i,clip,seconds,position,yaw,scale,&capsule))return false;
        enabled++;
    }
    return enabled>0;
}
bool pe_training_melee_init(PeTrainingMelee*h,const PeTrainingMeleeSetup*s){
    if(!h||!setup_valid(s)||pe_hitboxes_count(s->drawn_capsule)!=1)return false;
    const PeHitboxDefinition*d=pe_hitboxes_get(s->drawn_capsule,0);
    if(!d||d->kind!=PE_HITBOX_ATTACK||d->group!=PE_HITBOX_RIGHT||d->joint==PE_HITBOX_DISABLED||d->form!=0)return false;
    PeTrainingMelee next={0};next.setup=*s;next.target_hp=s->target_hp;pe_combat_init(&next.action);
    for(unsigned stage=0;stage<4;stage++){
        next.body_clip[stage]=pe_actor_find_clip(s->body,attack_names[stage]);
        next.knife_clip[stage]=pe_actor_find_clip(s->drawn_knife,attack_names[stage]);
        const PeActorClip*a=pe_actor_clip(s->body,next.body_clip[stage]),*b=pe_actor_clip(s->drawn_knife,next.knife_clip[stage]);
        if(!clocks_equal(a,b)||a->loop)return false;
        const PeCombatClip*events=pe_combat_clip(s->gender,PE_COMBAT_THIRD_PERSON,stage);
        for(unsigned i=0;i<5;i++)if(events->events[i].time>a->duration)return false;
        PeHitboxCapsule capsule;
        if(!pe_hitbox_pose(s->drawn_capsule,s->drawn_knife,0,next.knife_clip[stage],0,(PeVec3){0},0,1,&capsule))return false;
    }
    if(!target_pose_valid(&next,PE_ACTOR_BIND_POSE,0,(PeVec3){0},0,1))return false;
    next.ready=true;*h=next;return true;
}
bool pe_training_melee_begin(PeTrainingMelee*h,PeTrainingMeleeGate gate,float*stamina){
    if(!valid(h)||!gate_valid(gate)||!gate_allowed(gate)||!stamina||!scalar(*stamina,1000000)||h->action.active)return false;
    PeTrainingMelee next=*h;float remaining=*stamina;
    if(!pe_combat_begin(&next.action,next.setup.gender,PE_COMBAT_THIRD_PERSON,next.setup.stamina_reduce_percent,&remaining))return false;
    *h=next;*stamina=remaining;return true;
}
bool pe_training_melee_queue(PeTrainingMelee*h){return valid(h)&&pe_combat_queue_combo(&h->action);}
void pe_training_melee_cancel(PeTrainingMelee*h){if(h&&h->ready)pe_combat_stop(&h->action);}
bool pe_training_melee_step(PeTrainingMelee*h,const PeTrainingMeleeFrame*f,float*stamina,PeTrainingMeleeUpdate*out){
    if(!valid(h)||!f||!stamina||!out||!scalar(*stamina,1000000)||!gate_valid(f->gate)||
       !point(f->player_position,1000000)||!point(f->target_position,1000000)||!isfinite(f->player_yaw)||!isfinite(f->target_yaw)||
       !scalar(f->player_scale,1000)||f->player_scale==0||!scalar(f->target_scale,1000)||f->target_scale==0||
       !isfinite(f->target_seconds)||f->target_seconds<0||f->target_seconds>86400||!pe_actor_clip(h->setup.target_actor,f->target_clip)||!scalar(f->shield_protection,1))return false;
    PeTrainingMelee next=*h;float remaining=*stamina;PeTrainingMeleeUpdate result={0};
    if(next.action.active&&!gate_allowed(f->gate)){
        pe_combat_stop(&next.action);result.cancelled=true;result.advance=PE_COMBAT_ENDED;*h=next;*out=result;return true;
    }
    if(!target_pose_valid(&next,f->target_clip,f->target_seconds,f->target_position,f->target_yaw,f->target_scale))return false;
    double rate=pe_combat_stationary_clip_rate(PE_COMBAT_THIRD_PERSON,pe_combat_animation_speed(remaining));
    result.advance=pe_combat_advance(&next.action,rate*PE_TRAINING_MELEE_DT,PE_TRAINING_MELEE_DT,&remaining);
    if(result.advance==PE_COMBAT_INVALID)return false;
    if(next.action.active){
        if(!pe_hitbox_pose(next.setup.drawn_capsule,next.setup.drawn_knife,next.weapon_index,next.knife_clip[next.action.stage],next.action.clip_time,
                          f->player_position,f->player_yaw,f->player_scale,&result.weapon))return false;
        result.weapon_valid=true;
        PeKnifeContactAttack attack={.entity_id=next.setup.player_id,.weapon=result.weapon,.attack=next.setup.attack,.critical=f->critical,.eligible=f->attacker_eligible};
        PeKnifeContactTarget target={.entity_id=next.setup.target_id,.actor=next.setup.target_actor,.hitboxes=next.setup.target_defence,
            .clip=f->target_clip,.seconds=f->target_seconds,.position=f->target_position,.yaw=f->target_yaw,.scale=f->target_scale,
            .defense=next.setup.target_defense,.hp=next.target_hp,.max_hp=next.setup.target_max_hp,
            .shield_block=f->shield_block,.shield_protection=f->shield_protection,.eligible=f->target_eligible};
        result.contact_status=pe_knife_contact_resolve(&next.action,&attack,&target,&result.contact);
        if(result.contact_status==PE_KNIFE_CONTACT_INVALID)return false;
        next.target_hp=target.hp;
    }
    *h=next;*stamina=remaining;*out=result;return true;
}
