#include "pe_human_phy.h"
#include "pe_repulsed.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static bool near(float a,float b){return fabsf(a-b)<=1e-5f+fabsf(b)*1e-5f;}
static bool near3(PeVec3 a,float x,float y,float z){return near(a.x,x)&&near(a.y,y)&&near(a.z,z);}
static PeHumanPhy fresh(void){PeHumanPhy p;memset(&p,0xA5,sizeof p);pe_human_phy_init(&p);return p;}
static PeHumanPhyFrame frame(float time,const PeVec3 *normals,size_t count){
    return (PeHumanPhyFrame){.time=time,.delta_time=1/60.f,.forward={0,0,1},
        .ground_normals=normals,.ground_normal_count=count};
}
static PeHumanPhyStep step(PeVec3 velocity){
    return (PeHumanPhyStep){.velocity=velocity,.position={3,5,7},.up={0,1,0},.fixed_delta_time=.02f};
}
/* Stand on flat ground past the source's initial half-second impact window. */
static PeHumanPhy standing(void){
    PeHumanPhy p=fresh();PeVec3 up={0,1,0};PeHumanPhyFrame f=frame(1,&up,1);
    assert(pe_human_phy_update(&p,&f)&&p.phy_ground&&p.grounded&&p.fall_ground);
    return p;
}
static void defaults_and_speed(void){
    PeHumanPhy p=fresh();
    assert(p.move_speed==5&&p.net_move_speed_scale==1&&p.gravity==-9.81000042f&&p.is_controller);
    assert(near3(p.gravity_up,0,1,0)&&!p.grounded&&!p.phy_ground&&p.fixed_updates==0&&p.drag==0);
    assert(near3(p.request_velocity,0,0,0)&&near3(p.applied_velocity,0,0,0));
    assert(pe_human_phy_reset_speed(&p,7)&&p.move_speed==7);
    assert(pe_human_phy_reset_speed(&p,0)&&p.move_speed==.100000001f); /* floored, never zero */
    assert(pe_human_phy_reset_speed(&p,-3)&&p.move_speed==.100000001f);
}
static void request_sum_clamp_and_latch(void){
    PeHumanPhy p=fresh();
    assert(pe_human_phy_apply_move_request(&p,(PeVec3){0,0,10}));
    assert(p.request_velocity.z==10); /* exactly MaxSubSpeed is not rescaled */
    assert(pe_human_phy_cancel_move_request(&p));
    /* Callers in one frame add before the clamp. */
    assert(pe_human_phy_apply_move_request(&p,(PeVec3){3,0,0}));
    assert(pe_human_phy_apply_move_request(&p,(PeVec3){0,0,4})&&near3(p.request_velocity,3,0,4));
    assert(pe_human_phy_apply_move_request(&p,(PeVec3){3,0,4})&&near3(p.request_velocity,6,0,8));
    assert(pe_human_phy_apply_move_request(&p,(PeVec3){6,0,8})&&near3(p.request_velocity,6,0,8));
    /* The clamp acts on the running sum, so an opposing request subtracts from 10. */
    assert(pe_human_phy_apply_move_request(&p,(PeVec3){-3,0,-4})&&near3(p.request_velocity,3,0,4));
    /* Pending is invisible to FixedUpdate until LateUpdate. */
    PeHumanPhyForces out;PeHumanPhy q=standing();
    assert(pe_human_phy_apply_move_request(&q,(PeVec3){0,0,16})&&near3(q.request_velocity,0,0,10));
    assert(pe_human_phy_fixed_update(&q,step((PeVec3){0}),&out)&&out.ran&&near3(out.velocity_change,0,0,0));
    assert(pe_human_phy_late_update(&q,(PeVec3){1,2,3}));
    assert(near3(q.inertia_velocity,1,2,3)&&near3(q.applied_velocity,0,0,10)&&near3(q.request_velocity,0,0,0));
    assert(pe_human_phy_fixed_update(&q,step((PeVec3){1,0,2}),&out)&&near3(out.velocity_change,-1,0,8));
    /* Without renewal the next LateUpdate drops it. */
    assert(pe_human_phy_late_update(&q,(PeVec3){0})&&near3(q.applied_velocity,0,0,0));
    /* Cancel clears pending and already-latched velocity together. */
    assert(pe_human_phy_apply_move_request(&q,(PeVec3){2,0,0})&&pe_human_phy_late_update(&q,(PeVec3){0}));
    assert(pe_human_phy_apply_move_request(&q,(PeVec3){5,0,0})&&pe_human_phy_cancel_move_request(&q));
    assert(near3(q.request_velocity,0,0,0)&&near3(q.applied_velocity,0,0,0));
}
static void fixed_update_counter_freeze_and_kinematic(void){
    PeHumanPhy p=standing();PeHumanPhyForces out;PeHumanPhyStep s=step((PeVec3){0});
    assert(pe_human_phy_fixed_update(&p,s,&out)&&out.ran&&p.fixed_updates==1);
    assert(pe_human_phy_fixed_update(&p,s,&out)&&out.ran&&p.fixed_updates==2);
    /* A third call before the next Update applies nothing, not even gravity. */
    assert(pe_human_phy_fixed_update(&p,s,&out)&&!out.ran&&p.fixed_updates==3);
    assert(near3(out.acceleration,0,0,0)&&near3(out.velocity_change,0,0,0));
    PeVec3 up={0,1,0};PeHumanPhyFrame f=frame(1.1f,&up,1);
    assert(pe_human_phy_update(&p,&f)&&p.fixed_updates==0&&!p.fall_ground);
    assert(pe_human_phy_fixed_update(&p,s,&out)&&out.ran);
    p.freeze_update=true;
    assert(pe_human_phy_fixed_update(&p,s,&out)&&!out.ran&&p.fixed_updates==1);
    p.freeze_update=false;
    /* Kinematic controller: velocity zeroed, no move, counter untouched. */
    s.rigidbody_kinematic=true;p.applied_velocity=(PeVec3){0,0,4};
    assert(pe_human_phy_fixed_update(&p,s,&out)&&out.ran&&out.kinematic&&out.zero_velocity);
    assert(!out.move_position&&p.fixed_updates==1&&near3(out.acceleration,0,0,0));
    p.is_controller=false;p.fixed_updates=9;
    assert(pe_human_phy_fixed_update(&p,s,&out)&&out.move_position&&p.fixed_updates==9);
    assert(near3(out.move_position_target,3,5,7.08f));
}
static void forces(void){
    PeHumanPhy p=standing();PeHumanPhyForces out;
    /* Grounded: one VelocityChange replaces the whole velocity; no drag. */
    p.applied_velocity=(PeVec3){0,0,6};
    assert(pe_human_phy_fixed_update(&p,step((PeVec3){1,-2,3}),&out));
    assert(near3(out.velocity_change,-1,2,3)&&near3(out.acceleration,0,-9.81000042f,0));
    assert(!out.set_position&&near3(out.position,3,5,7));
    /* The movement direction and request add, then scale by netMoveSpeedScale. */
    p.fixed_updates=0;p.desired_direction=(PeVec3){1,0,0};p.net_move_speed_scale=.5f;
    assert(pe_human_phy_fixed_update(&p,step((PeVec3){0}),&out)&&near3(out.velocity_change,2.5f,0,3));
    p.net_move_speed_scale=1;p.desired_direction=(PeVec3){0};
    /* Airborne: only the part on the plane of transform.up is replaced. */
    p.fixed_updates=0;p.phy_ground=false;
    assert(pe_human_phy_fixed_update(&p,step((PeVec3){1,-2,3}),&out)&&near3(out.velocity_change,-1,0,3));
    p.fixed_updates=0;p.applied_velocity=(PeVec3){0,4,6};
    assert(pe_human_phy_fixed_update(&p,step((PeVec3){0,-2,0}),&out)&&near3(out.velocity_change,0,0,6));
    p.fixed_updates=0;p.use_rope_gun=true;
    assert(pe_human_phy_fixed_update(&p,step((PeVec3){0,-2,0}),&out)&&near3(out.velocity_change,0,6,6));
    p.use_rope_gun=false;p.phy_ground=true;
    /* No desired velocity: quadratic drag opposes motion, nothing else. */
    p.fixed_updates=0;p.applied_velocity=(PeVec3){0};
    assert(pe_human_phy_fixed_update(&p,step((PeVec3){0,0,4}),&out));
    assert(near3(out.velocity_change,0,0,0)&&near3(out.acceleration,0,-9.81000042f,-1.5f*16));
    /* The stop snap needs a nonzero sub-acceleration, which the female prefab lacks. */
    p.fixed_updates=0;
    assert(pe_human_phy_fixed_update(&p,step((PeVec3){0,0,.04f}),&out)&&near3(out.velocity_change,0,0,0));
    p.fixed_updates=0;p.sub_acc=(PeVec3){0,.1f,0};
    assert(pe_human_phy_fixed_update(&p,step((PeVec3){0,0,.04f}),&out)&&near3(out.velocity_change,0,0,-.04f));
    p.sub_acc=(PeVec3){0};
    /* Above 100 m/s the request is discarded for a full stop; the 50 m/s
     * limiter then adds its own correction in the same call. */
    p.fixed_updates=0;p.applied_velocity=(PeVec3){0,0,6};
    assert(pe_human_phy_fixed_update(&p,step((PeVec3){0,0,120}),&out)&&near3(out.velocity_change,0,0,-190));
    p.fixed_updates=0;
    assert(pe_human_phy_fixed_update(&p,step((PeVec3){0,0,60}),&out)&&near3(out.velocity_change,0,0,-64));
    /* Below-world rescue. */
    p.fixed_updates=0;p.applied_velocity=(PeVec3){0};
    PeHumanPhyStep fallen=step((PeVec3){0,-3,0});fallen.position.y=-10001;
    assert(pe_human_phy_fixed_update(&p,fallen,&out)&&out.set_position);
    assert(near3(out.position,3,-10,7)&&near3(out.velocity_change,0,3,0));
}
static void ground_water_and_drag(void){
    PeHumanPhy p=fresh();PeVec3 up={0,1,0};PeHumanPhyFrame f=frame(.25f,&up,1);
    /* Time.time-0 inside the leave window counts as impacting at startup. */
    assert(pe_human_phy_update(&p,&f)&&!p.grounded&&!p.phy_ground&&p.drag==.00600000005f);
    f.time=.5f;assert(pe_human_phy_update(&p,&f)&&p.grounded&&p.fall_ground&&p.drag==1.5f);
    /* Contacts gone: physical ground drops at once, grounded lingers under .5 s. */
    f=frame(.9f,NULL,0);assert(pe_human_phy_update(&p,&f)&&!p.phy_ground&&p.grounded&&!p.fall_ground);
    f.time=1.0f;assert(pe_human_phy_update(&p,&f)&&!p.grounded&&p.drag==.00600000005f);
    /* A kinematic controller body counts as physical ground without contacts. */
    f=frame(1.1f,NULL,0);f.rigidbody_kinematic=true;
    assert(pe_human_phy_update(&p,&f)&&p.phy_ground&&p.grounded&&p.fall_ground);
    assert(pe_human_phy_apply_impact(&p,1.1f)&&!p.grounded&&!p.phy_ground);
    f.time=1.5f;assert(pe_human_phy_update(&p,&f)&&!p.grounded);
    f.time=1.6f;assert(pe_human_phy_update(&p,&f)&&p.grounded);
    /* Gravity follows normals within 60 degrees of up; steeper ones only tilt the slope angle. */
    PeVec3 normals[2]={{0,.8f,.6f},{.9f,.43588989f,0}};
    f=frame(2,normals,2);f.forward=(PeVec3){0,0,1};
    assert(pe_human_phy_update(&p,&f)&&near3(p.gravity_up,0,.8f,.6f));
    PeHumanPhyForces out;
    assert(pe_human_phy_fixed_update(&p,step((PeVec3){0}),&out));
    assert(near3(out.acceleration,0,-9.81000042f*.8f,-9.81000042f*.6f));
    f=frame(2.1f,normals,1);
    assert(pe_human_phy_update(&p,&f)&&near(p.forward_ground_angle,-36.869898f));
    f.forward=(PeVec3){0,0,-1};
    assert(pe_human_phy_update(&p,&f)&&near(p.forward_ground_angle,36.869898f));
    f=frame(2.2f,NULL,0);f.delta_time=.1f; /* decays by Lerp(angle,0,3*dt) without contacts */
    assert(pe_human_phy_update(&p,&f)&&near(p.forward_ground_angle,36.869898f*.7f)&&near3(p.gravity_up,0,1,0));
    /* Water: the probe index follows the grounded state just computed. */
    f=frame(3,&up,1);f.position=(PeVec3){0,10,0};f.point_in_water[0]=1;
    assert(pe_human_phy_update(&p,&f)&&p.grounded&&!p.feet_in_water);
    f.time=3.1f;f.point_in_water[1]=1;f.water_ray_hit=true;f.water_ray_y=11.2f;
    assert(pe_human_phy_update(&p,&f)&&p.feet_in_water&&!p.spine_in_water&&near(p.water_to_root,1.2f));
    f.time=3.2f;f.water_ray_y=11.6f;
    assert(pe_human_phy_update(&p,&f)&&p.spine_in_water&&!p.head_in_water&&near3(p.water_acc,0,9.81000042f,0));
    assert(p.drag==1.5f); /* grounded outranks water */
    f.time=3.3f;f.water_ray_hit=false; /* no surface found: treated as fully submerged */
    assert(pe_human_phy_update(&p,&f)&&p.head_in_water&&p.water_to_root==1.75f);
    /* Ground state reads the previous frame's spine flag, so contacts are ignored once. */
    assert(near3(p.gravity_up,0,1,0));
    f=frame(4,NULL,0);f.position=(PeVec3){0,10,0};f.point_in_water[0]=1;f.water_ray_hit=true;f.water_ray_y=11.6f;
    assert(pe_human_phy_update(&p,&f)&&!p.grounded&&p.spine_in_water&&p.drag==1.29999995f);
    /* Rising near the surface: upward demand removed and the root held below it. */
    f.time=4.1f;f.water_ray_y=11.55f;assert(pe_human_phy_update(&p,&f)&&near(p.water_to_root,1.55f));
    p.applied_velocity=(PeVec3){0,2,3};PeHumanPhyStep s=step((PeVec3){0});s.position=(PeVec3){0,10,0};
    assert(pe_human_phy_fixed_update(&p,s,&out)&&near3(out.velocity_change,0,0,3));
    assert(out.set_position&&near3(out.position,0,10,0)); /* held at water-1.5-.05 */
    s.position.y=9.9f;p.fixed_updates=0;
    assert(pe_human_phy_fixed_update(&p,s,&out)&&out.set_position&&near3(out.position,0,10,0));
    /* A root already a spine height above that level is left alone; the demand is still removed. */
    s.position.y=11.6f;p.fixed_updates=0;
    assert(pe_human_phy_fixed_update(&p,s,&out)&&!out.set_position&&near3(out.velocity_change,0,0,3));
    /* Not while a positive sub-acceleration is lifting the body. */
    p.sub_acc=(PeVec3){0,1,0};p.fixed_updates=0;p.phy_ground=false;
    assert(pe_human_phy_fixed_update(&p,s,&out)&&!out.set_position&&near3(out.velocity_change,0,2,3));
}
static void repulsed_requests(void){
    /* One frame: Repulsed Update requests, LateUpdate latches, the next
     * frame's FixedUpdate consumes. Force 750 asks for 16 m/s and is held to 10. */
    PeHumanPhy p=standing();PeRepulsed a={0};PeRepulsedUpdate u;PeHumanPhyForces out;
    PeRepulsedStart in={.force=750,.projected_direction={0,0,1}};
    PeRepulsedTick t={.delta_time=1/60.f,.context_present=true,.human_phy_present=true};
    assert(pe_repulsed_begin(&a,in,&u)&&!u.apply_move_request);
    assert(pe_repulsed_step(&a,t,&u)&&u.apply_move_request&&u.move_request.z>10);
    assert(pe_human_phy_apply_move_request(&p,u.move_request)&&near3(p.request_velocity,0,0,10));
    assert(pe_human_phy_late_update(&p,(PeVec3){0}));
    assert(pe_human_phy_fixed_update(&p,step((PeVec3){0}),&out)&&near3(out.velocity_change,0,0,10));
    /* Force 400 stays under the clamp. */
    PeRepulsed b={0};in.force=400;PeHumanPhy q=standing();
    assert(pe_repulsed_begin(&b,in,&u)&&pe_repulsed_step(&b,t,&u)&&u.move_request.z<=9);
    assert(pe_human_phy_apply_move_request(&q,u.move_request)&&q.request_velocity.z==u.move_request.z);
    /* Cleanup cancels both halves; the same LateUpdate latches zero. */
    assert(pe_human_phy_late_update(&q,(PeVec3){0})&&pe_repulsed_cancel(&b,t,&u)&&u.cancel_move_request);
    assert(pe_human_phy_cancel_move_request(&q)&&pe_human_phy_late_update(&q,(PeVec3){0,0,5}));
    assert(pe_human_phy_fixed_update(&q,step((PeVec3){0,0,5}),&out));
    assert(near3(out.velocity_change,0,0,0)&&near(out.acceleration.z,-1.5f*25));
}
static void invalid_input(void){
    PeHumanPhy p=standing(),before=p;PeHumanPhyForces out,original;
    memset(&out,0x5A,sizeof out);original=out;
    PeHumanPhyStep s=step((PeVec3){0});s.velocity.x=NAN;
    assert(!pe_human_phy_fixed_update(&p,s,&out)&&!memcmp(&p,&before,sizeof p)&&!memcmp(&out,&original,sizeof out));
    s=step((PeVec3){0});s.fixed_delta_time=-1;
    assert(!pe_human_phy_fixed_update(&p,s,&out)&&!pe_human_phy_fixed_update(&p,step((PeVec3){0}),NULL));
    assert(!pe_human_phy_fixed_update(NULL,step((PeVec3){0}),&out));
    assert(!pe_human_phy_apply_move_request(&p,(PeVec3){INFINITY,0,0})&&!pe_human_phy_apply_move_request(NULL,(PeVec3){0}));
    assert(!pe_human_phy_reset_speed(&p,NAN)&&!pe_human_phy_apply_impact(&p,NAN));
    assert(!pe_human_phy_late_update(&p,(PeVec3){0,NAN,0})&&!pe_human_phy_cancel_move_request(NULL));
    PeHumanPhyFrame f=frame(2,NULL,1); /* count without storage */
    assert(!pe_human_phy_update(&p,&f)&&!pe_human_phy_update(&p,NULL));
    PeVec3 bad={NAN,1,0};f=frame(2,&bad,1);assert(!pe_human_phy_update(&p,&f));
    f=frame(2,NULL,0);f.delta_time=-1;assert(!pe_human_phy_update(&p,&f));
    assert(!memcmp(&p,&before,sizeof p));
    p.drag=NAN;assert(!pe_human_phy_fixed_update(&p,step((PeVec3){0}),&out)&&!memcmp(&out,&original,sizeof out));
}
int pe_test_human_phy(void){
    defaults_and_speed();request_sum_clamp_and_latch();fixed_update_counter_freeze_and_kinematic();
    forces();ground_water_and_drag();repulsed_requests();invalid_input();
    return 0;
}
