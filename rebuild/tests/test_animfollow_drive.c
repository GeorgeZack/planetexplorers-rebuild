#include "pe_animfollow_drive.h"
#include "pe_animfollow.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned failures;
#define CHECK(v) do{if(!(v)){fprintf(stderr,"animfollow drive:%d %s\n",__LINE__,#v);failures++;}}while(0)
#define NEAR(a,b) CHECK(fabsf((a)-(b))<.001f)
static PeAnimFollowDriveTick tick(void){
    PeAnimFollowDriveTick in={.active=true,.secondary_update=2,.torque_cap=100,.force_cap=100,
        .joint_cap=80,.joint_damping=.5f,.drag=17,.angular_drag=35};
    for(unsigned i=0;i<PE_AF_DRIVE_BODIES;i++)in.bodies[i].axis=(PeVec3){1,0,0};
    return in;
}
int pe_test_animfollow_drive(void){
    failures=0;PeAnimFollowDrive s;CHECK(pe_animfollow_drive_init(&s,.02f));CHECK(s.reciprocal_fixed_delta_time==50);
    CHECK(s.joint_mask==2046&&s.joint_damping==.5f&&s.drive_damper==0&&s.drive_spring==10000);
    PeAnimFollowDriveBinding b;CHECK(pe_animfollow_drive_binding(0,&b)&&b.rigidbody==299397&&b.master_transform==150806&&b.joint==0);
    CHECK(pe_animfollow_drive_binding(10,&b)&&b.rigidbody==299352&&b.joint==309755&&b.slave_transform==135803);
    PeAnimFollowDriveBinding before=b;CHECK(!pe_animfollow_drive_binding(11,&b)&&!memcmp(&before,&b,sizeof b));
    PeAnimFollowDriveTick in=tick();in.bodies[0].slave_position=(PeVec3){10,20,30};in.bodies[0].master_position=(PeVec3){13,24,30};
    in.bodies[1].master_position=(PeVec3){3,4,0};in.bodies[1].angle=270;s.torque_last_error[0]=(PeVec3){5,0,0};in.bodies[0].angle=360;
    PeAnimFollowDriveUpdate u;CHECK(pe_animfollow_drive_step(&s,in,&u));CHECK(s.frame_counter==1&&u.driven&&u.capture_nonrigid&&u.write_nonrigid&&!u.blend_nonrigid);
    CHECK(u.nonrigid_begin==2&&u.nonrigid_end==65);NEAR(u.bodies[0].force.x,60);NEAR(u.bodies[0].force.y,80);
    NEAR(u.bodies[1].force.x,12);NEAR(u.bodies[1].force.y,16);NEAR(u.bodies[1].torque.x,-288.5166f);
    CHECK(u.bodies[0].force_position.x==10&&u.bodies[0].force_position.z==30&&u.bodies[0].force_mode==2);
    CHECK(u.bodies[0].torque.x==0&&s.torque_last_error[0].x==5&&u.total_torque_error.x==-90);
    CHECK(u.total_force_error.x==6&&u.total_force_error.y==8);CHECK(s.force_last_error[0].x==3&&s.force_last_error[1].y==4);
    CHECK(!u.joints[0].apply&&u.joints[6].spring==40&&u.joints[9].spring==40&&u.joints[10].spring==80&&u.joints[10].damper==.5f);
    CHECK(u.joints[10].maximum_force>3e38f&&u.joints[10].mode==1&&u.bodies[0].set_drag&&u.bodies[0].angular_drag==35);
    CHECK(!u.bodies[0].set_target_rotation&&u.bodies[10].set_target_rotation);
    CHECK(pe_animfollow_drive_step(&s,in,&u));CHECK(!u.capture_nonrigid&&!u.write_nonrigid&&!u.joints[1].apply);NEAR(u.bodies[1].torque.x,-272.7f);
    /* One-argument overload preserves the shared drive, even with NaN in its
     * unused damping parameter. Null joints and root never receive writes. */
    CHECK(pe_animfollow_drive_set_joints(&s,20,true,1.25f,&u));s.joint_mask&=~(1u<<3);
    CHECK(pe_animfollow_drive_set_joints(&s,80,false,NAN,&u));CHECK(!u.joints[0].apply&&!u.joints[3].apply&&u.joints[6].spring==40&&u.joints[6].damper==1.25f&&s.joint_damping==1.25f);
    uint16_t mask=0;unsigned motion=0;CHECK(pe_animfollow_drive_joint_limits(&s,true,&mask,&motion)&&mask==s.joint_mask&&motion==1);
    CHECK(pe_animfollow_drive_joint_limits(&s,false,&mask,&motion)&&motion==2);
    CHECK(pe_animfollow_drive_sync(7,5,&mask)&&mask==5);mask=17;CHECK(!pe_animfollow_drive_sync(4096,5,&mask)&&mask==17);
    /* No clock/error reset when inactive or assignments are missing. */
    PeAnimFollowDrive saved=s;in.active=false;in.secondary_update=0;in.force_cap=NAN;
    CHECK(pe_animfollow_drive_step(&s,in,&u)&&!u.driven&&!memcmp(&saved,&s,sizeof s));in.active=true;in.missing_assignments=true;
    CHECK(pe_animfollow_drive_step(&s,in,&u)&&!u.driven&&!memcmp(&saved,&s,sizeof s));
    /* Force errors accumulate before the enabled check; disabled histories stay
     * old. Opposing body errors cancel rather than summing magnitudes. */
    CHECK(pe_animfollow_drive_init(&s,.02f));in=tick();s.force_enabled=false;s.torque_enabled=false;
    in.bodies[0].master_position=(PeVec3){4,0,0};in.bodies[1].master_position=(PeVec3){-4,0,0};in.bodies[0].angle=NAN;
    CHECK(pe_animfollow_drive_step(&s,in,&u)&&u.total_force_error.x==0&&!u.bodies[0].apply_force&&!u.bodies[0].apply_torque&&s.force_last_error[0].x==0);
    s.force_enabled=true;in.force_cap=1000;CHECK(pe_animfollow_drive_step(&s,in,&u));NEAR(u.bodies[0].force.x,159.392f);
    /* FixEuler is one subtraction, not modulo/repeated normalization. */
    CHECK(pe_animfollow_drive_init(&s,.02f));in=tick();in.bodies[0].angle=720;in.bodies[1].angle=-450;
    CHECK(pe_animfollow_drive_step(&s,in,&u)&&s.torque_last_error[0].x==360&&s.torque_last_error[1].x==-450&&u.total_torque_error.x==-90);
    /* Nonrigid write cadence is independent of mimic capture; >2 requests a
     * caller-owned Lerp rather than fabricating a solved quaternion. */
    s.mimic_nonrigids=false;s.frame_counter=20;in.secondary_update=20;
    CHECK(pe_animfollow_drive_step(&s,in,&u)&&!u.capture_nonrigid&&u.write_nonrigid&&u.blend_nonrigid);NEAR(u.nonrigid_lerp,.1f);
    s.frame_counter=INT32_MAX;CHECK(pe_animfollow_drive_step(&s,in,&u)&&s.frame_counter==INT32_MIN&&!u.write_nonrigid);
    CHECK(pe_animfollow_drive_step(&s,in,&u)&&u.write_nonrigid&&!u.capture_nonrigid);
    /* Invalid/overflowing requests cannot commit any earlier joint/history
     * update or replace caller output. */
    for(unsigned k=0;k<5;k++){
        CHECK(pe_animfollow_drive_init(&s,.02f));in=tick();memset(&u,0xa5,sizeof u);PeAnimFollowDriveUpdate output=u;saved=s;
        if(k==0)in.secondary_update=0;
        if(k==1)in.bodies[10].angle=NAN;
        if(k==2){in.bodies[10].master_position.x=FLT_MAX;in.bodies[10].slave_position.x=-FLT_MAX;}
        if(k==3)in.torque_cap=FLT_MAX;
        if(k==4)in.drag=-1;
        CHECK(!pe_animfollow_drive_step(&s,in,&u)&&!memcmp(&s,&saved,sizeof s)&&!memcmp(&u,&output,sizeof u));
    }
    saved=s;CHECK(!pe_animfollow_drive_init(&s,0)&&!memcmp(&s,&saved,sizeof s));CHECK(!pe_animfollow_drive_init(&s,NAN));
    /* The lifecycle consumes the previous UNCLAMPED total error, never force
     * strength or this tick's newly reset accumulator. */
    CHECK(pe_animfollow_drive_init(&s,.02f));in=tick();in.bodies[0].master_position.x=16;in.force_cap=.1f;
    CHECK(pe_animfollow_drive_step(&s,in,&u));NEAR(u.bodies[0].force.x,.1f);CHECK(s.total_force_error.x==16);
    PeAnimFollow life;PeAnimFollowUpdate lu;CHECK(pe_animfollow_init(&life));life.active=true;
    PeAnimFollowTick li={.fixed_delta_time=.02f,.limb_error_magnitude=s.total_force_error.x};
    CHECK(pe_animfollow_step(&life,li,&lu)&&life.falling&&lu.on_fall_begin);
    return failures;
}
