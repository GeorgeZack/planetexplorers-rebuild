#include "pe_animfollow.h"
#include <assert.h>
#include <math.h>
#include <string.h>
static bool near(float a,float b){return fabsf(a-b)<2e-5f;}
static PeAnimFollowTick tick(void){return (PeAnimFollowTick){.fixed_delta_time=.02f,.animator_present=true,.controller_present=true,.current_state=PE_AF_GROUND_MOVE,.root_position={1,2,3},.entity_height=1.75f};}
static void classifiers_and_active(void){
    PeAnimFollowState k;assert(pe_animfollow_state("Base Layer.Ragdoll.GetupBackMirror_Water",&k)&&k==PE_AF_BACK_MIRROR_WATER);
    assert(pe_animfollow_state("Base Layer.GroundLocomotion.Move",&k)&&k==PE_AF_GROUND_MOVE);
    assert(pe_animfollow_state("GetUpFront",&k)&&k==PE_AF_UNKNOWN); /* signal name is not a full state path */
    assert(pe_animfollow_get_up_transition("AnyState -> GetupFront_Water")&&!pe_animfollow_get_up_transition("AnyState -> GetUpFront_Water"));
    assert(!strcmp(pe_animfollow_signal_name(PE_AF_FRONT_MIRROR_SIGNAL),"GetUpFrontMirror")&&pe_animfollow_signal_name(PE_AF_NO_SIGNAL)==NULL);
    PeAnimFollow s;PeAnimFollowUpdate u;assert(pe_animfollow_init(&s)&&s.torque==100&&s.force==100&&s.joint_torque==10000&&s.drag==.5f&&s.angular_drag==1&&s.secondary_update==2&&!s.active&&!s.wrapper_active);
    assert(pe_animfollow_activate(&s,false,&u)&&u.active_changed&&u.sync_transforms&&u.renderer_swap&&u.body_policy_changed&&s.active&&!s.wrapper_active&&s.shot_by_bullet);
    assert(pe_animfollow_activate(&s,true,&u)&&!u.active_changed&&s.get_up_ready&&s.shot_by_bullet);
    assert(!strcmp(pe_animfollow_lock_tag(),"RagdollLock"));
    PeAnimFollowBodyPolicy p=pe_animfollow_body_policy(true,false);assert(!p.kinematic&&!p.collider_trigger&&p.collider_enabled);
    p=pe_animfollow_body_policy(true,true);assert(p.kinematic&&!p.collider_trigger&&p.collider_enabled);
    p=pe_animfollow_body_policy(false,false);assert(p.kinematic&&p.collider_trigger&&!p.collider_enabled);
    assert(pe_animfollow_deactivate(&s,false,&u)&&s.active&&s.get_up_ready&&!u.on_get_up_finished);
    assert(pe_animfollow_deactivate(&s,true,&u)&&!s.active&&!s.wrapper_active&&u.on_get_up_finished&&u.active_changed);
    assert(pe_animfollow_deactivate(&s,true,&u)&&u.on_get_up_finished&&!u.active_changed); /* callback not deduped */
}
static void falling_and_readiness(void){
    PeAnimFollow s;PeAnimFollowUpdate u;PeAnimFollowTick t=tick();assert(pe_animfollow_init(&s)&&pe_animfollow_activate(&s,false,&u));
    assert(pe_animfollow_step(&s,t,&u)&&u.on_fall_begin&&s.falling&&!s.getting_up&&!s.orientated&&s.wrapper_active&&s.active&&s.joint_limits&&s.secondary_update==100&&s.torque==0&&s.joint_torque==0);
    assert(u.set_joint_torque&&u.restore_friction&&u.set_joint_limits&&u.set_animator_speed&&u.animator_speed==1&&u.clear_collision_speed);
    t.root_speed=.2f;assert(pe_animfollow_step(&s,t,&u)&&!u.on_fall_finished&&u.set_master_position);
    t.root_speed=.199f;assert(pe_animfollow_step(&s,t,&u)&&u.on_fall_finished&&!s.getting_up&&u.capture_root_alignment&&s.calls==3);
    assert(pe_animfollow_step(&s,t,&u)&&u.on_fall_finished&&!u.capture_root_alignment&&s.calls==4); /* no readiness; repeats callback */
    assert(pe_animfollow_deactivate(&s,false,&u)&&pe_animfollow_step(&s,t,&u)&&s.getting_up&&s.falling&&s.orientate&&u.signal==PE_AF_FRONT_SIGNAL&&u.animator_speed==.25f);
    /* Plane boundary chooses front. Only exact ground Front/Back invert the signal. */
    for(unsigned i=0;i<4;i++){
        assert(pe_animfollow_init(&s)&&pe_animfollow_activate(&s,true,&u));t=tick();t.current_state=i==0?PE_AF_FRONT:i==1?PE_AF_BACK:i==2?PE_AF_FRONT_WATER:PE_AF_UNKNOWN;t.forward_dot_down=i==1?-1:0;
        assert(pe_animfollow_step(&s,t,&u)&&pe_animfollow_step(&s,t,&u));
        assert(u.signal==(i==0?PE_AF_FRONT_MIRROR_SIGNAL:i==1?PE_AF_BACK_MIRROR_SIGNAL:PE_AF_FRONT_SIGNAL));
    }
    /* Non-idle fall uses serialized residual settings, then previous error drives the next phase. */
    assert(pe_animfollow_init(&s)&&pe_animfollow_activate(&s,false,&u));t=tick();t.current_state=PE_AF_UNKNOWN;t.collision_count=1;t.collision_speed=8.01f;
    assert(pe_animfollow_step(&s,t,&u)&&near(s.torque,.04f)&&near(s.force,.1f)&&s.joint_torque==80&&s.fell_on_speed&&u.joint_torque_request==80);
    assert(pe_animfollow_step(&s,t,&u)&&near(s.joint_torque,77.6f));
    assert(pe_animfollow_activate(&s,false,&u)&&pe_animfollow_step(&s,t,&u)&&!u.on_fall_begin&&near(s.joint_torque,77.6f)); /* repeated shot during fall does not reset residuals */
    /* An error-triggered fall changes callback flags without implicitly setting active. */
    assert(pe_animfollow_init(&s));t=tick();t.limb_error_magnitude=15;assert(pe_animfollow_step(&s,t,&u)&&!s.falling);
    t.limb_error_magnitude=15.01f;assert(pe_animfollow_step(&s,t,&u)&&u.on_fall_begin&&s.wrapper_active&&!s.active);
}
static void get_up_and_error(void){
    PeAnimFollow s;PeAnimFollowUpdate u;PeAnimFollowTick t=tick();assert(pe_animfollow_init(&s)&&pe_animfollow_activate(&s,true,&u));
    assert(pe_animfollow_step(&s,t,&u)&&pe_animfollow_step(&s,t,&u)&&s.getting_up&&!s.orientated);
    t.current_state=PE_AF_FRONT;t.in_get_up_transition=true;
    assert(pe_animfollow_step(&s,t,&u)&&!u.on_get_up_begin&&!s.orientated);
    t.in_get_up_transition=false;
    assert(pe_animfollow_step(&s,t,&u)&&u.on_get_up_begin&&u.align_master_horizontal&&u.zero_friction&&!s.falling&&s.orientated&&s.drag==17&&s.angular_drag==35&&s.secondary_update==20&&u.animator_speed==.35f);
    assert(near(s.torque,.012f)&&near(s.force,.009f)&&near(s.joint_torque,.015f));
    s.torque=.5f; /* exact wake-up boundary uses the stronger ramp */
    assert(pe_animfollow_step(&s,t,&u)&&u.animator_speed==.75f&&s.secondary_update==4&&!s.joint_limits&&u.set_joint_limits);
    t.current_state=PE_AF_UNKNOWN;t.limb_error_magnitude=.3f;
    assert(pe_animfollow_step(&s,t,&u)&&s.getting_up&&s.active&&s.delayed_get_up_done&&!u.on_get_up_finished&&u.restore_friction&&s.drag==.5f&&s.secondary_update==2);
    t.limb_error_magnitude=.299f;
    assert(pe_animfollow_step(&s,t,&u)&&!s.getting_up&&!s.active&&!s.wrapper_active&&u.on_get_up_finished&&u.active_changed&&s.torque>.5f); /* contact recovery still runs after finish */
    /* IsIdle fallback wins even with large mismatch; null animator faults only
     * when the earlier error predicate does not suffice. */
    assert(pe_animfollow_init(&s));s.getting_up=s.orientated=s.active=true;s.torque=.5f;
    t=tick();t.limb_error_magnitude=3;
    assert(pe_animfollow_step(&s,t,&u)&&u.on_get_up_finished&&!s.active);
    assert(pe_animfollow_init(&s));s.getting_up=s.orientated=s.active=true;s.torque=.5f;t.animator_present=false;
    assert(pe_animfollow_step(&s,t,&u)&&u.source_missing_animator_fault&&s.active&&s.getting_up&&!u.clear_collision_speed);
    t.limb_error_magnitude=0;assert(pe_animfollow_step(&s,t,&u)&&u.on_get_up_finished&&!s.active);
}
static void world_and_contact(void){
    PeAnimFollow s;PeAnimFollowUpdate u;PeAnimFollowTick t=tick();assert(pe_animfollow_init(&s)&&pe_animfollow_activate(&s,false,&u)&&pe_animfollow_step(&s,t,&u));
    t.master_entity_animator_present=t.root_under_water=t.no_ground_below=true;
    assert(pe_animfollow_step(&s,t,&u)&&u.set_water_bool&&u.water_bool&&u.master_position.y==.25f);
    t.no_ground_below=false;assert(pe_animfollow_step(&s,t,&u)&&!u.water_bool&&u.master_position.y==2);
    t.master_entity_animator_present=false;assert(pe_animfollow_step(&s,t,&u)&&!u.set_water_bool&&u.master_position.y==2);
    assert(pe_animfollow_init(&s));t=tick();t.collision_count=1;
    assert(pe_animfollow_step(&s,t,&u)&&s.torque==4&&s.force==3&&s.joint_torque==5&&s.contact_time==.02f&&s.no_contact_time==0); /* 70*.02 clamps to 1 */
    t.collision_count=0;assert(pe_animfollow_step(&s,t,&u)&&near(s.torque,5.92f)&&s.contact_time==0&&s.no_contact_time==.02f);
    PeAnimFollow before=s;t.missing_assignments=true;
    assert(pe_animfollow_step(&s,t,&u)&&!memcmp(&s,&before,sizeof s)&&!u.clear_collision_speed);
}
static void invalid(void){
    PeAnimFollow s;PeAnimFollowUpdate u;assert(pe_animfollow_init(&s));PeAnimFollow before=s;
    memset(&u,0x5a,sizeof u);PeAnimFollowUpdate saved=u;PeAnimFollowTick t=tick();t.fixed_delta_time=NAN;
    assert(!pe_animfollow_step(&s,t,&u));t=tick();t.entity_height=INFINITY;assert(!pe_animfollow_step(&s,t,&u));
    t=tick();t.current_state=99;assert(!pe_animfollow_step(&s,t,&u));
    assert(!pe_animfollow_step(NULL,tick(),&u)&&!pe_animfollow_activate(&s,true,NULL)&&!pe_animfollow_deactivate(NULL,false,&u));
    assert(!memcmp(&s,&before,sizeof s)&&!memcmp(&u,&saved,sizeof u));
}
int pe_test_animfollow(void){classifiers_and_active();falling_and_readiness();get_up_and_error();world_and_contact();invalid();return 0;}
