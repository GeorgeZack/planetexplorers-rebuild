#include "pe_grounder.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned failures;
#define CHECK(v) do{if(!(v)){fprintf(stderr,"grounder:%d %s\n",__LINE__,#v);failures++;}}while(0)
#define NEAR(a,b,e) CHECK(fabsf((a)-(b))<(e))
enum {LEFT,RIGHT};
/* Ground is y = base + slope*z, lowered by `drop` where x < 0. No ground at all when `none`. */
typedef struct {float base,slope,drop; bool none; unsigned casts;} Ground;
static bool ray(void *context,PeVec3 origin,PeVec3 direction,float max_distance,PeGrounderHit *hit){
    Ground *g=context;g->casts++;
    if(g->none||direction.y!=-1)return false;
    float y=g->base+g->slope*origin.z-(origin.x<0?g->drop:0),distance=origin.y-y;
    if(distance<0||distance>max_distance)return false;
    float m=sqrtf(1+g->slope*g->slope);
    *hit=(PeGrounderHit){.point={origin.x,y,origin.z},.normal={0,1/m,-g->slope/m},.distance=distance};
    return true;
}
static PeGrounderInput frame(Ground *ground,float time,float dt){
    return (PeGrounderInput){.time=time,.delta_time=dt,.enabled=true,.weight=1,.root_position={0,1,0},.root_forward={0,0,1},.root_right={1,0,0},
        .root_up={0,1,0},.foot={{-.1f,1.08f,0},{.1f,1.08f,0}},.ray=ray,.context=ground};
}
static PeGrounder fresh(void){
    PeGrounder g;memset(&g,0,sizeof g);
    CHECK(pe_grounder_init(&g,pe_grounder_female_settings(),(PeGrounderStart){.delta_time=.02f,.root_position={0,1,0},.foot={{-.1f,1.08f,0},{.1f,1.08f,0}}}));
    return g;
}
/* Run to a steady state at 50 Hz. */
static PeGrounderOutput settle(PeGrounder *g,Ground *ground,unsigned frames){
    PeGrounderOutput out={0};
    for(unsigned i=1;i<=frames;i++){PeGrounderInput in=frame(ground,.02f+.02f*i,.02f);CHECK(pe_grounder_update(g,&in,&out));}
    return out;
}
static float angle(PeFbbikQuat q){return 2*acosf(fminf(1,fabsf(q.w)))*57.29578f;}

int pe_test_grounder(void){
    failures=0;
    const PeGrounderSettings *s=pe_grounder_female_settings();
    CHECK(s->max_step==.4f&&s->foot_speed==2.5f&&s->foot_radius==.1f&&s->prediction==.05f&&s->foot_rotation_speed==10);
    CHECK(s->pelvis_speed==2&&s->pelvis_damper==.01f&&s->lower_pelvis_weight==.2f&&s->lift_pelvis_weight==.8f&&s->spine_bend==4.5f&&s->spine_speed==2);
    CHECK(s->spine_horizontal[1]==.8f&&s->spine_horizontal[2]==.8f&&s->spine_horizontal[0]==0&&s->spine_vertical[1]==0);
    NEAR(s->max_foot_rotation_angle,26.70706f,1e-5f);

    /* Motion_Move_Human's curve: 5 on flat ground up to 40 at a right angle, clamped beyond.
     * Interior values are from an exact rational evaluation of the serialized keys. */
    float limit=-1;
    static const float at[][2]={{-90,40},{-60,30},{0,5},{60,30},{90,40},{-200,40},{1e6f,40},
        {-75,34.7250736f},{-30,18.0498528f},{10,6.95450430f},{45,26.7173636f},{75,34.7228384f}};
    for(unsigned i=0;i<sizeof at/sizeof *at;i++){CHECK(pe_grounder_foot_rotation_limit(at[i][0],&limit));NEAR(limit,at[i][1],2e-5f);}
    limit=7;CHECK(!pe_grounder_foot_rotation_limit(NAN,&limit)&&limit==7&&!pe_grounder_foot_rotation_limit(0,NULL));
    /* The original mutates the component before the update; so does the caller here. */
    PeGrounder live={0};CHECK(!pe_grounder_set_live(&live,0,true));
    live=fresh();CHECK(pe_grounder_set_live(&live,0,false)&&live.settings.max_foot_rotation_angle==5&&live.settings.spine_bend==0);
    CHECK(pe_grounder_set_live(&live,60,true)&&live.settings.max_foot_rotation_angle==30&&live.settings.spine_bend==4.5f);
    CHECK(!pe_grounder_set_live(&live,INFINITY,false)&&live.settings.max_foot_rotation_angle==30&&live.settings.spine_bend==4.5f);
    CHECK(!pe_grounder_set_live(NULL,0,true));

    /* Root standing on flat ground: nothing is requested, three rays per leg and one for the root. */
    Ground flat={.base=1};PeGrounder g=fresh();PeGrounderOutput out=settle(&g,&flat,1);
    CHECK(out.ran&&out.grounded&&out.root_grounded&&flat.casts==7);
    CHECK(out.pelvis_offset.y==0&&out.foot_offset[LEFT].y==0&&out.foot_offset[RIGHT].y==0&&angle(out.foot_rotation[LEFT])<1e-3f);
    for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++)CHECK(out.effector_offset[e].x==0&&out.effector_offset[e].z==0);

    /* Ground 0.1 below the root. One frame: LerpValue moves 0.05, Lerp a further 5% of the rest;
     * the pelvis takes dt*pelvisSpeed of the summed target. */
    Ground low={.base=.9f};g=fresh();out=settle(&g,&low,1);
    NEAR(g.leg[LEFT].height_from_ground,.1f,1e-6f);NEAR(g.leg[LEFT].ik_offset,.0525f,1e-6f);
    NEAR(out.pelvis_offset.y,-.0525f*.04f,1e-6f);NEAR(out.foot_offset[LEFT].y,-.0525f+.0021f,1e-6f);
    /* Steady state: the pelvis lowers by the full gap and the feet are left where the animation put them. */
    out=settle(&g,&low,600);
    NEAR(g.leg[LEFT].ik_offset,.1f,1e-5f);NEAR(out.pelvis_offset.y,-.1f,1e-4f);NEAR(out.foot_offset[LEFT].y,0,1e-4f);NEAR(out.foot_offset[RIGHT].y,0,1e-4f);

    /* A step: the left foot's ground is 0.2 lower. The pelvis takes 20% of the larger offset and
     * 80% of the smaller, so it drops 0.04, the left foot reaches down and the right is lifted. */
    Ground step={.base=1,.drop=.2f};g=fresh();out=settle(&g,&step,600);
    NEAR(g.leg[LEFT].ik_offset,.2f,1e-5f);NEAR(g.leg[RIGHT].ik_offset,0,1e-5f);
    NEAR(out.pelvis_offset.y,-.04f,1e-4f);NEAR(out.foot_offset[LEFT].y,-.16f,1e-4f);NEAR(out.foot_offset[RIGHT].y,.04f,1e-4f);
    /* The spine leans by the shoulders only, horizontally, at 0.8 of the stored offset. */
    CHECK(fabsf(g.spine_offset.x)>1e-3f&&g.spine_offset.y==0);
    NEAR(out.effector_offset[1].x,g.spine_offset.x*.8f,1e-7f);NEAR(out.effector_offset[2].x,g.spine_offset.x*.8f,1e-7f);
    CHECK(out.effector_offset[0].x==0&&out.effector_offset[5].x==0&&out.effector_offset[7].x==0&&out.effector_offset[1].y==0);
    /* Half weight halves the pelvis and foot requests; state advances the same way. */
    PeGrounder half=fresh();PeGrounderOutput weak={0};
    for(unsigned i=1;i<=600;i++){PeGrounderInput in=frame(&step,.02f+.02f*i,.02f);in.weight=.5f;CHECK(pe_grounder_update(&half,&in,&weak));}
    NEAR(half.leg[LEFT].ik_offset,.2f,1e-5f);NEAR(weak.pelvis_offset.y,-.02f,1e-4f);NEAR(weak.foot_offset[LEFT].y,(-.2f+.02f)*.5f,1e-4f);

    /* A step deeper than maxStep is clamped to it and that foot no longer counts as grounded. */
    Ground deep={.base=1,.drop=.6f};g=fresh();out=settle(&g,&deep,600);
    NEAR(g.leg[LEFT].ik_offset,.4f,1e-5f);NEAR(g.leg[LEFT].height_from_ground,.6f,1e-6f);
    CHECK(!g.leg[LEFT].grounded&&g.leg[RIGHT].grounded&&out.grounded);
    /* Ground above the root lifts the feet, but no higher than maxStep less the foot's own height. */
    Ground high={.base=1.35f};g=fresh();out=settle(&g,&high,600);
    NEAR(g.leg[LEFT].height_from_ground,-.35f,1e-6f);NEAR(g.leg[LEFT].ik_offset,-.32f,1e-6f);

    /* A slope tilts the foot toward the ground normal; a steep one stops at maxFootRotationAngle. */
    Ground gentle={.base=1,.slope=.17632698f};g=fresh();out=settle(&g,&gentle,600);   /* 10 degrees */
    NEAR(angle(out.foot_rotation[LEFT]),10,.02f);CHECK(out.foot_rotation[LEFT].x<0);
    Ground steep={.base=1,.slope=.83909963f};g=fresh();out=settle(&g,&steep,600);     /* 40 degrees */
    NEAR(angle(out.foot_rotation[LEFT]),26.70706f,.02f);

    /* No ground: the root is not grounded, every target is zero and the damper stays off. */
    Ground none={.none=true};g=fresh();out=settle(&g,&none,50);
    CHECK(out.ran&&!out.root_grounded&&!out.grounded&&g.leg[LEFT].ik_offset==0&&out.pelvis_offset.y==0&&g.pelvis_damper==0);
    NEAR(g.leg[LEFT].height_from_ground,.72f,1e-6f);            /* root over the miss point two steps below the foot */

    /* The pelvis damper: a root rising 0.1 on flat ground pulls the pelvis back by 1% of the rise. */
    g=fresh();settle(&g,&flat,100);CHECK(g.pelvis_damper==1);
    PeGrounderInput risen=frame(&flat,.02f+.02f*101,.02f);risen.root_position.y+=.1f;risen.foot[LEFT].y+=.1f;risen.foot[RIGHT].y+=.1f;
    Ground raised={.base=1.1f};risen.context=&raised;
    CHECK(pe_grounder_update(&g,&risen,&out));NEAR(out.pelvis_offset.y,-.001f,1e-6f);

    /* The first update measures time from the stored Time.deltaTime, as the original does. */
    g=fresh();PeGrounderInput first=frame(&low,.52f,.02f);CHECK(pe_grounder_update(&g,&first,&out));
    NEAR(g.leg[LEFT].ik_offset,.1f,1e-6f);                       /* LerpValue 0.05, then Lerp with t = 0.5*2.5 clamped to 1 */
    /* A repeated Time.time leaves the legs and pelvis as they were. */
    PeGrounder before=g;CHECK(pe_grounder_update(&g,&first,&out));
    CHECK(g.leg[LEFT].ik_offset==before.leg[LEFT].ik_offset&&g.pelvis_height==before.pelvis_height);

    /* Disabled or zero weight requests nothing and advances nothing. */
    g=fresh();settle(&g,&step,20);before=g;
    PeGrounderInput off=frame(&step,5,.02f);off.enabled=false;CHECK(pe_grounder_update(&g,&off,&out)&&!out.ran&&!memcmp(&g,&before,sizeof g));
    off.enabled=true;off.weight=0;CHECK(pe_grounder_update(&g,&off,&out)&&!out.ran&&out.foot_rotation[LEFT].w==1&&!memcmp(&g,&before,sizeof g));

    /* Invalid input fails without touching state or output. */
    PeGrounderOutput kept=out;PeGrounderInput bad=frame(&step,6,.02f);
    bad.ray=0;CHECK(!pe_grounder_update(&g,&bad,&out));
    bad=frame(&step,6,.02f);bad.foot[RIGHT].y=NAN;CHECK(!pe_grounder_update(&g,&bad,&out));
    bad=frame(&step,6,.02f);bad.time=INFINITY;CHECK(!pe_grounder_update(&g,&bad,&out));
    CHECK(!pe_grounder_update(0,&bad,&out)&&!memcmp(&g,&before,sizeof g)&&!memcmp(&out,&kept,sizeof out));
    PeGrounder blank;memset(&blank,0,sizeof blank);bad=frame(&step,6,.02f);CHECK(!pe_grounder_update(&blank,&bad,&out));
    PeGrounderSettings broken=*s;broken.max_step=0;
    CHECK(!pe_grounder_init(&blank,&broken,(PeGrounderStart){.delta_time=.02f})&&!blank.initiated);
    broken=*s;broken.spine_bend=NAN;CHECK(!pe_grounder_init(&blank,&broken,(PeGrounderStart){.delta_time=.02f}));
    CHECK(!pe_grounder_init(&blank,s,(PeGrounderStart){.delta_time=NAN})&&!pe_grounder_init(0,s,(PeGrounderStart){0}));
    return failures?1:0;
}
