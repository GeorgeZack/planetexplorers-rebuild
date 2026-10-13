#include "pe_ik_pose.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned failures;
#define CHECK(v) do{if(!(v)){fprintf(stderr,"ik_pose:%d %s\n",__LINE__,#v);failures++;}}while(0)
#define NEAR(a,b,e) CHECK(fabsf((a)-(b))<(e))
static const PeFbbikRig *rig;
static unsigned char file[65536];static size_t used;
static void put(const void *p,size_t n){memcpy(file+used,p,n);used+=n;}
static void put_u32(uint32_t v){unsigned char b[4]={(unsigned char)v,(unsigned char)(v>>8),(unsigned char)(v>>16),(unsigned char)(v>>24)};put(b,4);}
static void put_f32(float f){uint32_t v;memcpy(&v,&f,4);put_u32(v);}
/* Three frames at 2 fps over 0.75 s: bone 3 rises 0, 1, 2 and turns about y by 0, 90, 180 degrees. */
static void build(uint32_t loop){
    used=0;put("PEIK",4);put_u32(1);put_u32(rig->bone_count);put_u32(1);
    for(unsigned b=0;b<((rig->bone_count+3u)&~3u);b++){unsigned char p=b<rig->bone_count?(unsigned char)rig->parent[b]:0;put(&p,1);}
    char name[32]={0};strcpy(name,"Rise");put(name,32);put_f32(2);put_u32(3);put_f32(.75f);put_u32(loop);
    for(int f=0;f<3;f++)for(unsigned b=0;b<rig->bone_count;b++){
        float half=b==3?f*0.78539816f:0;
        float v[7]={0,b==3?(float)f:0,0,0,sinf(half),0,cosf(half)};
        for(int j=0;j<7;j++)put_f32(v[j]);
    }
}
static const char *path="build/test-ik-pose.peik";
static PeIkPose *load(void){
    FILE *f=fopen(path,"wb");CHECK(f);if(!f)return NULL;
    CHECK(fwrite(file,1,used,f)==used);fclose(f);
    char error[64]="";PeIkPose *pose=pe_ik_pose_load(path,rig,error,sizeof error);
    if(!pose)CHECK(error[0]);
    return pose;
}
static bool rejected(void){PeIkPose *p=load();bool bad=!p;pe_ik_pose_free(p);return bad;}
int pe_test_ik_pose(void){
    failures=0;rig=pe_fbbik_female_rig();
    PeFbbikTransform local[PE_FBBIK_MAX_BONES];memset(local,0,sizeof local);
    build(0);PeIkPose *pose=load();CHECK(pose);
    if(pose){
        const PeActorClip *clip=pe_ik_pose_clip(pose,0);
        CHECK(pe_ik_pose_clips(pose)==1&&clip&&!strcmp(clip->name,"Rise")&&clip->frames==3&&clip->fps==2&&clip->duration==.75f&&!clip->loop&&clip->terminal_at_duration);
        CHECK(pe_ik_pose_find_clip(pose,"Rise")==0&&pe_ik_pose_find_clip(pose,"Other")==PE_ACTOR_BIND_POSE&&!pe_ik_pose_clip(pose,1));
        CHECK(pe_ik_pose_sample(pose,0,0,local));NEAR(local[3].position.y,0,1e-6f);NEAR(local[3].rotation.w,1,1e-6f);CHECK(local[2].rotation.w==1);
        CHECK(pe_ik_pose_sample(pose,0,.25,local));NEAR(local[3].position.y,.5f,1e-6f);
        NEAR(local[3].rotation.y,sinf(0.39269908f),2e-3f);                 /* normalized lerp: close to 45 degrees */
        NEAR(local[3].rotation.x*local[3].rotation.x+local[3].rotation.y*local[3].rotation.y+local[3].rotation.z*local[3].rotation.z+local[3].rotation.w*local[3].rotation.w,1,1e-6f);
        CHECK(pe_ik_pose_sample(pose,0,.5,local));NEAR(local[3].position.y,1,1e-6f);
        /* The last interval is the 0.25 s to the terminal sample at duration, not a full frame. */
        CHECK(pe_ik_pose_sample(pose,0,.625,local));NEAR(local[3].position.y,1.5f,1e-6f);
        CHECK(pe_ik_pose_sample(pose,0,.75,local));NEAR(local[3].position.y,2,1e-6f);
        CHECK(pe_ik_pose_sample(pose,0,9,local));NEAR(local[3].position.y,2,1e-6f);   /* clamps */
        PeFbbikTransform kept=local[3];
        CHECK(!pe_ik_pose_sample(pose,1,0,local)&&!pe_ik_pose_sample(pose,0,-1,local)&&!pe_ik_pose_sample(pose,0,NAN,local)&&!pe_ik_pose_sample(pose,0,0,0));
        CHECK(!pe_ik_pose_sample(0,0,0,local)&&!memcmp(&kept,&local[3],sizeof kept));
        pe_ik_pose_free(pose);
    }
    build(1);pose=load();CHECK(pose);
    if(pose){CHECK(pe_ik_pose_sample(pose,0,1.0,local));NEAR(local[3].position.y,.5f,1e-6f);pe_ik_pose_free(pose);}   /* loops: 1.0 mod 0.75 */

    /* Malformed files fail closed. */
    build(0);file[0]='X';CHECK(rejected());
    build(0);file[4]=2;CHECK(rejected());
    build(0);file[8]^=1;CHECK(rejected());                               /* bone count differs from the rig */
    build(0);file[16+3]^=1;CHECK(rejected());                            /* parent differs from the rig */
    build(0);used-=4;CHECK(rejected());
    build(0);put_u32(0);CHECK(rejected());                               /* trailing bytes */
    size_t clip_at=16+((rig->bone_count+3u)&~3u);
    build(0);file[clip_at]='/';CHECK(rejected());
    build(0);file[clip_at+36]=1;CHECK(rejected());                       /* one frame */
    build(2);CHECK(rejected());                                          /* loop flag */
    build(0);{float d=.4f;uint32_t v;memcpy(&v,&d,4);file[clip_at+40]=(unsigned char)v;file[clip_at+41]=(unsigned char)(v>>8);file[clip_at+42]=(unsigned char)(v>>16);file[clip_at+43]=(unsigned char)(v>>24);}
    CHECK(rejected());                                                   /* duration not after the penultimate sample */
    build(0);{float n=NAN;memcpy(file+clip_at+48,&n,4);}CHECK(rejected());
    build(0);{float w=.5f;memcpy(file+clip_at+48+24,&w,4);}CHECK(rejected());   /* nonunit quaternion */
    build(0);
    char error[64]="";CHECK(!pe_ik_pose_load("build/missing.peik",rig,error,sizeof error)&&error[0]);
    CHECK(!pe_ik_pose_load(path,0,error,sizeof error)&&!pe_ik_pose_load(0,rig,error,sizeof error));
    CHECK(pe_ik_pose_clips(0)==0&&!pe_ik_pose_clip(0,0));pe_ik_pose_free(0);
    remove(path);
    return failures?1:0;
}
