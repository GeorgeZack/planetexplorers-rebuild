#include "pe_test_paths.h"
#include "pe_root_motion.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"root-motion:%d %s\n",__LINE__,#c);failures++;}}while(0)
enum {COUNT=185,BYTES=32+32*COUNT};
static void u32(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(i*8));}
static void f64(unsigned char *p,double n){uint64_t bits;memcpy(&bits,&n,8);for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(bits>>(i*8));}
static void fixture(unsigned char *p,bool exact_end){
    memset(p,0,BYTES);memcpy(p,"PERM",4);u32(p+4,1);u32(p+8,408);u32(p+12,20110014);u32(p+16,COUNT);f64(p+24,PE_ROOT_MOTION_DURATION);
    for(unsigned i=0;i<COUNT;i++){
        double t=exact_end&&i==COUNT-1?PE_ROOT_MOTION_DURATION:i*.01;
        f64(p+32+32*i,t);f64(p+40+32*i,t*t);f64(p+48+32*i,-2*t);f64(p+56+32*i,3*t);
    }
}
static PeRootMotion *load(const char *path,const unsigned char *p,size_t bytes){
    FILE *f=fopen(path,"wb");if(!f){CHECK(f);return NULL;}
    CHECK(fwrite(p,1,bytes,f)==bytes);CHECK(!fclose(f));char error[128];
    PeRootMotion *m=pe_root_motion_load(path,error,sizeof error);CHECK(m?!error[0]:error[0]);return m;
}
static bool close3(PeVec3 a,PeVec3 b){return fabsf(a.x-b.x)<2e-6f&&fabsf(a.y-b.y)<2e-6f&&fabsf(a.z-b.z)<2e-6f;}
int pe_test_root_motion(void){
    failures=0;char path[1024];pe_test_format(path,sizeof path,"%s/rebuild/build/root-motion-test-%ld.perm",pe_test_root(),(long)getpid());
    unsigned char bytes[BYTES+1];fixture(bytes,false);PeRootMotion *m=load(path,bytes,BYTES);CHECK(m);
    if(!m)return failures;
    CHECK(pe_root_motion_count(m)==COUNT&&pe_root_motion_duration(m)==PE_ROOT_MOTION_DURATION);
    PeVec3 p={9,8,7};CHECK(pe_root_motion_sample(m,0,&p)&&close3(p,(PeVec3){0}));
    CHECK(pe_root_motion_sample(m,.015,&p)&&close3(p,(PeVec3){.00025f,-.03f,.045f}));
    double end=PE_ROOT_MOTION_DURATION,alpha=(end-1.83)/.01;
    CHECK(pe_root_motion_sample(m,end,&p));CHECK(close3(p,(PeVec3){(float)(1.83*1.83+(1.84*1.84-1.83*1.83)*alpha),(float)(-2*end),(float)(3*end)}));
    PeVec3 whole,a,b;CHECK(pe_root_motion_delta(m,.003,end,&whole));
    CHECK(pe_root_motion_delta(m,.003,.887,&a)&&pe_root_motion_delta(m,.887,end,&b));
    CHECK(close3(whole,(PeVec3){a.x+b.x,a.y+b.y,a.z+b.z}));
    CHECK(pe_root_motion_delta(m,end,end,&p)&&close3(p,(PeVec3){0}));
    const double bad[]={-1e-9,end+1e-9,NAN,INFINITY};
    for(size_t i=0;i<sizeof bad/sizeof *bad;i++){
        PeVec3 sentinel={9,8,7};p=sentinel;CHECK(!pe_root_motion_sample(m,bad[i],&p)&&!memcmp(&p,&sentinel,sizeof p));
        CHECK(!pe_root_motion_delta(m,0,bad[i],&p)&&!memcmp(&p,&sentinel,sizeof p));
        CHECK(!pe_root_motion_delta(m,bad[i],end,&p)&&!memcmp(&p,&sentinel,sizeof p));
    }
    p=(PeVec3){9,8,7};CHECK(!pe_root_motion_delta(m,1,.5,&p)&&p.x==9&&p.y==8&&p.z==7);
    CHECK(!pe_root_motion_sample(NULL,0,&p)&&!pe_root_motion_sample(m,0,NULL));
    CHECK(!pe_root_motion_delta(m,0,0,NULL));pe_root_motion_free(m);
    fixture(bytes,true);m=load(path,bytes,BYTES);CHECK(m);pe_root_motion_free(m);
    for(unsigned variant=0;variant<13;variant++){
        fixture(bytes,false);size_t n=BYTES;
        switch(variant){
        case 0:bytes[0]='X';break;
        case 1:u32(bytes+8,315);break;
        case 2:u32(bytes+12,20110011);break;
        case 3:u32(bytes+16,3601);break;
        case 4:u32(bytes+20,1);break;
        case 5:f64(bytes+24,NAN);break;
        case 6:f64(bytes+32,.00001);break;
        case 7:f64(bytes+64,0);break;
        case 8:f64(bytes+64,.017);break;
        case 9:f64(bytes+40,1);break;
        case 10:f64(bytes+80,INFINITY);break;
        case 11:n--;break;
        default:bytes[n++]=0;break;
        }
        m=load(path,bytes,n);CHECK(!m);pe_root_motion_free(m);
    }
    CHECK(!pe_root_motion_load(NULL,NULL,0));CHECK(!pe_root_motion_count(NULL)&&!pe_root_motion_duration(NULL));pe_root_motion_free(NULL);
    CHECK(!remove(path));return failures;
}
