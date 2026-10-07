#include "pe_melee_facing.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"melee-facing:%d %s\n",__LINE__,#c);failures++;}}while(0)
static const double radians=3.14159265358979323846/180.;
static bool near(float a,float b){return fabs(remainder((double)a-b,360.))<.0001;}
static PeVec3 direction(float yaw){return (PeVec3){(float)sin(yaw*radians),0,(float)cos(yaw*radians)};}
/* Independent relative-rotation oracle: the argument of a chord on the
 * unit complex half-angle circle. Production blends absolute quaternions. */
static float oracle(float from,float to,float t){
    double delta=remainder((double)to-from,360.)*radians*.5;
    double change=2.*atan2(t*sin(delta),1.-t+t*cos(delta))/radians;
    return (float)remainder(from+change,360.);
}

int pe_test_melee_facing(void){
    failures=0;
    PeVec3 out={0};
    CHECK(pe_melee_facing_direction((PeVec3){3,17,4},-123,&out));
    CHECK(fabsf(out.x-.6f)<1e-7f&&out.y==0&&fabsf(out.z-.8f)<1e-7f);
    CHECK(pe_melee_facing_direction((PeVec3){0},90,&out));
    CHECK(fabsf(out.x-1)<1e-7f&&fabsf(out.z)<1e-7f);
    CHECK(pe_melee_facing_direction((PeVec3){0,123,0},-90,&out));
    CHECK(fabsf(out.x+1)<1e-7f&&out.y==0&&fabsf(out.z)<1e-7f);
    CHECK(pe_melee_facing_direction((PeVec3){1e-6f,0,0},35,&out));
    CHECK(fabsf(out.x-direction(35).x)<1e-7f);
    CHECK(pe_melee_facing_direction((PeVec3){2e-5f,0,0},35,&out));
    CHECK(out.x==1&&out.z==0);
    CHECK(pe_melee_facing_direction((PeVec3){FLT_MAX,0,FLT_MAX},35,&out));
    CHECK(fabsf(out.x-.70710678f)<1e-7f&&out.x==out.z);
    CHECK(pe_melee_facing_direction((PeVec3){0},FLT_MAX,&out));
    CHECK(isfinite(out.x)&&isfinite(out.z)&&fabs(hypot(out.x,out.z)-1)<1e-6);

    float yaw=456;
    CHECK(pe_melee_facing_yaw(725,direction(90),NULL,0,&yaw)&&yaw==725);
    CHECK(pe_melee_facing_yaw(0,direction(90),NULL,1.f/60,&yaw)&&near(yaw,90));
    CHECK(pe_melee_facing_yaw(0,direction(-90),NULL,FLT_MAX,&yaw)&&near(yaw,-90));
    CHECK(pe_melee_facing_yaw(0,(PeVec3){0,0,-1},NULL,1.f/120,&yaw)&&near(yaw,90));
    CHECK(pe_melee_facing_yaw(170,direction(-170),NULL,1.f/120,&yaw)&&near(yaw,180));
    CHECK(pe_melee_facing_yaw(-170,direction(170),NULL,1.f/120,&yaw)&&near(yaw,-180));
    CHECK(pe_melee_facing_yaw(0,direction(160),NULL,.25f/60,&yaw));
    CHECK(near(yaw,oracle(0,160,.25f))&&fabsf(yaw-40)>1); /* Not yaw lerp. */
    for(int from=-170;from<=170;from+=34)for(int to=-175;to<=175;to+=35)
        for(int step=0;step<=10;step++){
            if(fabs(remainder((double)to-from,360.))==180)continue;
            float dt=step/600.f;
            CHECK(pe_melee_facing_yaw((float)from,direction((float)to),NULL,dt,&yaw));
            CHECK(near(yaw,oracle((float)from,(float)to,fminf(1,60*dt))));
        }
    PeSwordTargetResult target={.selected=true,.aim_active=true,.direction={1,6,0}};
    PeSwordTargetResult before=target;
    CHECK(pe_melee_facing_yaw(0,direction(-90),&target,0,&yaw)&&near(yaw,90));
    CHECK(!memcmp(&target,&before,sizeof target)); /* A read-only frozen stage target. */
    target.direction=(PeVec3){0,1,0};
    CHECK(pe_melee_facing_yaw(27,direction(-90),&target,.1f,&yaw)&&yaw==27);
    target.direction=(PeVec3){0};target.aim_active=false;
    CHECK(pe_melee_facing_yaw(0,direction(-90),&target,.1f,&yaw)&&near(yaw,-90));
    target=(PeSwordTargetResult){0};
    CHECK(pe_melee_facing_yaw(0,direction(90),&target,.1f,&yaw)&&near(yaw,90));

    PeVec3 sentinel={7,8,9};out=sentinel;
    CHECK(!pe_melee_facing_direction((PeVec3){NAN,0,1},0,&out));
    CHECK(!memcmp(&out,&sentinel,sizeof out));
    CHECK(!pe_melee_facing_direction((PeVec3){1,INFINITY,1},0,&out));
    CHECK(!memcmp(&out,&sentinel,sizeof out));
    CHECK(!pe_melee_facing_direction((PeVec3){1,0,1},NAN,&out));
    CHECK(!memcmp(&out,&sentinel,sizeof out));
    CHECK(!pe_melee_facing_direction((PeVec3){1,0,1},0,NULL));
    yaw=456;
    CHECK(!pe_melee_facing_yaw(NAN,direction(90),NULL,.1f,&yaw)&&yaw==456);
    CHECK(!pe_melee_facing_yaw(0,(PeVec3){NAN,0,1},NULL,.1f,&yaw)&&yaw==456);
    CHECK(!pe_melee_facing_yaw(0,(PeVec3){0},NULL,.1f,&yaw)&&yaw==456);
    CHECK(!pe_melee_facing_yaw(0,(PeVec3){0,1,0},NULL,.1f,&yaw)&&yaw==456);
    CHECK(!pe_melee_facing_yaw(0,direction(90),NULL,-.1f,&yaw)&&yaw==456);
    CHECK(!pe_melee_facing_yaw(0,direction(90),NULL,INFINITY,&yaw)&&yaw==456);
    CHECK(!pe_melee_facing_yaw(0,direction(90),NULL,NAN,&yaw)&&yaw==456);
    CHECK(!pe_melee_facing_yaw(0,direction(90),NULL,.1f,NULL));
    target=(PeSwordTargetResult){.aim_active=true,.direction={1,0,0}};
    CHECK(!pe_melee_facing_yaw(0,direction(90),&target,.1f,&yaw)&&yaw==456);
    target=(PeSwordTargetResult){.selected=true,.aim_active=true,.direction={1,NAN,0}};
    CHECK(!pe_melee_facing_yaw(0,direction(90),&target,.1f,&yaw)&&yaw==456);
    printf("melee facing checks: %s\n",failures?"FAIL":"PASS");
    return failures;
}
