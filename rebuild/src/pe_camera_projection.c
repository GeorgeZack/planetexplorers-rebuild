#include "pe_camera_projection.h"
#include <math.h>
static bool finite3(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static float dot(PeVec3 a,PeVec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static PeVec3 scale(PeVec3 a,float s){return(PeVec3){a.x*s,a.y*s,a.z*s};}
static PeVec3 add(PeVec3 a,PeVec3 b){return(PeVec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static PeVec3 cross(PeVec3 a,PeVec3 b){return(PeVec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static bool unit(PeVec3 a,PeVec3 *out){float n=dot(a,a);if(!finite3(a)||!isfinite(n)||n<1e-12f)return false;*out=scale(a,1/sqrtf(n));return true;}
bool pe_camera_projection_basis(PeVec3 f,PeVec3 u,PeVec3 *right,PeVec3 *up){
    PeVec3 r,v;if(!right||!up||!unit(f,&f)||!unit(u,&u)||!unit(cross(u,f),&r)||!unit(cross(f,r),&v))return false;
    *right=r;*up=v;return true;
}
static bool basis(const PeCameraProjection *p,PeVec3 *f,PeVec3 *r,PeVec3 *u,float *t){
    if(!p||!finite3(p->position)||!isfinite(p->width)||!isfinite(p->height)||p->width<=0||p->height<=0||
       !isfinite(p->near_clip)||p->near_clip<=0||!isfinite(p->fov_degrees)||p->fov_degrees<=0||p->fov_degrees>=179||
       !unit(p->forward,f)||!pe_camera_projection_basis(*f,p->up,r,u))return false;
    *t=tanf(p->fov_degrees*.008726646259971648f);return isfinite(*t)&&*t>0;
}
bool pe_camera_projection_ray(const PeCameraProjection *p,float x,float y,PeCameraRay *out){
    PeVec3 f,r,u,d;float t;if(!out||!isfinite(x)||!isfinite(y)||!basis(p,&f,&r,&u,&t))return false;
    float sx=(2*x/p->width-1)*(p->width/p->height)*t,sy=(1-2*y/p->height)*t;
    if(!unit(add(f,add(scale(r,sx),scale(u,sy))),&d))return false;
    float advance=p->near_clip/dot(d,f);PeCameraRay ray={add(p->position,scale(d,advance)),d};
    if(!isfinite(advance)||advance<=0||!finite3(ray.origin))return false;
    *out=ray;return true;
}
bool pe_camera_projection_screen(const PeCameraProjection *p,PeVec3 point,float *x,float *y){
    PeVec3 f,r,u;float t;if(!x||!y||!finite3(point)||!basis(p,&f,&r,&u,&t))return false;
    PeVec3 delta=add(point,scale(p->position,-1));float depth=dot(delta,f);if(!isfinite(depth)||depth<=0)return false;
    float px=(dot(delta,r)/(depth*t*(p->width/p->height))+1)*p->width*.5f;
    float py=(1-dot(delta,u)/(depth*t))*p->height*.5f;if(!isfinite(px)||!isfinite(py))return false;
    *x=px;*y=py;return true;
}
