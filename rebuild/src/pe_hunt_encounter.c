#include "pe_hunt_encounter.h"
#include "pe_settlement.h"
#include "../generated/pe_adventure_hunt_encounter_source.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool scalar(float x,float max){return isfinite(x)&&x>=0&&x<=max;}
static bool point(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static void error_set(char *error,size_t capacity,const char *text){if(error&&capacity)snprintf(error,capacity,"%s",text);}
static bool pose_valid(const PeHuntEncounter *h,const PeHuntEncounterActor *a){
    if(!pe_actor_clip(h->actor,a->clip)||!isfinite(a->seconds)||a->seconds<0||a->seconds>86400||
       !point(a->motion.position)||!isfinite(a->motion.yaw_degrees))return false;
    unsigned enabled=0;
    for(size_t i=0;i<pe_hitboxes_count(h->defence);i++){
        const PeHitboxDefinition *d=pe_hitboxes_get(h->defence,i);
        if(d->kind!=PE_HITBOX_DEFENCE||d->joint==PE_HITBOX_DISABLED)continue;
        PeHitboxCapsule capsule;
        if(!pe_hitbox_pose(h->defence,h->actor,i,a->clip,a->seconds,a->motion.position,a->motion.yaw_degrees,a->motion.scale,&capsule))return false;
        enabled++;
    }
    return enabled>0;
}
static bool valid(const PeHuntEncounter *h){
    if(!h||!h->ready||!h->actor||!h->defence||h->starting_kills>=PE_HUNT_REQUIRED_KILLS||
       h->credited_count>PE_HUNT_REQUIRED_KILLS-h->starting_kills)return false;
    if(h->active!=(h->starting_kills+h->credited_count<PE_HUNT_REQUIRED_KILLS))return false;
    for(unsigned i=0;i<h->credited_count;i++){
        if(!h->credited_entities[i])return false;
        for(unsigned j=0;j<i;j++)if(h->credited_entities[j]==h->credited_entities[i])return false;
    }
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++){
        const PeHuntEncounterActor *a=&h->actors[i];
        if(!a->entity_id||a->prototype!=13||!a->motion.ready||!scalar(a->motion.max_hp,1000000)||a->motion.max_hp==0||
           !scalar(a->hp,a->motion.max_hp)||(!h->active&&a->present))return false;
        for(unsigned j=0;j<i;j++)if(a->entity_id==h->actors[j].entity_id)return false;
    }
    return true;
}
static bool progress_valid(const PeHuntEncounter*h,unsigned kills){return valid(h)&&kills==h->starting_kills+h->credited_count;}
bool pe_hunt_encounter_prepare(PeHuntEncounter *h,const PeWorld *world,const PeSandboxCollision *collision,
    const PeActor *actor,const PeHitboxes *defence,unsigned kills,char *error,size_t capacity){
    if(!h||!world||!pe_settlement_is_route(world)||!collision||!pe_sandbox_collision_stats(collision).generation||!actor||!defence||
       kills>=PE_HUNT_REQUIRED_KILLS||!PE_HUNT_ENCOUNTER_REFERENCE_AVAILABLE||PE_HUNT_ENCOUNTER_ACTOR_COUNT!=PE_HUNT_ENCOUNTER_COUNT){
        error_set(error,capacity,"Hunt cohort requires captured actors, active progress and the supported route");return false;
    }
    PeHuntEncounter next={0};next.actor=actor;next.defence=defence;next.starting_kills=kills;
    next.idle_clip=pe_actor_find_clip(actor,"idle");next.walk_clip=pe_actor_find_clip(actor,"walk");next.run_clip=pe_actor_find_clip(actor,"run");
    if(!pe_actor_clip(actor,next.idle_clip)||!pe_actor_clip(actor,next.walk_clip)||!pe_actor_clip(actor,next.run_clip)){
        error_set(error,capacity,"Hunt actor is missing locomotion clips");return false;
    }
    PeHareQueries queries=pe_hare_queries_sandbox(collision);
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++){
        const PeHuntEncounterSourceActor *source=&pe_hunt_encounter_source_actors[i];PeHuntEncounterActor *a=&next.actors[i];
        PeVec3 position={source->position[0]-world->origin_x,source->position[1]-world->origin_y,source->position[2]-world->origin_z};
        PeHareConfig config=pe_hare_adventure_config(position,source->yaw_degrees);
        if(!source->source_entity_id||source->prototype!=13||!pe_hare_init_query(&a->motion,&queries,&config,source->source_agent_id,source->scale,false)){
            if(error&&capacity)snprintf(error,capacity,"Hunt actor %u has no supported captured placement",i);
            return false;
        }
        /* The general ground sweep adds a tiny normal-directed skin offset.
         * Preserve the archived horizontal anchor exactly, then revalidate the
         * vertically settled capsule; never slide a blocked spawn elsewhere. */
        a->motion.position.x=position.x;a->motion.position.z=position.z;
        if(pe_sandbox_collision_overlap(collision,a->motion.position,a->motion.radius,a->motion.height)||
           !pe_sandbox_collision_support(collision,a->motion.position,a->motion.radius,a->motion.height,.1f,.65f).hit){
            error_set(error,capacity,"Hunt captured XZ has no supported settled capsule");return false;
        }
        a->motion.max_hp=source->max_hp;a->motion.attack=source->attack;a->motion.defense=source->defense;
        a->hp=source->hp;a->entity_id=source->source_entity_id;a->source_agent_id=source->source_agent_id;a->prototype=source->prototype;
        a->camp_id=source->camp_id;a->damage_id=source->damage_id;a->default_player_id=source->default_player_id;
        a->present=true;a->clip=next.idle_clip;
        if(!pose_valid(&next,a)){error_set(error,capacity,"Hunt actor defence pose is incompatible");return false;}
    }
    next.ready=next.active=true;
    if(!valid(&next)){error_set(error,capacity,"Captured hunt cohort is inconsistent");return false;}
    *h=next;if(error&&capacity)*error=0;return true;
}
void pe_hunt_encounter_clear(PeHuntEncounter*h){if(h)memset(h,0,sizeof *h);}
bool pe_hunt_encounter_step(PeHuntEncounter*h,const PeWorld*world,const PeSandboxCollision*collision,const PeHuntEncounterMotion motion[PE_HUNT_ENCOUNTER_COUNT]){
    if(!valid(h)||!pe_settlement_is_route(world)||!collision||!pe_sandbox_collision_stats(collision).generation||!motion)return false;
    PeHuntEncounter next=*h;PeHareQueries queries=pe_hare_queries_sandbox(collision);
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++){
        PeHuntEncounterActor *a=&next.actors[i];if(!a->present||a->hp==0)continue;
        if(motion[i].movement_locked){
            if(pe_sandbox_collision_overlap(collision,a->motion.position,a->motion.radius,a->motion.height))return false;
            a->motion.velocity=(PeVec3){0};a->clip=motion[i].locked_clip;a->seconds=motion[i].locked_seconds;
        }else{
            if(!pe_hare_step_query(&a->motion,&queries,&motion[i].target))return false;
            float speed=hypotf(a->motion.velocity.x,a->motion.velocity.z);
            size_t clip=speed>3?next.run_clip:speed>.05f?next.walk_clip:next.idle_clip;
            if(clip!=a->clip)a->seconds=0;
            a->clip=clip;a->seconds+=PE_HARE_DT;
            const PeActorClip *definition=pe_actor_clip(next.actor,a->clip);
            if(definition&&definition->loop&&definition->duration>0)a->seconds=fmod(a->seconds,definition->duration);
        }
        if(!pose_valid(&next,a))return false;
    }
    *h=next;return true;
}
PeHuntDeathResult pe_hunt_encounter_death(PeHuntEncounter*h,unsigned*kills,PeHuntEncounterDeath death){
    if(!kills||!progress_valid(h,*kills)||!death.entity_id||death.prototype<=0||!scalar(death.hp_before,1000000)||
       !scalar(death.hp_after,1000000)||death.hp_before<=0||death.hp_after!=0)return PE_HUNT_DEATH_INVALID;
    if(!h->active||death.prototype!=13)return PE_HUNT_DEATH_IGNORED;
    for(unsigned i=0;i<h->credited_count;i++)if(h->credited_entities[i]==death.entity_id)return PE_HUNT_DEATH_IGNORED;
    PeHuntEncounter next=*h;
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++)if(next.actors[i].entity_id==death.entity_id){
        if(!next.actors[i].present||next.actors[i].hp!=death.hp_before)return PE_HUNT_DEATH_INVALID;
        next.actors[i].hp=0;
    }
    next.credited_entities[next.credited_count++]=death.entity_id;
    unsigned count=*kills+1;
    if(count==PE_HUNT_REQUIRED_KILLS){next.active=false;for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++)next.actors[i].present=false;}
    *h=next;*kills=count;return count==PE_HUNT_REQUIRED_KILLS?PE_HUNT_DEATH_COMPLETE:PE_HUNT_DEATH_CREDITED;
}
bool pe_hunt_encounter_melee_with_weapon(PeHuntEncounter*h,PeSandboxMelee*melee,const PeSandboxMeleeFrame*frame,
    const bool eligible[PE_HUNT_ENCOUNTER_COUNT],PeSlot*weapon,float base_attack,float*stamina,unsigned*kills,PeSandboxMeleeUpdate*update,bool*completed){
    if(!melee||!frame||!eligible||!stamina||!isfinite(*stamina)||*stamina<0||*stamina>1000000||
       !kills||!update||!completed||!progress_valid(h,*kills))return false;
    PeHuntEncounter next=*h;PeSandboxMelee staged_melee=*melee;float remaining=*stamina;unsigned count=*kills;
    PeSlot staged_weapon=weapon?*weapon:(PeSlot){0};
    PeKnifeContactTarget targets[PE_HUNT_ENCOUNTER_COUNT];size_t target_count=next.active?PE_HUNT_ENCOUNTER_COUNT:0;
    for(size_t i=0;i<target_count;i++){
        const PeHuntEncounterActor*a=&next.actors[i];
        targets[i]=(PeKnifeContactTarget){.entity_id=a->entity_id,.actor=next.actor,.hitboxes=next.defence,
            .clip=a->clip,.seconds=a->seconds,.position=a->motion.position,.yaw=a->motion.yaw_degrees,.scale=a->motion.scale,
            .defense=a->motion.defense,.hp=a->hp,.max_hp=a->motion.max_hp,.eligible=a->present&&eligible[i]};
    }
    PeSandboxMeleeUpdate result;
    bool resolved=staged_melee.tick_pending?
        pe_sandbox_melee_resolve_with_weapon(&staged_melee,frame,targets,target_count,weapon?&staged_weapon:NULL,base_attack,&result):
        pe_sandbox_melee_step_with_weapon(&staged_melee,frame,targets,target_count,weapon?&staged_weapon:NULL,base_attack,&remaining,&result);
    if(!resolved)return false;
    bool finished=false;
    for(size_t i=0;i<target_count;i++)if(result.status[i]==PE_KNIFE_CONTACT_HIT){
        PeHuntEncounterActor*a=&next.actors[i];
        if(result.contact[i].died&&next.active){
            PeHuntDeathResult event=pe_hunt_encounter_death(&next,&count,(PeHuntEncounterDeath){a->entity_id,a->prototype,a->hp,targets[i].hp});
            if(event==PE_HUNT_DEATH_INVALID||event==PE_HUNT_DEATH_IGNORED)return false;
            if(event==PE_HUNT_DEATH_COMPLETE)finished=true;
        }else a->hp=targets[i].hp;
    }
    if(weapon)*weapon=staged_weapon;
    *h=next;*melee=staged_melee;*stamina=remaining;*kills=count;*update=result;*completed=finished;return true;
}
bool pe_hunt_encounter_melee(PeHuntEncounter*h,PeSandboxMelee*melee,const PeSandboxMeleeFrame*frame,
    const bool eligible[PE_HUNT_ENCOUNTER_COUNT],float*stamina,unsigned*kills,PeSandboxMeleeUpdate*update,bool*completed){
    return pe_hunt_encounter_melee_with_weapon(h,melee,frame,eligible,NULL,0,stamina,kills,update,completed);
}
