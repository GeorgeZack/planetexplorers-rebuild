#include "pe_actor.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE (256u*1024u*1024u)
#define MAX_JOINTS 256u

typedef struct { PeActorVertex vertex; float weight[4]; uint16_t joint[4]; } Vertex;
typedef struct { PeActorClip info; float *palette; } Clip;
struct PeActor { uint64_t skeleton_hash; PeActorStats stats; Vertex *vertices; uint32_t *indices; float *bind; Clip *clips; };
typedef struct { const unsigned char *data; size_t size, offset; bool ok; } Reader;
static void error_set(char *error,size_t capacity,const char *message) {
    if (error && capacity) snprintf(error,capacity,"%s",message);
}
static uint32_t read_u32(Reader *r) {
    if (!r->ok || r->size-r->offset<4) { r->ok=false; return 0; }
    const unsigned char *p=r->data+r->offset; r->offset+=4;
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static uint16_t read_u16(Reader *r) {
    if (!r->ok || r->size-r->offset<2) { r->ok=false; return 0; }
    const unsigned char *p=r->data+r->offset; r->offset+=2;
    return (uint16_t)((uint16_t)p[0]|(uint16_t)p[1]<<8);
}
static float read_float(Reader *r,float bound) {
    uint32_t bits=read_u32(r); float f; memcpy(&f,&bits,4);
    if (!isfinite(f) || fabsf(f)>bound) r->ok=false;
    return f;
}
/* Cofactor matrix / determinant = inverse transpose of the affine 3x3. */
static bool normal_matrix(const float *m,float *n) {
    double a=m[0],b=m[1],c=m[2],d=m[4],e=m[5],f=m[6],g=m[8],h=m[9],i=m[10];
    double q[9]={e*i-f*h,f*g-d*i,d*h-e*g,c*h-b*i,a*i-c*g,b*g-a*h,b*f-c*e,c*d-a*f,a*e-b*d};
    double det=a*q[0]+b*q[1]+c*q[2];
    if (!isfinite(det) || fabs(det)<1e-12) return false;
    for (int k=0;k<9;k++) { double v=q[k]/det; if (!isfinite(v)||fabs(v)>1e12) return false; if(n)n[k]=(float)v; }
    return true;
}
static bool read_matrices(Reader *r,float *dst,size_t count) {
    for(size_t k=0;k<count;k++) {
        for(int j=0;j<12;j++) dst[k*12+j]=read_float(r,1000000);
        if(!r->ok || !normal_matrix(dst+k*12,NULL)) return false;
    }
    return true;
}
void pe_actor_free(PeActor *a) {
    if (!a) return;
    if(a->clips) for(size_t k=0;k<a->stats.clips;k++) free(a->clips[k].palette);
    free(a->clips); free(a->bind); free(a->indices); free(a->vertices); free(a);
}
PeActor *pe_actor_load(const char *path,char *error,size_t capacity) {
    error_set(error,capacity,"");
    if(!path) { error_set(error,capacity,"missing actor path"); return NULL; }
    FILE *f=fopen(path,"rb");
    if(!f) { error_set(error,capacity,"cannot open actor"); return NULL; }
    if(fseek(f,0,SEEK_END)!=0) { fclose(f); error_set(error,capacity,"cannot size actor"); return NULL; }
    long length=ftell(f);
    if(length<24 || (unsigned long)length>MAX_FILE || fseek(f,0,SEEK_SET)!=0) { fclose(f); error_set(error,capacity,"invalid actor size"); return NULL; }
    size_t size=(size_t)length; unsigned char *bytes=malloc(size);
    if(!bytes) { fclose(f); error_set(error,capacity,"actor allocation failed"); return NULL; }
    bool complete=fread(bytes,1,size,f)==size;
    int extra=fgetc(f); bool io_ok=!ferror(f); fclose(f);
    if(!complete||extra!=EOF||!io_ok) { free(bytes); error_set(error,capacity,"actor read failed"); return NULL; }
    Reader r={bytes,size,4,true}; PeActor *a=NULL;
    uint32_t version=read_u32(&r);
    if(memcmp(bytes,"PEAC",4)!=0 || (version!=1&&version!=2)) goto bad;
    a=calloc(1,sizeof(*a)); if(!a) goto bad;
    a->stats.vertices=read_u32(&r); a->stats.indices=read_u32(&r);
    a->stats.joints=read_u32(&r); a->stats.clips=read_u32(&r);
    size_t nv=a->stats.vertices,ni=a->stats.indices,nj=a->stats.joints,nc=a->stats.clips;
    if(!nv||nv>1000000||!ni||ni>3000000||ni%3||!nj||nj>MAX_JOINTS||!nc||nc>PE_ACTOR_MAX_CLIPS) goto bad;
    if(nv*56+ni*4+nj*48+nc*48>size-r.offset) goto bad;
    a->vertices=calloc(nv,sizeof(Vertex)); a->indices=malloc(ni*sizeof(uint32_t));
    a->bind=malloc(nj*12*sizeof(float)); a->clips=calloc(nc,sizeof(Clip));
    if(!a->vertices||!a->indices||!a->bind||!a->clips) goto bad;
    for(size_t k=0;k<nv;k++) {
        Vertex *v=a->vertices+k;
        for(int j=0;j<3;j++) v->vertex.position[j]=read_float(&r,10000);
        double norm=0;
        for(int j=0;j<3;j++) { v->vertex.normal[j]=read_float(&r,10000); norm+=(double)v->vertex.normal[j]*v->vertex.normal[j]; }
        if(norm<1e-12) goto bad;
        for(int j=0;j<2;j++) v->vertex.uv[j]=read_float(&r,1000000);
        double sum=0;
        for(int j=0;j<4;j++) { v->weight[j]=read_float(&r,1); if(v->weight[j]<0) goto bad; sum+=v->weight[j]; }
        if(fabs(sum-1)>0.001) goto bad;
        for(int j=0;j<4;j++) { v->joint[j]=read_u16(&r); if(v->joint[j]>=nj) goto bad; v->weight[j]=(float)(v->weight[j]/sum); }
        if(!r.ok) goto bad;
    }
    for(size_t k=0;k<ni;k++) { a->indices[k]=read_u32(&r); if(a->indices[k]>=nv) goto bad; }
    if(!read_matrices(&r,a->bind,nj)) goto bad;
    a->skeleton_hash=UINT64_C(14695981039346656037);
    for(size_t k=8;k<20;k++){a->skeleton_hash^=bytes[k];a->skeleton_hash*=UINT64_C(1099511628211);}
    for(size_t k=24;k<r.offset;k++){a->skeleton_hash^=bytes[k];a->skeleton_hash*=UINT64_C(1099511628211);}
    for(size_t k=0;k<nc;k++) {
        Clip *c=a->clips+k;
        if(size-r.offset<48) goto bad;
        memcpy(c->info.name,bytes+r.offset,32); r.offset+=32;
        bool nul=false;
        for(int j=0;j<32;j++) { unsigned char ch=(unsigned char)c->info.name[j]; if(!ch) nul=true; else if(nul||ch<33||ch>126||ch=='/'||ch=='\\') goto bad; }
        if(!nul||!c->info.name[0]) goto bad;
        for(size_t j=0;j<k;j++) if(strcmp(c->info.name,a->clips[j].info.name)==0) goto bad;
        c->info.fps=read_float(&r,240); c->info.frames=read_u32(&r);
        c->info.duration=read_float(&r,86400); uint32_t loop=read_u32(&r); c->info.loop=loop!=0;
        c->info.terminal_at_duration=version==2;
        if(!r.ok||c->info.fps<=0||!c->info.frames||c->info.frames>20000||c->info.duration<=0||loop>1) goto bad;
        /* Last sample covers duration; exporter may retain terminal guard frames. */
        if((double)c->info.duration*c->info.fps>(double)(c->info.frames-1)+0.001) goto bad;
        if(version==2&&(c->info.frames<2||(double)c->info.duration<=(double)(c->info.frames-2)/c->info.fps))goto bad;
        size_t matrices=(size_t)c->info.frames*nj;
        if(matrices>(size-r.offset)/48) goto bad;
        c->palette=malloc(matrices*12*sizeof(float)); if(!c->palette) goto bad;
        if(!read_matrices(&r,c->palette,matrices)) goto bad;
    }
    if(!r.ok||r.offset!=size) goto bad;
    free(bytes); return a;
bad:
    free(bytes); pe_actor_free(a); error_set(error,capacity,"invalid or unsupported PEAC actor"); return NULL;
}
uint64_t pe_actor_skeleton_hash(const PeActor *a){return a?a->skeleton_hash:0;}
PeActorStats pe_actor_stats(const PeActor *a) { return a?a->stats:(PeActorStats){0}; }
const uint32_t *pe_actor_indices(const PeActor *a) { return a?a->indices:NULL; }
const PeActorClip *pe_actor_clip(const PeActor *a,size_t c) { return a&&c<a->stats.clips?&a->clips[c].info:NULL; }
size_t pe_actor_find_clip(const PeActor *a,const char *name) {
    if(a&&name) for(size_t c=0;c<a->stats.clips;c++) if(strcmp(name,a->clips[c].info.name)==0) return c;
    return PE_ACTOR_BIND_POSE;
}
typedef struct { const float *p[2]; float normals[2][MAX_JOINTS*9]; float alpha; } Pose;
static bool pose_prepare(const PeActor *a,size_t clip,double seconds,Pose *p,bool normals) {
    if(!a||!isfinite(seconds)||seconds<0) return false;
    if(clip==PE_ACTOR_BIND_POSE) { p->p[0]=p->p[1]=NULL; p->alpha=0; return true; }
    if(clip>=a->stats.clips) return false;
    const Clip *c=a->clips+clip;
    double t=c->info.loop?fmod(seconds,c->info.duration):fmin(seconds,c->info.duration);
    double sample=t*c->info.fps; size_t f0=(size_t)floor(sample);
    if(c->info.terminal_at_duration){
        size_t penultimate=c->info.frames-2;
        double start=(double)penultimate/c->info.fps;
        if(t>=start){sample=penultimate+(t-start)/(c->info.duration-start);f0=(size_t)floor(sample);}
    }
    if(f0>=c->info.frames) f0=c->info.frames-1;
    size_t f1=f0+1<c->info.frames?f0+1:f0;
    p->alpha=(float)fmin(1,fmax(0,sample-f0));
    p->p[0]=c->palette+f0*a->stats.joints*12; p->p[1]=c->palette+f1*a->stats.joints*12;
    if(normals)for(int f=0;f<2;f++) for(size_t j=0;j<a->stats.joints;j++)
        if(!normal_matrix(p->p[f]+j*12,p->normals[f]+j*9)) return false;
    return true;
}
static void vertex_pose(const Vertex *v,const Pose *p,PeActorVertex *out) {
    *out=v->vertex;
    if(p->p[0]) {
        double pos[2][3]={{0}},normal[2][3]={{0}};
        for(int f=0;f<2;f++) for(int j=0;j<4;j++) {
            double w=v->weight[j]; if(w==0) continue;
            const float *m=p->p[f]+v->joint[j]*12,*n=p->normals[f]+v->joint[j]*9;
            for(int k=0;k<3;k++) {
                pos[f][k]+=w*(m[k*4]*v->vertex.position[0]+m[k*4+1]*v->vertex.position[1]+m[k*4+2]*v->vertex.position[2]+m[k*4+3]);
                normal[f][k]+=w*(n[k*3]*v->vertex.normal[0]+n[k*3+1]*v->vertex.normal[1]+n[k*3+2]*v->vertex.normal[2]);
            }
        }
        double len=0;
        for(int k=0;k<3;k++) { out->position[k]=(float)(pos[0][k]*(1-p->alpha)+pos[1][k]*p->alpha); out->normal[k]=(float)(normal[0][k]*(1-p->alpha)+normal[1][k]*p->alpha); len+=(double)out->normal[k]*out->normal[k]; }
        /* Opposed frame/influence normals can cancel; preserve a finite unit
         * normal instead of normalizing zero. This is an interpolation limit. */
        if(len<1e-20) memcpy(out->normal,v->vertex.normal,sizeof(out->normal));
    }
    double length=sqrt((double)out->normal[0]*out->normal[0]+(double)out->normal[1]*out->normal[1]+(double)out->normal[2]*out->normal[2]);
    for(int k=0;k<3;k++) out->normal[k]=(float)(out->normal[k]/length);
}
bool pe_actor_pose(const PeActor *a,size_t clip,double seconds,PeActorVertex *out,size_t capacity) {
    if(!a||!out||capacity<a->stats.vertices) return false;
    Pose p; if(!pose_prepare(a,clip,seconds,&p,true)) return false;
    for(size_t k=0;k<a->stats.vertices;k++) vertex_pose(a->vertices+k,&p,out+k);
    return true;
}
bool pe_actor_pose_vertex(const PeActor *a,size_t clip,double seconds,size_t vertex,PeActorVertex *out) {
    if(!a||!out||vertex>=a->stats.vertices) return false;
    Pose p; if(!pose_prepare(a,clip,seconds,&p,true)) return false;
    vertex_pose(a->vertices+vertex,&p,out); return true;
}

bool pe_actor_pose_joint_point(const PeActor *a,size_t clip,double seconds,size_t joint,const float point[3],float output[3]) {
    if(!a||!point||!output||joint>=a->stats.joints)return false;
    for(int k=0;k<3;k++)if(!isfinite(point[k])||fabsf(point[k])>10000)return false;
    Pose p;if(!pose_prepare(a,clip,seconds,&p,false))return false;
    float result[3];
    for(int k=0;k<3;k++){
        double value=point[k];
        if(p.p[0]){
            double v[2];
            for(int f=0;f<2;f++){const float*m=p.p[f]+joint*12+k*4;v[f]=(double)m[0]*point[0]+(double)m[1]*point[1]+(double)m[2]*point[2]+m[3];}
            value=v[0]*(1-p.alpha)+v[1]*p.alpha;
        }
        if(!isfinite(value)||fabs(value)>3.402823466e38)return false;
        result[k]=(float)value;
    }
    memcpy(output,result,sizeof(result));return true;
}

static bool blend_weights(const PeActor *a,const PeActorBlendSample *samples,size_t count,double *total,size_t *positive){
    if(!a||!samples||!count||count>PE_ACTOR_BLEND_MAX)return false;
    double sum=0;size_t active=0;
    for(size_t i=0;i<count;i++){
        if(!isfinite(samples[i].weight)||samples[i].weight<0||!isfinite(samples[i].seconds)||samples[i].seconds<0||
           (samples[i].clip!=PE_ACTOR_BIND_POSE&&samples[i].clip>=a->stats.clips))return false;
        sum+=samples[i].weight;if(samples[i].weight>0)active++;
    }
    if(!isfinite(sum)||sum<=0)return false;
    *total=sum;*positive=active;return true;
}
bool pe_actor_blend_valid(const PeActor *a,const PeActorBlendSample *samples,size_t count){
    double total;size_t positive;
    if(!blend_weights(a,samples,count,&total,&positive))return false;
    /* Actors are immutable validated palettes; prepare checks exact sampled
     * normal transforms before any caller mutates a pose or submits drawing. */
    Pose pose;
    for(size_t i=0;i<count;i++)if(samples[i].weight>0&&!pose_prepare(a,samples[i].clip,samples[i].seconds,&pose,true))return false;
    return true;
}
typedef struct { Pose poses[PE_ACTOR_BLEND_MAX];double weights[PE_ACTOR_BLEND_MAX];size_t count; } Blend;
static bool blend_prepare(const PeActor *a,const PeActorBlendSample *samples,size_t count,Blend *b){
    double total;size_t positive;
    if(!blend_weights(a,samples,count,&total,&positive))return false;
    b->count=0;
    for(size_t i=0;i<count;i++)if(samples[i].weight>0){
        size_t j=b->count++;
        if(!pose_prepare(a,samples[i].clip,samples[i].seconds,&b->poses[j],true))return false;
        b->weights[j]=(double)samples[i].weight/total;
    }
    return true;
}
static void vertex_blend(const Vertex *v,const Blend *b,PeActorVertex *out){
    if(b->count==1){vertex_pose(v,&b->poses[0],out);return;}
    double position[3]={0},normal[3]={0};PeActorVertex sample;
    for(size_t i=0;i<b->count;i++){
        vertex_pose(v,&b->poses[i],&sample);
        for(int k=0;k<3;k++){position[k]+=b->weights[i]*sample.position[k];normal[k]+=b->weights[i]*sample.normal[k];}
    }
    double length=sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
    if(length<1e-10){
        for(int k=0;k<3;k++)normal[k]=v->vertex.normal[k];
        length=sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);
    }
    *out=v->vertex;
    for(int k=0;k<3;k++){out->position[k]=(float)position[k];out->normal[k]=(float)(normal[k]/length);}
}
bool pe_actor_pose_blend(const PeActor *a,const PeActorBlendSample *samples,size_t count,PeActorVertex *out,size_t capacity){
    if(!a||!out||capacity<a->stats.vertices)return false;
    Blend b;if(!blend_prepare(a,samples,count,&b))return false;
    for(size_t i=0;i<a->stats.vertices;i++)vertex_blend(a->vertices+i,&b,out+i);
    return true;
}
bool pe_actor_pose_vertex_blend(const PeActor *a,const PeActorBlendSample *samples,size_t count,size_t vertex,PeActorVertex *out){
    if(!a||!out||vertex>=a->stats.vertices)return false;
    Blend b;if(!blend_prepare(a,samples,count,&b))return false;
    vertex_blend(a->vertices+vertex,&b,out);return true;
}
bool pe_actor_pose_joint_point_blend(const PeActor *a,const PeActorBlendSample *samples,size_t count,size_t joint,const float point[3],float output[3]){
    if(!a||!point||!output||joint>=a->stats.joints)return false;
    double total;size_t positive;
    if(!blend_weights(a,samples,count,&total,&positive))return false;
    double sum[3]={0};float result[3]={0};
    for(size_t i=0;i<count;i++)if(samples[i].weight>0){
        if(!pe_actor_pose_joint_point(a,samples[i].clip,samples[i].seconds,joint,point,result))return false;
        for(int k=0;k<3;k++)sum[k]+=(double)samples[i].weight/total*result[k];
    }
    /* Avoid extra arithmetic at the exact single-pose endpoint. */
    if(positive>1)for(int k=0;k<3;k++)result[k]=(float)sum[k];
    memcpy(output,result,sizeof result);return true;
}
static bool driven_valid(const PeActor *a,const unsigned char *driver,const float *delta){
    if(!a||!driver||!delta)return false;
    for(size_t j=0;j<a->stats.joints;j++){
        if(driver[j]>=PE_ACTOR_DRIVE_BONES)return false;
        for(int k=0;k<12;k++)if(!isfinite(delta[driver[j]*12+k])||fabsf(delta[driver[j]*12+k])>1000000)return false;
    }
    return true;
}
bool pe_actor_pose_blend_driven(const PeActor *a,const PeActorBlendSample *samples,size_t count,const unsigned char *driver,
                                const float *delta,PeActorVertex *out,size_t capacity){
    if(!a||!out||capacity<a->stats.vertices||!driven_valid(a,driver,delta))return false;
    double total;size_t positive;
    if(!blend_weights(a,samples,count,&total,&positive))return false;
    static const float identity[12]={1,0,0,0,0,1,0,0,0,0,1,0};
    float moved[MAX_JOINTS*12];Pose pose={.p={moved,moved}};size_t joints=a->stats.joints;
    double blended[MAX_JOINTS][12];memset(blended,0,joints*sizeof blended[0]);
    for(size_t i=0;i<count;i++)if(samples[i].weight>0){
        Pose p;if(!pose_prepare(a,samples[i].clip,samples[i].seconds,&p,false))return false;
        double w=(double)samples[i].weight/total;
        for(size_t j=0;j<joints;j++)for(int k=0;k<12;k++)
            blended[j][k]+=w*(p.p[0]?(double)p.p[0][j*12+k]*(1-p.alpha)+(double)p.p[1][j*12+k]*p.alpha:identity[k]);
    }
    for(size_t j=0;j<joints;j++){
        const float *d=delta+driver[j]*12;const double *m=blended[j];
        for(int r=0;r<3;r++)for(int k=0;k<4;k++){
            double v=(double)d[r*4]*m[k]+(double)d[r*4+1]*m[4+k]+(double)d[r*4+2]*m[8+k]+(k==3?d[r*4+3]:0);
            if(!isfinite(v)||fabs(v)>1000000)return false;
            moved[j*12+r*4+k]=(float)v;
        }
        if(!normal_matrix(moved+j*12,pose.normals[0]+j*9))return false;
    }
    memcpy(pose.normals[1],pose.normals[0],joints*9*sizeof(float));
    for(size_t i=0;i<a->stats.vertices;i++)vertex_pose(a->vertices+i,&pose,out+i);
    return true;
}
bool pe_actor_pose_joint_point_blend_driven(const PeActor *a,const PeActorBlendSample *samples,size_t count,const unsigned char *driver,
                                            const float *delta,size_t joint,const float point[3],float output[3]){
    float p[3];
    if(!output||!driven_valid(a,driver,delta)||!pe_actor_pose_joint_point_blend(a,samples,count,joint,point,p))return false;
    const float *d=delta+driver[joint]*12;float result[3];
    for(int r=0;r<3;r++){result[r]=d[r*4]*p[0]+d[r*4+1]*p[1]+d[r*4+2]*p[2]+d[r*4+3];if(!isfinite(result[r]))return false;}
    memcpy(output,result,sizeof result);return true;
}
