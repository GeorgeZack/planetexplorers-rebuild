#include "pe_sword_variant.h"
#include "../generated/pe_sword_variant_source.h"
#include <math.h>
bool pe_sword_variant_speed(PeVec3 v,float *out){
    if(!out||!isfinite(v.x)||!isfinite(v.y)||!isfinite(v.z))return false;
    float square=(float)((double)v.x*v.x+(double)v.y*v.y+(double)v.z*v.z);
    if(!isfinite(square))return false;
    float speed=(float)sqrt((double)square);
    if(!isfinite(speed))return false;
    *out=speed;return true;
}
bool pe_sword_variant_select(PeSwordVariantInput i,PeSwordVariant *out){
    float speed;if(!out||!pe_sword_variant_speed(i.velocity,&speed))return false;
    PeSwordVariant v=PE_SWORD_VARIANT_ORDINARY;
    if(i.in_air)v=PE_SWORD_VARIANT_AIR;
    else if(i.motor_present&&i.spine_in_water)v=PE_SWORD_VARIANT_WATER;
    else if(i.movement_present){
        if(speed>pe_sword_variant_source.sprint_speed)v=PE_SWORD_VARIANT_SPRINT;
        else if(speed>pe_sword_variant_source.move_speed)v=PE_SWORD_VARIANT_RUNNING;
    }
    *out=v;return true;
}
