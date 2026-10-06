#include "pe_sword_targeting.h"
#include <math.h>

static bool point(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z)&&fabsf(p.x)<=1000000&&fabsf(p.y)<=1000000&&fabsf(p.z)<=1000000;}
static PeVec3 sub(PeVec3 a,PeVec3 b){return (PeVec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static double dot(PeVec3 a,PeVec3 b){return (double)a.x*b.x+(double)a.y*b.y+(double)a.z*b.z;}
static double length(PeVec3 a){return sqrt(dot(a,a));}
static PeVec3 unit(PeVec3 a){double n=length(a);return n>1e-5?(PeVec3){a.x/n,a.y/n,a.z/n}:(PeVec3){0};}
bool pe_sword_target_query(PeVec3 player,PeSwordTargetQuery*out){
    if(!out||!point(player))return false;
    *out=(PeSwordTargetQuery){.center={player.x,player.y+PE_SWORD_TARGET_HEIGHT,player.z},
        .radius=PE_SWORD_TARGET_RADIUS,.layer_mask=PE_SWORD_TARGET_MASK};return true;
}
static bool valid(const PeSwordTargetCandidate*c){
    if(c->layer>=32||c->part_count>PE_SWORD_TARGET_PARTS||(c->part_count&&!c->parts)||
       (!c->has_defence&&c->part_count)||(c->center_present&&!point(c->center)))return false;
    for(size_t i=0;i<c->part_count;i++)if(c->parts[i].enabled){
        PeHitboxCapsule p=c->parts[i].capsule;
        if(!point(p.a)||!point(p.b)||!isfinite(p.radius)||p.radius<=0||p.radius>1000)return false;
    }
    return true;
}
static PeVec3 closest(PeHitboxCapsule c,PeVec3 p,double*distance,bool*inside){
    PeVec3 axis=sub(c.b,c.a);double n=dot(axis,axis);
    double t=n>0?fmax(0,fmin(1,dot(sub(p,c.a),axis)/n)):0;
    PeVec3 q={c.a.x+axis.x*t,c.a.y+axis.y*t,c.a.z+axis.z*t};
    PeVec3 d=sub(q,p);double size=length(d);*inside=size<c.radius;
    if(*inside){*distance=0;return p;}
    *distance=size-c.radius;
    PeVec3 direction=unit(d);
    return (PeVec3){q.x-direction.x*c.radius,q.y-direction.y*c.radius,q.z-direction.z*c.radius};
}
static bool defence(const PeSwordTargetCandidate*c,PeVec3 p,PeVec3*hit,size_t*part){
    bool found=false;double minimum=PE_SWORD_TARGET_RADIUS;
    for(size_t i=0;i<c->part_count;i++)if(c->parts[i].enabled){
        double distance;bool inside;PeVec3 q=closest(c->parts[i].capsule,p,&distance,&inside);
        if(inside){*hit=q;*part=i;return true;}
        if(distance>(c->parts[i].center_bone?PE_SWORD_TARGET_RADIUS:minimum))continue;
        *hit=q;*part=i;
        if(c->parts[i].center_bone)return true;
        minimum=distance;found=true;
    }
    return found;
}
static bool in_angle(PeVec3 direction,PeVec3 attack){
    direction.y=0;attack.y=0;
    PeVec3 a=unit(direction),b=unit(attack);
    /* Unity's zero-vector normalized dot also admits this source115deg cone.
     * Strict angle<115 is equivalent to dot>cos(115) for normalized vectors. */
    return dot(a,b)>cos(PE_SWORD_TARGET_ANGLE*3.14159265358979323846/180.);
}
bool pe_sword_target_acquire(PeVec3 player,PeVec3 attack,const PeSwordTargetCandidate*candidates,size_t count,PeSwordTargetResult*out){
    PeSwordTargetQuery query;
    if(!out||count>PE_SWORD_TARGET_CANDIDATES||(count&&!candidates)||!point(attack)||
       hypot(attack.x,attack.z)<=1e-5||!pe_sword_target_query(player,&query))return false;
    for(size_t i=0;i<count;i++)if(!valid(&candidates[i]))return false;
    PeSwordTargetResult next={.candidate_index=SIZE_MAX,.part_index=SIZE_MAX};
    uint64_t seen[PE_SWORD_TARGET_CANDIDATES];size_t seen_count=0;double minimum=100;
    for(size_t i=0;i<count;i++){
        const PeSwordTargetCandidate*c=&candidates[i];
        if(!c->overlaps_query||!(query.layer_mask&(UINT32_C(1)<<c->layer))||!c->view_id||c->self)continue;
        bool duplicate=false;for(size_t j=0;j<seen_count;j++)if(seen[j]==c->view_id){duplicate=true;break;}
        if(duplicate)continue;
        seen[seen_count++]=c->view_id;
        if(c->ragdoll||!c->conflicting)continue;
        PeVec3 hit;size_t part=SIZE_MAX;
        if(c->has_defence){if(!defence(c,query.center,&hit,&part))continue;}
        else{if(!c->center_present)continue;hit=c->center;}
        PeVec3 direction=sub(hit,query.center);double distance=length(direction);
        if(distance>=minimum||!in_angle(direction,attack))continue;
        if(!c->has_defence)minimum=distance;
        /* With horizontal angle<115deg, the full 3D angle is<=max(horizontal,
         * 90deg), so source's subsequent Slerp clamp cannot be reached here. */
        direction=unit(direction);
        next=(PeSwordTargetResult){.selected=true,.aim_active=dot(direction,direction)>0,
            .candidate_index=i,.part_index=part,.view_id=c->view_id,.point=hit,.direction=direction};
    }
    *out=next;return true;
}
