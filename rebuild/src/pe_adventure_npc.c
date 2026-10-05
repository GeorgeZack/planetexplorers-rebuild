#include "pe_adventure_npc.h"
#include "../generated/pe_adventure_npc_collider.h"
#include <float.h>
#include <math.h>
#include <stdio.h>

static bool finite3(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static void error_set(char *error,size_t cap,const char *message){if(error&&cap)snprintf(error,cap,"%s",message);}
static PeTreeCapsule capsule(PeVec3 root,const float values[5]){
    return (PeTreeCapsule){{root.x+values[0],root.y+values[1],root.z+values[2]},values[3],values[4]};
}
PeTreeCapsule pe_adventure_npc_capsule(const PeAdventureNpc *npc){
    return npc&&npc->placed?capsule(npc->position,pe_adventure_npc_physical):(PeTreeCapsule){0};
}
static bool valid_pose(const PeAdventureNpc *npc){
    return npc&&npc->placed&&finite3(npc->position)&&finite3(npc->spawn_position)&&isfinite(npc->yaw)&&
        npc->position.x>=0&&npc->position.x<PE_WORLD_X&&npc->position.z>=0&&npc->position.z<PE_WORLD_Z&&
        npc->position.y>=0&&npc->position.y<PE_WORLD_Y;
}
static bool settle(const PeSandboxCollision *c,PeVec3 root,PeVec3 *out){
    float r=pe_adventure_npc_physical[3],h=pe_adventure_npc_physical[4];
    float offset=pe_adventure_npc_physical[1]-h*.5f;
    PeVec3 feet={root.x,root.y+offset,root.z},result;
    if(!pe_sandbox_collision_settle(c,feet,r,h,4,5,&result))return false;
    /* Synthetic crop-bottom planes must not manufacture an NPC in empty air. */
    PeSceneHit floor=pe_sandbox_collision_ray_geometry(c,(PeVec3){result.x,result.y+.05f,result.z},(PeVec3){0,-1,0},1);
    if(!floor.hit||floor.normal.y<.65f)return false;
    result.y-=offset;*out=result;return true;
}
bool pe_adventure_npc_refresh_at(PeAdventureNpc *npc,const PeWorld *world,const PeSandboxCollision *c,PeVec3 anchor,char *error,size_t cap){
    if(error&&cap)*error=0;
    uint64_t generation=pe_sandbox_collision_stats(c).generation;
    if(!npc||!world||!finite3(anchor)||anchor.x<0||anchor.z<0||anchor.x>=PE_WORLD_X||anchor.z>=PE_WORLD_Z||world->source>1||!generation||(npc->placed&&!valid_pose(npc))){
        error_set(error,cap,"Invalid supplier placement state or collision snapshot");return false;
    }
    if(npc->placed&&npc->collision_generation==generation)return true;
    PeAdventureNpc next=*npc;PeVec3 settled;
    if(npc->placed&&settle(c,npc->position,&settled)){
        next.position=settled;next.collision_generation=generation;*npc=next;return true;
    }
    /* Provisional local encounter policy, not a recovered Adventure town seed:
     * center first, then half-meter rings, increasing angle from +X. */
    for(int ring=0;ring<=16;ring++)for(int step=0;step<(ring?32:1);step++){
        double angle=step*(6.2831853071795864769/32);
        float x=anchor.x+(float)(.5*ring*cos(angle)),z=anchor.z+(float)(.5*ring*sin(angle));
        PeSceneHit floor=pe_sandbox_collision_ray_geometry(c,(PeVec3){x,PE_WORLD_Y-1,z},(PeVec3){0,-1,0},PE_WORLD_Y-1);
        if(!floor.hit||floor.normal.y<.65f)continue;
        PeVec3 preferred={x,floor.position.y+.25f,z};
        if(!settle(c,preferred,&settled))continue;
        next.position=settled;next.collision_generation=generation;next.placed=true;
        if(!npc->placed){next.spawn_position=settled;next.yaw=0;}
        *npc=next;return true;
    }
    error_set(error,cap,"No clear supported supplier position within the bounded encounter search");return false;
}
bool pe_adventure_npc_refresh(PeAdventureNpc *npc,const PeWorld *world,const PeSandboxCollision *c,char *error,size_t cap){
    return pe_adventure_npc_refresh_at(npc,world,c,(PeVec3){32,0,27},error,cap);
}
static void sphere_entry(const double p[3],const double d[3],double y,double r,int side,double *best){
    double cy=p[1]-y,b=p[0]*d[0]+cy*d[1]+p[2]*d[2];
    double disc=b*b-(p[0]*p[0]+cy*cy+p[2]*p[2]-r*r);
    if(disc<0)return;
    double t=-b-sqrt(disc);
    if(t>=0&&t<*best&&side*(cy+t*d[1])>=0)*best=t;
}
static double ray_capsule(PeTreeCapsule c,PeVec3 origin,const double d[3]){
    double p[]={(double)origin.x-c.center.x,(double)origin.y-c.center.y,(double)origin.z-c.center.z};
    double half=c.height*.5-c.radius,r=c.radius,dy=p[1]-fmax(-half,fmin(half,p[1]));
    /* Unity collider rays do not select an exit from an interior origin. */
    if(p[0]*p[0]+dy*dy+p[2]*p[2]<r*r)return DBL_MAX;
    double best=DBL_MAX,a=d[0]*d[0]+d[2]*d[2],b=p[0]*d[0]+p[2]*d[2];
    double disc=b*b-a*(p[0]*p[0]+p[2]*p[2]-r*r);
    if(a>0&&disc>=0){
        double t=(-b-sqrt(disc))/a,y=p[1]+t*d[1];
        if(t>=0&&y>=-half&&y<=half)best=t;
    }
    sphere_entry(p,d,-half,r,-1,&best);sphere_entry(p,d,half,r,1,&best);return best;
}
bool pe_adventure_npc_pick(const PeAdventureNpc *npc,PeVec3 player,PeVec3 origin,PeVec3 direction,float obstacle,float *out){
    if(!valid_pose(npc)||!out||!finite3(player)||!finite3(origin)||!finite3(direction)||isnan(obstacle)||obstacle<0)return false;
    double length=hypot(hypot((double)direction.x,direction.y),direction.z);if(length==0)return false;
    double relative[]={(double)player.x-npc->position.x,(double)player.y-npc->position.y,(double)player.z-npc->position.z};
    double root_distance=0,bound_distance=0;
    const float *v=pe_adventure_npc_physical;
    for(int i=0;i<3;i++){
        if(fabs(relative[i])>5.625)return false;
        root_distance+=relative[i]*relative[i];
        double d=fmax(0,fabs(relative[i]-v[i])-(i==1?v[4]*.5:v[3]));bound_distance+=d*d;
    }
    if(root_distance>81||bound_distance>20.25)return false;
    double unit[]={direction.x/length,direction.y/length,direction.z/length};
    PeTreeCapsule physical=capsule(npc->position,v),defence=capsule(npc->position,pe_adventure_npc_defence),outer=physical;
    outer.radius+=.1f;outer.height+=.2f;
    double distance=ray_capsule(physical,origin,unit);
    if(distance>100)distance=ray_capsule(defence,origin,unit);
    if(distance>100)distance=ray_capsule(outer,origin,unit);
    if(distance>100||distance>fmin((double)obstacle,100)+10)return false;
    *out=(float)distance;return true;
}
