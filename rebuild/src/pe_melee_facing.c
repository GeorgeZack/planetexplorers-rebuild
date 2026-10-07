#include "pe_melee_facing.h"
#include "../generated/pe_attack_playback_source.h"
#include <math.h>

static const double radians=3.14159265358979323846/180.;
static bool finite_vector(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static double wrapped(double yaw){return remainder(yaw,360.);}

bool pe_melee_facing_direction(PeVec3 movement,float yaw,PeVec3 *out){
    if(!out||!finite_vector(movement)||!isfinite(yaw))return false;
    double length=hypot((double)movement.x,(double)movement.z);
    PeVec3 direction;
    if(length>1e-5)direction=(PeVec3){(float)(movement.x/length),0,(float)(movement.z/length)};
    else{
        double angle=wrapped(yaw)*radians;
        direction=(PeVec3){(float)sin(angle),0,(float)cos(angle)};
    }
    *out=direction;return true;
}

bool pe_melee_facing_yaw(float yaw,PeVec3 attack,const PeSwordTargetResult *target,float dt,float *out){
    if(!out||!isfinite(yaw)||!finite_vector(attack)||hypot((double)attack.x,(double)attack.z)<=1e-5||
       !isfinite(dt)||dt<0)return false;
    if(target&&(!finite_vector(target->direction)||(!target->selected&&target->aim_active)))return false;
    if(target&&target->selected&&target->aim_active){
        double horizontal=hypot((double)target->direction.x,(double)target->direction.z);
        /* Unity LookRotation on a flattened zero vector cannot define a yaw;
         * retain the supported upright body's heading instead. */
        if(horizontal<=1e-5){*out=yaw;return true;}
        *out=(float)(atan2((double)target->direction.x,(double)target->direction.z)/radians);return true;
    }
    if(dt==0){*out=yaw;return true;}
    double t=fmin(1.,(double)PE_SWORD_ROTATE_ACCELERATION*dt);
    double angle=wrapped(yaw)*radians*.5;
    double goal=atan2((double)attack.x,(double)attack.z)*.5;
    double sy=sin(angle),sw=cos(angle),ty=sin(goal),tw=cos(goal);
    if(sy*ty+sw*tw<0){ty=-ty;tw=-tw;}
    double y=(1.-t)*sy+t*ty,w=(1.-t)*sw+t*tw;
    double norm=hypot(y,w);
    if(!isfinite(norm)||norm<=0)return false;
    y/=norm;w/=norm;
    float next=(float)wrapped(2.*atan2(y,w)/radians);
    if(!isfinite(next))return false;
    *out=next;return true;
}
