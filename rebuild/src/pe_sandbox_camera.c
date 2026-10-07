#include "pe_sandbox_camera.h"
#include "../generated/pe_camera_environment_source.h"
#include <math.h>
#include <stddef.h>

static float bound(float n,float minimum,float maximum) {
    return n<minimum?minimum:n>maximum?maximum:n;
}
static bool valid(const PeSandboxCamera *c) {
    return c&&isfinite(c->desired_distance)&&isfinite(c->distance)&&isfinite(c->velocity)&&
        c->desired_distance>=PE_SANDBOX_CAMERA_MIN_DISTANCE&&c->desired_distance<=PE_SANDBOX_CAMERA_BUILD_MAX&&
        c->distance>=PE_SANDBOX_CAMERA_MIN_DISTANCE&&c->distance<=PE_SANDBOX_CAMERA_BUILD_MAX;
}
static void constrain(PeSandboxCamera *c,float maximum) {
    c->desired_distance=bound(c->desired_distance,PE_SANDBOX_CAMERA_MIN_DISTANCE,maximum);
    c->distance=bound(c->distance,PE_SANDBOX_CAMERA_MIN_DISTANCE,maximum);
    if((c->distance==maximum&&c->velocity>0)||
       (c->distance==PE_SANDBOX_CAMERA_MIN_DISTANCE&&c->velocity<0))c->velocity=0;
}
/* Float arithmetic and evaluation order follow installed UnityEngine.dll
 * Mathf.SmoothDamp(float,float,ref float,float,float,float), smoothTime=.5,
 * maxSpeed=+infinity. Its managed IL is method0x0600063f (163bytes), including
 * the .48/.235 rational decay and strict overshoot test. See SANDBOX_CAMERA.md.
 * dt0 is intercepted by our host contract, avoiding that old method's 0/0
 * overshoot velocity edge. No Unity runtime is needed by this implementation. */
static float damp(float current,float target,float *velocity,float dt) {
    const float omega=4.0f;
    float x=omega*dt;
    float decay=1.0f/(1.0f+x+.48f*x*x+.235f*x*x*x);
    float change=current-target;
    float original_target=target;
    target=current-change;
    float impulse=(*velocity+omega*change)*dt;
    *velocity=(*velocity-omega*impulse)*decay;
    float output=target+(change+impulse)*decay;
    if((original_target-current>0)==(output>original_target)) {
        output=original_target;
        *velocity=(output-original_target)/dt;
    }
    return output;
}
void pe_sandbox_camera_init(PeSandboxCamera *c) {
    if(c)*c=(PeSandboxCamera){PE_SANDBOX_CAMERA_DEFAULT_DISTANCE,PE_SANDBOX_CAMERA_DEFAULT_DISTANCE,0};
}
bool pe_sandbox_camera_step(PeSandboxCamera *c,PeSandboxCameraInput in) {
    if(!valid(c)||!isfinite(in.dt)||in.dt<0||in.dt>1||!isfinite(in.wheel_notches))return false;
    PeSandboxCamera next=*c;
    float maximum=in.building?PE_SANDBOX_CAMERA_BUILD_MAX:PE_SANDBOX_CAMERA_NORMAL_MAX;
    constrain(&next,maximum);
    if(!in.paused&&in.third_person&&in.dt>0) {
        if(in.input_allowed) {
            /* Use double only for bounded adapter accumulation so even a huge
             * finite scroll cannot overflow before clamping. */
            double wanted=(double)next.desired_distance-(double)in.wheel_notches*.8;
            next.desired_distance=wanted<PE_SANDBOX_CAMERA_MIN_DISTANCE?PE_SANDBOX_CAMERA_MIN_DISTANCE:
                wanted>maximum?maximum:(float)wanted;
        }
        next.distance=damp(next.distance,next.desired_distance,&next.velocity,in.dt);
        if(!isfinite(next.distance)||!isfinite(next.velocity))return false;
        constrain(&next,maximum);
    }
    *c=next;return true;
}

static bool finite_vector(PeVec3 v){return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
bool pe_sandbox_camera_activity(PeVec3 anchor,PeSandboxCameraRayQuery query,
    void *context,float *activity){
    if(!query||!activity||!finite_vector(anchor))return false;
    const float maximum=PE_CAMERA_ACTIVITY_MAX,accuracy=PE_CAMERA_ACTIVITY_ACCURACY;
    const float rad=0.0174532924f;
    float step=360.f/ceilf(bound(accuracy,.5f,5.f)*4.f),minimum=maximum;
    double weights=0,sum=0;
    for(float pitch=-90;pitch<90.01f;pitch+=step){
        if(pitch< -35)continue;
        float cosine=cosf(pitch*rad),sine=sinf(pitch*rad);
        float begin=sinf(pitch*1000.f)*180.f;
        float yaw_step=360.f/ceilf(bound(accuracy*cosine,.01f,5.f)*4.f);
        for(float yaw=begin;yaw<begin+359.99f;yaw+=yaw_step){
            PeVec3 dir={cosine*cosf(yaw*rad),sine,cosine*sinf(yaw*rad)};
            float length=sqrtf(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z);
            PeVec3 normalized={dir.x/length,dir.y/length,dir.z/length};
            float distance=maximum;
            if(!query(context,anchor,normalized,maximum,PE_SANDBOX_CAMERA_OBSTACLE_MASK,&distance)||
               !isfinite(distance)||distance<0||distance>maximum)return false;
            float direction_weight=powf(bound(dir.y,-1,1)+1,1.5f);
            float distance_weight=maximum/(distance+maximum*.3f);
            if(distance<minimum)minimum=distance;
            /* Source float products accumulate into double sums. */
            weights+=(double)(direction_weight*distance_weight);
            sum+=(double)(direction_weight*distance_weight*distance);
        }
    }
    float average=(float)(sum/weights),result=fmaxf(minimum,average*.5f);
    if(!(weights>0)||!isfinite(result)||result<0||result>maximum)return false;
    *activity=result;return true;
}
void pe_sandbox_camera_recovery_init(PeSandboxCameraRecovery *r){if(r)*r=(PeSandboxCameraRecovery){0};}
static bool recovery_valid(const PeSandboxCameraRecovery *r){
    return r&&isfinite(r->hold_seconds)&&r->hold_seconds>=-.10001f&&r->hold_seconds<=PE_CAMERA_CLAMP_HOLD;
}
static bool environment_valid(const PeSandboxCamera *c){
    return c&&isfinite(c->desired_distance)&&isfinite(c->distance)&&isfinite(c->velocity)&&
        c->desired_distance>=PE_SANDBOX_CAMERA_MIN_DISTANCE&&c->desired_distance<=PE_SANDBOX_CAMERA_BUILD_MAX&&
        c->distance>=0&&c->distance<=PE_SANDBOX_CAMERA_BUILD_MAX;
}
bool pe_sandbox_camera_follow_step(PeSandboxCamera *c,PeSandboxCameraRecovery *r,
    PeSandboxCameraInput in,float activity){
    if(!environment_valid(c)||!recovery_valid(r)||!isfinite(activity)||activity<0||activity>PE_CAMERA_ACTIVITY_MAX||
       !isfinite(in.dt)||in.dt<0||in.dt>1||!isfinite(in.wheel_notches))return false;
    PeSandboxCamera next=*c;PeSandboxCameraRecovery phase=*r;
    float maximum=in.building?PE_SANDBOX_CAMERA_BUILD_MAX:PE_SANDBOX_CAMERA_NORMAL_MAX;
    next.desired_distance=bound(next.desired_distance,PE_SANDBOX_CAMERA_MIN_DISTANCE,maximum);
    /* Preserve the existing host's immediate upper range policy on mode
     * transitions; never lift a geometry-shortened distance to the minimum. */
    if(next.distance>maximum)next.distance=maximum;
    if(next.distance==maximum&&next.velocity>0)next.velocity=0;
    if(!in.paused&&in.third_person&&in.dt>0){
        if(in.input_allowed&&in.wheel_notches!=0){
            double wanted=(double)next.desired_distance-(double)in.wheel_notches*.8;
            next.desired_distance=wanted<PE_SANDBOX_CAMERA_MIN_DISTANCE?PE_SANDBOX_CAMERA_MIN_DISTANCE:
                wanted>maximum?maximum:(float)wanted;
            phase.hold_seconds=0;
        }
        if(phase.hold_seconds>0)phase.hold_seconds-=bound(in.dt,PE_CAMERA_HOLD_DT_MIN,PE_CAMERA_HOLD_DT_MAX);
        /* Graph order: a previous GeometryClamp overrides a same-frame wheel
         * unlock, then clears its boolean latch before the new geometry pass. */
        if(phase.clamp_pending){phase.hold_seconds=PE_CAMERA_CLAMP_HOLD;phase.clamp_pending=false;}
        float max_distance=bound(activity*2,PE_SANDBOX_CAMERA_MIN_DISTANCE,maximum);
        float coefficient=bound(activity*.15f,.15f,1);
        float target=bound(next.desired_distance*coefficient,PE_SANDBOX_CAMERA_MIN_DISTANCE,max_distance);
        if(phase.hold_seconds<=0)next.distance=damp(next.distance,target,&next.velocity,in.dt);
        else if(next.distance<PE_SANDBOX_CAMERA_MIN_DISTANCE)
            next.distance=damp(next.distance,PE_SANDBOX_CAMERA_MIN_DISTANCE,&next.velocity,in.dt);
        if(!environment_valid(&next))return false;
    }
    *c=next;*r=phase;return true;
}
bool pe_sandbox_camera_feedback(PeSandboxCamera *c,PeSandboxCameraRecovery *r,float distance,bool clamped){
    if(!environment_valid(c)||!recovery_valid(r)||!isfinite(distance)||distance<0||distance>PE_SANDBOX_CAMERA_BUILD_MAX)return false;
    if(clamped){c->distance=distance;c->velocity=0;r->clamp_pending=true;}
    return true;
}

void pe_sandbox_camera_controls_init(PeSandboxCameraControls *c) {
    if(c)*c=(PeSandboxCameraControls){.mode=PE_SANDBOX_CAMERA_FREE};
}
void pe_sandbox_camera_controls_cancel(PeSandboxCameraControls *c) {
    if(c){c->right_tracking=false;c->right_dragged=false;c->right_seconds=0;c->right_motion=0;}
}
bool pe_sandbox_camera_controls_step(PeSandboxCameraControls *c,
    PeSandboxCameraControlInput in,PeSandboxCameraControlResult *out) {
    if(!c||!out||c->mode<PE_SANDBOX_CAMERA_FREE||c->mode>PE_SANDBOX_CAMERA_FIRST_PERSON||
       !isfinite(c->right_seconds)||c->right_seconds<0||!isfinite(c->right_motion)||c->right_motion<0||
       !isfinite(in.dt)||in.dt<0||in.dt>1||!isfinite(in.mouse_dx)||!isfinite(in.mouse_dy))return false;
    PeSandboxCameraControls next=*c;
    PeSandboxCameraControlResult result={.world_input=in.focused&&in.input_allowed};
    if(!result.world_input)pe_sandbox_camera_controls_cancel(&next);
    else {
        if(in.cycle_pressed){
            next.mode=(PeSandboxCameraMode)((next.mode+1)%3);
            result.cycle_accepted=true;
            pe_sandbox_camera_controls_cancel(&next);
        } else {
            bool began=false;
            if(in.right_pressed&&(in.captured||!in.pointer_over_ui)){
                pe_sandbox_camera_controls_cancel(&next);next.right_tracking=true;began=true;
            }
            if(next.right_tracking){
                /* Ignore press-frame motion (possibly preceding the press).
                 * Saturation avoids overflow during a very long held gesture. */
                if(!began){
                    next.right_seconds=fminf(.25f,next.right_seconds+in.dt);
                    double motion=hypot((double)in.mouse_dx,(double)in.mouse_dy);
                    next.right_motion=(float)fmin(3.,next.right_motion+motion);
                    if(next.right_motion>=3)next.right_dragged=true;
                }
                if(in.right_released){
                    result.right_click=next.right_seconds<.25f&&!next.right_dragged;
                    pe_sandbox_camera_controls_cancel(&next);
                }else if(!in.right_down)pe_sandbox_camera_controls_cancel(&next);
            }
        }
    }
    result.third_person=next.mode!=PE_SANDBOX_CAMERA_FIRST_PERSON;
    bool free_cursor=in.free_cursor&&!result.cycle_accepted;
    result.capture=result.world_input&&(in.captured||!in.pointer_over_ui)&&
        (next.mode==PE_SANDBOX_CAMERA_FREE?(next.right_tracking&&in.right_down&&
            (next.right_dragged||next.right_seconds>=.25f)):!free_cursor);
    result.look=result.capture&&in.captured;
    result.pointer_cursor=!result.capture;
    *c=next;*out=result;return true;
}
