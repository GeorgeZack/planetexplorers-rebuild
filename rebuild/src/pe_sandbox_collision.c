#include "pe_sandbox_collision.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { PeSceneCollision *scene; signed char *orientation; } Terrain;
struct PeSandboxCollision {
    bool ready;
    PeWorld world;
    Terrain terrain[4];
    PeSceneCollision *construction;
    PeB45Cell cells[PE_SANDBOX_COLLISION_MAX_CELLS];
    PeSandboxBox boxes[PE_SANDBOX_COLLISION_MAX_BOXES];
    PeTreeCapsule trees[PE_SANDBOX_COLLISION_MAX_TREES];
    size_t cell_count,box_count,tree_count;
    PeSandboxCollisionStats stats;
};
static const float skin=.003f,step_height=.35f,walkable=.65f;
static PeVec3 add(PeVec3 a,PeVec3 b){return (PeVec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static PeVec3 mul(PeVec3 a,float b){return (PeVec3){a.x*b,a.y*b,a.z*b};}
static float dot(PeVec3 a,PeVec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static float length(PeVec3 a){return sqrtf(dot(a,a));}
static float component(PeVec3 a,int axis){return axis==0?a.x:(axis==1?a.y:a.z);}
static bool finite3(PeVec3 a){return isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z);}
static void error_set(char *error,size_t cap,const char *message){if(error&&cap)snprintf(error,cap,"%s",message);}
static void terrain_free(Terrain *t){pe_scene_free(t->scene);free(t->orientation);*t=(Terrain){0};}
PeSandboxCollision *pe_sandbox_collision_create(void){return calloc(1,sizeof(PeSandboxCollision));}
void pe_sandbox_collision_free(PeSandboxCollision *c){if(c){for(int i=0;i<4;i++)terrain_free(&c->terrain[i]);pe_scene_free(c->construction);free(c);}}
PeSandboxCollisionStats pe_sandbox_collision_stats(const PeSandboxCollision *c){return c?c->stats:(PeSandboxCollisionStats){0};}
PeSceneHit pe_sandbox_collision_ray_geometry(const PeSandboxCollision *c,PeVec3 p,PeVec3 d,float range){
    PeSceneHit best={0};
    if(!c||!c->ready||!finite3(p)||!finite3(d)||!isfinite(range)||range<=0)return best;
    for(int i=0;i<5;i++){
        PeSceneHit hit=pe_scene_raycast(i==4?c->construction:c->terrain[i].scene,p,d,range);
        if(hit.hit&&(!best.hit||hit.distance<best.distance))best=hit;
    }
    return best;
}
static bool box_valid(PeSandboxBox b){
    if(!finite3(b.minimum)||!finite3(b.maximum))return false;
    for(int a=0;a<3;a++)if(component(b.minimum,a)<0||component(b.maximum,a)>(a==1?PE_WORLD_Y-1:PE_WORLD_X-1)||component(b.maximum,a)<=component(b.minimum,a))return false;
    return true;
}
static bool cells_equal(const PeB45Cell *a,const PeB45Cell *b){return a->x==b->x&&a->y==b->y&&a->z==b->z&&a->type==b->type&&a->material==b->material;}
static bool boxes_equal(PeSandboxBox a,PeSandboxBox b){return a.minimum.x==b.minimum.x&&a.minimum.y==b.minimum.y&&a.minimum.z==b.minimum.z&&a.maximum.x==b.maximum.x&&a.maximum.y==b.maximum.y&&a.maximum.z==b.maximum.z;}
static void emit_box(PeSandboxBox b,PeVertex *out){
    static const unsigned indices[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,3,7,6,3,6,2,0,4,7,0,7,3,1,2,6,1,6,5};
    PeVec3 lo=b.minimum,hi=b.maximum;
    PeVec3 p[]={{lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},{hi.x,lo.y,hi.z},{lo.x,lo.y,hi.z},{lo.x,hi.y,lo.z},{hi.x,hi.y,lo.z},{hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z}};
    for(int i=0;i<36;i++)out[i]=(PeVertex){.position=p[indices[i]]};
}
bool pe_sandbox_collision_update(PeSandboxCollision *c,const PeWorld *w,const PeB45Cell *cells,size_t nc,const PeSandboxBox *boxes,size_t nb,char *error,size_t cap){
    return pe_sandbox_collision_update_with_trees(c,w,cells,nc,boxes,nb,NULL,0,error,cap);
}
static bool trees_equal(PeTreeCapsule a,PeTreeCapsule b){
    return a.center.x==b.center.x&&a.center.y==b.center.y&&a.center.z==b.center.z&&a.radius==b.radius&&a.height==b.height;
}
bool pe_sandbox_collision_update_with_trees(PeSandboxCollision *c,const PeWorld *w,const PeB45Cell *cells,size_t nc,const PeSandboxBox *boxes,size_t nb,const PeTreeCapsule *trees,size_t nt,char *error,size_t cap){
    if(error&&cap)*error=0;
    if(!c||!w||nc>PE_SANDBOX_COLLISION_MAX_CELLS||nb>PE_SANDBOX_COLLISION_MAX_BOXES||nt>PE_SANDBOX_COLLISION_MAX_TREES||(nc&&!cells)||(nb&&!boxes)||(nt&&!trees)||w->source>1){error_set(error,cap,"Invalid sandbox collision inputs");return false;}
    for(size_t i=0;i<nc;i++){
        const PeB45Cell *p=&cells[i];
        if(!pe_b45_valid(p)||p->x<0||p->y<0||p->z<0||p->x>=2*(PE_WORLD_X-1)||p->y>=2*(PE_WORLD_Y-1)||p->z>=2*(PE_WORLD_Z-1)){error_set(error,cap,"Invalid sandbox construction cell");return false;}
        for(size_t j=0;j<i;j++)if(p->x==cells[j].x&&p->y==cells[j].y&&p->z==cells[j].z){error_set(error,cap,"Duplicate sandbox construction cell");return false;}
    }
    for(size_t i=0;i<nb;i++)if(!box_valid(boxes[i])){error_set(error,cap,"Invalid sandbox furniture box");return false;}
    for(size_t i=0;i<nt;i++)if(!pe_tree_collision_valid(&trees[i])){error_set(error,cap,"Invalid sandbox tree capsule");return false;}
    bool changed[4],build_changed=!c->ready||c->cell_count!=nc||c->box_count!=nb,any=false;
    bool trees_changed=!c->ready||c->tree_count!=nt;
    for(size_t i=0;!trees_changed&&i<nt;i++)trees_changed=!trees_equal(c->trees[i],trees[i]);
    for(size_t i=0;!build_changed&&i<nc;i++)build_changed=!cells_equal(&c->cells[i],&cells[i]);
    for(size_t i=0;!build_changed&&i<nb;i++)build_changed=!boxes_equal(c->boxes[i],boxes[i]);
    Terrain pending[4]={{0}};PeSceneCollision *construction=NULL;
    for(int i=0;i<4;i++){
        int x=(i&1)*32,z=(i>>1)*32;
        changed[i]=!c->ready||pe_world_mesh_region_changed(w,&c->world,x,0,z,x?31:32,31,z?31:32);
        if(!changed[i])continue;
        any=true;PeMeshData mesh={0};
        if(!pe_world_mesh_region(w,x,0,z,x?31:32,31,z?31:32,&mesh)){error_set(error,cap,"Sandbox terrain extraction failed");goto fail;}
        pending[i].scene=pe_scene_from_mesh(&mesh,(PeVec3){0},error,cap);
        pending[i].orientation=mesh.count?malloc(mesh.count):NULL;
        if(!pending[i].scene||(mesh.count&&!pending[i].orientation)){pe_mesh_free(&mesh);if(error&&cap&&!error[0])error_set(error,cap,"Sandbox orientation allocation failed");goto fail;}
        for(size_t j=0;j<mesh.count;j+=3){
            PeVec3 a=mesh.vertices[j].position,b=add(mesh.vertices[j+1].position,mul(a,-1)),d=add(mesh.vertices[j+2].position,mul(a,-1));
            PeVec3 n={b.y*d.z-b.z*d.y,b.z*d.x-b.x*d.z,b.x*d.y-b.y*d.x};
            for(int k=0;k<3;k++){float v=component(n,k);pending[i].orientation[j+k]=(signed char)((v>1e-12f)-(v< -1e-12f));}
        }
        pe_mesh_free(&mesh);
    }
    if(build_changed){
        PeMeshData mesh={0};size_t capacity=(nc+nb)*36;
        mesh.vertices=capacity?calloc(capacity,sizeof *mesh.vertices):NULL;
        if(capacity&&!mesh.vertices){error_set(error,cap,"Sandbox construction allocation failed");goto fail;}
        for(size_t i=0;i<nc;i++)mesh.count+=pe_b45_mesh(&cells[i],mesh.vertices+mesh.count,capacity-mesh.count);
        for(size_t i=0;i<nb;i++){emit_box(boxes[i],mesh.vertices+mesh.count);mesh.count+=36;}
        construction=pe_scene_from_mesh(&mesh,(PeVec3){0},error,cap);pe_mesh_free(&mesh);if(!construction)goto fail;
    }
    for(int i=0;i<4;i++)if(changed[i]){terrain_free(&c->terrain[i]);c->terrain[i]=pending[i];c->stats.terrain_rebuilds++;}
    if(build_changed){pe_scene_free(c->construction);c->construction=construction;c->stats.construction_rebuilds++;}
    c->world=*w;if(nc)memcpy(c->cells,cells,nc*sizeof *cells);if(nb)memcpy(c->boxes,boxes,nb*sizeof *boxes);c->cell_count=nc;c->box_count=nb;c->ready=true;
    if(nt)memcpy(c->trees,trees,nt*sizeof *trees);
    c->tree_count=nt;
    if(any||build_changed||trees_changed)c->stats.generation++;
    c->stats.triangles=pe_scene_stats(c->construction).triangles;
    for(int i=0;i<4;i++)c->stats.triangles+=pe_scene_stats(c->terrain[i].scene).triangles;
    return true;
fail:
    for(int i=0;i<4;i++)terrain_free(&pending[i]);
    pe_scene_free(construction);return false;
}
static bool capsule_valid(const PeSandboxCollision *c,PeVec3 p,float r,float h){return c&&c->ready&&finite3(p)&&isfinite(r)&&isfinite(h)&&r>0&&r<=PE_WORLD_Y/2.0f&&h>=2*r&&h<=PE_WORLD_Y-1;}
static bool outside(PeVec3 p,float r,float h){return p.x<r||p.z<r||p.x>PE_WORLD_X-1-r||p.z>PE_WORLD_Z-1-r||p.y<0||p.y+h>PE_WORLD_Y-1;}
static bool terrain_inside(const PeSandboxCollision *c,PeVec3 p){
    /* Surface queries deliberately return two-sided normals. Retain each MC
       face's original orientation to classify completely enclosed capsules. */
    const PeVec3 directions[]={{0,1,0},{0,-1,0},{1,0,0},{-1,0,0},{0,0,1},{0,0,-1}};
    const int axes[]={1,1,0,0,2,2};
    for(int d=0;d<6;d++){
        PeSceneHit best={0};int region=-1;
        for(int i=0;i<4;i++){PeSceneHit hit=pe_scene_raycast(c->terrain[i].scene,p,directions[d],PE_WORLD_X+PE_WORLD_Y+PE_WORLD_Z);if(hit.hit&&(!best.hit||hit.distance<best.distance)){best=hit;region=i;}}
        if(region>=0){int sign=c->terrain[region].orientation[best.triangle*3+axes[d]];if(sign)return sign*component(directions[d],axes[d])>0;}
    }
    /* An open cropped region may have no surface in any axial direction (e.g.
       a uniform full chunk). The sample then labels this surface-free region;
       it is not used to approximate capsule contact against MC geometry. */
    return pe_world_get(&c->world,(int)lroundf(p.x),(int)lroundf(p.y),(int)lroundf(p.z)).volume>=PE_ISO_LEVEL;
}
bool pe_sandbox_collision_overlap_geometry(const PeSandboxCollision *c,PeVec3 p,float r,float h){
    if(!capsule_valid(c,p,r,h)||outside(p,r,h))return true;
    for(int i=0;i<4;i++)if(pe_scene_capsule_overlap(c->terrain[i].scene,p,r,h))return true;
    if(pe_scene_capsule_overlap(c->construction,p,r,h))return true;
    PeVec3 middle={p.x,p.y+h*.5f,p.z};
    for(size_t i=0;i<c->cell_count;i++)if(pe_b45_contains(&c->cells[i],middle))return true;
    for(size_t i=0;i<c->box_count;i++){PeSandboxBox b=c->boxes[i];if(middle.x>b.minimum.x&&middle.x<b.maximum.x&&middle.y>b.minimum.y&&middle.y<b.maximum.y&&middle.z>b.minimum.z&&middle.z<b.maximum.z)return true;}
    return terrain_inside(c,middle);
}
bool pe_sandbox_collision_overlap(const PeSandboxCollision *c,PeVec3 p,float r,float h){
    if(pe_sandbox_collision_overlap_geometry(c,p,r,h))return true;
    for(size_t i=0;i<c->tree_count;i++)if(pe_tree_collision_overlap(&c->trees[i],p,r,h))return true;
    return false;
}
PeSceneHit pe_sandbox_collision_sweep_geometry(const PeSandboxCollision *c,PeVec3 p,float r,float h,PeVec3 delta){
    PeSceneHit best={0};if(!capsule_valid(c,p,r,h)||!finite3(delta)||length(delta)>1000||outside(p,r,h))return best;
    for(int i=0;i<5;i++){
        PeSceneHit hit=pe_scene_capsule_sweep(i==4?c->construction:c->terrain[i].scene,p,r,h,delta);
        if(hit.hit&&(!best.hit||hit.fraction<best.fraction))best=hit;
    }
    const float lo[]={r,0,r},hi[]={PE_WORLD_X-1-r,PE_WORLD_Y-1-h,PE_WORLD_Z-1-r};
    for(int a=0;a<3;a++){
        float v=component(delta,a),at=component(p,a);if(v==0)continue;
        float t=((v>0?hi[a]:lo[a])-at)/v;
        if(t>=0&&t<=1&&(!best.hit||t<best.fraction)){
            PeVec3 normal={0};if(a==0)normal.x=v>0?-1:1;if(a==1)normal.y=v>0?-1:1;if(a==2)normal.z=v>0?-1:1;
            best=(PeSceneHit){.hit=true,.fraction=t,.distance=length(delta)*t,.position=add(p,mul(delta,t)),.normal=normal,.triangle=UINT32_MAX};
        }
    }
    return best;
}
PeSceneHit pe_sandbox_collision_sweep(const PeSandboxCollision *c,PeVec3 p,float r,float h,PeVec3 delta){
    PeSceneHit best=pe_sandbox_collision_sweep_geometry(c,p,r,h,delta);
    if(!capsule_valid(c,p,r,h)||!finite3(delta)||length(delta)>1000||outside(p,r,h))return best;
    for(size_t i=0;i<c->tree_count;i++){
        PeSceneHit hit=pe_tree_collision_sweep(&c->trees[i],p,r,h,delta);
        if(hit.hit&&(!best.hit||hit.fraction<best.fraction))best=hit;
    }
    return best;
}
PeSceneHit pe_sandbox_collision_support(const PeSandboxCollision *c,PeVec3 p,float r,float h,float drop,float minimum){
    if(!isfinite(drop)||drop<=0||drop>PE_WORLD_Y||!isfinite(minimum)||minimum<0||minimum>1)return (PeSceneHit){0};
    PeSceneHit hit=pe_sandbox_collision_sweep(c,p,r,h,(PeVec3){0,-drop,0});return hit.hit&&hit.normal.y>=minimum?hit:(PeSceneHit){0};
}
static PeVec3 before_hit(PeVec3 p,PeVec3 delta,PeSceneHit hit){return add(p,mul(delta,fmaxf(0,hit.fraction-skin/fmaxf(length(delta),skin))));}
bool pe_sandbox_collision_settle(const PeSandboxCollision *c,PeVec3 p,float r,float h,float max_lift,float max_drop,PeVec3 *out){
    if(!out||!capsule_valid(c,p,r,h)||outside(p,r,h)||!isfinite(max_lift)||max_lift<0||max_lift>4||!isfinite(max_drop)||max_drop<=0||max_drop>5)return false;
    bool initially_clear=!pe_sandbox_collision_overlap(c,p,r,h);
    int steps=(int)ceilf(max_lift/.25f);
    for(int i=0;i<=steps;i++){
        PeVec3 candidate=p;candidate.y+=fminf(i*.25f,max_lift);
        if(outside(candidate,r,h))break;
        if(pe_sandbox_collision_overlap(c,candidate,r,h))continue;
        if(initially_clear&&i>0&&pe_sandbox_collision_sweep(c,p,r,h,(PeVec3){0,candidate.y-p.y,0}).hit)break;
        PeVec3 down={0,-max_drop,0};PeSceneHit floor=pe_sandbox_collision_support(c,candidate,r,h,max_drop,walkable);
        if(!floor.hit)continue;
        PeVec3 settled=before_hit(candidate,down,floor);
        if(!pe_sandbox_collision_overlap(c,settled,r,h)){*out=settled;return true;}
    }
    return false;
}
static PeVec3 slide(const PeSandboxCollision *c,PeVec3 p,float r,float h,PeVec3 delta,PeVec3 *velocity,bool *steep){
    for(int i=0;i<5&&length(delta)>.00001f;i++){
        PeSceneHit hit=pe_sandbox_collision_sweep(c,p,r,h,delta);if(!hit.hit)return add(p,delta);
        p=add(add(p,mul(delta,hit.fraction)),mul(hit.normal,skin));if(hit.normal.y<walkable)*steep=true;
        delta=mul(delta,1-hit.fraction);float into=dot(delta,hit.normal);if(into<0)delta=add(delta,mul(hit.normal,-into));
        into=dot(*velocity,hit.normal);if(into<0)*velocity=add(*velocity,mul(hit.normal,-into));
    }
    return p;
}
bool pe_sandbox_collision_move(const PeSandboxCollision *c,PeVec3 feet,PeVec3 velocity,float dt,float r,float h,bool grounded,bool allow_step,PeSandboxMotion *out){
    if(!out||!finite3(velocity)||!isfinite(dt)||dt<=0||dt>.25f||length(velocity)>1000||pe_sandbox_collision_overlap(c,feet,r,h))return false;
    PeVec3 original_velocity=velocity,delta=mul(velocity,dt);bool steep=false,moving=hypotf(delta.x,delta.z)>.00001f;
    PeVec3 p=slide(c,feet,r,h,delta,&velocity,&steep);
    if(grounded&&allow_step&&moving&&steep){
        PeVec3 up={0,step_height,0},horizontal={delta.x,0,delta.z},raised=add(feet,up);
        if(!pe_sandbox_collision_sweep(c,feet,r,h,up).hit&&!pe_sandbox_collision_sweep(c,raised,r,h,horizontal).hit){
            raised=add(raised,horizontal);PeVec3 down={0,-(step_height+.08f),0};PeSceneHit floor=pe_sandbox_collision_support(c,raised,r,h,-down.y,.01f);
            /* A capsule first meets a short ledge at its rounded bottom, whose
               contact normal may be steep although the ledge top is flat.
               Accept a clear bounded step position without declaring it ground;
               the ordinary walkable-support probe below remains authoritative. */
            if(floor.hit){PeVec3 step=before_hit(raised,down,floor);if(step.y<=feet.y+step_height+skin&&!pe_sandbox_collision_overlap(c,step,r,h)&&hypotf(step.x-feet.x,step.z-feet.z)>hypotf(p.x-feet.x,p.z-feet.z)+.00001f){p=step;velocity=original_velocity;velocity.y=0;}}
        }
    }
    if(allow_step&&velocity.y>0)velocity.y=0;
    if(!moving&&grounded&&allow_step){p.x=feet.x;p.z=feet.z;velocity.x=velocity.z=0;}
    if(pe_sandbox_collision_overlap(c,p,r,h)){p=feet;velocity=(PeVec3){0};}
    bool supported=false;
    if(velocity.y<=0){
        float drop=grounded&&allow_step?step_height:.06f;PeVec3 down={0,-drop,0};PeSceneHit floor=pe_sandbox_collision_support(c,p,r,h,drop,walkable);
        if(floor.hit){PeVec3 next=before_hit(p,down,floor);if(!pe_sandbox_collision_overlap(c,next,r,h)){p=next;velocity.y=0;supported=true;}}
    }
    *out=(PeSandboxMotion){p,velocity,supported};return true;
}
