#include "pe_player_locomotion.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"player locomotion:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static bool near(double a,double b){return fabs(a-b)<1e-6;}
int pe_test_player_locomotion(void){
    failures=0;double d[PE_LOCOMOTION_COUNT]={2,1,.6666666667,1,.6666666667,1,1.1,1.2,.8,.7,.75,.65};PePlayerLocomotion s,before;PePlayerLocomotionInput in={.grounded=true};
    CHECK(pe_player_locomotion_init(&s,d));CHECK(!strcmp(pe_player_locomotion_name(s.clip),"NormalIdle"));
    CHECK(pe_player_locomotion_step(&s,in,.25));CHECK(near(s.seconds,.25));
    in.velocity.z=2;in.walk=true;CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_WALK&&s.seconds==0);
    CHECK(pe_player_locomotion_step(&s,in,.25));CHECK(near(s.seconds,.25));
    in.walk=false;in.velocity.z=5;CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_RUN&&near(s.seconds/d[s.clip],.25));
    double time=s.seconds;in.sprint=true;in.velocity.z=8;CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(near(s.seconds,time+.16));
    in.velocity=(PeVec3){0};CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_IDLE&&s.seconds==0);
    in.jump_started=true;in.grounded=false;in.velocity.y=7;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(s.clip==PE_LOCOMOTION_JUMP&&s.seconds==0);
    in.jump_started=false;CHECK(pe_player_locomotion_step(&s,in,.84));CHECK(s.clip==PE_LOCOMOTION_JUMP&&near(s.seconds,.84));
    in.velocity.y=-5;CHECK(pe_player_locomotion_step(&s,in,.02));CHECK(s.clip==PE_LOCOMOTION_FALL&&s.seconds==0);
    CHECK(pe_player_locomotion_step(&s,in,1));CHECK(s.clip==PE_LOCOMOTION_FALL&&s.seconds<d[PE_LOCOMOTION_FALL]);
    in.grounded=true;in.velocity.y=0;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(s.clip==PE_LOCOMOTION_LAND&&s.seconds==0);
    CHECK(pe_player_locomotion_step(&s,in,.69));CHECK(s.clip==PE_LOCOMOTION_LAND);
    CHECK(pe_player_locomotion_step(&s,in,.02));CHECK(s.clip==PE_LOCOMOTION_IDLE);
    /* Short ground-probe gaps don't replay Land; ledge falls wait strictly>.3. */
    in.grounded=false;CHECK(pe_player_locomotion_step(&s,in,.2));CHECK(s.clip==PE_LOCOMOTION_IDLE);
    in.grounded=true;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(s.clip==PE_LOCOMOTION_IDLE&&!s.airborne);
    in.grounded=false;CHECK(pe_player_locomotion_step(&s,in,.3));CHECK(s.clip==PE_LOCOMOTION_IDLE);
    in.grounded=true;CHECK(pe_player_locomotion_step(&s,in,.5));CHECK(s.clip==PE_LOCOMOTION_IDLE&&!s.airborne);
    in.grounded=false;CHECK(pe_player_locomotion_step(&s,in,.3));CHECK(s.clip==PE_LOCOMOTION_IDLE);
    CHECK(pe_player_locomotion_step(&s,in,.001));CHECK(s.clip==PE_LOCOMOTION_FALL);
    in.grounded=true;in.velocity.x=5;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(s.clip==PE_LOCOMOTION_RUN);
    /* Jump ground grace, immediate movement exit from land, clocks bound under
     * long sessions, and invalid-state/input atomicity. */
    in.velocity=(PeVec3){0};in.jump_started=true;CHECK(pe_player_locomotion_step(&s,in,.01));in.jump_started=false;
    CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_JUMP);
    CHECK(pe_player_locomotion_step(&s,in,.001));CHECK(s.clip==PE_LOCOMOTION_LAND);
    in.velocity.x=2;in.walk=true;in.sprint=false;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(s.clip==PE_LOCOMOTION_WALK);
    for(int i=0;i<10000;i++){CHECK(pe_player_locomotion_step(&s,in,.1));}
    CHECK(s.seconds>=0&&s.seconds<d[s.clip]);
    before=s;CHECK(!pe_player_locomotion_step(&s,in,NAN));CHECK(!pe_player_locomotion_step(&s,in,-1));CHECK(!pe_player_locomotion_step(&s,in,2));
    in.velocity.x=NAN;CHECK(!pe_player_locomotion_step(&s,in,.1));CHECK(!memcmp(&s,&before,sizeof s));
    /* Every direction uses actual speed, with gait phase retained across all
     * eight walk/run clips despite their differing authored durations. */
    CHECK(pe_player_locomotion_init(&s,d));
    in=(PePlayerLocomotionInput){.grounded=true,.directional=true,.walk=true,.velocity={2,0,0},.local_right=-2};
    CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_WALK_LEFT);
    CHECK(pe_player_locomotion_step(&s,in,.22));CHECK(near(s.seconds/d[s.clip],.2));
    const PePlayerLocomotionClip clips[]={PE_LOCOMOTION_WALK_RIGHT,PE_LOCOMOTION_WALK_BACK,PE_LOCOMOTION_WALK,PE_LOCOMOTION_RUN_LEFT,PE_LOCOMOTION_RUN_RIGHT,PE_LOCOMOTION_RUN_BACK,PE_LOCOMOTION_RUN};
    const float right[]={2,0,0,-5,5,0,0},forward[]={0,-2,2,0,0,-5,5};
    for(unsigned i=0;i<sizeof clips/sizeof clips[0];i++){
        in.walk=i<3;in.velocity.x=in.walk?2:5;in.local_right=right[i];in.local_forward=forward[i];
        CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==clips[i]);CHECK(near(s.seconds/d[s.clip],.2));
    }
    in.local_right=5;in.local_forward=-5;CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_RUN_BACK); /* Exact diagonal tie. */
    in.local_right=-5;in.local_forward=4.99f;CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_RUN_LEFT);
    double phase=s.seconds/d[s.clip];in.local_right=5;in.local_forward=5;
    CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_RUN&&near(s.seconds/d[s.clip],phase));
    /* Speed is measured from world displacement, not magnitude of the local
     * direction hint. Sprint selects run and retains source rate fallback. */
    in.local_right=1;in.local_forward=0;in.walk=true;in.sprint=true;in.velocity.x=8;
    CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_RUN_RIGHT);
    time=s.seconds;CHECK(pe_player_locomotion_step(&s,in,.05));CHECK(near(s.seconds,time+.08));
    in.sprint=false;in.local_right=0;in.local_forward=-2;in.velocity.x=2;
    CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_WALK_BACK);
    time=s.seconds;CHECK(pe_player_locomotion_step(&s,in,.05));CHECK(near(s.seconds,time+.05));
    in.velocity=(PeVec3){0};CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_IDLE); /* Held direction against obstruction. */
    in.velocity.x=2;in.jump_started=true;CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_JUMP);
    in.jump_started=false;CHECK(pe_player_locomotion_step(&s,in,.11));CHECK(s.clip==PE_LOCOMOTION_WALK_BACK); /* Moving landing. */
    before=s;in.local_right=NAN;CHECK(!pe_player_locomotion_step(&s,in,.1));CHECK(!memcmp(&s,&before,sizeof s));
    in.local_right=0;in.local_forward=INFINITY;CHECK(!pe_player_locomotion_step(&s,in,0));CHECK(!memcmp(&s,&before,sizeof s));
    in.local_forward=-101;CHECK(!pe_player_locomotion_step(&s,in,.1));CHECK(!memcmp(&s,&before,sizeof s));
    in.directional=false;in.local_right=NAN;CHECK(pe_player_locomotion_step(&s,in,.1));CHECK(s.clip==PE_LOCOMOTION_WALK);
    before=s;CHECK(pe_player_locomotion_step(&s,in,0));CHECK(!memcmp(&s,&before,sizeof s));
    for(int i=0;i<PE_LOCOMOTION_COUNT;i++){
        double keep=d[i];d[i]=0;CHECK(!pe_player_locomotion_init(&s,d));CHECK(!memcmp(&s,&before,sizeof s));d[i]=keep;
    }
    /* Initial/reset snapshot snaps to idle; read-only queries and paused steps
     * cannot advance a presentation clock or begin a transition. */
    PePlayerLocomotionPose pose,old_pose,unchanged;
    CHECK(pe_player_locomotion_init(&s,d));before=s;
    CHECK(pe_player_locomotion_snapshot(&s,&pose));CHECK(!memcmp(&s,&before,sizeof s));
    for(int i=0;i<PE_LOCOMOTION_COUNT;i++){CHECK(pose.weights[i]==(i==PE_LOCOMOTION_IDLE?1:0));CHECK(pose.seconds[i]==0);}
    in=(PePlayerLocomotionInput){.grounded=true,.directional=true,.walk=true,.velocity={2,0,0},.local_right=-2};
    CHECK(pe_player_locomotion_step(&s,in,0));CHECK(!memcmp(&s,&before,sizeof s));
    /* A first16ms direction change retains most of the outgoing idle pose;
     * all contributors remain bounded and normalized. Outgoing idle advances. */
    CHECK(pe_player_locomotion_step(&s,in,.016));CHECK(pe_player_locomotion_snapshot(&s,&pose));
    double decay=exp(-.016/PE_LOCOMOTION_BLEND_TAU);
    CHECK(near(pose.weights[PE_LOCOMOTION_IDLE],decay));
    CHECK(near(pose.weights[PE_LOCOMOTION_WALK_LEFT],1-decay));
    CHECK(near(pose.seconds[PE_LOCOMOTION_IDLE],.016));CHECK(pose.seconds[PE_LOCOMOTION_WALK_LEFT]==0);
    for(int k=0;k<100;k++)CHECK(pe_player_locomotion_step(&s,in,.01));
    CHECK(pe_player_locomotion_snapshot(&s,&old_pose));
    CHECK(old_pose.weights[PE_LOCOMOTION_WALK_LEFT]>.999);
    /* Opposite direction, then immediate reversal: weights remain a mixture
     * and the original contributor keeps advancing instead of being reset. */
    in.local_right=2;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(pe_player_locomotion_snapshot(&s,&pose));
    CHECK(pose.weights[PE_LOCOMOTION_WALK_LEFT]>.9&&pose.weights[PE_LOCOMOTION_WALK_RIGHT]>0);
    double expected_phase=fmod(old_pose.seconds[PE_LOCOMOTION_WALK_RIGHT]/d[PE_LOCOMOTION_WALK_RIGHT]+.01/d[PE_LOCOMOTION_WALK_RIGHT],1);
    CHECK(near(pose.seconds[PE_LOCOMOTION_WALK_LEFT]/d[PE_LOCOMOTION_WALK_LEFT],expected_phase));
    old_pose=pose;in.local_right=-2;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(pe_player_locomotion_snapshot(&s,&pose));
    expected_phase=fmod(old_pose.seconds[PE_LOCOMOTION_WALK_LEFT]/d[PE_LOCOMOTION_WALK_LEFT]+.01/d[PE_LOCOMOTION_WALK_LEFT],1);
    CHECK(near(pose.seconds[PE_LOCOMOTION_WALK_LEFT]/d[PE_LOCOMOTION_WALK_LEFT],expected_phase));
    CHECK(pose.weights[PE_LOCOMOTION_WALK_RIGHT]>0&&pose.weights[PE_LOCOMOTION_WALK_RIGHT]<old_pose.weights[PE_LOCOMOTION_WALK_RIGHT]);
    /* Alternating all eight gaits cannot restart foot phase or create weight
     * spikes. Every phase is shared even when durations/rates differ. */
    for(int k=0;k<3000;k++){
        old_pose=pose;in.walk=(k%2)==0;in.velocity.x=in.walk?2:5;
        in.local_right=(k%3)==0?0:((k%3)==1?-in.velocity.x:in.velocity.x);
        in.local_forward=(k%3)==0?((k%2)?-in.velocity.x:in.velocity.x):0;
        CHECK(pe_player_locomotion_step(&s,in,.008));CHECK(pe_player_locomotion_snapshot(&s,&pose));
        double total=0,phase_now=pose.seconds[PE_LOCOMOTION_WALK]/d[PE_LOCOMOTION_WALK];
        for(int i=0;i<PE_LOCOMOTION_COUNT;i++){
            CHECK(isfinite(pose.weights[i])&&pose.weights[i]>=0&&pose.weights[i]<=1);
            CHECK(fabs(pose.weights[i]-old_pose.weights[i])<=1-exp(-.008/PE_LOCOMOTION_BLEND_TAU)+1e-12);
            CHECK(pose.seconds[i]>=0&&pose.seconds[i]<=d[i]);total+=pose.weights[i];
            if(i==PE_LOCOMOTION_WALK||i==PE_LOCOMOTION_RUN||i>=PE_LOCOMOTION_WALK_LEFT)CHECK(near(pose.seconds[i]/d[i],phase_now));
        }
        CHECK(fabs(total-1)<1e-12);
    }
    /* Constant-target exponential weights compose across frame partitions. */
    PePlayerLocomotion whole=s,parts=s;
    CHECK(pe_player_locomotion_step(&whole,in,.12));
    for(int k=0;k<12;k++)CHECK(pe_player_locomotion_step(&parts,in,.01));
    for(int i=0;i<PE_LOCOMOTION_COUNT;i++)CHECK(near(whole.blend.weights[i],parts.blend.weights[i]));
    /* Outgoing non-looping Jump continues toward its end while Fall fades in. */
    CHECK(pe_player_locomotion_init(&s,d));in=(PePlayerLocomotionInput){.jump_started=true};
    CHECK(pe_player_locomotion_step(&s,in,.01));in.jump_started=false;
    CHECK(pe_player_locomotion_step(&s,in,.84));CHECK(pe_player_locomotion_snapshot(&s,&old_pose));
    CHECK(pe_player_locomotion_step(&s,in,.02));CHECK(pe_player_locomotion_snapshot(&s,&pose));
    CHECK(s.clip==PE_LOCOMOTION_FALL&&pose.weights[PE_LOCOMOTION_JUMP]>0&&pose.weights[PE_LOCOMOTION_FALL]>0);
    CHECK(near(pose.seconds[PE_LOCOMOTION_JUMP],old_pose.seconds[PE_LOCOMOTION_JUMP]+.02));
    CHECK(pe_player_locomotion_step(&s,in,.2));CHECK(pe_player_locomotion_snapshot(&s,&pose));CHECK(pose.seconds[PE_LOCOMOTION_JUMP]==d[PE_LOCOMOTION_JUMP]);
    /* Re-entering a one-shot must replay it; its decaying positive weight is
     * not permission to leave a second Jump/Land stuck on the first final pose. */
    in.grounded=true;CHECK(pe_player_locomotion_step(&s,in,.01));
    CHECK(pe_player_locomotion_step(&s,in,.69));CHECK(pe_player_locomotion_step(&s,in,.02));
    CHECK(pe_player_locomotion_snapshot(&s,&pose));CHECK(pose.seconds[PE_LOCOMOTION_JUMP]==d[PE_LOCOMOTION_JUMP]);
    in.grounded=false;in.jump_started=true;CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(pe_player_locomotion_snapshot(&s,&pose));
    CHECK(s.clip==PE_LOCOMOTION_JUMP&&pose.seconds[PE_LOCOMOTION_JUMP]==0);
    in.jump_started=false;CHECK(pe_player_locomotion_step(&s,in,.4));in.grounded=true;
    CHECK(pe_player_locomotion_step(&s,in,.01));CHECK(pe_player_locomotion_snapshot(&s,&pose));
    CHECK(s.clip==PE_LOCOMOTION_LAND&&pose.seconds[PE_LOCOMOTION_LAND]==0);
    /* Malformed snapshots are rejected atomically by both update and query. */
    before=s;unchanged=pose;
    CHECK(!pe_player_locomotion_snapshot(NULL,&pose));CHECK(!memcmp(&pose,&unchanged,sizeof pose));
    CHECK(!pe_player_locomotion_snapshot(&s,NULL));
    for(int kind=0;kind<5;kind++){
        PePlayerLocomotion bad=before;
        if(kind==0)bad.blend.weights[PE_LOCOMOTION_IDLE]=NAN;
        if(kind==1)bad.blend.weights[PE_LOCOMOTION_IDLE]=-.1;
        if(kind==2)memset(bad.blend.weights,0,sizeof bad.blend.weights);
        if(kind==3)bad.blend.seconds[PE_LOCOMOTION_RUN]=d[PE_LOCOMOTION_RUN]+1;
        if(kind==4)bad.blend_gait=PE_LOCOMOTION_IDLE;
        PePlayerLocomotion saved=bad;
        CHECK(!pe_player_locomotion_step(&bad,in,.01));CHECK(!memcmp(&bad,&saved,sizeof bad));
        CHECK(!pe_player_locomotion_snapshot(&bad,&pose));CHECK(!memcmp(&pose,&unchanged,sizeof pose));
    }
    CHECK(pe_player_locomotion_init(&s,d));CHECK(pe_player_locomotion_snapshot(&s,&pose));
    CHECK(pose.weights[PE_LOCOMOTION_IDLE]==1&&pose.weights[PE_LOCOMOTION_JUMP]==0&&pose.seconds[PE_LOCOMOTION_IDLE]==0);
    CHECK(!strcmp(pe_player_locomotion_name(PE_LOCOMOTION_WALK_LEFT),"NormalWL"));
    CHECK(!strcmp(pe_player_locomotion_name(PE_LOCOMOTION_RUN_BACK),"NormalRB"));
    CHECK(!pe_player_locomotion_name(PE_LOCOMOTION_COUNT));return failures;
}
