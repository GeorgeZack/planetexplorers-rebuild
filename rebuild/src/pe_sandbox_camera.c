#include "pe_sandbox_camera.h"
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
