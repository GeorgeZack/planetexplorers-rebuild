#include "pe_camp_repair_interaction.h"
#include "pe_settlement.h"
#include "../generated/pe_camp_repair_source.h"
#include <math.h>

static bool finite3(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static PeVec3 sub(PeVec3 a,PeVec3 b){return(PeVec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static float axis(PeVec3 p,int i){return i==0?p.x:(i==1?p.y:p.z);}
/* Exact archived quaternion, normalized as a rotation. Cube symmetry does not
 * justify discarding the retained float yaw residue before a ray query. */
static PeVec3 rotate(PeVec3 p,bool inverse){
    const double y=PE_CAMP_REPAIR_ROTATION_Y,w=PE_CAMP_REPAIR_ROTATION_W;
    double c=(w*w-y*y)/(w*w+y*y),s=2*w*y/(w*w+y*y);
    if(inverse)s=-s;
    return(PeVec3){(float)(c*p.x+s*p.z),p.y,(float)(-s*p.x+c*p.z)};
}
bool pe_camp_repair_present(const PeGame *g,const PeSandboxCollision *collision,bool ready,PeVec3 *anchor){
    if(!g||g->mode!=PE_GAME_ADVENTURE||!ready||!pe_settlement_matches(&g->world)||
       !pe_sandbox_collision_stats(collision).generation)return false;
    PeVec3 p={PE_CAMP_REPAIR_SOURCE_X-g->world.origin_x,
              PE_CAMP_REPAIR_SOURCE_Y-g->world.origin_y,
              PE_CAMP_REPAIR_SOURCE_Z-g->world.origin_z};
    if(anchor)*anchor=p;
    return true;
}
bool pe_camp_repair_in_range(const PeGame *g,const PeSandboxCollision *collision,bool ready){
    PeVec3 anchor;
    if(!pe_camp_repair_present(g,collision,ready,&anchor)||!finite3(g->player)||
       !isfinite(g->hp)||g->hp<=0||g->revival.phase==PE_REVIVAL_GETTING_UP)return false;
    PeVec3 d=sub(g->player,anchor);
    return (double)d.x*d.x+(double)d.y*d.y+(double)d.z*d.z<=
           (double)PE_CAMP_REPAIR_CLOSE_METERS*PE_CAMP_REPAIR_CLOSE_METERS;
}
static bool source_reach(PeVec3 player,PeVec3 root){
    PeVec3 delta=sub(player,root);float rough=PE_CAMP_REPAIR_PICK_METERS*PE_CAMP_REPAIR_PICK_ROUGH_FACTOR;
    if(fabsf(delta.x)>rough||fabsf(delta.y)>rough||fabsf(delta.z)>rough)return false;
    /* World AABB of the one yaw-rotated source cube, as Collider.bounds. */
    PeVec3 edge=rotate((PeVec3){PE_CAMP_REPAIR_BOX_HALF,0,PE_CAMP_REPAIR_BOX_HALF},false);
    float xz=fmaxf(fabsf(edge.x),fabsf(edge.z));
    double x=fmax(0,fabs(delta.x)-xz),z=fmax(0,fabs(delta.z)-xz);
    double y=fmax(0,fabs(delta.y-PE_CAMP_REPAIR_BOX_CENTER_Y)-PE_CAMP_REPAIR_BOX_HALF);
    return x*x+y*y+z*z<=(double)PE_CAMP_REPAIR_PICK_METERS*PE_CAMP_REPAIR_PICK_METERS;
}
PeSceneHit pe_camp_repair_pick(const PeGame *g,const PeSandboxCollision *collision,bool ready,PeVec3 origin,PeVec3 direction){
    PeSceneHit miss={0};PeVec3 root;
    if(!pe_camp_repair_in_range(g,collision,ready)||!finite3(origin)||!finite3(direction)||
       !pe_camp_repair_present(g,collision,ready,&root)||!source_reach(g->player,root))return miss;
    double length=sqrt((double)direction.x*direction.x+(double)direction.y*direction.y+(double)direction.z*direction.z);
    if(!(length>0)||!isfinite(length))return miss;
    direction=(PeVec3){(float)(direction.x/length),(float)(direction.y/length),(float)(direction.z/length)};
    PeVec3 local=rotate(sub(origin,root),true),dir=rotate(direction,true);
    local.y-=PE_CAMP_REPAIR_BOX_CENTER_Y;
    float half=PE_CAMP_REPAIR_BOX_HALF;
    if(fabsf(local.x)<half&&fabsf(local.y)<half&&fabsf(local.z)<half)return miss;
    double enter=0,leave=PE_CAMP_REPAIR_RAY_METERS;PeVec3 normal={0};
    for(int i=0;i<3;i++){
        double p=axis(local,i),d=axis(dir,i);
        if(d==0){if(p < -half||p > half)return miss;continue;}
        double a=(-half-p)/d,b=(half-p)/d;float sign=-1;
        if(a>b){double t=a;a=b;b=t;sign=1;}
        if(a>enter){enter=a;normal=(PeVec3){0};if(i==0)normal.x=sign;else if(i==1)normal.y=sign;else normal.z=sign;}
        if(b<leave)leave=b;
        if(leave<enter)return miss;
    }
    if(!(enter>0)||enter>PE_CAMP_REPAIR_RAY_METERS)return miss;
    PeSceneHit obstacle=pe_sandbox_collision_ray_geometry(collision,origin,direction,PE_CAMP_REPAIR_RAY_METERS);
    if(obstacle.hit&&(!isfinite(obstacle.distance)||obstacle.distance<enter-.0001))return miss;
    float distance=(float)enter;
    return(PeSceneHit){.hit=true,.distance=distance,.fraction=distance/PE_CAMP_REPAIR_RAY_METERS,
        .position={origin.x+direction.x*distance,origin.y+direction.y*distance,origin.z+direction.z*distance},
        .normal=rotate(normal,false),.triangle=UINT32_MAX};
}
