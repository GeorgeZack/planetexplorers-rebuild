#include "pe_training_build.h"
#include "pe_training_terrain.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static bool same(PeB45Cell a,PeB45Cell b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
static bool cell_valid(PeB45Cell c){return pe_b45_valid(&c)&&c.material==0&&
    c.x>=-256&&c.x<256&&c.y>=-256&&c.y<256&&c.z>=-256&&c.z<256;}
void pe_training_build_init(PeTrainingBuild*b){if(b){memset(b,0,sizeof*b);b->revision=1;}}
bool pe_training_build_valid(const PeTrainingBuild*b){
    if(!b||!b->revision||b->count>PE_TRAINING_BUILD_CAPACITY)return false;
    for(size_t i=0;i<b->count;i++){
        if(!cell_valid(b->cells[i]))return false;
        for(size_t j=0;j<i;j++)if(same(b->cells[i],b->cells[j]))return false;
    }
    return true;
}
bool pe_training_build_mesh(const PeTrainingBuild*b,PeMeshData*m){
    if(!m||m->vertices||m->count||!pe_training_build_valid(b))return false;
    if(!b->count)return true;
    PeVertex *vertices=malloc(b->count*PE_B45_MAX_VERTICES*sizeof *vertices);
    if(!vertices)return false;
    size_t count=0;
    for(size_t i=0;i<b->count;i++){
        size_t n=pe_b45_mesh(&b->cells[i],vertices+count,PE_B45_MAX_VERTICES);
        if(!n){free(vertices);return false;}count+=n;
    }
    *m=(PeMeshData){vertices,count};return true;
}
bool pe_training_build_collision(const PeTrainingBuild*b,PeSceneCollision**snapshot,char*error,size_t capacity){
    if(!snapshot){if(error&&capacity)snprintf(error,capacity,"Missing construction snapshot");return false;}
    PeMeshData mesh={0};
    if(!pe_training_build_mesh(b,&mesh)){if(error&&capacity)snprintf(error,capacity,"Invalid construction or allocation failure");return false;}
    PeSceneCollision *next=pe_scene_from_mesh(&mesh,(PeVec3){0},error,capacity);
    free(mesh.vertices);if(!next)return false;
    pe_scene_free(*snapshot);*snapshot=next;return true;
}
bool pe_training_build_add(PeTrainingBuild*b,uint32_t*dirt,const PeB45Cell*cells,size_t count,PeVec3 feet,float radius,float height){
    if(!pe_training_build_valid(b)||!dirt||*dirt>PE_TRAINING_BUILD_ITEM_LIMIT||!cells||
       !count||count>PE_TRAINING_BUILD_BATCH||count>PE_TRAINING_BUILD_CAPACITY-b->count||b->revision==UINT32_MAX||
       !isfinite(feet.x)||!isfinite(feet.y)||!isfinite(feet.z)||!isfinite(radius)||!isfinite(height)||radius<=0||height<radius*2)return false;
    uint32_t cost=pe_b45_build_cost((uint32_t)count);if(*dirt<cost)return false;
    PeTrainingBuild next=*b;
    for(size_t i=0;i<count;i++){
        if(!cell_valid(cells[i]))return false;
        for(size_t j=0;j<next.count;j++)if(same(cells[i],next.cells[j]))return false;
        next.cells[next.count++]=cells[i];
    }
    /* Triangle overlap handles partial contact. A contained capsule midpoint
     * also rejects capsules completely enclosed by a solid primitive. */
    PeVec3 midpoint={feet.x,feet.y+height*.5f,feet.z};
    for(size_t i=0;i<next.count;i++)if(pe_b45_contains(&next.cells[i],midpoint))return false;
    PeSceneCollision *scene=NULL;if(!pe_training_build_collision(&next,&scene,NULL,0))return false;
    bool overlap=pe_scene_capsule_overlap(scene,feet,radius,height);pe_scene_free(scene);
    if(overlap)return false;
    next.revision++;*b=next;*dirt-=cost;return true;
}
bool pe_training_build_remove(PeTrainingBuild*b,uint32_t*dirt,const size_t*indices,size_t count){
    if(!pe_training_build_valid(b)||!dirt||*dirt>PE_TRAINING_BUILD_ITEM_LIMIT||!indices||
       !count||count>PE_TRAINING_BUILD_BATCH||b->revision==UINT32_MAX)return false;
    bool removed[PE_TRAINING_BUILD_CAPACITY]={0};
    for(size_t i=0;i<count;i++){
        if(indices[i]>=b->count||removed[indices[i]])return false;
        removed[indices[i]]=true;
    }
    uint32_t refund=pe_b45_remove_refund((uint32_t)count);
    if(refund>PE_TRAINING_BUILD_ITEM_LIMIT-*dirt)return false;
    PeTrainingBuild next=*b;next.count=0;
    for(size_t i=0;i<b->count;i++)if(!removed[i])next.cells[next.count++]=b->cells[i];
    memset(next.cells+next.count,0,(b->count-next.count)*sizeof next.cells[0]);
    next.revision++;*b=next;*dirt+=refund;return true;
}
bool pe_training_build_clear(PeTrainingBuild*b){
    if(!pe_training_build_valid(b))return false;
    if(!b->count)return true;
    if(b->revision==UINT32_MAX)return false;
    memset(b->cells,0,sizeof b->cells);b->count=0;b->revision++;return true;
}
PeB45Hit pe_training_build_raycast(const PeTrainingBuild*b,PeVec3 origin,PeVec3 direction,float range,size_t*index){
    PeB45Hit result={0};if(!pe_training_build_valid(b))return result;
    size_t selected=0;
    for(size_t i=0;i<b->count;i++){
        PeB45Hit candidate;
        if(pe_b45_raycast(&b->cells[i],origin,direction,range,&candidate)&&(!result.hit||candidate.distance<result.distance)){
            result=candidate;selected=i;
        }
    }
    if(result.hit&&index)*index=selected;
    return result;
}

static bool box_inside(PeVec3 p,PeVec3 low,float scale){return p.x>=low.x&&p.x<low.x+scale&&p.y>=low.y&&p.y<low.y+scale&&p.z>=low.z&&p.z<low.z+scale;}
static bool grid_hit(PeVec3 o,PeVec3 d,PeVec3 low,float scale,float*t,PeVec3*normal){
    float origins[3]={o.x,o.y,o.z},dirs[3]={d.x,d.y,d.z},mins[3]={low.x,low.y,low.z};
    float entry=-INFINITY,leave=INFINITY;int axis=-1;float sign=0;
    for(int i=0;i<3;i++){
        if(fabsf(dirs[i])<1e-12f){if(origins[i]<mins[i]||origins[i]>=mins[i]+scale)return false;continue;}
        float a=(mins[i]-origins[i])/dirs[i],b=(mins[i]+scale-origins[i])/dirs[i];
        float near=fminf(a,b),far=fmaxf(a,b);
        if(near>entry){entry=near;axis=i;sign=dirs[i]>0?-1:1;}
        leave=fminf(leave,far);if(entry>leave)return false;
    }
    if(axis<0||entry<0||entry>10000)return false;
    *t=entry;*normal=(PeVec3){axis==0?sign:0,axis==1?sign:0,axis==2?sign:0};return true;
}
PeTrainingBuildTarget pe_training_build_target(const PeWorld*w,const PeTrainingBuild*b,PeVec3 o,PeVec3 d,bool subtract){
    PeTrainingBuildTarget result={.index=SIZE_MAX};
    if(!pe_training_build_valid(b)||!pe_training_terrain_valid(w)||!isfinite(o.x)||!isfinite(o.y)||!isfinite(o.z))return result;
    double length=hypot(hypot(d.x,d.y),d.z);if(!isfinite(length)||length<1e-12)return result;
    d=(PeVec3){(float)(d.x/length),(float)(d.y/length),(float)(d.z/length)};
    bool inside=false;
    for(size_t i=0;i<b->count;i++){
        PeB45Cell c=b->cells[i];if(box_inside(o,(PeVec3){c.x*.5f,c.y*.5f,c.z*.5f},.5f)){inside=true;break;}
    }
    if(!inside)for(size_t i=0;i<b->count;i++){
        PeB45Cell c=b->cells[i];float distance;PeVec3 normal;
        if(grid_hit(o,d,(PeVec3){c.x*.5f,c.y*.5f,c.z*.5f},.5f,&distance,&normal)&&(!result.hit||distance<result.distance))
            result=(PeTrainingBuildTarget){.hit=true,.distance=distance,.normal=normal,.index=i};
    }
    if(!subtract){
        double fx=floor((double)o.x-w->origin_x+.5),fy=floor((double)o.y-w->origin_y+.5),fz=floor((double)o.z-w->origin_z+.5);
        inside=fx>=0&&fx<PE_WORLD_X&&fy>=0&&fy<PE_WORLD_Y&&fz>=0&&fz<PE_WORLD_Z&&pe_world_get(w,(int)fx,(int)fy,(int)fz).volume>=128;
        if(!inside)for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++){
            if(pe_world_get(w,x,y,z).volume<128)continue;
            float distance;PeVec3 normal;
            if(grid_hit(o,d,(PeVec3){x+w->origin_x-.5f,y+w->origin_y-.5f,z+w->origin_z-.5f},1,&distance,&normal)&&(!result.hit||distance<result.distance))
                result=(PeTrainingBuildTarget){.hit=true,.terrain=true,.distance=distance,.normal=normal,.index=SIZE_MAX};
        }
    }
    if(!result.hit)return result;
    result.position=(PeVec3){o.x+d.x*result.distance,o.y+d.y*result.distance,o.z+d.z*result.distance};
    float side=subtract?-.25f:.25f;
    double x=floor(((double)result.position.x+result.normal.x*side)*2),y=floor(((double)result.position.y+result.normal.y*side)*2),z=floor(((double)result.position.z+result.normal.z*side)*2);
    if(x< -256||x>=256||y< -256||y>=256||z< -256||z>=256)return (PeTrainingBuildTarget){.index=SIZE_MAX};
    result.cell=(PeB45Cell){(int)x,(int)y,(int)z,4,0};return result;
}
