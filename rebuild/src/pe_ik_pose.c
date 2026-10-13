#include "pe_ik_pose.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE (16u*1024u*1024u)
#define MAX_FRAMES 20000u
typedef struct {PeActorClip info; float *frames;} Clip;   /* frames * bones * 7 */
struct PeIkPose {unsigned bones; size_t clips; Clip *clip;};

static void fail(char *error,size_t capacity,const char *message){if(error&&capacity)snprintf(error,capacity,"%s",message);}
static uint32_t u32(const unsigned char *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static float f32(const unsigned char *p){uint32_t v=u32(p);float f;memcpy(&f,&v,4);return f;}

void pe_ik_pose_free(PeIkPose *pose){
    if(!pose)return;
    if(pose->clip)for(size_t i=0;i<pose->clips;i++)free(pose->clip[i].frames);
    free(pose->clip);free(pose);
}
PeIkPose *pe_ik_pose_load(const char *path,const PeFbbikRig *rig,char *error,size_t capacity){
    if(!path||!rig||!rig->bone_count||rig->bone_count>PE_FBBIK_MAX_BONES){fail(error,capacity,"invalid IK pose arguments");return NULL;}
    FILE *f=fopen(path,"rb");
    if(!f){fail(error,capacity,"IK pose open failed");return NULL;}
    long length=fseek(f,0,SEEK_END)==0?ftell(f):-1;
    if(length<16||(unsigned long)length>MAX_FILE||fseek(f,0,SEEK_SET)!=0){fclose(f);fail(error,capacity,"invalid IK pose size");return NULL;}
    size_t size=(size_t)length;unsigned char *bytes=malloc(size);
    if(!bytes){fclose(f);fail(error,capacity,"IK pose allocation failed");return NULL;}
    bool complete=fread(bytes,1,size,f)==size;int extra=fgetc(f);bool io_ok=!ferror(f);fclose(f);
    PeIkPose *pose=NULL;size_t at=16;
    if(!complete||extra!=EOF||!io_ok)goto bad;
    if(memcmp(bytes,"PEIK",4)!=0||u32(bytes+4)!=1)goto bad;
    uint32_t bones=u32(bytes+8),clips=u32(bytes+12);size_t padded=(bones+3u)&~(size_t)3u;
    if(bones!=rig->bone_count||!clips||clips>PE_ACTOR_MAX_CLIPS||size-at<padded)goto bad;
    for(uint32_t b=0;b<padded;b++)if(b<bones?(signed char)bytes[at+b]!=rig->parent[b]:bytes[at+b]!=0)goto bad;
    at+=padded;
    pose=calloc(1,sizeof *pose);if(!pose)goto bad;
    pose->bones=bones;pose->clip=calloc(clips,sizeof *pose->clip);if(!pose->clip)goto bad;
    pose->clips=clips;
    for(uint32_t k=0;k<clips;k++){
        Clip *c=&pose->clip[k];
        if(size-at<48)goto bad;
        memcpy(c->info.name,bytes+at,32);bool nul=false;
        for(int j=0;j<32;j++){unsigned char ch=(unsigned char)c->info.name[j];if(!ch)nul=true;else if(nul||ch<33||ch>126||ch=='/'||ch=='\\')goto bad;}
        if(!nul||!c->info.name[0])goto bad;
        for(uint32_t j=0;j<k;j++)if(strcmp(c->info.name,pose->clip[j].info.name)==0)goto bad;
        c->info.fps=f32(bytes+at+32);c->info.frames=u32(bytes+at+36);c->info.duration=f32(bytes+at+40);
        uint32_t loop=u32(bytes+at+44);c->info.loop=loop!=0;c->info.terminal_at_duration=true;at+=48;
        if(!isfinite(c->info.fps)||!isfinite(c->info.duration)||c->info.fps<=0||c->info.fps>240||c->info.frames<2||c->info.frames>MAX_FRAMES||loop>1)goto bad;
        /* The final sample is at duration, strictly after the penultimate one. */
        if((double)c->info.duration<=(double)(c->info.frames-2)/c->info.fps||(double)c->info.duration*c->info.fps>(double)(c->info.frames-1)+0.001)goto bad;
        size_t values=(size_t)c->info.frames*bones*7;
        if(values>(size-at)/4)goto bad;
        c->frames=malloc(values*sizeof(float));if(!c->frames)goto bad;
        for(size_t i=0;i<values;i+=7){
            float *v=c->frames+i;double norm=0;
            for(int j=0;j<7;j++){v[j]=f32(bytes+at+(i+(size_t)j)*4);if(!isfinite(v[j])||(j<3&&fabsf(v[j])>100))goto bad;if(j>=3)norm+=(double)v[j]*v[j];}
            if(fabs(norm-1)>1e-3)goto bad;
            float scale=(float)(1/sqrt(norm));for(int j=3;j<7;j++)v[j]*=scale;
        }
        at+=values*4;
    }
    if(at!=size)goto bad;
    free(bytes);return pose;
bad:
    free(bytes);pe_ik_pose_free(pose);fail(error,capacity,"invalid or unsupported PEIK pose");return NULL;
}
size_t pe_ik_pose_clips(const PeIkPose *pose){return pose?pose->clips:0;}
const PeActorClip *pe_ik_pose_clip(const PeIkPose *pose,size_t clip){return pose&&clip<pose->clips?&pose->clip[clip].info:NULL;}
size_t pe_ik_pose_find_clip(const PeIkPose *pose,const char *name){
    if(pose&&name)for(size_t c=0;c<pose->clips;c++)if(strcmp(name,pose->clip[c].info.name)==0)return c;
    return PE_ACTOR_BIND_POSE;
}
bool pe_ik_pose_sample(const PeIkPose *pose,size_t clip,double seconds,PeFbbikTransform local[PE_FBBIK_MAX_BONES]){
    if(!pose||!local||clip>=pose->clips||!isfinite(seconds)||seconds<0)return false;
    const Clip *c=&pose->clip[clip];
    /* The clip clock of pe_actor's pose_prepare. */
    double t=c->info.loop?fmod(seconds,c->info.duration):fmin(seconds,c->info.duration);
    double sample=t*c->info.fps;size_t penultimate=c->info.frames-2;double start=(double)penultimate/c->info.fps;
    if(t>=start)sample=penultimate+(t-start)/(c->info.duration-start);
    size_t f0=(size_t)floor(sample);
    if(f0>=c->info.frames)f0=c->info.frames-1;
    size_t f1=f0+1<c->info.frames?f0+1:f0;float alpha=(float)fmin(1,fmax(0,sample-f0));
    const float *a=c->frames+f0*pose->bones*7,*b=c->frames+f1*pose->bones*7;
    for(unsigned bone=0;bone<pose->bones;bone++,a+=7,b+=7){
        PeFbbikTransform *out=&local[bone];
        out->position=(PeVec3){a[0]+(b[0]-a[0])*alpha,a[1]+(b[1]-a[1])*alpha,a[2]+(b[2]-a[2])*alpha};
        float sign=a[3]*b[3]+a[4]*b[4]+a[5]*b[5]+a[6]*b[6]<0?-1.f:1.f,q[4],norm=0;
        for(int j=0;j<4;j++){q[j]=a[3+j]+(sign*b[3+j]-a[3+j])*alpha;norm+=q[j]*q[j];}
        norm=sqrtf(norm);
        out->rotation=norm>1e-5f?(PeFbbikQuat){q[0]/norm,q[1]/norm,q[2]/norm,q[3]/norm}:(PeFbbikQuat){a[3],a[4],a[5],a[6]};
    }
    return true;
}
