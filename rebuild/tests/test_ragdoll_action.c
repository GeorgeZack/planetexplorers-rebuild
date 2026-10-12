#include "pe_ragdoll_action.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>
#define BIT(n) (UINT64_C(1)<<(n))
static PeRagdollStart start(void){
    return (PeRagdollStart){.view_present=true,.motor_present=true,.parameter_present=true,
        .entity_present=true,.entity_view_present=true,.bone_present=true,.direction={0,0,2},.force=50,.hit_point={1,2,3}};
}
static PeRagdollTick tick(float dt){return (PeRagdollTick){.delta_time=dt,.view_present=true,.ragdoll_active=true};}
static void operation(PeRagdollUpdate u,unsigned i,PeRagdollOpType type,bool value,float delay){
    assert(i<u.op_count&&u.ops[i].type==type&&u.ops[i].value==value&&u.ops[i].delay==delay);
}
static void relations(void){
    PeRagdollRelation r;
    assert(pe_ragdoll_action_relation(PE_RAGDOLL_WENTFLY,&r)&&r.required_masks==0&&r.pause_actions==BIT(40)&&r.end_actions==0);
    assert(!(r.forbidden_masks&BIT(7))&&(r.forbidden_masks&BIT(8))&&(r.forbidden_masks&BIT(44)));
    assert(r.end_immediately_actions&BIT(14));
    assert(pe_ragdoll_action_source_admissible(PE_RAGDOLL_WENTFLY,BIT(1)|BIT(7)));
    assert(!pe_ragdoll_action_source_admissible(PE_RAGDOLL_WENTFLY,BIT(8)));
    assert(pe_ragdoll_action_relation(PE_RAGDOLL_KNOCKED,&r)&&!(r.forbidden_masks&BIT(8))&&(r.forbidden_masks&BIT(7))&&(r.forbidden_masks&BIT(55)));
    assert(pe_ragdoll_action_relation(PE_RAGDOLL_GET_UP,&r)&&r.required_masks==(BIT(8)|BIT(44))&&r.end_immediately_actions==(BIT(7)|BIT(8)));
    assert(!pe_ragdoll_action_source_admissible(PE_RAGDOLL_GET_UP,0)&&!pe_ragdoll_action_source_admissible(PE_RAGDOLL_GET_UP,BIT(8)));
    assert(pe_ragdoll_action_source_admissible(PE_RAGDOLL_GET_UP,BIT(8)|BIT(44)));
    assert(!pe_ragdoll_action_source_admissible(PE_RAGDOLL_GET_UP,BIT(8)|BIT(44)|BIT(17)));
    assert(!pe_ragdoll_action_source_admissible(PE_RAGDOLL_GET_UP,UINT64_MAX));
    assert(!pe_ragdoll_action_relation(0,&r)&&!pe_ragdoll_action_relation(PE_RAGDOLL_GET_UP,NULL));
}
static void wentfly(void){
    PeRagdollAction a={.kind=PE_RAGDOLL_WENTFLY};PeRagdollUpdate u;PeRagdollStart in=start();
    assert(pe_ragdoll_action_begin(&a,in,&u)&&u.started&&a.running&&a.mask&&a.freeze_requested&&u.op_count==5);
    operation(u,0,PE_RAGDOLL_SET_MASK,true,0);operation(u,1,PE_RAGDOLL_FREEZE,true,0);
    operation(u,2,PE_RAGDOLL_ACTIVATE,true,0);operation(u,3,PE_RAGDOLL_CLEAR_DESIRED_MOVEMENT,false,0);operation(u,4,PE_RAGDOLL_CANCEL_MOVE_REQUEST,false,0);
    assert(u.hit_force.z==100&&u.hit_normal.z==-1&&u.hit_point.y==2);
    PeRagdollAction before=a;assert(pe_ragdoll_action_begin(&a,in,&u)&&u.repeated_ignored&&!memcmp(&a,&before,sizeof a));
    assert(pe_ragdoll_action_step(&a,tick(100),&u)&&u.op_count==0&&a.running); /* no invented timer */
    PeRagdollTick t=tick(0);t.ready_get_up=true;
    assert(pe_ragdoll_action_step(&a,t,&u)&&u.manager_complete&&!a.running&&u.op_count==3&&u.continue_actions==BIT(40));
    operation(u,0,PE_RAGDOLL_SET_MASK,false,0);operation(u,1,PE_RAGDOLL_FREEZE,false,0);operation(u,2,PE_RAGDOLL_REQUEST_GET_UP,true,0);
    assert(pe_ragdoll_action_begin(&a,in,&u));t=tick(0);t.ragdoll_active=false;t.ready_get_up=true;
    assert(pe_ragdoll_action_step(&a,t,&u)&&u.manager_complete&&u.op_count==2); /* not ragdolled takes precedence */
    assert(pe_ragdoll_action_begin(&a,in,&u)&&pe_ragdoll_action_end(&a,true,false,&u)&&a.running&&u.op_count==3&&!u.manager_complete);
    assert(pe_ragdoll_action_end(&a,true,true,&u)&&u.manager_complete&&u.op_count==2);
    assert(pe_ragdoll_action_end(&a,true,true,&u)&&u.op_count==0);
    /* Missing parameter and target view reproduce partial calls up to a source null dereference. */
    a=(PeRagdollAction){.kind=PE_RAGDOLL_WENTFLY};in=start();in.parameter_present=false;
    assert(pe_ragdoll_action_begin(&a,in,&u)&&u.source_fault==PE_RAGDOLL_FAULT_PARAMETER&&u.op_count==1&&a.mask&&!a.freeze_requested);
    assert(pe_ragdoll_action_end(&a,false,true,&u));in=start();in.entity_view_present=false;
    assert(pe_ragdoll_action_begin(&a,in,&u)&&u.source_fault==PE_RAGDOLL_FAULT_ENTITY_VIEW&&u.op_count==2&&a.freeze_requested);
    assert(pe_ragdoll_action_end(&a,false,true,&u));in=start();in.view_present=false;
    assert(pe_ragdoll_action_begin(&a,in,&u)&&a.running&&!a.mask&&u.op_count==0);
    t=tick(0);t.view_present=false;before=a;
    assert(pe_ragdoll_action_step(&a,t,&u)&&u.source_fault==PE_RAGDOLL_FAULT_UPDATE_VIEW&&!u.manager_complete&&!memcmp(&a,&before,sizeof a));
    assert(pe_ragdoll_action_end(&a,false,true,&u));
    in=start();in.entity_present=false;assert(pe_ragdoll_action_begin(&a,in,&u)&&u.op_count==4&&u.source_fault==0);
    t=tick(0);t.ragdoll_active=false;assert(pe_ragdoll_action_step(&a,t,&u)&&u.manager_complete);
    in=start();in.bone_present=false;in.motor_present=false;
    assert(pe_ragdoll_action_begin(&a,in,&u)&&u.op_count==2&&pe_ragdoll_action_end(&a,false,true,&u));
    in=start();in.direction=(PeVec3){0};assert(pe_ragdoll_action_begin(&a,in,&u)&&u.hit_normal.z==0&&u.hit_force.z==0);
}
static void knocked_and_get_up(void){
    PeRagdollAction a={.kind=PE_RAGDOLL_KNOCKED};PeRagdollUpdate u;PeRagdollStart in=start();
    assert(pe_ragdoll_action_begin(&a,in,&u)&&a.remaining==2&&a.auto_get_up&&u.op_count==6);
    operation(u,0,PE_RAGDOLL_ACTIVATE,false,0);operation(u,1,PE_RAGDOLL_INJURED,false,0);
    operation(u,2,PE_RAGDOLL_SET_MASK,true,0);operation(u,3,PE_RAGDOLL_FREEZE,true,0);
    assert(pe_ragdoll_action_step(&a,tick(1.5f),&u)&&a.remaining==.5f&&u.op_count==0);
    PeRagdollAction before=a;assert(pe_ragdoll_action_begin(&a,in,&u)&&u.repeated_ignored&&!memcmp(&a,&before,sizeof a));
    assert(pe_ragdoll_action_step(&a,tick(.5f),&u)&&a.running&&!a.mask&&!a.freeze_requested&&!a.auto_get_up&&!u.manager_complete&&u.op_count==4);
    operation(u,0,PE_RAGDOLL_FREEZE,false,0);operation(u,1,PE_RAGDOLL_SET_MASK,false,0);
    operation(u,2,PE_RAGDOLL_INJURED,true,1);operation(u,3,PE_RAGDOLL_REQUEST_GET_UP,true,0);
    assert(pe_ragdoll_action_step(&a,tick(100),&u)&&u.op_count==0&&a.running);
    /* GetUp's immediate relation ends Knocked; cleanup is intentionally called
     * again before GetUp disables defence and replaces the delayed enable. */
    assert(pe_ragdoll_action_end(&a,true,true,&u)&&u.manager_complete&&u.op_count==3);
    operation(u,2,PE_RAGDOLL_INJURED,true,1);
    PeRagdollAction get={.kind=PE_RAGDOLL_GET_UP};
    assert(pe_ragdoll_action_begin(&get,in,&u)&&u.started&&u.op_count==4);
    operation(u,0,PE_RAGDOLL_DEACTIVATE,false,0);operation(u,1,PE_RAGDOLL_INJURED,false,0);
    operation(u,2,PE_RAGDOLL_FREEZE,true,0);operation(u,3,PE_RAGDOLL_SET_MASK,true,0);
    assert(pe_ragdoll_action_step(&get,tick(100),&u)&&get.running&&u.op_count==0); /* no clip-length timer */
    assert(pe_ragdoll_action_begin(&get,in,&u)&&u.reset&&!u.started&&u.op_count==4);
    PeRagdollTick t=tick(0);t.ragdoll_active=false;
    assert(pe_ragdoll_action_step(&get,t,&u)&&u.manager_complete&&u.op_count==4);
    operation(u,0,PE_RAGDOLL_SET_MASK,false,0);operation(u,1,PE_RAGDOLL_FREEZE,false,0);
    operation(u,2,PE_RAGDOLL_DEACTIVATE,true,0);operation(u,3,PE_RAGDOLL_INJURED,true,1.5f);
    in=start();in.ragdoll_active=true;assert(pe_ragdoll_action_begin(&a,in,&u)&&u.op_count==5&&u.ops[0].type==PE_RAGDOLL_INJURED);
    assert(pe_ragdoll_action_end(&a,true,false,&u)&&u.manager_complete&&u.op_count==3); /* inherited EndAction */
    in.view_present=false;t=tick(3);t.view_present=false;assert(pe_ragdoll_action_begin(&a,in,&u)&&u.op_count==4&&pe_ragdoll_action_step(&a,t,&u)&&u.op_count==3&&a.remaining==-1);
    assert(pe_ragdoll_action_end(&a,false,true,&u)&&u.op_count==2);
    assert(pe_ragdoll_action_begin(&get,in,&u)&&u.op_count==2);t.view_present=false;
    assert(pe_ragdoll_action_step(&get,t,&u)&&u.manager_complete&&u.op_count==2);
}
static void freeze_and_helpers(void){
    PeRagdollFreeze s={0};PeRagdollPhysics p;
    assert(pe_ragdoll_freeze_set(&s,PE_RAGDOLL_WENTFLY,true,&p)&&!p.model_physics&&p.ragdoll_physics);
    assert(pe_ragdoll_freeze_set(&s,PE_RAGDOLL_WENTFLY,true,&p)&&s.classes==1); /* class set, not refcount */
    assert(pe_ragdoll_freeze_set(&s,PE_RAGDOLL_GET_UP,true,&p)&&s.classes==5);
    assert(pe_ragdoll_freeze_set(&s,PE_RAGDOLL_WENTFLY,false,&p)&&!p.model_physics&&p.ragdoll_physics);
    s.network=true;assert(pe_ragdoll_freeze_set(&s,PE_RAGDOLL_GET_UP,false,&p)&&!p.model_physics&&p.ragdoll_physics);
    s.network=false;s.system=true;assert(pe_ragdoll_freeze_set(&s,PE_RAGDOLL_GET_UP,false,&p)&&!p.model_physics&&!p.ragdoll_physics);
    s.system=false;s.other_classes=true;assert(pe_ragdoll_freeze_set(&s,PE_RAGDOLL_GET_UP,false,&p)&&!p.model_physics&&p.ragdoll_physics);
    s.other_classes=false;assert(pe_ragdoll_freeze_set(&s,PE_RAGDOLL_GET_UP,false,&p)&&p.model_physics&&p.ragdoll_physics);
    bool ready;
    assert(pe_ragdoll_ready_get_up(true,.49f,.5f,&ready)&&ready);
    assert(pe_ragdoll_ready_get_up(true,.5f,.5f,&ready)&&!ready);
    assert(pe_ragdoll_ready_get_up(true,.51f,.5f,&ready)&&!ready);
    assert(pe_ragdoll_ready_get_up(false,0,.5f,&ready)&&!ready);
    PeVec3 force;assert(pe_ragdoll_deferred_force((PeVec3){1,-2,3},&force)&&force.x==100&&force.y==-200&&force.z==300);
}
static void invalid(void){
    PeRagdollAction a={.kind=PE_RAGDOLL_WENTFLY},before=a;PeRagdollUpdate u;memset(&u,0x5a,sizeof u);PeRagdollUpdate saved=u;
    PeRagdollStart in=start();in.force=INFINITY;assert(!pe_ragdoll_action_begin(&a,in,&u));
    in=start();in.direction.x=FLT_MAX;assert(!pe_ragdoll_action_begin(&a,in,&u));
    assert(!pe_ragdoll_action_step(&a,tick(NAN),&u)&&!pe_ragdoll_action_step(&a,tick(-1),&u));
    assert(!pe_ragdoll_action_end(NULL,true,true,&u)&&!pe_ragdoll_action_begin(&a,start(),NULL));
    assert(!memcmp(&a,&before,sizeof a)&&!memcmp(&u,&saved,sizeof u));
    bool ready=true;assert(!pe_ragdoll_ready_get_up(true,NAN,1,&ready)&&ready);
    PeVec3 force={1,2,3};assert(!pe_ragdoll_deferred_force((PeVec3){FLT_MAX,0,0},&force)&&force.x==1);
    PeRagdollFreeze s={.classes=8},old=s;PeRagdollPhysics p={true,true};
    assert(!pe_ragdoll_freeze_set(&s,PE_RAGDOLL_WENTFLY,true,&p)&&!memcmp(&s,&old,sizeof s)&&p.model_physics&&p.ragdoll_physics);
}
int pe_test_ragdoll_action(void){relations();wentfly();knocked_and_get_up();freeze_and_helpers();invalid();return 0;}
