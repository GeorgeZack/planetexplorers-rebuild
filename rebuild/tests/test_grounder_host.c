#include "pe_grounder_host.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned failures;
#define CHECK(v) do{if(!(v)){fprintf(stderr,"grounder_host:%d %s\n",__LINE__,#v);failures++;}}while(0)
#define NEAR(a,b,e) CHECK(fabsf((a)-(b))<(e))
enum {LEFT_FOOT=7,RIGHT_FOOT=8};
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
/* Root on the ground at the origin; feet .1 either side in the model, .08 up. */
static PeGrounderHostInput frame(Ground *ground,float time,float yaw){
    double r=yaw*3.14159265358979323846/180;float c=(float)cos(r),s=(float)sin(r),m=sqrtf(1+ground->slope*ground->slope);
    return (PeGrounderHostInput){.time=time,.delta_time=.02f,.root={0,ground->base,0},.yaw_degrees=yaw,
        .foot={{-.1f*c,ground->base+.08f,.1f*s},{.1f*c,ground->base+.08f,-.1f*s}},.ground_contact=!ground->none,.ground_normal={0,1/m,-ground->slope/m},.spine_effect=true,.ray=ray,.context=ground};
}
static void settle(PeGrounderHost *h,Ground *ground,float yaw,unsigned frames){
    for(unsigned i=1;i<=frames;i++){PeGrounderHostInput in=frame(ground,.02f*i,yaw);CHECK(pe_grounder_host_update(h,&in));}
}
static float tilt(PeFbbikQuat q){return 2*acosf(fminf(1,fabsf(q.w)))*57.29578f;}

int pe_test_grounder_host(void){
    failures=0;
    /* Level ground: active, nothing requested, the level 5 degree limit and the full spine bend. */
    Ground flat={.base=1};PeGrounderHost h;memset(&h,0,sizeof h);settle(&h,&flat,0,50);
    CHECK(h.active&&h.started&&h.grounded&&h.root_grounded);
    NEAR(h.forward_ground_angle,0,1e-4f);NEAR(h.grounder.settings.max_foot_rotation_angle,5,1e-4f);CHECK(h.grounder.settings.spine_bend==4.5f);
    NEAR(h.pose.pelvis_offset.y,0,1e-5f);NEAR(h.effector_offset[LEFT_FOOT].y,0,1e-5f);NEAR(h.effector_offset[RIGHT_FOOT].y,0,1e-5f);
    CHECK(tilt(h.pose.foot_rotation[0])<1e-3f&&tilt(h.pose.foot_rotation[1])<1e-3f);
    for(unsigned e=0;e<PE_FBBIK_EFFECTORS;e++)CHECK(h.effector_offset[e].x==0&&h.effector_offset[e].z==0);
    /* Held hand equipment holds the spine mask. */
    PeGrounderHostInput in=frame(&flat,2,0);in.spine_effect=false;CHECK(pe_grounder_host_update(&h,&in)&&h.grounder.settings.spine_bend==0);

    /* Ground rising ahead: a positive angle, the curve's limit for it, and feet tilted up to that limit. */
    Ground slope={.base=1,.slope=.3f};float degrees=atanf(.3f)*57.29578f,limit;
    pe_grounder_host_reset(&h);CHECK(!h.started);settle(&h,&slope,0,200);
    NEAR(h.forward_ground_angle,degrees,1e-2f);CHECK(pe_grounder_foot_rotation_limit(degrees,&limit));
    NEAR(h.grounder.settings.max_foot_rotation_angle,limit,1e-2f);CHECK(limit>5&&limit<degrees);
    NEAR(tilt(h.pose.foot_rotation[0]),limit,.05f);NEAR(tilt(h.pose.foot_rotation[1]),limit,.05f);
    /* The tilt axis is world x here; in the model it stays x at yaw 0 and becomes z at yaw 90. */
    CHECK(fabsf(h.pose.foot_rotation[0].x)>10*fabsf(h.pose.foot_rotation[0].z));
    /* Facing downhill the angle is negative; the limit is the same. */
    pe_grounder_host_reset(&h);settle(&h,&slope,180,200);
    NEAR(h.forward_ground_angle,-degrees,1e-2f);NEAR(tilt(h.pose.foot_rotation[0]),limit,.1f);
    /* Across the slope the ground angle is zero, so the level limit applies. */
    pe_grounder_host_reset(&h);settle(&h,&slope,90,200);
    NEAR(h.forward_ground_angle,0,1e-2f);NEAR(h.grounder.settings.max_foot_rotation_angle,5,1e-2f);
    NEAR(tilt(h.pose.foot_rotation[0]),5,.05f);CHECK(fabsf(h.pose.foot_rotation[0].z)>10*fabsf(h.pose.foot_rotation[0].x));
    /* One foot stands .03 higher and one .03 lower. The pelvis takes 0.8 of the lift and 0.2 of the
     * drop, so it rises .018, and each foot's request is what remains of its own .03. */
    NEAR(h.pose.pelvis_offset.y,.018f,1e-3f);CHECK(h.effector_offset[LEFT_FOOT].y*h.effector_offset[RIGHT_FOOT].y<0);
    NEAR(fabsf(h.effector_offset[LEFT_FOOT].y-h.effector_offset[RIGHT_FOOT].y),.06f,1e-3f);
    CHECK(h.pose.pelvis_offset.x==0&&h.pose.pelvis_offset.z==0);

    /* Without a contact the angle decays by Lerp(angle, 0, 3*dt) instead of following the surface. */
    pe_grounder_host_reset(&h);settle(&h,&slope,0,200);float before=h.forward_ground_angle;
    in=frame(&slope,5,0);in.ground_contact=false;CHECK(pe_grounder_host_update(&h,&in));NEAR(h.forward_ground_angle,before*(1-3*.02f),1e-3f);
    /* A contact 90 degrees or more from up (a wall, a ceiling) leaves the angle where it was. */
    before=h.forward_ground_angle;in=frame(&slope,5.02f,0);in.ground_normal=(PeVec3){0,0,-1};
    CHECK(pe_grounder_host_update(&h,&in));CHECK(h.forward_ground_angle==before);
    in.ground_normal=(PeVec3){0,-1,0};CHECK(pe_grounder_host_update(&h,&in));CHECK(h.forward_ground_angle==before);
    /* The angle is the contact's, not the ground under the rays: a steeper contact gives a steeper angle. */
    in=frame(&flat,5.04f,0);in.ground_normal=(PeVec3){0,.8f,-.6f};CHECK(pe_grounder_host_update(&h,&in));
    NEAR(h.forward_ground_angle,atanf(.75f)*57.29578f,1e-2f);

    /* A step: the left side is .2 lower. The pelvis drops and the feet take opposite requests. */
    Ground step={.base=1,.drop=.2f};pe_grounder_host_reset(&h);settle(&h,&step,0,600);
    CHECK(h.pose.pelvis_offset.y<-.02f&&h.pose.pelvis_offset.y>-.2f);
    CHECK(h.effector_offset[LEFT_FOOT].y<-.05f&&h.effector_offset[RIGHT_FOOT].y>.01f);
    NEAR(h.pose.pelvis_offset.y+h.effector_offset[LEFT_FOOT].y,-.2f,2e-3f);NEAR(h.pose.pelvis_offset.y+h.effector_offset[RIGHT_FOOT].y,0,2e-3f);

    /* Nothing underfoot: still an update, not grounded, and the requests relax. */
    Ground none={.base=1,.none=true};pe_grounder_host_reset(&h);settle(&h,&none,0,5);CHECK(h.active&&!h.grounded&&!h.root_grounded);

    /* Invalid input clears active and keeps the state; a reset initiates again. */
    pe_grounder_host_reset(&h);settle(&h,&step,0,10);PeGrounderHost kept=h;
    in=frame(&step,1,0);in.delta_time=0;CHECK(!pe_grounder_host_update(&h,&in)&&!h.active);
    h.active=kept.active;CHECK(!memcmp(&h,&kept,sizeof h));
    in=frame(&step,1,0);in.root.x=NAN;CHECK(!pe_grounder_host_update(&h,&in)&&!h.active);
    in=frame(&step,1,0);in.ray=NULL;CHECK(!pe_grounder_host_update(&h,&in));
    in=frame(&step,1,0);in.ground_normal.y=NAN;CHECK(!pe_grounder_host_update(&h,&in));
    in=frame(&step,1,0);in.yaw_degrees=INFINITY;CHECK(!pe_grounder_host_update(&h,&in));
    in=frame(&step,1,0);CHECK(!pe_grounder_host_update(NULL,&in)&&!pe_grounder_host_update(&h,NULL));
    pe_grounder_host_reset(&h);CHECK(!h.started&&!h.active);pe_grounder_host_reset(NULL);
    return failures?1:0;
}
