#include "pe_tree_collision.h"
#include "../generated/pe_tree_collider.h"
#include <float.h>
#include <math.h>

static bool finite3(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
PeTreeCapsule pe_tree_collision_source(PeVec3 root){
    return (PeTreeCapsule){{root.x+pe_tree_collider_source[0],root.y+pe_tree_collider_source[1],
                           root.z+pe_tree_collider_source[2]},pe_tree_collider_source[3],pe_tree_collider_source[4]};
}
bool pe_tree_collision_valid(const PeTreeCapsule *t){
    return t&&finite3(t->center)&&fabsf(t->center.x)<=1024&&fabsf(t->center.y)<=1024&&fabsf(t->center.z)<=1024&&
        isfinite(t->radius)&&isfinite(t->height)&&t->radius>0&&t->radius<=10&&t->height>=2*t->radius&&t->height<=40;
}
static bool valid(const PeTreeCapsule *t,PeVec3 p,float r,float h){
    return pe_tree_collision_valid(t)&&finite3(p)&&isfinite(r)&&isfinite(h)&&r>0&&r<=16&&h>=2*r&&h<=40;
}
static double axis_distance(const double p[3],double half,double n[3]){
    n[0]=p[0];n[1]=p[1]-fmax(-half,fmin(half,p[1]));n[2]=p[2];
    double distance=hypot(hypot(n[0],n[1]),n[2]);
    if(distance>0){for(int i=0;i<3;i++)n[i]/=distance;}
    else{n[0]=1;n[1]=n[2]=0;}
    return distance;
}
bool pe_tree_collision_overlap(const PeTreeCapsule *t,PeVec3 feet,float r,float h){
    if(!valid(t,feet,r,h))return true;
    double p[]={(double)feet.x-t->center.x,(double)feet.y+h*.5-t->center.y,(double)feet.z-t->center.z},n[3];
    double half=t->height*.5-t->radius+h*.5-r;
    return axis_distance(p,half,n)<(double)t->radius+r;
}
/* Entry roots of the finite cylinder and its hemispheres. Sphere roots are
 * restricted to the exposed hemisphere, so internal overlap seams cannot win. */
static void sphere(const double p[3],const double d[3],double cy,double radius,int sign,double *best){
    double y=p[1]-cy,b=p[0]*d[0]+y*d[1]+p[2]*d[2];
    double disc=b*b-(p[0]*p[0]+y*y+p[2]*p[2]-radius*radius);
    if(disc<=0)return;
    double at=-b-sqrt(disc),hit_y=y+at*d[1];
    if(at>=0&&at<*best&&sign*hit_y>=0)*best=at;
}
PeSceneHit pe_tree_collision_sweep(const PeTreeCapsule *t,PeVec3 feet,float r,float h,PeVec3 delta){
    PeSceneHit hit={0};if(!valid(t,feet,r,h)||!finite3(delta))return hit;
    double length=hypot(hypot((double)delta.x,delta.y),delta.z);if(length==0||length>1000)return hit;
    double p[]={(double)feet.x-t->center.x,(double)feet.y+h*.5-t->center.y,(double)feet.z-t->center.z};
    double d[]={delta.x/length,delta.y/length,delta.z/length},n[3];
    double half=t->height*.5-t->radius+h*.5-r,radius=(double)t->radius+r;
    double distance=axis_distance(p,half,n),travel=DBL_MAX;
    if(distance<=radius){
        if(d[0]*n[0]+d[1]*n[1]+d[2]*n[2]>=-1e-12)return hit;
        travel=0;
    }else{
        double a=d[0]*d[0]+d[2]*d[2],b=p[0]*d[0]+p[2]*d[2];
        double disc=b*b-a*(p[0]*p[0]+p[2]*p[2]-radius*radius);
        if(a>0&&disc>0){
            double at=(-b-sqrt(disc))/a,y=p[1]+at*d[1];
            if(at>=0&&y>=-half&&y<=half)travel=at;
        }
        sphere(p,d,-half,radius,-1,&travel);sphere(p,d,half,radius,1,&travel);
    }
    if(travel>length)return hit;
    double contact[]={p[0]+d[0]*travel,p[1]+d[1]*travel,p[2]+d[2]*travel};
    axis_distance(contact,half,n);
    if(d[0]*n[0]+d[1]*n[1]+d[2]*n[2]>=-1e-12)return hit;
    double tree_half=t->height*.5-t->radius;
    double axis_y=t->center.y+fmax(-tree_half,fmin(tree_half,contact[1]));
    hit=(PeSceneHit){.hit=true,.fraction=(float)(travel/length),.distance=(float)travel,
        .position={(float)(t->center.x+n[0]*t->radius),(float)(axis_y+n[1]*t->radius),(float)(t->center.z+n[2]*t->radius)},
        .normal={(float)n[0],(float)n[1],(float)n[2]},.triangle=UINT32_MAX};
    return hit;
}
