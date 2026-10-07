#include "pe_sandbox_melee.h"
#include "pe_sword_variant.h"
#include <math.h>
#include <string.h>
static const char *attack_names[]={"Attack6","Attack7","Attack8","Attack9"};
static bool scalar(float x,float max){return isfinite(x)&&x>=0&&x<=max;}
static bool point(PeVec3 p,float max){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z)&&fabsf(p.x)<=max&&fabsf(p.y)<=max&&fabsf(p.z)<=max;}
static bool gate_valid(PeSandboxMeleeGate g){return point(g.velocity,100);}
static bool playback_gate_valid(const PeSandboxMelee *h,PeSandboxMeleeGate g){
    return !h->setup.playback_enabled||(isfinite(g.scaled_time)&&g.scaled_time>=0&&g.scaled_time>=h->playback.time);
}
static bool gate_common(const PeSandboxMelee *h,PeSandboxMeleeGate g){
    return g.alive&&g.weapon_owned&&g.grounded&&!g.in_water&&g.third_person&&g.action_allowed&&
        g.equipped_item_id==h->setup.weapon_item_id&&g.equipped_instance_id&&g.drawn_instance_id==g.equipped_instance_id;
}
static bool stationary_allowed(PeSandboxMeleeGate g){return !g.move_requested&&hypot(g.velocity.x,g.velocity.z)<=.05;}
static bool action_allowed(const PeSandboxMelee *h,PeSandboxMeleeGate g){
    return gate_common(h,g)&&(h->action.variant==PE_COMBAT_RUNNING?h->setup.running_motion_ready:
        (h->setup.ordinary_motion_ready||stationary_allowed(g)));
}
static bool begin_variant(const PeSandboxMelee *h,PeSandboxMeleeGate g,PeCombatVariant *variant){
    if(!gate_common(h,g))return false;
    PeSwordVariant selected;
    if(!pe_sword_variant_select((PeSwordVariantInput){.velocity=g.velocity,.movement_present=true,
        .in_air=!g.grounded,.motor_present=true,.spine_in_water=g.in_water},&selected))return false;
    if(selected==PE_SWORD_VARIANT_RUNNING){if(!h->setup.running_motion_ready)return false;*variant=PE_COMBAT_RUNNING;return true;}
    if(selected!=PE_SWORD_VARIANT_ORDINARY)return false;
    if(!h->setup.ordinary_motion_ready&&!stationary_allowed(g))return false;
    *variant=PE_COMBAT_STATIONARY;return true;
}
static bool setup_valid(const PeSandboxMeleeSetup *s){
    return s&&s->body&&s->drawn_weapon&&s->drawn_capsule&&s->player_id&&
        pe_combat_item_animation_speed(s->weapon_item_id,100)>0&&
        pe_combat_clip(s->gender,PE_COMBAT_THIRD_PERSON,0)&&scalar(s->attack,1000000)&&scalar(s->stamina_reduce_percent,1000)&&
        (!s->history_enabled||point(s->history_local_center,1000000))&&
        (!s->ordinary_motion_ready||s->gender==PE_COMBAT_FEMALE);
}
static bool valid(const PeSandboxMelee *h){
    if(!h||h->effected_count>PE_COMBAT_TARGET_CAPACITY)return false;
    for(unsigned i=0;i<h->effected_count;i++){
        if(!h->effected[i])return false;
        for(unsigned j=0;j<i;j++)if(h->effected[i]==h->effected[j])return false;
    }
    return h->ready&&setup_valid(&h->setup)&&pe_combat_variant_clip(h->action.gender,h->action.view,h->action.variant,h->action.stage)&&
        (h->action.variant!=PE_COMBAT_RUNNING||h->setup.running_motion_ready)&&h->action.target_count<=PE_COMBAT_TARGET_CAPACITY&&
        (!h->action.active||(h->instance_id&&h->action.gender==h->setup.gender&&h->action.view==PE_COMBAT_THIRD_PERSON));
}
static bool weapon_valid(const PeSandboxMelee*h,const PeSandboxMeleeFrame*f,const PeSlot*w,float base){
    return !w||(isfinite(pe_sword_attack(w,base))&&h->setup.weapon_item_id==w->item_id&&
        f->gate.equipped_item_id==w->item_id&&f->gate.equipped_instance_id==w->instance_id&&
        (!f->gate.drawn_instance_id||f->gate.drawn_instance_id==w->instance_id)&&
        (!h->action.active||h->instance_id==w->instance_id));
}
static bool clocks_equal(const PeActorClip*a,const PeActorClip*b){
    return a&&b&&!strcmp(a->name,b->name)&&a->fps==b->fps&&a->frames==b->frames&&a->duration==b->duration&&
        a->loop==b->loop&&a->terminal_at_duration==b->terminal_at_duration;
}
static bool target_valid(const PeKnifeContactTarget *t,uint64_t player){
    if(!t->entity_id||t->entity_id==player||!t->actor||!t->hitboxes||
       (t->clip!=PE_ACTOR_BIND_POSE&&!pe_actor_clip(t->actor,t->clip))||
       !isfinite(t->seconds)||t->seconds<0||t->seconds>86400||!point(t->position,1000000)||!isfinite(t->yaw)||
       !scalar(t->scale,1000)||t->scale==0||!scalar(t->max_hp,1000000)||t->max_hp==0||
       !scalar(t->hp,t->max_hp)||!scalar(t->defense,1000000)||!scalar(t->shield_protection,1))return false;
    unsigned enabled=0;
    for(size_t i=0;i<pe_hitboxes_count(t->hitboxes);i++){
        const PeHitboxDefinition *d=pe_hitboxes_get(t->hitboxes,i);
        if(d->kind!=PE_HITBOX_DEFENCE||d->joint==PE_HITBOX_DISABLED)continue;
        PeHitboxCapsule capsule;
        if(!pe_hitbox_pose(t->hitboxes,t->actor,i,t->clip,t->seconds,t->position,t->yaw,t->scale,&capsule))return false;
        enabled++;
    }
    return enabled>0;
}
bool pe_sandbox_melee_init(PeSandboxMelee *h,const PeSandboxMeleeSetup *s){
    if(!h||!setup_valid(s)||pe_hitboxes_count(s->drawn_capsule)!=1)return false;
    const PeHitboxDefinition *d=pe_hitboxes_get(s->drawn_capsule,0);
    if(!d||d->kind!=PE_HITBOX_ATTACK||d->group!=PE_HITBOX_RIGHT||d->joint==PE_HITBOX_DISABLED||d->form!=0)return false;
    PeSandboxMelee next={0};next.setup=*s;pe_combat_init(&next.action);
    for(unsigned stage=0;stage<4;stage++){
        next.body_clip[stage]=pe_actor_find_clip(s->body,attack_names[stage]);
        next.weapon_clip[stage]=pe_actor_find_clip(s->drawn_weapon,attack_names[stage]);
        const PeActorClip *a=pe_actor_clip(s->body,next.body_clip[stage]),*b=pe_actor_clip(s->drawn_weapon,next.weapon_clip[stage]);
        if(!clocks_equal(a,b)||a->loop)return false;
        const PeCombatClip *events=pe_combat_clip(s->gender,PE_COMBAT_THIRD_PERSON,stage);
        for(unsigned i=0;i<5;i++)if(events->events[i].time>a->duration)return false;
        PeHitboxCapsule capsule;
        if(!pe_hitbox_pose(s->drawn_capsule,s->drawn_weapon,0,next.weapon_clip[stage],0,(PeVec3){0},0,1,&capsule))return false;
    }
    if(s->running_motion_ready){
        if(s->gender!=PE_COMBAT_FEMALE)return false;
        next.running_body_clip=pe_actor_find_clip(s->body,"RunSwordAttack");
        next.running_weapon_clip=pe_actor_find_clip(s->drawn_weapon,"RunSwordAttack");
        const PeActorClip *a=pe_actor_clip(s->body,next.running_body_clip),*b=pe_actor_clip(s->drawn_weapon,next.running_weapon_clip);
        if(!clocks_equal(a,b)||a->loop)return false;
        const PeCombatClip *events=pe_combat_variant_clip(s->gender,PE_COMBAT_THIRD_PERSON,PE_COMBAT_RUNNING,0);
        for(unsigned i=0;i<5;i++)if(events->events[i].time>a->duration)return false;
        PeHitboxCapsule capsule;
        if(!pe_hitbox_pose(s->drawn_capsule,s->drawn_weapon,0,next.running_weapon_clip,0,(PeVec3){0},0,1,&capsule))return false;
    }
    next.ready=true;*h=next;return true;
}
bool pe_sandbox_melee_begin(PeSandboxMelee*h,PeSandboxMeleeGate gate,float*stamina){
    PeCombatVariant variant;
    if(!valid(h)||h->tick_pending||!gate_valid(gate)||!playback_gate_valid(h,gate)||!begin_variant(h,gate,&variant)||!stamina||!scalar(*stamina,1000000)||h->action.active)return false;
    PeSandboxMelee next=*h;float remaining=*stamina;
    if(!pe_combat_begin_variant(&next.action,next.setup.gender,PE_COMBAT_THIRD_PERSON,variant,next.setup.stamina_reduce_percent,&remaining))return false;
    next.instance_id=gate.equipped_instance_id;next.history_active=false;next.effected_count=0;*h=next;*stamina=remaining;return true;
}
bool pe_sandbox_melee_queue(PeSandboxMelee*h,PeSandboxMeleeGate gate){
    return valid(h)&&!h->tick_pending&&gate_valid(gate)&&playback_gate_valid(h,gate)&&action_allowed(h,gate)&&h->instance_id==gate.equipped_instance_id&&pe_combat_queue_combo(&h->action);
}
void pe_sandbox_melee_cancel(PeSandboxMelee*h){
    if(h&&h->ready){pe_combat_stop(&h->action);h->instance_id=0;h->tick_pending=false;h->history_active=false;h->effected_count=0;
        h->tick_cancelled=false;h->tick_advance=PE_COMBAT_IDLE;h->tick_interval=(PeCombatInterval){0};}
}
bool pe_sandbox_melee_advance(PeSandboxMelee*h,PeSandboxMeleeGate gate,float*stamina){
    if(!valid(h)||h->tick_pending||!gate_valid(gate)||!stamina||!scalar(*stamina,1000000))return false;
    PeSandboxMelee next=*h;float remaining=*stamina;next.tick_interval=(PeCombatInterval){0};next.tick_cancelled=false;
    float animator_speed=pe_combat_item_animation_speed(next.setup.weapon_item_id,remaining);
    if(next.setup.playback_enabled){
        PeAttackPlaybackSample sample;float curve=1;
        if(!pe_attack_playback_sample(&next.playback,gate.scaled_time,next.setup.weapon_item_id,remaining,&sample)||
           (sample.curve_active&&!pe_attack_playback_curve(sample.curve_time,&curve))||
           !pe_attack_playback_speed(sample,curve,&animator_speed))return false;
    }
    if(next.action.active&&(!action_allowed(&next,gate)||next.instance_id!=gate.equipped_instance_id)){
        pe_sandbox_melee_cancel(&next);next.tick_cancelled=true;next.tick_advance=PE_COMBAT_ENDED;
    }else{
        /* Running is a direct TP leaf with speed1. Host qualification remains
         * explicit; this applies the same source Animator stamina multiplier. */
        double rate=pe_combat_stationary_clip_rate(PE_COMBAT_THIRD_PERSON,animator_speed);
        next.tick_advance=pe_combat_advance_interval(&next.action,rate*PE_SANDBOX_MELEE_DT,PE_SANDBOX_MELEE_DT,&remaining,&next.tick_interval);
        if(next.tick_advance==PE_COMBAT_INVALID)return false;
        if(!next.action.active)next.instance_id=0;
    }
    next.tick_pending=true;*h=next;*stamina=remaining;return true;
}
bool pe_sandbox_melee_interval(const PeSandboxMelee*h,PeCombatInterval*out){
    if(!valid(h)||!h->tick_pending||!out)return false;
    *out=h->tick_interval;return true;
}
bool pe_sandbox_melee_resolve_with_weapon(PeSandboxMelee*h,const PeSandboxMeleeFrame*f,PeKnifeContactTarget*targets,size_t count,PeSlot*weapon,float base_attack,PeSandboxMeleeUpdate*out){
    if(!valid(h)||!h->tick_pending||!f||!out||count>PE_SANDBOX_MELEE_TARGETS||(count&&!targets)||
       !gate_valid(f->gate)||!point(f->player_position,1000000)||!isfinite(f->player_yaw)||
       !scalar(f->player_scale,1000)||f->player_scale==0)return false;
    if(h->setup.playback_enabled&&(!h->playback.initialized||f->gate.scaled_time!=h->playback.time))return false;
    if(!weapon_valid(h,f,weapon,base_attack))return false;
    PeSlot staged_weapon=weapon?*weapon:(PeSlot){0};
    for(size_t i=0;i<count;i++){
        if(!target_valid(&targets[i],h->setup.player_id))return false;
        for(size_t j=0;j<i;j++)if(targets[i].entity_id==targets[j].entity_id)return false;
    }
    PeSandboxMelee next=*h;PeSandboxMeleeUpdate result={0};result.target_count=count;
    result.advance=next.tick_advance;result.cancelled=next.tick_cancelled;result.interval=next.tick_interval;
    /* Geometry may remove support during the just-completed movement sweep. */
    if(next.action.active&&(!action_allowed(&next,f->gate)||next.instance_id!=f->gate.equipped_instance_id)){
        pe_sandbox_melee_cancel(&next);result.cancelled=true;result.advance=PE_COMBAT_ENDED;
    }
    PeKnifeContactTarget staged[PE_SANDBOX_MELEE_TARGETS];
    if(count)memcpy(staged,targets,count*sizeof *targets);
    if(!next.action.active||!next.action.contact_open){next.history_active=false;next.effected_count=0;}
    if(next.action.active){
        size_t clip=next.action.variant==PE_COMBAT_RUNNING&&next.action.stage==0?next.running_weapon_clip:next.weapon_clip[next.action.stage];
        if(!pe_hitbox_pose(next.setup.drawn_capsule,next.setup.drawn_weapon,0,clip,next.action.clip_time,
                          f->player_position,f->player_yaw,f->player_scale,&result.weapon))return false;
        result.weapon_valid=true;
        if(next.setup.history_enabled&&next.action.contact_open){
            PeVec3 local=next.setup.history_local_center;
            float yaw=f->player_yaw*.01745329251994329577f,s=sinf(yaw),c=cosf(yaw);
            PeVec3 center={f->player_position.x+f->player_scale*(c*local.x+s*local.z),
                f->player_position.y+f->player_scale*local.y,
                f->player_position.z+f->player_scale*(-s*local.x+c*local.z)};
            next.history=(PeCapsuleHistory){
                .previous_a=next.history_active?next.history.current.a:result.weapon.a,
                .previous_b=next.history_active?next.history.current.b:result.weapon.b,
                .current=result.weapon,.parent_center=center};
            /* Validate once even with no candidates; queries never own time. */
            PeCapsuleHistoryContact check;
            if(pe_capsule_history_contact(&next.history,result.weapon,&check)==PE_CAPSULE_HISTORY_INVALID)return false;
            next.history_active=true;
        }
        for(size_t i=0;i<count;i++){
            PeKnifeContactAttack attack={.entity_id=next.setup.player_id,.weapon=result.weapon,.attack=weapon?pe_sword_attack(&staged_weapon,base_attack):next.setup.attack,
                .critical=f->critical[i],.eligible=f->attacker_eligible,
                .history=next.setup.history_enabled&&next.history_active?&next.history:NULL};
            if(next.setup.playback_enabled&&next.action.contact_open){
                bool effected=false;
                for(unsigned k=0;k<next.effected_count;k++)effected|=next.effected[k]==staged[i].entity_id;
                if(effected||!staged[i].trigger_candidate){result.status[i]=PE_KNIFE_CONTACT_IGNORED;continue;}
                PeKnifeContactResult geometry;
                PeKnifeContactStatus probe=pe_knife_contact_probe(&attack,&staged[i],&geometry);
                if(probe==PE_KNIFE_CONTACT_INVALID)return false;
                if(probe==PE_KNIFE_CONTACT_MISS){result.status[i]=probe;continue;}
                if(next.effected_count==PE_COMBAT_TARGET_CAPACITY)return false;
                next.effected[next.effected_count++]=staged[i].entity_id;result.trigger_contact[i]=true;
                if(!f->attacker_eligible||!staged[i].trigger_can_damage){result.status[i]=PE_KNIFE_CONTACT_IGNORED;continue;}
                if(!pe_attack_playback_contact(&next.playback))return false;
                result.playback_notified[i]=true;
            }
            result.status[i]=pe_knife_contact_resolve(&next.action,&attack,&staged[i],&result.contact[i]);
            if(result.status[i]==PE_KNIFE_CONTACT_INVALID)return false;
            if(weapon&&result.status[i]==PE_KNIFE_CONTACT_HIT&&!pe_sword_wear(&staged_weapon))return false;
        }
    }
    next.tick_pending=false;
    for(size_t i=0;i<count;i++)targets[i].hp=staged[i].hp;
    if(weapon)*weapon=staged_weapon;
    *h=next;*out=result;return true;
}
bool pe_sandbox_melee_resolve(PeSandboxMelee*h,const PeSandboxMeleeFrame*f,PeKnifeContactTarget*targets,size_t count,PeSandboxMeleeUpdate*out){
    return pe_sandbox_melee_resolve_with_weapon(h,f,targets,count,NULL,0,out);
}
bool pe_sandbox_melee_step_with_weapon(PeSandboxMelee*h,const PeSandboxMeleeFrame*f,PeKnifeContactTarget*targets,size_t count,PeSlot*weapon,float base_attack,float*stamina,PeSandboxMeleeUpdate*out){
    if(!valid(h)||!f||!stamina||!out||!weapon_valid(h,f,weapon,base_attack))return false;
    PeSandboxMelee next=*h;float remaining=*stamina;
    if(!pe_sandbox_melee_advance(&next,f->gate,&remaining)||!pe_sandbox_melee_resolve_with_weapon(&next,f,targets,count,weapon,base_attack,out))return false;
    *h=next;*stamina=remaining;return true;
}
bool pe_sandbox_melee_step(PeSandboxMelee*h,const PeSandboxMeleeFrame*f,PeKnifeContactTarget*targets,size_t count,float*stamina,PeSandboxMeleeUpdate*out){
    return pe_sandbox_melee_step_with_weapon(h,f,targets,count,NULL,0,stamina,out);
}
