#include "pe_camera_geometry.h"
#include "../generated/pe_camera_geometry_source.h"
#include <math.h>

static PeVec3 add(PeVec3 a,PeVec3 b){return(PeVec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static PeVec3 sub(PeVec3 a,PeVec3 b){return(PeVec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static PeVec3 mul(PeVec3 a,float f){return(PeVec3){a.x*f,a.y*f,a.z*f};}
static float dot(PeVec3 a,PeVec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static PeVec3 cross(PeVec3 a,PeVec3 b){return(PeVec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static float length(PeVec3 a){return sqrtf(dot(a,a));}
static bool finite3(PeVec3 a){return isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z);}
static float clamp(float n,float low,float high){return fminf(high,fmaxf(low,n));}
/* Native small-vector normalization guard; source Vector3 normalization is
 * not a claim of recovered engine arithmetic. Keep zero directions explicit. */
static PeVec3 normalized(PeVec3 a){float n=length(a);return n>1e-5f?mul(a,1.f/n):(PeVec3){0};}
static bool basis(const PeCameraGeometryInput *i){
    if(!finite3(i->forward)||!finite3(i->right)||!finite3(i->up))return false;
    if(fabsf(dot(i->forward,i->forward)-1)>.0005f||fabsf(dot(i->right,i->right)-1)>.0005f||
       fabsf(dot(i->up,i->up)-1)>.0005f||fabsf(dot(i->forward,i->right))>.0005f||
       fabsf(dot(i->forward,i->up))>.0005f||fabsf(dot(i->right,i->up))>.0005f)return false;
    return length(sub(cross(i->right,i->up),i->forward))<=.0005f;
}
typedef struct {PeCameraGeometryQueryFn function;void *context;} Query;
static bool ask(Query q,PeCameraGeometryQueryKind kind,PeVec3 origin,PeVec3 direction,PeVec3 end,
                float radius,float distance,PeCameraGeometryHit *hit){
    if(!finite3(origin)||!finite3(direction)||!finite3(end)||!isfinite(radius)||radius<0||
       !isfinite(distance)||distance<0)return false;
    if((kind==PE_CAMERA_GEOMETRY_OVERLAP||kind==PE_CAMERA_GEOMETRY_SPHERE_CAST)&&radius<=0)return false;
    PeCameraGeometryQuery request={kind,origin,direction,end,radius,distance,PE_CAMERA_GEOMETRY_MASK,true};
    PeCameraGeometryHit answer={0};
    if(!q.function(q.context,&request,&answer)||!isfinite(answer.distance)||answer.distance<0||
       answer.distance>distance||(!answer.hit&&answer.distance!=0))return false;
    *hit=answer;return true;
}
static bool overlap(Query q,PeVec3 center,float radius,bool *occupied){
    PeCameraGeometryHit hit;
    if(!ask(q,PE_CAMERA_GEOMETRY_OVERLAP,center,(PeVec3){0},(PeVec3){0},radius,0,&hit))return false;
    *occupied=hit.hit;return true;
}
static bool near_radius(Query q,PeVec3 target,float maximum,float *result){
    float minimum=PE_CAMERA_GEOMETRY_MIN_RADIUS;bool occupied;
    if(!overlap(q,target,minimum,&occupied))return false;
    if(occupied){*result=minimum;return true;}
    if(!overlap(q,target,maximum,&occupied))return false;
    if(!occupied){*result=maximum;return true;}
    while(maximum-minimum>PE_CAMERA_GEOMETRY_SEARCH_EPSILON){
        float mid=(minimum+maximum)*.5f;
        /* Overflow/stalled float search must fail, never loop indefinitely. */
        if(!isfinite(mid)||mid<=minimum||mid>=maximum)return false;
        if(!overlap(q,target,mid,&occupied))return false;
        if(occupied)maximum=mid;else minimum=mid;
    }
    *result=minimum;return true;
}
void pe_camera_geometry_init(PeCameraGeometryState *state){if(state)*state=(PeCameraGeometryState){0};}
bool pe_camera_geometry_evaluate(const PeCameraGeometryState *state,const PeCameraGeometryInput *i,
        PeCameraGeometryQueryFn callback,void *context,PeCameraGeometryResult *output){
    if(!state||!i||!callback||!output||!isfinite(state->rise)||state->rise<0||!finite3(i->position)||!basis(i)||
       !isfinite(i->distance)||i->distance<0||!isfinite(i->near_clip)||i->near_clip<=0||
       !isfinite(i->fov_degrees)||i->fov_degrees<=0||i->fov_degrees>=180||
       !isfinite(i->aspect)||i->aspect<=0||!isfinite(i->activity)||i->activity<0)return false;
    Query q={callback,context};PeCameraGeometryResult r={.position=i->position};
    PeVec3 target=add(i->position,mul(i->forward,i->distance));
    float ur=sqrtf(1.f+i->aspect*i->aspect)*tanf(i->fov_degrees*.5f*.0174532924f);
    float coefficient=clamp(i->activity*PE_CAMERA_GEOMETRY_ACTIVITY_FACTOR,PE_CAMERA_GEOMETRY_ACTIVITY_MIN,PE_CAMERA_GEOMETRY_ACTIVITY_MAX);
    float maximum=i->near_clip*coefficient*ur;
    if(!finite3(target)||!isfinite(ur)||ur<=0||!isfinite(maximum)||maximum<=0||!near_radius(q,target,maximum,&r.ncr))return false;
    r.near_clip=r.ncr/ur;
    if(!isfinite(r.near_clip)||r.near_clip<=0)return false;
    PeVec3 eye=add(r.position,mul(i->forward,r.near_clip));bool dont_clamp=false;
    for(float radius=0;radius<PE_CAMERA_GEOMETRY_RING_END;radius+=PE_CAMERA_GEOMETRY_RING_STEP){
        int count=8;if(radius<2)count=4;else if(radius<1)count=1;
        for(int index=0;index<count;index++){
            float rad=index*(360/count)*.0174532924f;
            PeVec3 test_dir=mul(add(mul(i->right,cosf(rad)),mul(i->up,sinf(rad))),radius);
            PeVec3 point=add(test_dir,target);PeCameraGeometryHit hit={0};
            if(radius!=0&&!ask(q,PE_CAMERA_GEOMETRY_RAY,target,normalized(test_dir),(PeVec3){0},0,radius,&hit))return false;
            if(radius==0||!hit.hit){
                PeVec3 delta=sub(eye,point);
                if(!ask(q,PE_CAMERA_GEOMETRY_LINE,point,normalized(delta),eye,0,length(delta),&hit))return false;
                if(!hit.hit){dont_clamp=true;goto sphere_test;}
            }
        }
    }
sphere_test:;
    bool occupied=false;
    /* Preserve source short circuit: no eye overlap call when all lines block. */
    if(dont_clamp&&!overlap(q,eye,r.ncr,&occupied))return false;
    if(!dont_clamp||occupied){
        PeVec3 delta=sub(eye,target);float distance=length(delta);PeVec3 direction=normalized(delta);
        if(!isfinite(distance))return false;
        if(distance>0){
            PeCameraGeometryHit hit;
            if(!ask(q,PE_CAMERA_GEOMETRY_SPHERE_CAST,target,direction,(PeVec3){0},r.ncr-PE_CAMERA_GEOMETRY_CLAMP_SHRINK,distance,&hit))return false;
            if(hit.hit){eye=add(target,mul(direction,hit.distance));r.clamped=true;}
        }
        r.position=sub(eye,mul(i->forward,r.near_clip));
    }
    r.distance=length(sub(target,r.position));
    float max_rise=r.ncr;PeVec3 rise_dir=add((PeVec3){0,1,0},mul(i->forward,PE_CAMERA_GEOMETRY_RISE_FORWARD));
    PeCameraGeometryHit rise_hit;
    if(!ask(q,PE_CAMERA_GEOMETRY_SPHERE_CAST,eye,normalized(rise_dir),(PeVec3){0},r.ncr-PE_CAMERA_GEOMETRY_RISE_SHRINK,r.ncr*PE_CAMERA_GEOMETRY_RISE_QUERY_SCALE,&rise_hit))return false;
    if(rise_hit.hit)max_rise=rise_hit.distance*PE_CAMERA_GEOMETRY_RISE_HIT_FACTOR;
    float wanted=0,near_twice=r.near_clip*PE_CAMERA_GEOMETRY_RISE_NEAR_SCALE;
    if(r.distance<near_twice){float u=r.distance/near_twice;wanted=sqrtf(PE_CAMERA_GEOMETRY_RISE_RADICAND-u*u)*near_twice;}
    if(wanted>max_rise)wanted=max_rise;
    if(i->shoot_mode)wanted=0;
    r.rise=state->rise+(wanted-state->rise)*PE_CAMERA_GEOMETRY_RISE_LERP;
    r.position=add(r.position,mul(rise_dir,r.rise));
    if(!finite3(r.position)||!isfinite(r.distance)||!isfinite(r.rise)||r.rise<0)return false;
    *output=r;return true;
}
bool pe_camera_geometry_step(PeCameraGeometryState *state,const PeCameraGeometryInput *input,
        PeCameraGeometryQueryFn query,void *context,PeCameraGeometryResult *output){
    if(!output)return false;
    PeCameraGeometryResult result;
    if(!pe_camera_geometry_evaluate(state,input,query,context,&result))return false;
    state->rise=result.rise;*output=result;return true;
}
