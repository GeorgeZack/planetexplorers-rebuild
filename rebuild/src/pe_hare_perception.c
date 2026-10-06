#include "pe_hare_perception.h"
#include <math.h>
#include <string.h>

static bool point_valid(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z)&&fabsf(p.x)<=1000000&&fabsf(p.y)<=1000000&&fabsf(p.z)<=1000000;}
static bool reputation_valid(PeHareReputation r){return !r.present||(r.level>=0&&r.level<9);}
bool pe_hare_perception_permissions(PeHarePermissionInput in,PeHarePermissions *out){
    if(!out||!reputation_valid(in.forward)||!reputation_valid(in.reverse)||!isfinite(in.initial_threat)||!isfinite(in.damage_value))return false;
    bool attack_reputation=!(in.forward.present&&in.forward.level>=PE_HARE_REPUTATION_NEUTRAL)&&
        !(in.reverse.present&&in.reverse.level>=PE_HARE_REPUTATION_NEUTRAL);
    bool damage_reputation=in.adventure||(!(in.forward.present&&in.forward.level>PE_HARE_REPUTATION_NEUTRAL)&&
        !(in.reverse.present&&in.reverse.level>PE_HARE_REPUTATION_NEUTRAL));
    *out=(PeHarePermissions){attack_reputation&&in.force_conflict&&fabsf(in.initial_threat)>1.192092896e-7f,
        damage_reputation&&in.force_conflict&&in.damage_value!=0};return true;
}
static bool state_valid(const PeHarePerception *s){return s&&s->ready&&s->self_id&&s->target_id&&s->self_id!=s->target_id;}
static bool target_valid(PeHarePerceptionTarget t){return !t.present||(point_valid(t.position)&&point_valid(t.center)&&isfinite(t.max_radius)&&t.max_radius>=0&&t.max_radius<=1000000);}
static bool queries_valid(const PeHarePerceptionQueries *q){return q&&q->blocked;}
static bool in_radius(PeVec3 sensor,PeHarePerceptionTarget target,float radius){
    double x=(double)target.position.x-sensor.x,z=(double)target.position.z-sensor.z,r=(double)radius+target.max_radius;
    return x*x+z*z<=r*r;
}
static bool in_cone(PeHareVisionPose pose,PeVec3 center,float limit){
    double x=(double)center.x-pose.position.x,y=(double)center.y-pose.position.y,z=(double)center.z-pose.position.z;
    double ax=pose.axis.x,ay=pose.axis.y,az=pose.axis.z;
    double denominator=sqrt((x*x+y*y+z*z)*(ax*ax+ay*ay+az*az));
    /* Unity Vector3.Angle returns0 for degenerate vectors. */
    float angle=denominator<1e-15?0:(float)(acos(fmin(1,fmax(-1,(x*ax+y*ay+z*az)/denominator)))*57.29577951308232);
    return angle<limit;
}
static bool unblocked(const PeHarePerceptionQueries *q,PeVec3 p,PeVec3 target,bool *out){
    if(p.x==target.x&&p.y==target.y&&p.z==target.z){*out=true;return true;}
    bool blocked;
    if(!q->blocked(q->context,p,target,PE_HARE_PERCEPTION_BLOCK_MASK,&blocked))return false;
    *out=!blocked;return true;
}
bool pe_hare_perception_init(PeHarePerception *s,uint64_t self,uint64_t target){
    if(!s||!self||!target||self==target)return false;
    PeHarePerception next;memset(&next,0,sizeof(next));next.ready=true;next.self_id=self;next.target_id=target;*s=next;return true;
}
bool pe_hare_perception_vision(PeHarePerception *s,PeHareVisionConfig c,PeHareVisionPose p,
    PeHarePerceptionTarget t,const PeHarePerceptionQueries *q){
    if(!state_valid(s)||!target_valid(t)||!queries_valid(q)||!point_valid(p.position)||!point_valid(p.axis)||
       !isfinite(c.radius)||c.radius<0||c.radius>1000000||!isfinite(c.angle_degrees)||c.angle_degrees<0||c.angle_degrees>180)return false;
    PeHarePerception next=*s;next.vision_updates++;
    if(next.vision_updates%PE_HARE_VISION_PERIOD==0){
        next.visible=false;
        if(t.present&&t.has_view&&in_radius(p.position,t,c.radius)&&in_cone(p,t.center,c.angle_degrees)){
            bool clear;if(!unblocked(q,p.position,t.center,&clear))return false;
            next.visible=clear&&!t.is_snake;
        }
    }
    *s=next;return true;
}
bool pe_hare_perception_hearing(PeHarePerception *s,PeHareHearingConfig c,PeVec3 p,
    PeHarePerceptionTarget t,const PeHarePerceptionQueries *q){
    if(!state_valid(s)||!target_valid(t)||(!c.ignore_block&&!queries_valid(q))||!point_valid(p)||!isfinite(c.radius)||c.radius<0||c.radius>1000000)return false;
    PeHarePerception next=*s;next.hearing_updates++;
    if(next.hearing_updates%PE_HARE_HEARING_PERIOD==0){
        next.heard=false;
        if(t.present&&t.has_view&&in_radius(p,t,c.radius)){
            if(c.ignore_block)next.heard=true;
            else if(!unblocked(q,p,t.center,&next.heard))return false;
        }
    }
    *s=next;return true;
}
bool pe_hare_perception_collect(PeHarePerception *s,PeHarePerceptionTarget t,
    PeHareCollectionGate gate,PeHarePermissionInput in,PeHareAcquisition *out){
    PeHarePermissions permissions;
    if(!state_valid(s)||!target_valid(t)||!out||!pe_hare_perception_permissions(in,&permissions))return false;
    PeHarePerception next=*s;next.target_updates++;PeHareAcquisition result={0};
    result.tick_due=next.target_updates%PE_HARE_TARGET_PERIOD==0;
    result.visible=next.visible;result.heard=next.heard;result.permissions=permissions;
    result.target_id=s->target_id;result.initial_threat=in.initial_threat;
    result.collection_ran=result.tick_due&&gate.self_has_view&&gate.local_controller&&gate.scan&&gate.can_search&&gate.self_alive;
    result.collection_candidate=result.collection_ran&&(next.visible||next.heard)&&t.present&&t.alive&&
        !gate.already_enemy&&gate.carrier_allowed&&permissions.can_attack;
    *s=next;*out=result;return true;
}
