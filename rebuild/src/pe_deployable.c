#include "pe_deployable.h"
#include "../generated/pe_torch_source.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool finite3(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static bool alive(const PeGame *g){return g&&g->mode==PE_GAME_ADVENTURE&&isfinite(g->hp)&&g->hp>0;}
static int inventory_slot(const PeGame *g,uint32_t id){
    if(!g||!id)return -1;
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].instance_id==id&&g->inventory.slots[i].count)return i;
    return -1;
}
static int deployed_slot(const PeGame *g,uint32_t id){
    if(!g||!id||g->deployed_count>PE_MAX_DEPLOYABLES)return -1;
    for(unsigned i=0;i<g->deployed_count;i++)if(g->deployed[i].instance_id==id)return (int)i;
    return -1;
}
const PeDeployable *pe_deployable_find(const PeGame *g,uint32_t id){int i=deployed_slot(g,id);return i<0?NULL:&g->deployed[i];}
static bool pose_valid(const PeGame *g,const PeDeployable *d){
    const PeItemDef *item=pe_item_def(PE_TORCH_ITEM);
    return item&&d->item_id==PE_TORCH_ITEM&&d->instance_id&&d->instance_id<g->inventory.next_instance_id&&
        isfinite(d->durability)&&d->durability>=0&&d->durability<=item->durability_max&&d->quarter_turn<4&&finite3(d->position)&&
        d->position.x>=0&&d->position.x<pe_world_width(&g->world)&&d->position.y>=0&&d->position.y<pe_world_size_y(&g->world)&&d->position.z>=0&&d->position.z<pe_world_depth(&g->world);
}
bool pe_deployable_state_valid(const PeGame *g){
    if(!g||g->deployed_count>PE_MAX_DEPLOYABLES||(g->deployed_count&&g->mode!=PE_GAME_ADVENTURE))return false;
    for(unsigned i=0;i<PE_MAX_DEPLOYABLES;i++){
        const PeDeployable *d=&g->deployed[i];
        if(i>=g->deployed_count){
            if(d->item_id||d->instance_id||d->durability!=0||d->position.x!=0||d->position.y!=0||d->position.z!=0||d->quarter_turn)return false;
            continue;
        }
        if(!pose_valid(g,d)||inventory_slot(g,d->instance_id)>=0||g->equipped_instance_id==d->instance_id)return false;
        for(unsigned j=0;j<i;j++)if(g->deployed[j].instance_id==d->instance_id)return false;
        for(int j=0;j<PE_QUICKSLOTS;j++)if(g->quickslots[j].instance_id==d->instance_id)return false;
    }
    return true;
}
static void rotation(unsigned turn,float *c,float *s){static const float cosine[]={1,0,-1,0},sine[]={0,1,0,-1};*c=cosine[turn&3];*s=sine[turn&3];}
PeVec3 pe_deployable_point(const PeDeployable *d,PeVec3 p){
    if(!d)return (PeVec3){NAN,NAN,NAN};
    float c,s;rotation(d->quarter_turn,&c,&s);
    return (PeVec3){d->position.x+c*p.x+s*p.z,d->position.y-s*p.x+c*p.z,d->position.z-p.y};
}
bool pe_deployable_preview(const PeGame *g,const PeSandboxCollision *collision,uint32_t id,PeVec3 origin,PeVec3 direction,PeVec3 *out){
    if(!alive(g)||!out||!finite3(g->player)||!pe_deployable_state_valid(g)||g->deployed_count>=PE_MAX_DEPLOYABLES||!finite3(origin)||!finite3(direction))return false;
    int slot=inventory_slot(g,id);if(slot<0)return false;
    const PeSlot *item=&g->inventory.slots[slot];const PeItemDef *def=pe_item_def(PE_TORCH_ITEM);
    if(!def||item->item_id!=PE_TORCH_ITEM||item->count!=1||!isfinite(item->durability)||item->durability<0||item->durability>def->durability_max||id>=g->inventory.next_instance_id)return false;
    PeSceneHit hit=pe_sandbox_collision_ray_geometry(collision,origin,direction,PE_TORCH_RAY_DISTANCE);
    if(!hit.hit||!finite3(hit.position)||hit.position.x<0||hit.position.x>=pe_world_width(&g->world)||hit.position.y<0||hit.position.y>=pe_world_size_y(&g->world)||hit.position.z<0||hit.position.z>=pe_world_depth(&g->world))return false;
    double distance=hypot(hypot((double)hit.position.x-g->player.x,hit.position.y-g->player.y),hit.position.z-g->player.z);
    if(distance>PE_TORCH_PLACE_DISTANCE)return false;
    *out=hit.position;return true;
}
bool pe_deployable_place(PeGame *g,const PeSandboxCollision *collision,uint32_t id,PeVec3 origin,PeVec3 direction){
    PeVec3 position;
    if(!pe_deployable_preview(g,collision,id,origin,direction,&position)){if(g)snprintf(g->message,sizeof g->message,"Aim at a surface within 10 meters to place the torch.");return false;}
    int slot=inventory_slot(g,id);PeSlot item=g->inventory.slots[slot];
    PeDeployable placed={.item_id=item.item_id,.instance_id=id,.durability=item.durability,.position=position,.quarter_turn=0};
    g->inventory.slots[slot]=(PeSlot){0};if(g->equipped_instance_id==id)pe_game_unequip(g);
    for(int i=0;i<PE_QUICKSLOTS;i++)if(g->quickslots[i].instance_id==id)g->quickslots[i]=(PeQuickslot){0};
    g->deployed[g->deployed_count++]=placed;
    snprintf(g->message,sizeof g->message,"Torch placed. Get converts it to Charcoal.");return true;
}
static bool in_range(const PeGame *g,const PeDeployable *d){
    if(!alive(g)||!d||!finite3(g->player))return false;
    double relative[]={g->player.x-d->position.x,g->player.y-d->position.y,g->player.z-d->position.z};
    for(int i=0;i<3;i++)if(fabs(relative[i])>PE_TORCH_QUERY_DISTANCE*1.25)return false;
    float c,s;rotation(d->quarter_turn,&c,&s);double half=PE_TORCH_CAPSULE_HEIGHT*.5-PE_TORCH_CAPSULE_RADIUS;
    double center[]={s*PE_TORCH_CAPSULE_CENTER_Z,c*PE_TORCH_CAPSULE_CENTER_Z,0},extent[]={fabs(s)*half+PE_TORCH_CAPSULE_RADIUS,fabs(c)*half+PE_TORCH_CAPSULE_RADIUS,PE_TORCH_CAPSULE_RADIUS};
    double squared=0;for(int i=0;i<3;i++){double delta=fmax(0,fabs(relative[i]-center[i])-extent[i]);squared+=delta*delta;}
    return squared<=PE_TORCH_QUERY_DISTANCE*PE_TORCH_QUERY_DISTANCE;
}
bool pe_deployable_in_range(const PeGame *g,uint32_t id){return pe_deployable_state_valid(g)&&in_range(g,pe_deployable_find(g,id));}
static void sphere(const double p[3],const double v[3],double z,double r,int side,double *best){
    double dz=p[2]-z,b=p[0]*v[0]+p[1]*v[1]+dz*v[2],discriminant=b*b-(p[0]*p[0]+p[1]*p[1]+dz*dz-r*r);
    if(discriminant<0)return;
    double t=-b-sqrt(discriminant);if(t>=0&&t<*best&&side*(dz+t*v[2])>=0)*best=t;
}
static double ray_capsule(const PeDeployable *item,PeVec3 origin,const double direction[3]){
    float c,s;rotation(item->quarter_turn,&c,&s);
    double dx=(double)origin.x-item->position.x,dy=(double)origin.y-item->position.y,dz=(double)origin.z-item->position.z;
    double p[]={c*dx-s*dy,-dz,s*dx+c*dy-PE_TORCH_CAPSULE_CENTER_Z};
    double v[]={c*direction[0]-s*direction[1],-direction[2],s*direction[0]+c*direction[1]};
    double r=PE_TORCH_CAPSULE_RADIUS,half=PE_TORCH_CAPSULE_HEIGHT*.5-r,q=p[2]-fmax(-half,fmin(half,p[2]));
    if(p[0]*p[0]+p[1]*p[1]+q*q<r*r)return DBL_MAX;
    double best=DBL_MAX,a=v[0]*v[0]+v[1]*v[1],b=p[0]*v[0]+p[1]*v[1],discriminant=b*b-a*(p[0]*p[0]+p[1]*p[1]-r*r);
    if(a>0&&discriminant>=0){double t=(-b-sqrt(discriminant))/a,z=p[2]+t*v[2];if(t>=0&&z>=-half&&z<=half)best=t;}
    sphere(p,v,-half,r,-1,&best);sphere(p,v,half,r,1,&best);return best;
}
bool pe_deployable_pick(const PeGame *g,PeVec3 origin,PeVec3 direction,float obstacle,uint32_t *id,float *distance){
    if(!alive(g)||!id||!distance||!pe_deployable_state_valid(g)||!finite3(origin)||!finite3(direction)||isnan(obstacle)||obstacle<0)return false;
    double length=hypot(hypot((double)direction.x,direction.y),direction.z);if(length==0)return false;
    double v[]={direction.x/length,direction.y/length,direction.z/length},best=DBL_MAX;uint32_t selected=0;
    for(unsigned i=0;i<g->deployed_count;i++)if(in_range(g,&g->deployed[i])){
        double t=ray_capsule(&g->deployed[i],origin,v);
        if(t<=PE_TORCH_RAY_DISTANCE&&t<=fmin(obstacle,PE_TORCH_RAY_DISTANCE)+10&&t<best){best=t;selected=g->deployed[i].instance_id;}
    }
    if(!selected)return false;
    *id=selected;*distance=(float)best;return true;
}
bool pe_deployable_get(PeGame *g,uint32_t id){
    if(!pe_deployable_in_range(g,id))return false;
    int slot=deployed_slot(g,id);PeInventory inventory=g->inventory;
    if(!pe_inventory_add(&inventory,PE_TORCH_GET_ITEM,PE_TORCH_GET_COUNT)){snprintf(g->message,sizeof g->message,"Make room for Charcoal before retrieving this torch.");return false;}
    g->inventory=inventory;
    memmove(&g->deployed[slot],&g->deployed[slot+1],(g->deployed_count-(unsigned)slot-1)*sizeof g->deployed[0]);
    g->deployed[--g->deployed_count]=(PeDeployable){0};
    snprintf(g->message,sizeof g->message,"Retrieved one Charcoal; the placed torch was consumed.");return true;
}
bool pe_deployable_turn(PeGame *g,uint32_t id){
    if(!pe_deployable_in_range(g,id))return false;
    int slot=deployed_slot(g,id);g->deployed[slot].quarter_turn=(g->deployed[slot].quarter_turn+1)%4;return true;
}
