#include "pe_training_tree_hit.h"
#include <math.h>
#include <float.h>

PeVec3 pe_training_tree_center(void){
    /* Authored root(12,1.5,12),yaw-90; child(-.020660669,.082330704,
     * 1.07916927), collider local center(-.1,3,-.2). */
    return (PeVec3){11.120831f,4.5823307f,11.879339f};
}
static bool finite3(PeVec3 a){return isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z);}
static void sphere(double x,double y,double z,double dx,double dy,double dz,double cy,double *best){
    y-=cy;
    double b=x*dx+y*dy+z*dz,c=x*x+y*y+z*z-1,disc=b*b-c;
    if(disc<0)return;
    double t=-b-sqrt(disc);
    if(t>=0&&t<*best)*best=t;
}
bool pe_training_tree_hit(PeVec3 origin,PeVec3 forward,float *distance){
    if(!finite3(origin)||!finite3(forward))return false;
    double length=hypot(hypot((double)forward.x,forward.y),forward.z);
    if(length==0||!isfinite(length))return false;
    PeVec3 center=pe_training_tree_center();
    double x=(double)origin.x-center.x,y=(double)origin.y-center.y,z=(double)origin.z-center.z;
    double dy=forward.y/length,dx=forward.x/length,dz=forward.z/length;
    double outside_y=fmax(fabs(y)-2.5,0);
    if(x*x+outside_y*outside_y+z*z<1)return false;
    double best=DBL_MAX,a=dx*dx+dz*dz,b=x*dx+z*dz,c=x*x+z*z-1;
    if(a>0){
        double disc=b*b-a*c;
        if(disc>=0){
            double t=(-b-sqrt(disc))/a,hit_y=y+t*dy;
            if(t>=0&&hit_y>=-2.5&&hit_y<=2.5)best=t;
        }
    }
    sphere(x,y,z,dx,dy,dz,-2.5,&best);
    sphere(x,y,z,dx,dy,dz,2.5,&best);
    if(best>2)return false;
    if(distance)*distance=(float)best;
    return true;
}

/* Minkowski difference of two parallel upright capsule axis segments. Player
 * feet +[.3,1.5] and tree center +[-2.5,2.5] => feet-space center tree-.9,
 * half-axis3.1, radius1+.3. This is exact for these authored capsule sizes. */
#define PLAYER_TREE_RADIUS 1.3
#define PLAYER_TREE_HALF_AXIS 3.1
static double tree_distance(PeVec3 feet,PeVec3 *normal){
    PeVec3 c=pe_training_tree_center();
    double x=(double)feet.x-c.x,y=(double)feet.y-((double)c.y-.9),z=(double)feet.z-c.z;
    double axis=fmax(-PLAYER_TREE_HALF_AXIS,fmin(PLAYER_TREE_HALF_AXIS,y));
    y-=axis;double d=hypot(hypot(x,y),z);
    if(normal)*normal=d>0?(PeVec3){(float)(x/d),(float)(y/d),(float)(z/d)}:(PeVec3){1,0,0};
    return d;
}
bool pe_training_tree_overlap(PeVec3 feet){
    return !finite3(feet)||tree_distance(feet,NULL)<PLAYER_TREE_RADIUS;
}
static void expanded_sphere(double x,double y,double z,double dx,double dy,double dz,double cy,double *best){
    y-=cy;double b=x*dx+y*dy+z*dz;
    double disc=b*b-(x*x+y*y+z*z-PLAYER_TREE_RADIUS*PLAYER_TREE_RADIUS);
    if(disc<0)return;
    double t=-b-sqrt(disc);
    if(t>=0&&t<*best)*best=t;
}
PeSceneHit pe_training_tree_sweep(PeVec3 feet,PeVec3 delta){
    PeSceneHit hit={0};
    if(!finite3(feet)||!finite3(delta))return hit;
    double length=hypot(hypot((double)delta.x,delta.y),delta.z);
    if(!length||!isfinite(length))return hit;
    PeVec3 normal;double distance=tree_distance(feet,&normal),travel=DBL_MAX;
    double closing=(double)delta.x*normal.x+(double)delta.y*normal.y+(double)delta.z*normal.z;
    if(distance<=PLAYER_TREE_RADIUS){
        /* Tangent/separating contacts do not stop a slide or jump away. */
        if(closing>=0)return hit;
        travel=0;
    }else{
        PeVec3 c=pe_training_tree_center();
        double x=(double)feet.x-c.x,y=(double)feet.y-((double)c.y-.9),z=(double)feet.z-c.z;
        double dx=delta.x/length,dy=delta.y/length,dz=delta.z/length;
        double a=dx*dx+dz*dz,b=x*dx+z*dz;
        if(a>0){
            double disc=b*b-a*(x*x+z*z-PLAYER_TREE_RADIUS*PLAYER_TREE_RADIUS);
            if(disc>=0){double t=(-b-sqrt(disc))/a,h=y+t*dy;
                if(t>=0&&h>=-PLAYER_TREE_HALF_AXIS&&h<=PLAYER_TREE_HALF_AXIS)travel=t;
            }
        }
        expanded_sphere(x,y,z,dx,dy,dz,-PLAYER_TREE_HALF_AXIS,&travel);
        expanded_sphere(x,y,z,dx,dy,dz,PLAYER_TREE_HALF_AXIS,&travel);
    }
    if(travel>length)return hit;
    double fraction=travel/length;
    PeVec3 contact={(float)(feet.x+delta.x*fraction),(float)(feet.y+delta.y*fraction),(float)(feet.z+delta.z*fraction)};
    tree_distance(contact,&normal);
    /* A tangent-only touch has no inward component and cannot obstruct. */
    closing=(double)delta.x*normal.x+(double)delta.y*normal.y+(double)delta.z*normal.z;
    if(closing>=0)return hit;
    PeVec3 center=pe_training_tree_center();
    double axis_y=fmax((double)center.y-2.5,fmin((double)center.y+2.5,(double)contact.y+.9));
    hit.hit=true;hit.fraction=(float)fraction;hit.distance=(float)travel;hit.normal=normal;
    hit.position=(PeVec3){center.x+normal.x,(float)axis_y+normal.y,center.z+normal.z};
    hit.triangle=UINT32_MAX;
    return hit;
}
