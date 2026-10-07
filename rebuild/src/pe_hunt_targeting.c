#include "pe_hunt_targeting.h"
#include "../generated/pe_hunt_targeting_source.h"
#include <math.h>
#include <string.h>

static bool point(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z)&&fabsf(p.x)<=1000000&&fabsf(p.y)<=1000000&&fabsf(p.z)<=1000000;}
static bool assets(const PeHuntEncounter *h){
    if(!h->actor||!h->defence||pe_actor_skeleton_hash(h->actor)!=PE_HUNT_TARGET_SKELETON||
       pe_hitboxes_count(h->defence)<PE_HUNT_TARGET_PARTS)return false;
    for(unsigned i=0;i<PE_HUNT_TARGET_PARTS;i++){
        const PeHitboxDefinition *d=pe_hitboxes_get(h->defence,i);
        const PeHuntTargetPartSource *s=&pe_hunt_target_parts[i];
        if(!d||d->kind!=PE_HITBOX_DEFENCE||d->group!=0||d->joint!=s->joint||strcmp(d->name,s->name)||
           memcmp(d->bind_points,s->points,sizeof s->points))return false;
    }
    /* Extra attack records are permitted; an unqualified extra defence is not. */
    for(size_t i=PE_HUNT_TARGET_PARTS;i<pe_hitboxes_count(h->defence);i++)
        if(pe_hitboxes_get(h->defence,i)->kind==PE_HITBOX_DEFENCE)return false;
    return true;
}
static bool overlap(const PeHuntEncounterActor *a,PeSwordTargetQuery q){
    /* Source query transform and model root are identity siblings under the
     * view. Local capsule is axisZ; runtime root yaw/uniform scale apply once. */
    double angle=a->motion.yaw_degrees*.01745329251994329577;
    double s=sin(angle),c=cos(angle),scale=a->motion.scale;
    double x=(q.center.x-a->motion.position.x)*c-(q.center.z-a->motion.position.z)*s;
    double y=q.center.y-a->motion.position.y;
    double z=(q.center.x-a->motion.position.x)*s+(q.center.z-a->motion.position.z)*c;
    x-=pe_hunt_target_center[0]*scale;y-=pe_hunt_target_center[1]*scale;z-=pe_hunt_target_center[2]*scale;
    double half=pe_hunt_target_half_segment*scale;
    z-=fmax(-half,fmin(half,z));
    double radius=q.radius+pe_hunt_target_radius*scale;
    return x*x+y*y+z*z<=radius*radius;
}
bool pe_hunt_target_acquire(const PeHuntEncounter*h,PeVec3 player,PeVec3 attack,PeSwordTargetResult*out){
    PeSwordTargetQuery query;PeSwordTargetResult none;
    if(!h||!out||!pe_sword_target_query(player,&query)||
       !pe_sword_target_acquire(player,attack,NULL,0,&none))return false;
    if(!h->ready||!h->active){*out=none;return true;}
    if(!assets(h))return false;
    PeSwordTargetCandidate candidates[PE_HUNT_ENCOUNTER_COUNT]={0};
    PeSwordTargetPart parts[PE_HUNT_ENCOUNTER_COUNT][PE_HUNT_TARGET_PARTS]={0};
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++){
        const PeHuntEncounterActor *a=&h->actors[i];
        if(!a->entity_id||a->prototype!=PE_HARE_PROTOTYPE||!isfinite(a->hp)||a->hp<0||
           !isfinite(a->motion.max_hp)||a->motion.max_hp<=0||a->hp>a->motion.max_hp||
           a->default_player_id!=PE_HUNT_TARGET_PLAYER_ID)return false;
        for(unsigned j=0;j<i;j++)if(h->actors[j].entity_id==a->entity_id)return false;
        if(!a->present||a->hp==0)continue;
        if(!a->motion.ready||!point(a->motion.position)||!isfinite(a->motion.yaw_degrees)||
           !isfinite(a->motion.scale)||a->motion.scale<=0||a->motion.scale>1000||
           !isfinite(a->seconds)||a->seconds<0||a->seconds>86400||!pe_actor_clip(h->actor,a->clip))return false;
        PeSwordTargetCandidate *candidate=&candidates[i];
        *candidate=(PeSwordTargetCandidate){.view_id=a->entity_id,.layer=PE_HUNT_TARGET_LAYER,
            .overlaps_query=overlap(a,query),.conflicting=true,.has_defence=true,
            .parts=parts[i],.part_count=PE_HUNT_TARGET_PARTS};
        for(unsigned j=0;j<PE_HUNT_TARGET_PARTS;j++){
            parts[i][j].center_bone=pe_hunt_target_parts[j].center;
            parts[i][j].enabled=pe_hunt_target_parts[j].joint!=PE_HITBOX_DISABLED;
            if(parts[i][j].enabled&&!pe_hitbox_pose(h->defence,h->actor,j,a->clip,a->seconds,
                a->motion.position,a->motion.yaw_degrees,a->motion.scale,&parts[i][j].capsule))return false;
        }
    }
    return pe_sword_target_acquire(player,attack,candidates,PE_HUNT_ENCOUNTER_COUNT,out);
}
