#include "pe_sword_motion.h"
#include "../generated/pe_sword_variant_source.h"
#include <math.h>

bool pe_sword_motion_request(const double world_delta[3],PeVec3 current_velocity,
                             double dt,PeVec3 *request){
    if(!world_delta||!request||!isfinite(dt)||dt<=0||
       !isfinite(world_delta[0])||!isfinite(world_delta[1])||!isfinite(world_delta[2])||
       !isfinite(current_velocity.x)||!isfinite(current_velocity.y)||!isfinite(current_velocity.z))return false;
    double x=world_delta[0]/dt,z=world_delta[2]/dt;
    if(!isfinite(x)||!isfinite(z))return false;
    const double dx=x-current_velocity.x,dz=z-current_velocity.z,length=hypot(dx,dz);
    if(!isfinite(length))return false;
    const double limit=pe_sword_variant_source.velocity_change_limit;
    if(length>=limit){
        x=current_velocity.x+limit*(dx/length);
        z=current_velocity.z+limit*(dz/length);
    }
    const PeVec3 result={(float)x,0,(float)z};
    if(!isfinite(result.x)||!isfinite(result.z))return false;
    *request=result;return true;
}
