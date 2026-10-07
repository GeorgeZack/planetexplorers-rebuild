#include "pe_sandbox_camera.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {int mode,calls;float distance;PeVec3 anchor;} Query;
static bool ray(void *context,PeVec3 origin,PeVec3 dir,float maximum,uint32_t mask,float *distance){
    Query *q=context;q->calls++;
    assert(!memcmp(&origin,&q->anchor,sizeof origin));
    assert(mask==8468481u&&maximum==50);
    assert(fabsf(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z-1)<.00001f);
    assert(dir.y>=-.382685f); /* Source skips every pitch below -35deg. */
    if(q->mode==3&&q->calls==20)return false;
    if(q->mode==4){*distance=NAN;return true;}
    if(q->mode==5){*distance=51;return true;}
    if(q->mode==1){*distance=q->distance;return true;}
    if(q->mode==2){ /* Two infinite walls at +/-1m, all other directions clear. */
        *distance=fabsf(dir.x)>1.f/50?1/fabsf(dir.x):50;return true;
    }
    *distance=maximum;return true;
}
static PeSandboxCameraInput frame(float dt){return(PeSandboxCameraInput){.dt=dt,.third_person=true,.input_allowed=true};}
static void close_to(float a,float b,float tolerance){if(fabsf(a-b)>tolerance)fprintf(stderr,"camera environment %.9g != %.9g\n",a,b);assert(fabsf(a-b)<=tolerance);}
static void activity_tests(void){
    Query q={.anchor={10,20,30}};float result=-1;
    assert(pe_sandbox_camera_activity(q.anchor,ray,&q,&result));assert(q.calls==66&&result==50);
    q.calls=0;q.mode=1;q.distance=4;assert(pe_sandbox_camera_activity(q.anchor,ray,&q,&result)&&result==4&&q.calls==66);
    q.calls=0;q.distance=.1f;assert(pe_sandbox_camera_activity(q.anchor,ray,&q,&result)&&result==.1f); /* min=.5 argument unused */
    q.calls=0;q.distance=0;assert(pe_sandbox_camera_activity(q.anchor,ray,&q,&result)&&result==0);
    q.calls=0;q.mode=2;assert(pe_sandbox_camera_activity(q.anchor,ray,&q,&result));assert(result>1&&result<10);
    for(int mode=3;mode<=5;mode++){q.calls=0;q.mode=mode;result=123;assert(!pe_sandbox_camera_activity(q.anchor,ray,&q,&result)&&result==123);}
    q.mode=0;assert(!pe_sandbox_camera_activity((PeVec3){NAN,0,0},ray,&q,&result));
    assert(!pe_sandbox_camera_activity(q.anchor,NULL,&q,&result));assert(!pe_sandbox_camera_activity(q.anchor,ray,&q,NULL));
}
static void target_tests(void){
    PeSandboxCamera c,old;PeSandboxCameraRecovery r;
    pe_sandbox_camera_init(&c);old=c;old.desired_distance=1.8f;pe_sandbox_camera_recovery_init(&r);
    for(int i=0;i<60;i++){
        assert(pe_sandbox_camera_follow_step(&c,&r,frame(1.f/60),1));
        assert(pe_sandbox_camera_step(&old,frame(1.f/60)));
        assert(c.distance==old.distance&&c.velocity==old.velocity);
    }
    assert(c.desired_distance==6); /* Activity changes target, never user's level. */
    for(int i=0;i<180;i++)assert(pe_sandbox_camera_follow_step(&c,&r,frame(1.f/60),50));
    close_to(c.distance,6,.001f);
    c=(PeSandboxCamera){.desired_distance=20,.distance=20};r=(PeSandboxCameraRecovery){0};
    PeSandboxCameraInput in=frame(.1f);in.building=true;
    for(int i=0;i<100;i++)assert(pe_sandbox_camera_follow_step(&c,&r,in,4));
    close_to(c.distance,8,.00001f); /* coef .6 yields12, source activity max8 wins. */
    in.building=false;in.paused=true;assert(pe_sandbox_camera_follow_step(&c,&r,in,50));assert(c.desired_distance==10&&c.distance<=10);
}
static void recovery_tests(void){
    PeSandboxCamera c;PeSandboxCameraRecovery r;pe_sandbox_camera_init(&c);pe_sandbox_camera_recovery_init(&r);
    assert(pe_sandbox_camera_feedback(&c,&r,2.5f,true));assert(c.distance==2.5f&&c.velocity==0&&r.clamp_pending);
    assert(pe_sandbox_camera_feedback(&c,&r,6,false));assert(r.clamp_pending&&c.distance==2.5f);
    PeSandboxCameraInput in=frame(.1f);in.wheel_notches=1;
    assert(pe_sandbox_camera_follow_step(&c,&r,in,50));assert(!r.clamp_pending&&r.hold_seconds==2&&c.distance==2.5f);
    close_to(c.desired_distance,5.2f,.00001f); /* prior clamp overrides same-frame wheel unlock */
    in.wheel_notches=0;
    for(int i=0;i<19;i++){assert(pe_sandbox_camera_follow_step(&c,&r,in,50));assert(c.distance==2.5f&&r.hold_seconds>0);}
    assert(pe_sandbox_camera_follow_step(&c,&r,in,50));assert(r.hold_seconds<=0&&c.distance>2.5f);
    assert(pe_sandbox_camera_feedback(&c,&r,2.5f,true));assert(pe_sandbox_camera_follow_step(&c,&r,in,50));
    in.wheel_notches=-1;assert(pe_sandbox_camera_follow_step(&c,&r,in,50));assert(r.hold_seconds==0&&c.distance>2.5f);
    /* Below-minimum recovery continues during lock; only the lock clock is
     * clamped. Independent .5s SmoothDamp(.5) result from its rational decay. */
    assert(pe_sandbox_camera_feedback(&c,&r,1,true));in=frame(.5f);
    assert(pe_sandbox_camera_follow_step(&c,&r,in,50));close_to(c.distance,1.4470588f,.000001f);close_to(c.velocity,.9411765f,.000001f);assert(r.hold_seconds==2);
    assert(pe_sandbox_camera_follow_step(&c,&r,in,50));close_to(r.hold_seconds,1.9f,.000001f);
    in.dt=.0001f;assert(pe_sandbox_camera_follow_step(&c,&r,in,50));close_to(r.hold_seconds,1.899f,.000001f);
    assert(pe_sandbox_camera_feedback(&c,&r,.8f,true));PeSandboxCamera before=c;PeSandboxCameraRecovery prior=r;
    in=frame(.016f);in.paused=true;in.wheel_notches=5;
    assert(pe_sandbox_camera_follow_step(&c,&r,in,50));assert(!memcmp(&before,&c,sizeof c)&&!memcmp(&prior,&r,sizeof r));
    in.paused=false;in.third_person=false;assert(pe_sandbox_camera_follow_step(&c,&r,in,50));assert(!memcmp(&before,&c,sizeof c)&&!memcmp(&prior,&r,sizeof r));
    in.third_person=true;in.dt=0;assert(pe_sandbox_camera_follow_step(&c,&r,in,50));assert(!memcmp(&before,&c,sizeof c)&&!memcmp(&prior,&r,sizeof r));
    in.dt=.016f;in.input_allowed=false;assert(pe_sandbox_camera_follow_step(&c,&r,in,50));assert(c.distance>.8f&&c.desired_distance==before.desired_distance&&r.hold_seconds==2&&!r.clamp_pending);
}
static void invalid_tests(void){
    PeSandboxCamera c;PeSandboxCameraRecovery r;pe_sandbox_camera_init(&c);pe_sandbox_camera_recovery_init(&r);
    PeSandboxCamera before=c;PeSandboxCameraRecovery prior=r;
    const float invalid[]={-1,NAN,INFINITY,50.01f};
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;i++){
        assert(!pe_sandbox_camera_follow_step(&c,&r,frame(.1f),invalid[i]));
        assert(!memcmp(&before,&c,sizeof c)&&!memcmp(&prior,&r,sizeof r));
    }
    assert(!pe_sandbox_camera_feedback(&c,&r,-1,true));assert(!pe_sandbox_camera_feedback(&c,&r,NAN,true));
    assert(!pe_sandbox_camera_feedback(&c,&r,21,true));assert(!memcmp(&before,&c,sizeof c)&&!memcmp(&prior,&r,sizeof r));
    c.velocity=FLT_MAX;before=c;assert(!pe_sandbox_camera_follow_step(&c,&r,frame(1),50));assert(!memcmp(&before,&c,sizeof c)&&!memcmp(&prior,&r,sizeof r));
    pe_sandbox_camera_init(&c);r.hold_seconds=3;prior=r;assert(!pe_sandbox_camera_follow_step(&c,&r,frame(.1f),50));assert(!memcmp(&r,&prior,sizeof r));
    assert(!pe_sandbox_camera_follow_step(NULL,&r,frame(.1f),50));assert(!pe_sandbox_camera_follow_step(&c,NULL,frame(.1f),50));
    assert(!pe_sandbox_camera_feedback(NULL,&r,1,true));assert(!pe_sandbox_camera_feedback(&c,NULL,1,true));pe_sandbox_camera_recovery_init(NULL);
}
int main(void){activity_tests();target_tests();recovery_tests();invalid_tests();puts("Camera environment: source activity sampling, targets, clamp latch and recovery passed");return 0;}
