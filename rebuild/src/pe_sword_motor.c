#include "pe_sword_motor.h"
#include "pe_sword_motion.h"
#include "../generated/pe_human_phy_source.h"
#include <math.h>

static bool finite3(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
void pe_sword_motor_reset(PeSwordMotor *m){
    if(!m)return;
    *m=(PeSwordMotor){0};pe_human_phy_init(&m->phy);
}
bool pe_sword_motor_consume(PeSwordMotor *m,bool active,PeVec3 velocity,bool grounded,float dt,
                            PeSwordMotorStep *out){
    if(!m||!out||!finite3(velocity)||!isfinite(dt)||dt<=0)return false;
    if(!active&&!m->engaged){*out=(PeSwordMotorStep){0};return true;}
    PeSwordMotor next=*m;PeHumanPhyForces forces;
    /* SwordAttack ends Move and Sprint immediately, so the request is the
     * whole desired velocity. Vertical motion stays with the caller's step. */
    next.phy.desired_direction=(PeVec3){0};next.phy.fixed_updates=0;
    next.phy.phy_ground=next.phy.grounded=grounded;
    next.phy.drag=grounded?pe_human_phy_source_ground_drag:pe_human_phy_source_air_drag;
    next.phy.gravity=0;
    PeHumanPhyStep step={.velocity={velocity.x,0,velocity.z},.up={0,1,0},.fixed_delta_time=dt};
    if(!pe_human_phy_fixed_update(&next.phy,step,&forces)||!forces.ran)return false;
    PeSwordMotorStep result={.owns_motion=true,
        .velocity={velocity.x+forces.velocity_change.x+forces.acceleration.x*dt,0,
                   velocity.z+forces.velocity_change.z+forces.acceleration.z*dt},
        .coasting=next.phy.applied_velocity.x==0&&next.phy.applied_velocity.z==0};
    if(result.coasting&&result.velocity.x*velocity.x+result.velocity.z*velocity.z<0)
        result.velocity=(PeVec3){0}; /* one explicit step of quadratic drag must not reverse the body */
    if(!finite3(result.velocity))return false;
    next.engaged=active;
    *m=next;*out=result;return true;
}
bool pe_sword_motor_update(PeSwordMotor *m,bool running,const double delta[3],PeVec3 velocity,double dt){
    if(!m||!finite3(velocity)||!isfinite(dt)||dt<=0||
       (delta&&(!isfinite(delta[0])||!isfinite(delta[1])||!isfinite(delta[2]))))return false;
    PeSwordMotor next=*m;
    if(!running){
        /* OnEndAction: CancelMoveRequest. The delta of the ending frame and
         * the request before it never reach the body. */
        if(!pe_human_phy_cancel_move_request(&next.phy))return false;
        next.has_last_move=false;
    }else if(next.has_last_move){
        PeVec3 request;
        if(!pe_sword_motion_request(next.last_move,velocity,dt,&request)||
           !pe_human_phy_apply_move_request(&next.phy,request))return false;
    }
    if(delta){
        next.has_last_move=true;
        next.last_move[0]=delta[0];next.last_move[1]=delta[1];next.last_move[2]=delta[2];
    }else next.has_last_move=false;
    if(!pe_human_phy_late_update(&next.phy,velocity))return false;
    *m=next;return true;
}
