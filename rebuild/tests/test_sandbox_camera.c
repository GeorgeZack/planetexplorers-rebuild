#include "pe_sandbox_camera.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static PeSandboxCameraInput frame(float dt) {
    return (PeSandboxCameraInput){.dt=dt,.third_person=true,.input_allowed=true};
}
static void close_to(float value,float expected,float tolerance) {
    if(fabsf(value-expected)>tolerance)fprintf(stderr,"camera: %.9g != %.9g\n",value,expected);
    assert(fabsf(value-expected)<=tolerance);
}
static void control_tests(void) {
    PeSandboxCameraControls c;pe_sandbox_camera_controls_init(&c);
    PeSandboxCameraControlInput in={.dt=.016f,.focused=true,.input_allowed=true};
    PeSandboxCameraControlResult out={0};
    assert(c.mode==PE_SANDBOX_CAMERA_FREE);
    assert(pe_sandbox_camera_controls_step(&c,in,&out));
    assert(out.third_person&&out.world_input&&out.pointer_cursor&&!out.capture&&!out.look);
    /* Keyboard movement/actions are independent of pointer capture/hover. */
    in.pointer_over_ui=true;assert(pe_sandbox_camera_controls_step(&c,in,&out));
    assert(out.world_input&&!out.capture);
    in.right_pressed=in.right_down=true;assert(pe_sandbox_camera_controls_step(&c,in,&out));
    assert(!c.right_tracking&&!out.capture);
    in.pointer_over_ui=false;in.right_pressed=false;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(!out.capture); /* no inherited UI-held drag */
    in.right_down=false;in.right_released=true;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(!out.right_click);
    in.right_released=false;in.right_pressed=in.right_down=true;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));
    assert(!out.capture&&!out.look&&!out.right_click&&out.pointer_cursor);
    in.right_pressed=false;in.mouse_dx=3;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));
    assert(out.capture&&!out.look&&c.right_dragged); /* exact drag threshold */
    in.captured=true;in.mouse_dx=0;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(out.look);
    in.right_down=false;in.right_released=true;in.mouse_dx=0;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));
    assert(!out.capture&&!out.look&&!out.right_click&&out.world_input);
    /* A genuine short click is emitted only on release, not on press. */
    in.captured=false;in.right_released=false;in.right_pressed=in.right_down=true;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(!out.right_click&&!out.capture&&out.pointer_cursor);
    in.right_pressed=in.right_down=false;in.right_released=true;in.dt=.249f;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(out.right_click&&!out.capture&&out.pointer_cursor);
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(!out.right_click); /* no repeated release */
    in.captured=false;in.right_released=false;in.right_pressed=in.right_down=true;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));
    in.right_pressed=in.right_down=false;in.right_released=true;in.dt=.25f;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(!out.right_click); /* strict tutorial cutoff */
    /* A stationary hold leaves the original pointer untouched until exactly
     * .25s, then requests capture without applying a capture-frame warp. */
    in=(PeSandboxCameraControlInput){.dt=.01f,.focused=true,.input_allowed=true,.right_pressed=true,.right_down=true};
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(!out.capture);
    in.right_pressed=false;in.dt=.249f;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(!out.capture&&out.pointer_cursor);
    in.dt=.001f;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(out.capture&&!out.look&&!out.pointer_cursor);
    in.captured=true;in.dt=.01f;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(out.look);
    in.right_down=false;in.right_released=true;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(!out.capture&&!out.right_click);
    /* Back-and-forth movement still counts as a drag; net displacement zero
     * does not turn an orbit into an interaction or stow. */
    in=(PeSandboxCameraControlInput){.dt=.01f,.focused=true,.input_allowed=true,.right_pressed=true,.right_down=true};
    assert(pe_sandbox_camera_controls_step(&c,in,&out));
    in.right_pressed=false;in.mouse_dx=1.5f;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(!c.right_dragged&&!out.capture);
    in.mouse_dx=-1.5f;in.right_down=false;in.right_released=true;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(!out.right_click);
    /* F5 follows tutorial 0->1->2->0 and clears the effective Tab override. */
    in=(PeSandboxCameraControlInput){.dt=.01f,.focused=true,.input_allowed=true,.free_cursor=true,.cycle_pressed=true};
    assert(pe_sandbox_camera_controls_step(&c,in,&out));
    assert(c.mode==PE_SANDBOX_CAMERA_LOCKED&&out.third_person&&out.capture&&out.cycle_accepted&&!out.look);
    in.free_cursor=false;in.captured=true;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));
    assert(c.mode==PE_SANDBOX_CAMERA_FIRST_PERSON&&!out.third_person&&out.capture);
    assert(pe_sandbox_camera_controls_step(&c,in,&out));
    assert(c.mode==PE_SANDBOX_CAMERA_FREE&&out.third_person&&!out.capture);
    assert(pe_sandbox_camera_controls_step(&c,in,&out));
    in.cycle_pressed=false;in.free_cursor=true;
    assert(pe_sandbox_camera_controls_step(&c,in,&out));
    assert(!out.capture&&out.world_input&&out.pointer_cursor);
    /* UI press/hold/release ownership and focus outrank every camera mode. */
    for(int mode=0;mode<3;mode++){
        c=(PeSandboxCameraControls){.mode=(PeSandboxCameraMode)mode};
        in=(PeSandboxCameraControlInput){.dt=.01f,.focused=true,.input_allowed=true,.right_pressed=true,.right_down=true};
        assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(c.right_tracking);
        in.right_pressed=false;in.input_allowed=false;in.cycle_pressed=true;
        assert(pe_sandbox_camera_controls_step(&c,in,&out));
        assert(!out.capture&&!out.world_input&&!out.cycle_accepted&&!c.right_tracking&&c.mode==(PeSandboxCameraMode)mode);
        in.right_down=false;in.right_released=true;
        assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(!out.right_click&&!out.capture);
        in.input_allowed=true;in.focused=false;
        assert(pe_sandbox_camera_controls_step(&c,in,&out));assert(!out.world_input&&!out.capture);
    }
    /* A lost release and explicit host cancellation cannot leave a latent click. */
    pe_sandbox_camera_controls_init(&c);
    in=(PeSandboxCameraControlInput){.dt=.01f,.focused=true,.input_allowed=true,.right_pressed=true,.right_down=true};
    assert(pe_sandbox_camera_controls_step(&c,in,&out));
    in.right_pressed=in.right_down=false;assert(pe_sandbox_camera_controls_step(&c,in,&out));
    assert(!c.right_tracking&&!out.capture);
    in.right_pressed=in.right_down=true;assert(pe_sandbox_camera_controls_step(&c,in,&out));
    pe_sandbox_camera_controls_cancel(&c);assert(!c.right_tracking);
    PeSandboxCameraControls before=c;PeSandboxCameraControlResult previous=out;
    in.dt=NAN;assert(!pe_sandbox_camera_controls_step(&c,in,&out));
    assert(!memcmp(&c,&before,sizeof c)&&!memcmp(&out,&previous,sizeof out));
    in.dt=.01f;in.mouse_dx=INFINITY;assert(!pe_sandbox_camera_controls_step(&c,in,&out));
    assert(!memcmp(&c,&before,sizeof c)&&!memcmp(&out,&previous,sizeof out));
    assert(!pe_sandbox_camera_controls_step(NULL,in,&out));
    assert(!pe_sandbox_camera_controls_step(&c,in,NULL));
    pe_sandbox_camera_controls_init(NULL);pe_sandbox_camera_controls_cancel(NULL);
}
int main(void) {
    control_tests();
    /* Values invoked directly from installed Unity5.2.4f1 managed six-argument
     * SmoothDamp; regenerate with tools/reference_smoothdamp.c. Assembly hash:
     * f728161940fbb8f27659c979bfcabe3f544002858779abfaa1143065926c051b
     * These are independent oracle outputs, not an implementation mirror. */
    const struct {float current,target,velocity,dt;int steps;float result,speed;} oracle[]={
        {6,5.19999981f,0,.0166666675f,1,5.99835110f,-.199587852f},
        {6,5.19999981f,0,.0166666675f,30,5.52543783f,-.867835462f},
        {6,5.19999981f,0,.0166666675f,60,5.27354860f,-.235355705f},
        {6,10,0,.100000001f,1,6.24624634f,4.29000425f},
        {6,1.79999995f,0,.00100000005f,1,5.99996758f,-.0669317544f},
        {6,20,0,1,1,17.4747467f,8.08080864f},
        {6,5.19999981f,-100,.100000001f,1,5.19999981f,0},
        {6,6,12,.100000001f,1,6.80437565f,4.82625484f},
        {6,10,-3,.0329999998f,1,5.94444418f,-.431237072f},
        {6,10,0,.100000001f,10,9.63372421f,1.17208099f},
        {6,10,0,.0333333351f,30,9.63169003f,1.17859364f},
        {6,10,0,.00833333377f,120,9.63284874f,1.17488623f}
    };
    for(size_t k=0;k<sizeof oracle/sizeof oracle[0];k++) {
        PeSandboxCamera c={oracle[k].target,oracle[k].current,oracle[k].velocity};
        PeSandboxCameraInput in=frame(oracle[k].dt);in.building=true;
        for(int t=0;t<oracle[k].steps;t++)assert(pe_sandbox_camera_step(&c,in));
        close_to(c.distance,oracle[k].result,2e-5f);close_to(c.velocity,oracle[k].speed,5e-5f);
    }
    PeSandboxCamera c;pe_sandbox_camera_init(&c);
    assert(c.distance==6&&c.desired_distance==6&&c.velocity==0);
    PeSandboxCameraInput in=frame(1.f/60);in.wheel_notches=1;
    assert(pe_sandbox_camera_step(&c,in));close_to(c.desired_distance,5.2f,1e-6f);
    close_to(c.distance,5.99835110f,1e-6f); /* smooth, not immediate .8m jump */
    PeSandboxCamera frozen=c;
    in.paused=true;assert(pe_sandbox_camera_step(&c,in));assert(!memcmp(&c,&frozen,sizeof c));
    in.paused=false;in.dt=0;assert(pe_sandbox_camera_step(&c,in));assert(!memcmp(&c,&frozen,sizeof c));
    in=frame(.1f);in.third_person=false;in.wheel_notches=-5;
    assert(pe_sandbox_camera_step(&c,in));assert(!memcmp(&c,&frozen,sizeof c));
    in.third_person=true;in.input_allowed=false;
    assert(pe_sandbox_camera_step(&c,in));assert(c.desired_distance==frozen.desired_distance&&c.distance<frozen.distance);
    /* Input-blocked smoothing is identical to a frame with no scroll. */
    PeSandboxCamera expected=frozen;in.input_allowed=true;in.wheel_notches=0;
    assert(pe_sandbox_camera_step(&expected,in));assert(!memcmp(&c,&expected,sizeof c));
    in=frame(.1f);in.wheel_notches=FLT_MAX;
    assert(pe_sandbox_camera_step(&c,in));assert(c.desired_distance==PE_SANDBOX_CAMERA_MIN_DISTANCE);
    in.wheel_notches=-FLT_MAX;assert(pe_sandbox_camera_step(&c,in));assert(c.desired_distance==10);
    in.building=true;assert(pe_sandbox_camera_step(&c,in));assert(c.desired_distance==20);
    for(int k=0;k<200;k++){in.wheel_notches=0;assert(pe_sandbox_camera_step(&c,in));assert(c.distance<=20&&c.distance>=1.8f);}
    assert(c.distance>19.999f);
    /* Leaving build clamps desired and actual bounds even if the camera is
     * paused or in first person. Outward momentum cannot undo that clamp. */
    c=(PeSandboxCamera){20,19,5};in.building=false;in.paused=true;
    assert(pe_sandbox_camera_step(&c,in));assert(c.distance==10&&c.desired_distance==10&&c.velocity==0);
    c=(PeSandboxCamera){20,10,-2};assert(pe_sandbox_camera_step(&c,in));assert(c.velocity==-2);
    in.paused=false;in.third_person=false;c=(PeSandboxCamera){20,19,5};
    assert(pe_sandbox_camera_step(&c,in));assert(c.distance==10&&c.desired_distance==10&&c.velocity==0);
    /* Reversal converges without overshooting the requested distance. */
    c=(PeSandboxCamera){1.8f,6,-4};in=frame(.1f);
    for(int k=0;k<100;k++){assert(pe_sandbox_camera_step(&c,in));assert(c.distance>=1.8f&&c.distance<=6);}
    close_to(c.distance,1.8f,1e-5f);
    frozen=c;
    const float bad[]={NAN,INFINITY,-INFINITY,-.01f,1.01f};
    for(size_t k=0;k<sizeof bad/sizeof bad[0];k++) {
        in=frame(bad[k]);assert(!pe_sandbox_camera_step(&c,in));assert(!memcmp(&c,&frozen,sizeof c));
    }
    in=frame(.1f);in.wheel_notches=NAN;assert(!pe_sandbox_camera_step(&c,in));assert(!memcmp(&c,&frozen,sizeof c));
    c=(PeSandboxCamera){6,6,FLT_MAX};frozen=c;in=frame(1);
    assert(!pe_sandbox_camera_step(&c,in));assert(!memcmp(&c,&frozen,sizeof c));
    c=(PeSandboxCamera){NAN,6,0};assert(!pe_sandbox_camera_step(&c,frame(.1f)));
    assert(!pe_sandbox_camera_step(NULL,frame(.1f)));pe_sandbox_camera_init(NULL);
    assert((PE_SANDBOX_CAMERA_OBSTACLE_MASK&((1u<<14)|(1u<<19)|(1u<<21)))==0);
    assert((PE_SANDBOX_CAMERA_OBSTACLE_MASK&(1u<<12))!=0);
    puts("sandbox camera: free/locked/first-person input, RMB gestures, UI/focus gates, Unity5.2 zoom and tree-excluding mask passed");
    return 0;
}
