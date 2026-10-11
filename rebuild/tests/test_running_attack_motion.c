#include "pe_test_paths.h"
#include "pe_running_attack_motion.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"running-attack-motion:%d %s\n",__LINE__,#c);failures++;}}while(0)
static void u32(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static void f64(unsigned char *p,double n){uint64_t bits;memcpy(&bits,&n,8);for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(bits>>(8*i));}
static PeRootMotion *fixture(const char *path){
    unsigned char bytes[32+185*32]={0};memcpy(bytes,"PERM",4);
    u32(bytes+4,1);u32(bytes+8,408);u32(bytes+12,20110014);u32(bytes+16,185);f64(bytes+24,PE_ROOT_MOTION_DURATION);
    /* Synthetic parser/math fixture only: no claim this is original motion. */
    for(unsigned i=0;i<185;i++){
        double t=i*.01;unsigned char *row=bytes+32+i*32;
        f64(row,t);f64(row+8,2*t);f64(row+16,-3*t);f64(row+24,4*t);
    }
    FILE *f=fopen(path,"wb");CHECK(f);if(!f)return NULL;
    CHECK(fwrite(bytes,1,sizeof bytes,f)==sizeof bytes);CHECK(fclose(f)==0);
    char error[128];PeRootMotion *m=pe_root_motion_load(path,error,sizeof error);CHECK(m);return m;
}
static bool near(PeVec3 a,PeVec3 b){return fabsf(a.x-b.x)<2e-5f&&fabsf(a.y-b.y)<2e-5f&&fabsf(a.z-b.z)<2e-5f;}
static PeCombatInterval span(double from,double to){return (PeCombatInterval){.valid=true,.variant=PE_COMBAT_RUNNING,.gender=PE_COMBAT_FEMALE,.view=PE_COMBAT_THIRD_PERSON,.stage=0,.from=from,.to=to};}
static void rejected(const PeRootMotion *m,PeCombatInterval i,PeVec3 velocity,double yaw,double dt){
    PeVec3 sentinel={98,76,54},out=sentinel;
    CHECK(!pe_running_attack_motion_request(m,&i,velocity,yaw,dt,&out));
    CHECK(!memcmp(&sentinel,&out,sizeof out));
}
int pe_test_running_attack_motion(void){
    failures=0;char path[1024];pe_test_format(path,sizeof path,"%s/rebuild/build/running-attack-motion-test-%ld.perm",pe_test_root(),(long)getpid());
    PeRootMotion *m=fixture(path);if(!m)return failures;
    PeCombatInterval i=span(.5,.6);PeVec3 out={0},velocity={0,19,0};
    CHECK(pe_running_attack_motion_request(m,&i,velocity,0,.1,&out)&&near(out,(PeVec3){2,0,4}));
    CHECK(pe_running_attack_motion_request(m,&i,velocity,90,.1,&out)&&near(out,(PeVec3){4,0,-2}));
    CHECK(pe_running_attack_motion_request(m,&i,velocity,-90,.1,&out)&&near(out,(PeVec3){-4,0,2}));
    CHECK(pe_running_attack_motion_request(m,&i,velocity,180,.1,&out)&&near(out,(PeVec3){-2,0,-4}));
    CHECK(pe_running_attack_motion_request(m,&i,velocity,450,.1,&out)&&near(out,(PeVec3){4,0,-2}));
    /* Root Y and incoming vertical velocity do not enter the horizontal cap. */
    velocity=(PeVec3){2,1000,4};CHECK(pe_running_attack_motion_request(m,&i,velocity,0,.1,&out)&&near(out,(PeVec3){2,0,4}));
    velocity=(PeVec3){0};
    CHECK(pe_running_attack_motion_request(m,&i,velocity,0,.001,&out));
    CHECK(fabs(hypot(out.x,out.z)-12)<2e-5&&fabs(out.z-2*out.x)<2e-5);
    PeVec3 fast=out;CHECK(pe_running_attack_motion_request(m,&i,velocity,0,.002,&out)&&near(out,fast));
    /*12m/s velocity change, never12*dt; cap relative to current horizontal speed. */
    velocity=(PeVec3){20,99,4};CHECK(pe_running_attack_motion_request(m,&i,velocity,0,.1,&out)&&near(out,(PeVec3){8,0,4}));
    velocity=(PeVec3){14,99,4};CHECK(pe_running_attack_motion_request(m,&i,velocity,0,.1,&out)&&near(out,(PeVec3){2,0,4}));
    i=span(.6,.6);velocity=(PeVec3){0};CHECK(pe_running_attack_motion_request(m,&i,velocity,0,.1,&out)&&near(out,(PeVec3){0}));
    velocity=(PeVec3){20,8,0};CHECK(pe_running_attack_motion_request(m,&i,velocity,0,.1,&out)&&near(out,(PeVec3){8,0,0}));
    /* A real outgoing EndAction interval remains usable after combat resets. */
    PeCombat action;pe_combat_init(&action);float stamina=100;
    CHECK(pe_combat_begin_variant(&action,PE_COMBAT_FEMALE,PE_COMBAT_THIRD_PERSON,PE_COMBAT_RUNNING,1,&stamina));
    CHECK(pe_combat_advance_interval(&action,.5,.5,&stamina,&i)==PE_COMBAT_ADVANCED);
    CHECK(pe_combat_advance_interval(&action,.5,.5,&stamina,&i)==PE_COMBAT_ADVANCED);
    CHECK(pe_combat_advance_interval(&action,.4,.4,&stamina,&i)==PE_COMBAT_ENDED);
    CHECK(!action.active&&i.valid&&i.stage==0&&i.from==1.0&&i.to>1.16&&i.to<1.17);
    velocity=(PeVec3){0};double dt=i.to-i.from;
    CHECK(pe_running_attack_motion_request(m,&i,velocity,0,dt,&out)&&near(out,(PeVec3){2,0,4}));
    PeCombatInterval retained=i;
    CHECK(pe_combat_advance_interval(&action,.1,.1,&stamina,&i)==PE_COMBAT_IDLE&&!i.valid);
    rejected(m,i,velocity,0,.1);
    CHECK(pe_running_attack_motion_request(m,&retained,velocity,0,dt,&out)&&near(out,(PeVec3){2,0,4}));
    /* At combo transition, consume outgoing stage0 rather than subtract new0. */
    CHECK(pe_combat_begin_variant(&action,PE_COMBAT_FEMALE,PE_COMBAT_THIRD_PERSON,PE_COMBAT_RUNNING,1,&stamina));
    CHECK(pe_combat_advance_interval(&action,.5,.5,&stamina,&i)==PE_COMBAT_ADVANCED);CHECK(pe_combat_queue_combo(&action));
    CHECK(pe_combat_advance_interval(&action,0,0,&stamina,&i)==PE_COMBAT_ADVANCED);
    CHECK(pe_combat_advance_interval(&action,.8,.8,&stamina,&i)==PE_COMBAT_NEXT_STAGE);
    CHECK(action.stage==1&&action.clip_time==0&&i.valid&&i.stage==0&&i.from==.5&&i.to>.5);
    CHECK(pe_running_attack_motion_request(m,&i,velocity,0,i.to-i.from,&out)&&near(out,(PeVec3){2,0,4}));
    i=span(.5,.6);PeCombatInterval bad=i;bad.valid=false;rejected(m,bad,velocity,0,.1);
    bad=i;bad.variant=PE_COMBAT_STATIONARY;rejected(m,bad,velocity,0,.1);
    bad=i;bad.gender=PE_COMBAT_MALE;rejected(m,bad,velocity,0,.1);
    bad=i;bad.view=PE_COMBAT_FIRST_PERSON;rejected(m,bad,velocity,0,.1);
    bad=i;bad.stage=1;rejected(m,bad,velocity,0,.1);
    const double invalid[]={NAN,INFINITY,-INFINITY};
    for(unsigned j=0;j<3;j++){
        rejected(m,i,velocity,invalid[j],.1);rejected(m,i,velocity,0,invalid[j]);
        bad=i;bad.from=invalid[j];rejected(m,bad,velocity,0,.1);
        bad=i;bad.to=invalid[j];rejected(m,bad,velocity,0,.1);
        for(unsigned axis=0;axis<3;axis++){PeVec3 v={0};if(axis==0)v.x=(float)invalid[j];else if(axis==1)v.y=(float)invalid[j];else v.z=(float)invalid[j];rejected(m,i,v,0,.1);}
    }
    rejected(m,i,velocity,0,0);rejected(m,i,velocity,0,-.1);rejected(m,i,velocity,0,DBL_MIN*.01);
    bad=i;bad.from=-.01;rejected(m,bad,velocity,0,.1);bad=i;bad.from=.7;rejected(m,bad,velocity,0,.1);
    bad=i;bad.to=PE_ROOT_MOTION_DURATION+.01;rejected(m,bad,velocity,0,.1);
    rejected(NULL,i,velocity,0,.1);PeVec3 sentinel={9,8,7};out=sentinel;
    CHECK(!pe_running_attack_motion_request(m,NULL,velocity,0,.1,&out)&&!memcmp(&out,&sentinel,sizeof out));
    CHECK(!pe_running_attack_motion_request(m,&i,velocity,0,.1,NULL));
    CHECK(i.valid&&i.variant==PE_COMBAT_RUNNING&&i.gender==PE_COMBAT_FEMALE&&i.view==PE_COMBAT_THIRD_PERSON&&i.stage==0&&i.from==.5&&i.to==.6);
    pe_root_motion_free(m);CHECK(!remove(path));return failures;
}
