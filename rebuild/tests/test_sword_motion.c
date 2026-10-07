#include "pe_sword_motion.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sword motion:%d %s\n",__LINE__,#c);failures++;}}while(0)
static bool near(PeVec3 a,PeVec3 b){return fabsf(a.x-b.x)<1e-5f&&fabsf(a.y-b.y)<1e-5f&&fabsf(a.z-b.z)<1e-5f;}
static void rejected(const double delta[3],PeVec3 velocity,double dt){
    PeVec3 out={9,8,7},before=out;
    CHECK(!pe_sword_motion_request(delta,velocity,dt,&out)&&!memcmp(&before,&out,sizeof out));
}
int pe_test_sword_motion(void){
    failures=0;PeVec3 out={0};const double d[]={.2,900,.4};
    CHECK(pe_sword_motion_request(d,(PeVec3){0,-1000,0},.1,&out)&&near(out,(PeVec3){2,0,4}));
    CHECK(pe_sword_motion_request(d,(PeVec3){20,1,4},.1,&out)&&near(out,(PeVec3){8,0,4}));
    CHECK(pe_sword_motion_request(d,(PeVec3){14,-2,4},.1,&out)&&near(out,(PeVec3){2,0,4}));
    /* Threshold is inclusive and limits change, never total requested speed. */
    const double fast[]={30,0,40};
    CHECK(pe_sword_motion_request(fast,(PeVec3){3,0,4},1,&out)&&near(out,(PeVec3){10.2f,0,13.6f}));
    CHECK(pe_sword_motion_request(fast,(PeVec3){0},1,&out)&&near(out,(PeVec3){7.2f,0,9.6f}));
    PeVec3 capped=out;
    CHECK(pe_sword_motion_request(fast,(PeVec3){0},.5,&out)&&near(out,capped));
    const double zero[]={0,-99,0};
    CHECK(pe_sword_motion_request(zero,(PeVec3){5,9,0},.01,&out)&&near(out,(PeVec3){0}));
    CHECK(pe_sword_motion_request(zero,(PeVec3){20,9,0},.01,&out)&&near(out,(PeVec3){8,0,0}));
    CHECK(pe_sword_motion_request(zero,out,.01,&out)&&near(out,(PeVec3){0}));
    /* Incoming displacement is already world oriented; there is no hidden yaw. */
    const double west[]={-.2,0,.1};
    CHECK(pe_sword_motion_request(west,(PeVec3){0},.1,&out)&&near(out,(PeVec3){-2,0,1}));
    const double invalid[]={NAN,INFINITY,-INFINITY};
    for(unsigned i=0;i<3;i++){
        rejected(d,(PeVec3){0},invalid[i]);
        for(unsigned axis=0;axis<3;axis++){
            double bad[]={0,0,0};bad[axis]=invalid[i];rejected(bad,(PeVec3){0},1);
            PeVec3 velocity={0};if(axis==0)velocity.x=(float)invalid[i];else if(axis==1)velocity.y=(float)invalid[i];else velocity.z=(float)invalid[i];
            rejected(d,velocity,1);
        }
    }
    rejected(d,(PeVec3){0},0);rejected(d,(PeVec3){0},-1);rejected(d,(PeVec3){0},DBL_MIN*.01);
    const double overflow[]={DBL_MAX,0,DBL_MAX};rejected(overflow,(PeVec3){0},1);
    rejected(NULL,(PeVec3){0},1);CHECK(!pe_sword_motion_request(d,(PeVec3){0},1,NULL));
    CHECK(d[0]==.2&&d[1]==900&&d[2]==.4);
    return failures;
}
