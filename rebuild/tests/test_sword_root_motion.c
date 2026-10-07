#define _POSIX_C_SOURCE 200809L
#include "pe_sword_root_motion.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sword root:%d %s\n",__LINE__,#c);failures++;}}while(0)
enum {BYTES=2392};
static const uint32_t ids[]={231,240,247,254},counts[]={34,35,40,31};
static const double lengths[]={1.0666667222976685,1.100000262260437,1.2666668891906738,.9666671752929688};
static const double scale=.9932681322097778;
static void u32(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static void f64(unsigned char *p,double n){uint64_t bits;memcpy(&bits,&n,8);for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(bits>>(8*i));}
static bool write_file(const char *path,const unsigned char *bytes,size_t count){
    FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(bytes,1,count,f)==count;return fclose(f)==0&&ok;
}
static double begin(unsigned i){return i?0:-1.4901161193847656e-8;}
static double x(unsigned clip,unsigned knot){return 10+clip+(clip+1)*.125*knot*knot;}
static double z(unsigned clip,unsigned knot){return -20-(double)clip-(clip+1)*.375*knot;}
static void fixture(unsigned char bytes[BYTES],size_t offsets[4]){
    memset(bytes,0,BYTES);memcpy(bytes,"PESR",4);u32(bytes+4,1);u32(bytes+8,777);u32(bytes+12,4);f64(bytes+16,scale);
    size_t at=24;
    for(unsigned i=0;i<4;i++){
        offsets[i]=at;u32(bytes+at,ids[i]);u32(bytes+at+4,counts[i]);
        f64(bytes+at+8,lengths[i]);f64(bytes+at+16,begin(i));f64(bytes+at+24,30);at+=32;
        for(unsigned k=0;k<counts[i];k++){f64(bytes+at,x(i,k));f64(bytes+at+8,z(i,k));at+=16;}
    }
    CHECK(at==BYTES);
}
static PeVec3 expected(unsigned clip,double time){
    double phase=(time-begin(clip))*30,origin=-begin(clip)*30;
    unsigned k=(unsigned)phase;double f=phase-k;
    double rawx=x(clip,k)+(x(clip,k+1)-x(clip,k))*f;
    double rawz=z(clip,k)+(z(clip,k+1)-z(clip,k))*f;
    double basex=x(clip,0)+(x(clip,1)-x(clip,0))*origin;
    double basez=z(clip,0)+(z(clip,1)-z(clip,0))*origin;
    return (PeVec3){(float)((rawx-basex)*scale),0,(float)((rawz-basez)*scale)};
}
static void invalid_file(const char *path,const unsigned char *bytes,size_t count){
    CHECK(write_file(path,bytes,count));char error[96]="unchanged";
    PeSwordRootMotion *motion=pe_sword_root_motion_load(path,error,sizeof error);
    CHECK(!motion&&strstr(error,"PESR1"));pe_sword_root_motion_free(motion);
}
static void invalid_sample(const PeSwordRootMotion *m,uint32_t clip,double time){
    PeVec3 out={7,8,9},before=out;
    CHECK(!pe_sword_root_motion_sample(m,clip,time,&out)&&!memcmp(&out,&before,sizeof out));
}
static void invalid_delta(const PeSwordRootMotion *m,uint32_t clip,double a,double b){
    PeVec3 out={7,8,9},before=out;
    CHECK(!pe_sword_root_motion_delta(m,clip,a,b,&out)&&!memcmp(&out,&before,sizeof out));
}
static void requests(const PeSwordRootMotion *m){
    PeCombatInterval span={.valid=true,.gender=PE_COMBAT_FEMALE,.view=PE_COMBAT_THIRD_PERSON,
        .variant=PE_COMBAT_STATIONARY,.from=.1,.to=.11};
    for(unsigned stage=0;stage<4;stage++){
        span.stage=stage;span.variant=PE_COMBAT_STATIONARY;PeVec3 out;
        CHECK(pe_sword_root_motion_request(m,&span,(PeVec3){0},0,1,&out));
        PeVec3 a=expected(stage,.1),b=expected(stage,.11);
        CHECK(fabsf(out.x-(b.x-a.x))<1e-5f&&fabsf(out.z-(b.z-a.z))<1e-5f&&out.y==0);
        PeVec3 ordinary=out;
        CHECK(pe_sword_root_motion_request(m,&span,(PeVec3){0},90,1,&out));
        CHECK(fabsf(out.x-ordinary.z)<1e-6f&&fabsf(out.z+ordinary.x)<1e-6f);
        span.variant=PE_COMBAT_RUNNING;
        if(stage){CHECK(pe_sword_root_motion_request(m,&span,(PeVec3){0},0,1,&out));CHECK(!memcmp(&ordinary,&out,sizeof out));}
        else CHECK(!pe_sword_root_motion_request(m,&span,(PeVec3){0},0,1,&out));
    }
    span.variant=PE_COMBAT_STATIONARY;span.stage=0;
    for(unsigned i=0;i<10;i++){
        PeCombatInterval bad=span;double yaw=0,dt=1;PeVec3 out={7,8,9},before=out;
        if(i==0)bad.valid=false;
        if(i==1)bad.gender=PE_COMBAT_MALE;
        if(i==2)bad.view=PE_COMBAT_FIRST_PERSON;
        if(i==3)bad.variant=(PeCombatVariant)99;
        if(i==4)bad.stage=4;
        if(i==5)bad.from=.12;
        if(i==6)bad.to=NAN;
        if(i==7)yaw=INFINITY;
        if(i==8)dt=0;
        if(i==9)dt=NAN;
        CHECK(!pe_sword_root_motion_request(m,&bad,(PeVec3){0},yaw,dt,&out)&&!memcmp(&out,&before,sizeof out));
    }
    PeVec3 out={0};
    CHECK(!pe_sword_root_motion_request(m,NULL,out,0,1,&out)&&!pe_sword_root_motion_request(NULL,&span,out,0,1,&out));
    CHECK(!pe_sword_root_motion_request(m,&span,out,0,1,NULL));
}
int pe_test_sword_root_motion(void){
    failures=0;char path[]="/tmp/pe-sword-root-XXXXXX",error[128];int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return failures;close(fd);
    unsigned char bytes[BYTES],bad[BYTES+1];size_t offsets[4];fixture(bytes,offsets);
    CHECK(write_file(path,bytes,sizeof bytes));
    PeSwordRootMotion *motion=pe_sword_root_motion_load(path,error,sizeof error);CHECK(motion&&!*error);
    if(!motion)goto done;
    requests(motion);
    for(unsigned i=0;i<4;i++){
        CHECK(pe_sword_root_motion_duration(motion,ids[i])==lengths[i]);
        PeVec3 out={7,8,9};CHECK(pe_sword_root_motion_sample(motion,ids[i],0,&out)&&out.x==0&&out.y==0&&out.z==0);
        const double times[]={begin(i)+1./60,.3,lengths[i],nextafter(lengths[i],0)};
        for(unsigned j=0;j<sizeof times/sizeof times[0];j++){
            PeVec3 want=expected(i,times[j]);CHECK(pe_sword_root_motion_sample(motion,ids[i],times[j],&out));
            CHECK(out.x==want.x&&out.y==0&&out.z==want.z);
        }
        CHECK(pe_sword_root_motion_delta(motion,ids[i],.3,.3,&out)&&out.x==0&&out.y==0&&out.z==0);
        CHECK(pe_sword_root_motion_delta(motion,ids[i],0,lengths[i],&out));PeVec3 want=expected(i,lengths[i]);
        CHECK(out.x==want.x&&out.z==want.z);
        /* Negative dense begin is real source phase; raw knot0 is not the
         * authored time0 baseline. The difference is visible at float output. */
        if(i==0){CHECK(pe_sword_root_motion_sample(motion,ids[i],begin(i)+1./60,&out));CHECK(out.x!=(float)(.0625*scale));}
        invalid_sample(motion,ids[i],nextafter(lengths[i],INFINITY));
        invalid_delta(motion,ids[i],.4,.3);invalid_delta(motion,ids[i],0,lengths[i]+.1);
    }
    for(unsigned i=0;i<4;i++){
        const double invalid[]={NAN,INFINITY,-INFINITY,-DBL_MIN};
        invalid_sample(motion,ids[i],invalid[i]);invalid_delta(motion,ids[i],invalid[i],.5);invalid_delta(motion,ids[i],0,invalid[i]);
    }
    invalid_sample(motion,408,0);invalid_delta(motion,179,0,.1);invalid_sample(NULL,231,0);invalid_delta(NULL,231,0,.1);
    CHECK(!pe_sword_root_motion_sample(motion,231,0,NULL)&&!pe_sword_root_motion_delta(motion,231,0,0,NULL));
    CHECK(pe_sword_root_motion_duration(NULL,231)==0&&pe_sword_root_motion_duration(motion,408)==0);
    /* Loaded curves own their data, independent of the source file lifecycle. */
    CHECK(!remove(path));PeVec3 out;CHECK(pe_sword_root_motion_sample(motion,231,.2,&out));
    pe_sword_root_motion_free(motion);motion=NULL;
    for(unsigned field=0;field<4;field++){
        memcpy(bad,bytes,BYTES);u32(bad+field*4,99);invalid_file(path,bad,BYTES);
    }
    const double invalid[]={NAN,INFINITY,-INFINITY,0,-1,10001};
    for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++){
        memcpy(bad,bytes,BYTES);f64(bad+16,invalid[i]);invalid_file(path,bad,BYTES);
    }
    for(unsigned i=0;i<4;i++){
        size_t at=offsets[i];
        memcpy(bad,bytes,BYTES);u32(bad+at,ids[(i+1)%4]);invalid_file(path,bad,BYTES);
        memcpy(bad,bytes,BYTES);u32(bad+at+4,counts[i]-1);invalid_file(path,bad,BYTES);
        memcpy(bad,bytes,BYTES);u32(bad+at+4,UINT32_MAX);invalid_file(path,bad,BYTES);
        for(unsigned field=8;field<=24;field+=8){
            memcpy(bad,bytes,BYTES);f64(bad+at+field,NAN);invalid_file(path,bad,BYTES);
            memcpy(bad,bytes,BYTES);f64(bad+at+field,field==8?lengths[i]+.001:field==16?begin(i)+.001:60);invalid_file(path,bad,BYTES);
        }
        for(unsigned axis=0;axis<2;axis++)for(unsigned j=0;j<3;j++){
            memcpy(bad,bytes,BYTES);f64(bad+at+32+16*(counts[i]-1)+8*axis,j==0?NAN:j==1?INFINITY:10000.01);invalid_file(path,bad,BYTES);
        }
    }
    /* Truncation at header, curve header and final sample plus trailing bytes. */
    const size_t cuts[]={0,23,24,55,56,offsets[2]+31,BYTES-1};
    for(unsigned i=0;i<sizeof cuts/sizeof cuts[0];i++)invalid_file(path,bytes,cuts[i]);
    memcpy(bad,bytes,BYTES);bad[BYTES]=0;invalid_file(path,bad,BYTES+1);
    CHECK(!pe_sword_root_motion_load(NULL,error,sizeof error));
done:
    pe_sword_root_motion_free(motion);unlink(path);return failures;
}
