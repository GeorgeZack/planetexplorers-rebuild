#include "pe_hunt_combat.h"
#include "../generated/pe_hare_perception_source.h"
#include "../generated/pe_hare_history_center_source.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define RAD .01745329251994329577f
static bool point(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static bool player_valid(PeHuntCombatPlayer p,const PeActor *actor){return point(p.position)&&point(p.velocity)&&isfinite(p.yaw)&&
    isfinite(p.radius)&&p.radius>0&&isfinite(p.height)&&p.height>=2*p.radius&&isfinite(p.defense)&&p.defense>=0&&
    isfinite(p.max_hp)&&p.max_hp>0&&(!p.outfit||pe_outfit_valid(p.outfit))&&(!p.shield||pe_shield_slot_valid(p.shield))&&
    (!p.weapon||isfinite(pe_sword_attack(p.weapon,p.base_attack)))&&
    (!p.weapon||!p.shield||p.weapon->instance_id!=p.shield->instance_id)&&p.blend_count<=PE_ACTOR_BLEND_MAX&&
    (p.blend_count?pe_actor_blend_valid(actor,p.blend,p.blend_count):
        (isfinite(p.seconds)&&p.seconds>=0&&pe_actor_clip(actor,p.clip)));}
static bool running_defence_valid(const PeHuntCombat *h,const PeSandboxMelee *melee){
    if(!melee->setup.running_motion_ready)return true;
    const char *name="RunSwordAttack";
    const PeActorClip *body=pe_actor_clip(melee->setup.body,pe_actor_find_clip(melee->setup.body,name));
    const PeActorClip *defence=pe_actor_clip(h->player_actor,pe_actor_find_clip(h->player_actor,name));
    return body&&defence&&body->fps==defence->fps&&body->frames==defence->frames&&
        body->duration==defence->duration&&body->loop==defence->loop&&
        body->terminal_at_duration==defence->terminal_at_duration;
}
static uint64_t random_next(uint64_t *rng){uint64_t x=*rng;x^=x>>12;x^=x<<25;x^=x>>27;*rng=x;return x*UINT64_C(2685821657736338717);}
static PeVec3 world_point(const PeHare *h,const float p[3]){float s=sinf(h->yaw_degrees*RAD),c=cosf(h->yaw_degrees*RAD);
    return (PeVec3){h->position.x+h->scale*(c*p[0]+s*p[2]),h->position.y+h->scale*p[1],h->position.z+h->scale*(-s*p[0]+c*p[2])};}
static bool blocked(const void *context,PeVec3 from,PeVec3 to,uint32_t mask,bool *out){
    if(!out||mask!=PE_HARE_PERCEPTION_BLOCK_MASK||!point(from)||!point(to))return false;
    PeVec3 d={to.x-from.x,to.y-from.y,to.z-from.z};float n=sqrtf(d.x*d.x+d.y*d.y+d.z*d.z);
    if(n<1e-6f){*out=false;return true;}d.x/=n;d.y/=n;d.z/=n;
    *out=pe_sandbox_collision_ray_perception(context,from,d,n).hit;return true;
}
static PeHarePermissionInput relations(void){return (PeHarePermissionInput){.adventure=true,.force_conflict=true,.initial_threat=5,.damage_value=1};}
bool pe_hunt_combat_prepare(PeHuntCombat *h,const PeWorld*w,const PeSandboxCollision*c,const PeActor*a,const PeHitboxes*b,
    const PeActor*p,const PeHitboxes*pb,unsigned kills,char*error,size_t capacity){
    if(!h||!p||!pb||pe_actor_skeleton_hash(a)!=PE_HARE_SENSOR_SKELETON_HASH)return false;
    PeHuntCombat next={.player_actor=p,.player_defence=pb,.rng=UINT64_C(0x48554e5439313236)};
    if(!pe_hunt_encounter_prepare(&next.encounter,w,c,a,b,kills,error,capacity))return false;
    for(unsigned i=0;i<PE_HARE_ATTACK_COUNT;i++){char name[24];snprintf(name,sizeof name,"attack%u",i);
        next.attack_clips[i]=pe_actor_find_clip(a,name);const PeActorClip *clip=pe_actor_clip(a,next.attack_clips[i]);
        if(!clip||fabs(clip->duration-pe_hare_attack_def(i)->clip_duration)>.001)return false;}
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++){
        if(!pe_hare_perception_init(&next.sensors[i],next.encounter.actors[i].entity_id,1))return false;
        pe_hare_attack_init(&next.attacks[i]);pe_hare_skill_init(&next.skills[i]);}
    /* The host can render each supported locomotion, knife and tool pose while
     * enemies are present. An older forward-only defence actor must fail readiness,
     * rather than abort the running host when a tool is later selected. */
    static const char *const player_clips[]={"NormalIdle","NormalWF","NormalRF","Jump","Fall","Land",
        "Attack6","Attack7","Attack8","Attack9","Felling1","Felling2","Felling3",
        "Spade1","Spade2","Spade3","Mine1","Mine2","Mine3",
        "NormalWL","NormalWR","NormalWB","NormalRL","NormalRR","NormalRB"};
    for(size_t i=0;i<sizeof player_clips/sizeof player_clips[0];i++){
        if(!pe_actor_clip(p,pe_actor_find_clip(p,player_clips[i]))){
            if(error&&capacity)snprintf(error,capacity,"Player defence is missing %s",player_clips[i]);
            return false;
        }
    }
    size_t idle=pe_actor_find_clip(p,"NormalIdle");
    unsigned enabled=0;
    for(size_t i=0;i<pe_hitboxes_count(pb);i++){
        const PeHitboxDefinition *d=pe_hitboxes_get(pb,i);
        if(d->kind!=PE_HITBOX_DEFENCE||d->joint==PE_HITBOX_DISABLED)continue;
        PeHitboxCapsule posed;
        if(!pe_hitbox_pose(pb,p,i,idle,0,(PeVec3){0},0,1,&posed))return false;
        enabled++;
    }
    if(!enabled)return false;
    *h=next;return true;
}
bool pe_hunt_combat_sense(PeHuntCombat*h,const PeSandboxCollision*c,PeHuntCombatPlayer p){
    if(!h||!h->encounter.ready||!c||!player_valid(p,h->player_actor))return false;
    PeHuntCombat next=*h;PeHarePerceptionQueries queries={c,blocked};
    PeHarePerceptionTarget target={.present=true,.has_view=true,.alive=p.alive,.position=p.position,
        .center={p.position.x,p.position.y+p.height*.5f,p.position.z},.max_radius=p.radius};
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++){
        PeHuntEncounterActor *a=&next.encounter.actors[i];if(!a->present||a->hp<=0)continue;
        float origin[3],axis[3];
        if(!pe_actor_pose_joint_point(next.encounter.actor,a->clip,a->seconds,PE_HARE_SENSOR_JOINT,pe_hare_sensor_bind_origin,origin)||
           !pe_actor_pose_joint_point(next.encounter.actor,a->clip,a->seconds,PE_HARE_SENSOR_JOINT,pe_hare_sensor_bind_axis_point,axis))return false;
        PeVec3 o=world_point(&a->motion,origin),end=world_point(&a->motion,axis),v={end.x-o.x,end.y-o.y,end.z-o.z};
        float length=sqrtf(v.x*v.x+v.y*v.y+v.z*v.z);if(length<1e-6f)return false;v.x/=length;v.y/=length;v.z/=length;
        if(!pe_hare_perception_vision(&next.sensors[i],(PeHareVisionConfig){PE_HARE_SENSOR_VISION_RADIUS,PE_HARE_SENSOR_VISION_ANGLE},(PeHareVisionPose){o,v},target,&queries)||
           !pe_hare_perception_hearing(&next.sensors[i],(PeHareHearingConfig){PE_HARE_SENSOR_HEARING_RADIUS,PE_HARE_SENSOR_HEARING_IGNORE_BLOCK},o,target,&queries))return false;
        PeHareCollectionGate gate={.self_has_view=true,.self_alive=a->hp>0,.local_controller=true,.scan=true,.can_search=true,.carrier_allowed=true};
        PeHareAcquisition result;
        if(!pe_hare_perception_collect(&next.sensors[i],target,gate,relations(),&result))return false;
        if(result.collection_candidate&&result.initial_threat>0)next.hostile[i]=true;
        if(!p.alive)next.hostile[i]=false;
    }
    *h=next;return true;
}
bool pe_hunt_combat_step(PeHuntCombat*h,const PeWorld*w,const PeSandboxCollision*c,PeHuntCombatPlayer p,
    PeSandboxMelee*melee,const PeSandboxMeleeFrame*frame,float*hp,float*stamina,unsigned*kills,PeSandboxMeleeUpdate*update,bool*completed){
    if(!h||!h->encounter.ready||!w||!c||!player_valid(p,h->player_actor)||!melee||!frame||!hp||!stamina||!kills||!update||!completed||
       !isfinite(*hp)||*hp<0||*hp>p.max_hp||p.alive!=(*hp>0)||!running_defence_valid(h,melee))return false;
    if(p.weapon){
        if(melee->setup.weapon_item_id!=p.weapon->item_id||frame->gate.equipped_item_id!=p.weapon->item_id||
           frame->gate.equipped_instance_id!=p.weapon->instance_id||
           (frame->gate.drawn_instance_id&&frame->gate.drawn_instance_id!=p.weapon->instance_id)||
           (melee->action.active&&melee->instance_id!=p.weapon->instance_id))return false;
        if(p.outfit)for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)
            if(p.outfit->items[i].instance_id==p.weapon->instance_id)return false;
    }
    PeHuntCombat next=*h;PeSandboxMelee blade=*melee;float life=*hp,energy=*stamina;unsigned count=*kills;
    PeSlot weapon=p.weapon?*p.weapon:(PeSlot){0};
    const bool blade_active_at_entry=blade.action.active||(blade.tick_pending&&blade.tick_interval.valid);
    PeOutfit outfit=p.outfit?*p.outfit:(PeOutfit){0};
    PeSlot shield=p.shield?*p.shield:(PeSlot){0};
    PeSlot *staged_shield=p.shield?&shield:NULL;
    next.clock+=PE_HARE_SKILL_DT;PeHareQueries queries=pe_hare_queries_sandbox(c);
    PeHuntEncounterMotion motion[PE_HUNT_ENCOUNTER_COUNT]={0};bool eligible[PE_HUNT_ENCOUNTER_COUNT]={0};
    PeHarePermissions permissions;if(!pe_hare_perception_permissions(relations(),&permissions))return false;
    unsigned active=0;for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++)if(next.attacks[i].active||next.skills[i].active)active++;
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++){
        PeHuntEncounterActor *a=&next.encounter.actors[i];PeHareAttack *attack=&next.attacks[i];PeHareSkill *skill=&next.skills[i];
        eligible[i]=a->present&&a->hp>0&&permissions.can_damage;
        if(!a->present||a->hp<=0||!p.alive){if(attack->active||skill->active){active--;pe_hare_attack_end(attack);pe_hare_skill_cancel(skill);}continue;}
        motion[i].target=(PeHareTarget){.accepted=next.hostile[i],.position=p.position,.velocity=p.velocity,
            .radius=p.radius,.height=p.height,.yaw_degrees=p.yaw,.approach=PE_HARE_MELEE};
        PeHareEngagement engagement=pe_hare_engagement_query(&a->motion,&queries,&motion[i].target);
        bool can=next.hostile[i]&&p.grounded&&permissions.can_attack&&engagement.melee_geometry&&active<3;
        uint8_t candidates=pe_hare_attack_candidates(attack,next.clock,can,31);
        if(!attack->active&&!skill->active&&candidates){unsigned n=0;for(unsigned k=0;k<5;k++)n+=(candidates>>k)&1;
            if(!pe_hare_attack_begin(attack,next.clock,can,31,(unsigned)(random_next(&next.rng)%n))||!pe_hare_skill_begin(skill,attack->attack))return false;
            next.attack_seconds[i]=0;active++;
        }
        if(attack->active||skill->active){
            next.attack_seconds[i]+=PE_HARE_SKILL_DT;
            if(skill->active&&!pe_hare_skill_step(skill,0))return false;
            const PeHareAttackDef *def=pe_hare_attack_def(attack->attack);
            PeHareAttackEvents events;if(attack->active&&!pe_hare_attack_update(attack,next.attack_seconds[i]/def->clip_duration,&events))return false;
            if(next.attack_seconds[i]>=def->clip_duration&&!skill->active){pe_hare_attack_end(attack);active--;}
            else{motion[i].movement_locked=true;motion[i].locked_clip=next.attack_clips[attack->attack];motion[i].locked_seconds=next.attack_seconds[i];}
        }
    }
    if(!pe_hunt_encounter_step(&next.encounter,w,c,motion))return false;
    /* Animator-active parts update even during guide delay, dedupe or misses.
     * One fixed-tick sample precedes all contact queries; no target can advance
     * another target's history. Fresh activation pins source setter+LateUpdate. */
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++){
        PeHuntEncounterActor *a=&next.encounter.actors[i];
        if(!a->present||a->hp<=0||!p.alive){next.histories[i]=(PeHareDamageHistory){0};continue;}
        PeHareDamagePose pose={next.encounter.actor,next.encounter.defence,a->motion.position,a->motion.yaw_degrees,a->motion.scale};
        if(!pe_hare_damage_history_step(&next.histories[i],&next.attacks[i],pose,
            world_point(&a->motion,pe_hare_history_local_center)))return false;
    }
    PeSandboxMeleeUpdate result;bool finished=false;
    PeSandboxMeleeFrame blade_frame=*frame;
    blade_frame.gate.alive=blade_frame.gate.alive&&p.alive;
    blade_frame.attacker_eligible=blade_frame.attacker_eligible&&p.alive;
    if(!pe_hunt_encounter_melee_with_weapon(&next.encounter,&blade,&blade_frame,eligible,p.weapon?&weapon:NULL,p.base_attack,&energy,&count,&result,&finished))return false;
    for(size_t i=0;i<result.target_count;i++)if(result.status[i]==PE_KNIFE_CONTACT_HIT)next.hare_hits++;
    size_t player_clip=p.clip;double player_seconds=p.seconds;size_t player_blend_count=p.blend_count;
    if(blade.action.active){
        player_blend_count=0; /* The blade action owns the complete visible pose. */
        const char *name=pe_combat_pose_name(&blade.action);if(!name)return false;
        player_clip=pe_actor_find_clip(next.player_actor,name);player_seconds=blade.action.clip_time;
        if(!pe_actor_clip(next.player_actor,player_clip))return false;
    }
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT&&life>0;i++){
        PeHuntEncounterActor *a=&next.encounter.actors[i];PeHareAttack *attack=&next.attacks[i];PeHareSkill *skill=&next.skills[i];
        if(!a->present||a->hp<=0||!attack->active||!skill->contact_enabled||!permissions.can_damage||p.defence_disabled)continue;
        const PeHareAttackDef *def=pe_hare_attack_def(attack->attack);
        for(unsigned window=0;window<def->window_count;window++)if(attack->open_windows&(1u<<window))
            for(unsigned bone=1;bone<=2;bone*=2)if(def->windows[window].bones&bone){
                PeHareDamageAttack hit={.entity_id=a->entity_id,.pose={next.encounter.actor,next.encounter.defence,a->motion.position,a->motion.yaw_degrees,a->motion.scale},
                    .attack=a->motion.attack,.window=window,.guide=skill->guide,.bone=bone,.eligible=life>0,.history=&next.histories[i]};
                PeHareDamageTarget target={.entity_id=1,.pose={next.player_actor,next.player_defence,p.position,p.yaw,1},
                    .clip=player_clip,.seconds=player_seconds,.defense=p.defense+pe_outfit_defense(&outfit)+pe_shield_defense(staged_shield),
                    .shield_protection=pe_shield_protection(staged_shield),
                    .shield_block=p.shield_block&&!blade_active_at_entry&&!blade.action.active,.max_hp=p.max_hp,.eligible=life>0};
                target.blend_count=player_blend_count;
                if(player_blend_count)memcpy(target.blend,p.blend,player_blend_count*sizeof p.blend[0]);
                PeHareDamageResult damage;PeHareDamageStatus status=pe_hare_damage_resolve(attack,&hit,&target,&life,&damage);
                if(status==PE_HARE_DAMAGE_INVALID)return false;
                if(status==PE_HARE_DAMAGE_HIT){
                    /* Source wear uses calculated capsule damage, including
                     * overkill, and a broken piece loses its buff immediately. */
                    if(!pe_outfit_wear(&outfit,damage.damage)||
                       (staged_shield&&!pe_shield_wear(staged_shield,damage.damage)))return false;
                    next.player_hits++;
                }
            }
    }
    /* A death/owned-cohort cleanup may occur after clocks were advanced above.
     * Publish no active contact clock for those actors, even if the frontend
     * immediately stops ticking because the player died or quest completed. */
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++)if(life<=0||!next.encounter.actors[i].present||next.encounter.actors[i].hp<=0){
        pe_hare_attack_end(&next.attacks[i]);pe_hare_skill_cancel(&next.skills[i]);next.hostile[i]=false;
        next.histories[i]=(PeHareDamageHistory){0};
    }
    if(life<=0)pe_sandbox_melee_cancel(&blade);
    if(p.outfit)*p.outfit=outfit;
    if(p.shield)*p.shield=shield;
    if(p.weapon)*p.weapon=weapon;
    *h=next;*melee=blade;*hp=life;*stamina=energy;*kills=count;*update=result;*completed=finished;return true;
}
