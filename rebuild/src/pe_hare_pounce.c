#include "pe_hare_pounce.h"
#include "../generated/pe_hare_pounce_source.h"
#include <math.h>
#include <string.h>
const PeHarePounceDef *pe_hare_pounce_def(void){return &pe_hare_pounce_source;}
static bool vec(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
static bool valid(const PeHarePounce *s){
    return s&&s->initialized&&isfinite(s->last_cooldown)&&s->last_cooldown>=0&&
        isfinite(s->start_time)&&s->start_time>=0&&isfinite(s->stop_time)&&s->stop_time>=0&&
        isfinite(s->rotate_time)&&s->rotate_time>=0&&isfinite(s->last_time)&&s->last_time>=0&&
        isfinite(s->speed)&&s->speed>=0&&isfinite(s->saved_gravity);
}
static bool input_valid(PeHarePounceInput i){
    return isfinite(i.now)&&i.now>=0&&isfinite(i.gravity)&&isfinite(i.sqr_distance_logic)&&i.sqr_distance_logic>=0&&
        vec(i.position)&&vec(i.target_position)&&vec(i.forward)&&vec(i.direction_xz);
}
void pe_hare_pounce_init(PeHarePounce *s){if(s){memset(s,0,sizeof *s);s->initialized=true;}}
bool pe_hare_pounce_ready_cd(const PeHarePounce *s,float now,bool runnable){
    return valid(s)&&isfinite(now)&&now>=s->last_time&&runnable&&now-s->last_cooldown>pe_hare_pounce_source.cooldown;
}
bool pe_hare_pounce_ready_geometry(PeHarePounceInput i){
    const PeHarePounceDef *d=&pe_hare_pounce_source;
    return input_valid(i)&&i.target_present&&i.sqr_distance_logic>=d->min_range*d->min_range&&i.sqr_distance_logic<=d->max_range*d->max_range&&i.in_scope&&!i.blocked;
}
bool pe_hare_pounce_window(float time){return isfinite(time)&&time>=pe_hare_pounce_source.window_start&&time<=pe_hare_pounce_source.window_end;}
bool pe_hare_pounce_begin(PeHarePounce *s,PeHarePounceInput i,PeHarePounceRequests *out){
    if(!valid(s)||!out||s->active||!input_valid(i)||i.now<s->last_time)return false;
    PeHarePounceRequests r={.status=PE_HARE_POUNCE_FAILURE};
    if(!i.selected||!pe_hare_pounce_ready_geometry(i)){*out=r;return true;}
    PeHarePounce n=*s;n.active=true;n.started=n.stopped=n.faced=false;n.speed=0;
    n.rotate_time=n.last_time=i.now;n.saved_gravity=i.gravity;
    r.status=PE_HARE_POUNCE_RUNNING;r.gravity_set=true;r.clear_movement_effect=true;r.stop_move=true;
    *s=n;*out=r;return true;
}
bool pe_hare_pounce_tick(PeHarePounce *s,PeHarePounceInput i,PeHarePounceRequests *out){
    if(!valid(s)||!out||!s->active||!input_valid(i)||i.now<s->last_time)return false;
    PeHarePounce n=*s;PeHarePounceRequests r={.status=PE_HARE_POUNCE_RUNNING};n.last_time=i.now;
    if(!i.target_present){r.status=PE_HARE_POUNCE_FAILURE;*s=n;*out=r;return true;}
    PeVec3 delta={i.target_position.x-i.position.x,i.target_position.y-i.position.y,i.target_position.z-i.position.z};
    if(!vec(delta))return false;
    if(!n.faced)n.faced=i.in_scope;
    if(!n.faced){
        r.face_direction_set=true;r.face_direction=delta;
        if(i.now-n.rotate_time>5)r.status=PE_HARE_POUNCE_FAILURE;
    }else if(!n.started){
        n.started=true;n.start_time=n.last_cooldown=i.now;
        r.animator_set=r.animator_attack5=r.start_skill=true;r.skill_id=pe_hare_pounce_source.skill_id;
    }else{
        float elapsed=i.now-n.start_time;
        if(!isfinite(elapsed)||elapsed<0)return false;
        if(n.stopped&&elapsed>pe_hare_pounce_source.stop_time)r.status=PE_HARE_POUNCE_SUCCESS;
        else if(elapsed<pe_hare_pounce_source.start_time)r.stop_move=true;
        else if(elapsed>pe_hare_pounce_source.end_time){
            if(!n.stopped){
                n.stopped=true;n.stop_time=i.now;r.stop_move=r.animator_set=r.move_direction_set=r.face_direction_set=r.pursue_run=true;
                r.pursuit_target=i.target_position;r.status=PE_HARE_POUNCE_SUCCESS;
            }
        }else{
            if(n.speed<PE_HARE_POUNCE_EPSILON){
                /* Source PEUtil.MagnitudeH is magnitude of projected difference. */
                float distance=sqrtf(delta.x*delta.x+delta.z*delta.z);
                n.speed=(distance+1)/(pe_hare_pounce_source.end_time-pe_hare_pounce_source.start_time);
                if(!isfinite(n.speed))return false;
            }
            r.speed_set=r.move_direction_set=r.face_direction_set=true;r.speed=n.speed;r.move_direction=i.forward;r.face_direction=i.direction_xz;
        }
    }
    *s=n;*out=r;return true;
}
bool pe_hare_pounce_reset(PeHarePounce *s,PeHarePounceRequests *out){
    if(!valid(s)||!out)return false;
    PeHarePounce n=*s;PeHarePounceRequests r={.status=PE_HARE_POUNCE_SUCCESS};
    if(n.rotate_time>PE_HARE_POUNCE_EPSILON){
        r.speed_set=r.animator_set=r.move_direction_set=r.face_direction_set=r.gravity_set=true;r.gravity=n.saved_gravity;
        n.rotate_time=0;
    }
    n.active=false;*s=n;*out=r;return true;
}
