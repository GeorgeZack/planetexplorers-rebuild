#define _POSIX_C_SOURCE 200809L
#include "pe_sprint_root_motion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"sprint root:%d %s\n",__LINE__,#x);failures++;}}while(0)
static void u32(unsigned char *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void f64(unsigned char *p,double v){uint64_t bits;memcpy(&bits,&v,8);for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(bits>>(8*i));}
static void write_bytes(const char *path,const unsigned char *data,size_t n){FILE *f=fopen(path,"wb");CHECK(f&&fwrite(data,1,n,f)==n);if(f)CHECK(!fclose(f));}
int pe_test_sprint_root_motion(void){
    failures=0;unsigned char data[953]={0};memcpy(data,"PESR",4);u32(data+4,1);u32(data+8,777);u32(data+12,1);
    f64(data+16,.9932681322097778);u32(data+24,401);u32(data+28,56);f64(data+32,1.8000068664550781);f64(data+48,30);
    for(unsigned k=0;k<56;k++){f64(data+56+k*16,10+(double)k*k*.01);f64(data+64+k*16,-20+(double)k*.1);}
    char path[]="/tmp/pe-sprint-root-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return 1;close(fd);
    write_bytes(path,data,952);char error[96];PeSprintRootMotion *m=pe_sprint_root_motion_load(path,error,sizeof error);CHECK(m);
    if(m){
        CHECK(pe_sprint_root_motion_duration(m)==1.8000068664550781);
        PeVec3 out;CHECK(pe_sprint_root_motion_sample(m,0,&out)&&out.x==0&&out.y==0&&out.z==0);
        CHECK(pe_sprint_root_motion_sample(m,.05,&out));
        CHECK(fabs(out.x-.025*.9932681322097778)<1e-7&&fabs(out.z-.15*.9932681322097778)<1e-7);
        CHECK(pe_sprint_root_motion_sample(m,pe_sprint_root_motion_duration(m),&out));
        for(unsigned k=0;k<5;k++){
            double t=k==0?-1:k==1?2:k==2?NAN:k==3?INFINITY:0;PeVec3 before={7,8,9};out=before;
            CHECK(!pe_sprint_root_motion_sample(k==4?NULL:m,t,&out)&&!memcmp(&out,&before,sizeof out));
        }
        PeCombatInterval span={.valid=true,.variant=PE_COMBAT_SPRINT,.gender=PE_COMBAT_FEMALE,.view=PE_COMBAT_THIRD_PERSON,.from=.1,.to=.12};
        double delta[3];CHECK(pe_sprint_root_motion_world_delta(m,&span,90,delta));
        CHECK(fabs(delta[0]-.06*.9932681322097778)<1e-10&&delta[1]==0&&delta[2]<0);
        PeCombatInterval before=span;
        for(unsigned k=0;k<8;k++){
            PeCombatInterval bad=span;double result[3]={7,8,9},unchanged[3]={7,8,9};
            if(k==0)bad.valid=false;
            if(k==1)bad.stage=1;
            if(k==2)bad.variant=PE_COMBAT_RUNNING;
            if(k==3)bad.gender=PE_COMBAT_MALE;
            if(k==4)bad.view=PE_COMBAT_FIRST_PERSON;
            if(k==5)bad.to=.09;
            if(k==6)bad.from=NAN;
            CHECK(!pe_sprint_root_motion_world_delta(m,&bad,k==7?NAN:0,result)&&!memcmp(result,unchanged,sizeof result));
        }
        CHECK(!memcmp(&span,&before,sizeof span));pe_sprint_root_motion_free(m);
    }
    for(unsigned k=0;k<13;k++){
        unsigned char bad[953];memcpy(bad,data,sizeof bad);size_t n=952;
        if(k==0)bad[0]='X';
        if(k==1)u32(bad+4,2);
        if(k==2)u32(bad+8,778);
        if(k==3)u32(bad+12,4);
        if(k==4)f64(bad+16,1);
        if(k==5)u32(bad+24,408);
        if(k==6)u32(bad+28,55);
        if(k==7)f64(bad+32,1.8);
        if(k==8)f64(bad+40,.1);
        if(k==9)f64(bad+48,60);
        if(k==10)f64(bad+80,NAN);
        if(k==11)n=951;
        if(k==12)n=953;
        write_bytes(path,bad,n);m=pe_sprint_root_motion_load(path,error,sizeof error);CHECK(!m&&*error);pe_sprint_root_motion_free(m);
    }
    unlink(path);CHECK(!pe_sprint_root_motion_load(path,error,sizeof error));pe_sprint_root_motion_free(NULL);return failures;
}
