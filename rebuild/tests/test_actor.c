#define _POSIX_C_SOURCE 200809L
#include "pe_actor.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT "."
#endif
static int failures;
#define CHECK(c) do { if(!(c)){fprintf(stderr,"actor:%d: %s\n",__LINE__,#c);failures++;} } while(0)
static void u32(unsigned char *p,uint32_t x) { for(int i=0;i<4;i++)p[i]=(unsigned char)(x>>(8*i)); }
static void f32(unsigned char *p,float x) { uint32_t b;memcpy(&b,&x,4);u32(p,b); }
static void matrix(unsigned char *p,float sx,float x,float y) {
    float m[12]={sx,0,0,x,0,1,0,y,0,0,1,0};for(int i=0;i<12;i++)f32(p+4*i,m[i]);
}
static size_t fixture(unsigned char *b) {
    memset(b,0,1024);memcpy(b,"PEAC",4);u32(b+4,1);u32(b+8,3);u32(b+12,3);u32(b+16,2);u32(b+20,1);
    for(int k=0;k<3;k++) {
        unsigned char *p=b+24+k*56;
        f32(p,1);f32(p+4,(float)k);f32(p+12,1);f32(p+16,1);
        f32(p+32,.25f);f32(p+36,.75f);p[50]=1;
    }
    u32(b+192,0);u32(b+196,1);u32(b+200,2);
    matrix(b+204,1,100,0);matrix(b+252,1,100,0); /* Provenance only! */
    memcpy(b+300,"run",4);f32(b+332,2);u32(b+336,3);f32(b+340,.75f);u32(b+344,1);
    for(int k=0;k<3;k++) {matrix(b+348+k*96,2,(float)(k*4),0);matrix(b+396+k*96,1,(float)(k*4),2);}
    return 636;
}
static bool save_file(const char *path,const unsigned char *b,size_t n) {
    FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(b,1,n,f)==n;return fclose(f)==0&&ok;
}
static bool near(float a,float b) {return fabsf(a-b)<.0001f;}
static void malformed(const char *path,unsigned char *b,size_t n,size_t off,uint32_t value) {
    unsigned char old[4];memcpy(old,b+off,4);u32(b+off,value);CHECK(save_file(path,b,n));char error[128];PeActor *a=pe_actor_load(path,error,sizeof(error));CHECK(!a&&error[0]);pe_actor_free(a);memcpy(b+off,old,4);
}
int pe_test_actor(void) {
    failures=0;char path[]="/tmp/pe-actor-test-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return failures;close(fd);
    unsigned char b[1024];size_t n=fixture(b);CHECK(save_file(path,b,n));char error[128];PeActor *a=pe_actor_load(path,error,sizeof(error));CHECK(a&&!*error);
    if(a) {
        float point[3]={1,0,0},posed[3];CHECK(pe_actor_skeleton_hash(a)!=0);
        CHECK(pe_actor_pose_joint_point(a,0,.25,0,point,posed));CHECK(near(posed[0],4)&&near(posed[1],0));
        CHECK(pe_actor_pose_joint_point(a,0,.25,1,point,posed));CHECK(near(posed[0],3)&&near(posed[1],2));
        CHECK(pe_actor_pose_joint_point(a,0,.75,0,point,point));CHECK(near(point[0],2));
        float before_point[3];memcpy(before_point,posed,sizeof(posed));
        CHECK(!pe_actor_pose_joint_point(a,0,NAN,0,point,posed));CHECK(!memcmp(before_point,posed,sizeof(posed)));
        CHECK(!pe_actor_pose_joint_point(a,0,0,2,point,posed));CHECK(!memcmp(before_point,posed,sizeof(posed)));
        PeActorStats s=pe_actor_stats(a);CHECK(s.vertices==3&&s.indices==3&&s.joints==2&&s.clips==1);
        CHECK(pe_actor_indices(a)[2]==2);CHECK(pe_actor_find_clip(a,"run")==0);CHECK(pe_actor_find_clip(a,"none")==PE_ACTOR_BIND_POSE);
        const PeActorClip *clip=pe_actor_clip(a,0);CHECK(clip&&clip->frames==3&&clip->duration==.75f&&clip->loop);CHECK(!pe_actor_clip(a,1));
        PeActorVertex v,all[3];CHECK(pe_actor_pose_vertex(a,0,0,0,&v));CHECK(near(v.position[0],1.25f)&&near(v.position[1],1.5f));
        float length=sqrtf(.875f*.875f+1);CHECK(near(v.normal[0],.875f/length)&&near(v.normal[1],1/length));
        CHECK(pe_actor_pose_vertex(a,0,.25,0,&v));CHECK(near(v.position[0],3.25f));
        CHECK(pe_actor_pose_vertex(a,0,.625,0,&v));CHECK(near(v.position[0],6.25f));
        CHECK(pe_actor_pose_vertex(a,0,.75,0,&v));CHECK(near(v.position[0],1.25f));
        CHECK(pe_actor_pose_vertex(a,0,750000.25,0,&v));CHECK(near(v.position[0],3.25f));
        CHECK(pe_actor_pose(a,0,.25,all,3));CHECK(near(all[0].position[0],v.position[0])&&near(all[2].position[1],3.5f));
        CHECK(pe_actor_pose_vertex(a,PE_ACTOR_BIND_POSE,0,0,&v));CHECK(near(v.position[0],1)&&near(v.normal[0],sqrtf(.5f)));
        PeActorVertex old=v;CHECK(!pe_actor_pose_vertex(a,4,0,0,&v));CHECK(!memcmp(&old,&v,sizeof(v)));
        CHECK(!pe_actor_pose_vertex(a,0,NAN,0,&v));CHECK(!memcmp(&old,&v,sizeof(v)));
        CHECK(!pe_actor_pose_vertex(a,0,-1,0,&v));CHECK(!pe_actor_pose_vertex(a,0,0,3,&v));CHECK(!pe_actor_pose(a,0,0,&v,1));
        CHECK(!memcmp(&old,&v,sizeof(v)));pe_actor_free(a);
    }
    /* Every truncation must fail, including partial matrix/float tails. */
    for(size_t length=0;length<n;length++) {CHECK(save_file(path,b,length));a=pe_actor_load(path,error,sizeof(error));CHECK(!a);pe_actor_free(a);}
    b[n]=0;CHECK(save_file(path,b,n+1));a=pe_actor_load(path,error,sizeof(error));CHECK(!a);pe_actor_free(a);
    const size_t offsets[]={0,4,8,12,16,20,24,36,56,60,72,192,204,300,332,336,340,344,348};
    const uint32_t values[]={0,3,0xffffffff,2,257,33,0x7fc00000,0x7f800000,0,0xbf800000,0xffff,3,0,0,0,0xffffffff,0x7f800000,2,0};
    for(size_t k=0;k<sizeof(offsets)/sizeof(offsets[0]);k++)malformed(path,b,n,offsets[k],values[k]);
    /* Finite but insufficient frame coverage and an unterminated clip name. */
    malformed(path,b,n,340,0x40000000);memset(b+300,'x',32);CHECK(save_file(path,b,n));a=pe_actor_load(path,error,sizeof(error));CHECK(!a);pe_actor_free(a);
    fixture(b);u32(b+344,0);CHECK(save_file(path,b,n));a=pe_actor_load(path,error,sizeof(error));CHECK(a);
    if(a){PeActorVertex v;CHECK(pe_actor_pose_vertex(a,0,99,0,&v));CHECK(near(v.position[0],7.25f));pe_actor_free(a);}
    /* V2's terminal frame is at .75s, halfway before uniform 1s. V1 above
     * intentionally retains its legacy 7.25 result; v2 reaches full 9.25. */
    u32(b+4,2);CHECK(save_file(path,b,n));a=pe_actor_load(path,error,sizeof(error));CHECK(a);
    if(a){
        CHECK(pe_actor_clip(a,0)->terminal_at_duration);PeActorVertex v;float point[3]={1,0,0},posed[3];
        CHECK(pe_actor_pose_vertex(a,0,.25,0,&v));CHECK(near(v.position[0],3.25f));
        CHECK(pe_actor_pose_vertex(a,0,.625,0,&v));CHECK(near(v.position[0],7.25f));
        CHECK(pe_actor_pose_vertex(a,0,.75,0,&v));CHECK(near(v.position[0],9.25f));
        CHECK(pe_actor_pose_vertex(a,0,99,0,&v));CHECK(near(v.position[0],9.25f));
        CHECK(pe_actor_pose_joint_point(a,0,.625,0,point,posed));CHECK(near(posed[0],8));
        CHECK(pe_actor_pose_joint_point(a,0,.75,0,point,posed));CHECK(near(posed[0],10));
        pe_actor_free(a);
    }
    u32(b+344,1);CHECK(save_file(path,b,n));a=pe_actor_load(path,error,sizeof(error));CHECK(a);
    if(a){PeActorVertex v;float point[3]={1,0,0},posed[3];
        CHECK(pe_actor_pose_vertex(a,0,.625,0,&v));CHECK(near(v.position[0],7.25f));
        CHECK(pe_actor_pose_vertex(a,0,.75,0,&v));CHECK(near(v.position[0],1.25f));
        CHECK(pe_actor_pose_joint_point(a,0,1.375,0,point,posed));CHECK(near(posed[0],8));
        pe_actor_free(a);
    }
    /* Duplicate/reversed terminal timestamps must not divide by zero. */
    malformed(path,b,n,340,0x3f000000); /* duration .5 == penultimate */
    malformed(path,b,n,340,0x3e800000); /* duration .25 < penultimate */
    unlink(path);
    const char *actual=PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.peac";
    FILE *f=fopen(actual,"rb");if(f) {
        fclose(f);a=pe_actor_load(actual,error,sizeof(error));CHECK(a);
        if(a) {
            PeActorStats s=pe_actor_stats(a);CHECK(s.vertices>0&&s.joints>0&&s.clips>=2);
            size_t run=pe_actor_find_clip(a,"run"),idle=pe_actor_find_clip(a,"idle");CHECK(run!=PE_ACTOR_BIND_POSE&&idle!=PE_ACTOR_BIND_POSE);
            PeActorVertex *out=malloc(s.vertices*sizeof(*out));CHECK(out);
            if(out) {CHECK(pe_actor_pose(a,run,.137,out,s.vertices));for(size_t k=0;k<s.vertices;k++){double len=0;for(int j=0;j<3;j++){CHECK(isfinite(out[k].position[j]));len+=(double)out[k].normal[j]*out[k].normal[j];}CHECK(fabs(len-1)<.0001);}free(out);}pe_actor_free(a);
        }
    }
    return failures;
}
