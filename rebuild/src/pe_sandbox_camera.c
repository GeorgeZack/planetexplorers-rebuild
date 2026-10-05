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
