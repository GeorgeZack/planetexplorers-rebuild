#include "pe_fbbik.h"
#include "pe_hit_ik.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned failures;
#define CHECK(v) do{if(!(v)){fprintf(stderr,"fbbik:%d %s\n",__LINE__,#v);failures++;}}while(0)
enum {BODY,L_SHOULDER,R_SHOULDER,L_THIGH,R_THIGH,L_HAND,R_HAND,L_FOOT,R_FOOT};
static const PeFbbikRig *rig;
static PeVec3 at(const PeFbbikTransform *l,unsigned bone){PeFbbikTransform t={0};CHECK(pe_fbbik_world(rig,l,bone,&t));return t.position;}
static PeFbbikQuat facing(const PeFbbikTransform *l,unsigned bone){PeFbbikTransform t={0};CHECK(pe_fbbik_world(rig,l,bone,&t));return t.rotation;}
static float gap(PeVec3 a,PeVec3 b){float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return sqrtf(x*x+y*y+z*z);}
static float turn(PeFbbikQuat a,PeFbbikQuat b){return 1-fabsf(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w);}
static unsigned node_bone(unsigned chain,unsigned node){return rig->chain_bone[chain][node];}
/* Every limb segment keeps its animated length, and untouched bones their local pose. */
static void rigid(const PeFbbikTransform *before,const PeFbbikTransform *after){
    for(unsigned c=1;c<PE_FBBIK_CHAINS;c++)for(unsigned n=0;n<2;n++){
        float a=gap(at(before,node_bone(c,n)),at(before,node_bone(c,n+1))),b=gap(at(after,node_bone(c,n)),at(after,node_bone(c,n+1)));
        CHECK(fabsf(a-b)<2e-5f);
    }
}
/* A clearly bent pose: knees and elbows flexed about their local hinge axis. */
static void bent(PeFbbikTransform *l){
    CHECK(pe_fbbik_female_rest(l));
    float s=sinf(.35f),c=cosf(.35f);
    for(unsigned k=1;k<PE_FBBIK_CHAINS;k++){
        PeFbbikQuat *q=&l[node_bone(k,1)].rotation,r=*q;
        *q=(PeFbbikQuat){r.x*c+r.y*s,r.y*c-r.x*s,r.z*c+r.w*s,r.w*c-r.z*s};   /* r * rotation about local z */
    }
}
int pe_test_fbbik(void){
    failures=0;rig=pe_fbbik_female_rig();
    CHECK(rig&&rig->bone_count==24&&rig->iterations==1&&rig->spine_count==3&&rig->spine_root_index==1);
    CHECK(pe_fbbik_female_bone_id(node_bone(0,0))==400264&&pe_fbbik_female_bone_id(node_bone(1,2))==400246);
    CHECK(pe_fbbik_female_bone_id(node_bone(4,2))==400236&&pe_fbbik_female_bone_id(rig->head)==400284);
    CHECK(pe_fbbik_female_bone_id(rig->limb_parent[0])==400206&&rig->limb_parent[2]==PE_FBBIK_NONE&&!pe_fbbik_female_bone_id(24));
    CHECK(pe_fbbik_female_bone_id(rig->root)==400170&&pe_fbbik_female_bone_id(rig->spine[0])==400288);

    PeFbbikTransform rest[PE_FBBIK_MAX_BONES],pose[PE_FBBIK_MAX_BONES];
    CHECK(pe_fbbik_female_rest(rest));bent(pose);
    PeFbbik state;CHECK(pe_fbbik_init(&state,rig,rest));
    CHECK(state.bend_local_set[0]&&state.bend_local_set[1]&&state.bend_local_set[2]&&state.bend_local_set[3]);
    PeFbbikInput in;PeFbbikOutput out;CHECK(pe_fbbik_input(&state,pose,&in));
    CHECK(in.weight==1&&in.effector[L_HAND].position_weight==0);

    /* No offsets: the mapping reproduces the animated pose. */
    CHECK(pe_fbbik_solve(&state,&in,&out)&&out.solved);
    for(unsigned b=0;b<rig->bone_count;b++){CHECK(gap(at(pose,b),at(out.local,b))<1e-4f);CHECK(turn(facing(pose,b),facing(out.local,b))<1e-6f);}
    CHECK(gap(out.node[0],at(pose,node_bone(0,0)))<1e-5f);

    /* Zero weight is the original early return: untouched pose, nothing solved. */
    PeFbbikInput off=in;off.weight=0;off.effector[BODY].position_offset=(PeVec3){0,-.2f,0};
    CHECK(pe_fbbik_solve(&state,&off,&out)&&!out.solved&&!memcmp(out.local,pose,sizeof pose));

    /* Body offset: the body and its child thigh nodes sink, but the shoulders
     * carry no offset and the arm chains pull at full weight, so only part of
     * it lands. End effectors return to their animated positions. */
    PeFbbikInput sink=in;sink.effector[BODY].position_offset=(PeVec3){0,-.08f,0};
    CHECK(pe_fbbik_solve(&state,&sink,&out)&&out.solved);
    PeVec3 before=at(pose,node_bone(0,0)),after=at(out.local,node_bone(0,0));
    CHECK(after.y<before.y-.03f&&after.y>before.y-.06f);
    CHECK(gap(at(pose,node_bone(3,2)),at(out.local,node_bone(3,2)))<2e-3f&&gap(at(pose,node_bone(4,2)),at(out.local,node_bone(4,2)))<2e-3f);
    CHECK(at(out.local,node_bone(3,0)).y<at(pose,node_bone(3,0)).y-.03f);
    CHECK(turn(facing(pose,node_bone(3,2)),facing(out.local,node_bone(3,2)))<1e-5f);   /* feet maintain rotation */
    rigid(pose,out.local);
    /* The same offset on the shoulders and hands lets the whole upper body
     * follow; the planted feet and bent knees absorb it. */
    PeFbbikInput crouch=sink;
    for(unsigned e=L_SHOULDER;e<=R_HAND;e++)if(e!=L_THIGH&&e!=R_THIGH)crouch.effector[e].position_offset=(PeVec3){0,-.08f,0};
    CHECK(pe_fbbik_solve(&state,&crouch,&out)&&out.solved);
    CHECK(fabsf(at(out.local,node_bone(0,0)).y-before.y+.08f)<4e-3f);
    CHECK(fabsf(at(out.local,node_bone(1,2)).y-at(pose,node_bone(1,2)).y+.08f)<4e-3f);
    CHECK(gap(at(pose,node_bone(3,2)),at(out.local,node_bone(3,2)))<2e-3f);
    CHECK(pe_fbbik_solve(&state,&sink,&out));
    rigid(pose,out.local);
    /* Half solver weight scales the applied offset. */
    PeFbbikInput half=sink;half.weight=.5f;PeFbbikOutput part;
    CHECK(pe_fbbik_solve(&state,&half,&part)&&part.solved);
    CHECK(at(part.local,node_bone(0,0)).y>after.y+.01f&&at(part.local,node_bone(0,0)).y<before.y-.02f);

    /* Hand offset: an end effector follows its own offset; the feet stay planted. */
    PeFbbikInput reach=in;reach.effector[L_HAND].position_offset=(PeVec3){0,.05f,.06f};
    CHECK(pe_fbbik_solve(&state,&reach,&out)&&out.solved);
    PeVec3 hand=at(pose,node_bone(1,2)),moved=at(out.local,node_bone(1,2));
    CHECK(gap(moved,(PeVec3){hand.x,hand.y+.05f,hand.z+.06f})<3e-3f);
    CHECK(gap(at(pose,node_bone(4,2)),at(out.local,node_bone(4,2)))<2e-3f);
    CHECK(gap(at(pose,node_bone(2,2)),at(out.local,node_bone(2,2)))<2e-3f);
    rigid(pose,out.local);

    /* Position weight pins a hand to its target; rotation weight turns it. */
    PeFbbikInput pin=in;pin.effector[R_HAND].position=(PeVec3){hand.x-.05f,hand.y+.1f,hand.z+.1f};
    pin.effector[R_HAND].position=at(pose,node_bone(2,2));pin.effector[R_HAND].position.y+=.07f;pin.effector[R_HAND].position_weight=1;
    pin.effector[R_HAND].rotation=(PeFbbikQuat){0,0,0,1};pin.effector[R_HAND].rotation_weight=1;
    CHECK(pe_fbbik_solve(&state,&pin,&out)&&out.solved);
    CHECK(gap(at(out.local,node_bone(2,2)),pin.effector[R_HAND].position)<3e-3f);
    CHECK(turn(facing(out.local,node_bone(2,2)),pin.effector[R_HAND].rotation)<1e-5f);
    rigid(pose,out.local);

    /* PullBody: an out-of-reach weighted hand target raises the body through
     * pullBodyVertical alone; the female rig has no horizontal pull. */
    PeFbbikInput far=in;far.effector[L_HAND].position=(PeVec3){hand.x,hand.y+2,hand.z};far.effector[L_HAND].position_weight=1;
    CHECK(pe_fbbik_solve(&state,&far,&out)&&out.solved);
    CHECK(at(out.local,node_bone(0,0)).y>before.y+.05f);
    rigid(pose,out.local);

    /* An unreachable foot target: the knee is placed for 99.9% of the leg and
     * the calf then aims at the target, so the leg stays just short of straight. */
    PeFbbikInput straight=in;straight.effector[L_FOOT].position_offset=(PeVec3){0,-.5f,0};
    CHECK(pe_fbbik_solve(&state,&straight,&out)&&out.solved);
    float leg=gap(at(pose,node_bone(3,0)),at(pose,node_bone(3,1)))+gap(at(pose,node_bone(3,1)),at(pose,node_bone(3,2)));
    float span=gap(at(out.local,node_bone(3,0)),at(out.local,node_bone(3,2)));
    CHECK(span<leg*.9999f&&span>leg*.999f);rigid(pose,out.local);

    /* The recovered hit offsets are OnPreUpdate requests on these same nine
     * effectors: a head hit two frames in displaces the upper body. */
    PeHitIk hit={0};PeHitIkUpdate requests;
    PeHitIkStart blow={.transform_id=400284,.direction={0,0,1},.weight=1,.duration=.6f,.weight_scale=1,.controller_present=true,.solver_present=true};
    PeHitIkTick frame={.delta_time=.05f,.master_weight=1,.weight_scale=1,.controller_present=true,.solver_present=true,.enabled=true};
    CHECK(pe_hit_ik_begin(&hit,blow,&requests)&&pe_hit_ik_step(&hit,frame,&requests)&&pe_hit_ik_step(&hit,frame,&requests)&&requests.active_parts);
    PeFbbikInput struck=in;float requested=0,shifted=0;
    for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++){struck.effector[e].position_offset=requests.offsets[e];requested+=gap(requests.offsets[e],(PeVec3){0,0,0});}
    CHECK(requested>0&&pe_fbbik_solve(&state,&struck,&out)&&out.solved);
    for(unsigned b=0;b<rig->bone_count;b++)shifted+=gap(at(pose,b),at(out.local,b));
    CHECK(shifted>1e-3f);rigid(pose,out.local);

    /* iterations==0 is the offset-only path: the first spine bone translates. */
    PeFbbikRig flat=*rig;flat.iterations=0;PeFbbik simple;CHECK(pe_fbbik_init(&simple,&flat,rest));
    PeFbbikInput shift;CHECK(pe_fbbik_input(&simple,pose,&shift));shift.effector[BODY].position_offset=(PeVec3){.03f,0,0};
    CHECK(pe_fbbik_solve(&simple,&shift,&out)&&out.solved);
    CHECK(fabsf(at(out.local,rig->spine[0]).x-at(pose,rig->spine[0]).x-.03f)<1e-4f);
    CHECK(gap(at(pose,node_bone(3,2)),at(out.local,node_bone(3,2)))<3e-3f);

    /* Invalid input fails without touching the output. */
    PeFbbikOutput kept=out;PeFbbikInput bad=in;bad.effector[BODY].position_offset.x=NAN;
    CHECK(!pe_fbbik_solve(&state,&bad,&out)&&!memcmp(&kept,&out,sizeof out));
    bad=in;bad.local[3].rotation.w=INFINITY;CHECK(!pe_fbbik_solve(&state,&bad,&out)&&!memcmp(&kept,&out,sizeof out));
    CHECK(!pe_fbbik_solve(0,&in,&out)&&!pe_fbbik_solve(&state,0,&out)&&!pe_fbbik_solve(&state,&in,0));
    PeFbbik blank={0};CHECK(!pe_fbbik_solve(&blank,&in,&out)&&!pe_fbbik_input(&blank,pose,&in));
    PeFbbikRig broken=*rig;broken.parent[5]=7;PeFbbik untouched=state;
    CHECK(!pe_fbbik_init(&state,&broken,rest)&&!memcmp(&untouched,&state,sizeof state));
    broken=*rig;broken.spine_root_index=0;CHECK(!pe_fbbik_init(&state,&broken,rest));
    CHECK(!pe_fbbik_init(&state,rig,0)&&!pe_fbbik_world(rig,pose,24,&rest[0]));
    return failures?1:0;
}
