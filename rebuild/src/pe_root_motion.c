#include "pe_root_motion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {double seconds,position[3];} Sample;
struct PeRootMotion {size_t count;double duration;Sample samples[];};
static uint32_t u32(const unsigned char *p){
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static double f64(const unsigned char *p){
    uint64_t bits=0;for(unsigned i=0;i<8;i++)bits|=(uint64_t)p[i]<<(8*i);
    double value;memcpy(&value,&bits,sizeof value);return value;
}
static void failure(char *error,size_t capacity){
    if(error&&capacity)snprintf(error,capacity,"Invalid or unreadable PERM1 running Knife motion");
}
PeRootMotion *pe_root_motion_load(const char *path,char *error,size_t capacity){
    if(error&&capacity)error[0]=0;
    FILE *f=path?fopen(path,"rb"):NULL;PeRootMotion *m=NULL;unsigned char h[32],row[32];
    if(!f)goto bad;
    if(fread(h,1,sizeof h,f)!=sizeof h||memcmp(h,"PERM",4)||u32(h+4)!=1||
       u32(h+8)!=PE_ROOT_MOTION_CLIP||u32(h+12)!=PE_ROOT_MOTION_SKILL||u32(h+20))goto bad;
    uint32_t count=u32(h+16);double duration=f64(h+24);
    if(count<2||count>PE_ROOT_MOTION_MAX_SAMPLES||!isfinite(duration)||
       duration!=PE_ROOT_MOTION_DURATION)goto bad;
    m=malloc(sizeof *m+count*sizeof *m->samples);if(!m)goto bad;
    m->count=count;m->duration=duration;
    for(size_t i=0;i<count;i++){
        if(fread(row,1,sizeof row,f)!=sizeof row)goto bad;
        Sample *s=&m->samples[i];s->seconds=f64(row);
        if(!isfinite(s->seconds)||s->seconds<0||s->seconds>duration+1.0/60.0||
           (!i&&s->seconds!=0)||(i&&(s->seconds<=m->samples[i-1].seconds||
           s->seconds-m->samples[i-1].seconds>1.0/60.0)))goto bad;
        for(unsigned axis=0;axis<3;axis++){
            s->position[axis]=f64(row+8+8*axis);
            if(!isfinite(s->position[axis])||fabs(s->position[axis])>10000||
               (!i&&s->position[axis]!=0))goto bad;
        }
    }
    if(m->samples[count-2].seconds>=duration||m->samples[count-1].seconds<duration||
       fgetc(f)!=EOF||ferror(f))goto bad;
    fclose(f);return m;
bad:
    if(f)fclose(f);
    free(m);failure(error,capacity);return NULL;
}
void pe_root_motion_free(PeRootMotion *m){free(m);}
size_t pe_root_motion_count(const PeRootMotion *m){return m?m->count:0;}
double pe_root_motion_duration(const PeRootMotion *m){return m?m->duration:0;}
static bool sample(const PeRootMotion *m,double seconds,double out[3]){
    if(!m||!isfinite(seconds)||seconds<0||seconds>m->duration)return false;
    size_t lo=0,hi=m->count-1;
    while(hi-lo>1){size_t mid=lo+(hi-lo)/2;if(m->samples[mid].seconds<seconds)lo=mid;else hi=mid;}
    const Sample *a=&m->samples[lo],*b=&m->samples[hi];
    double alpha=(seconds-a->seconds)/(b->seconds-a->seconds);
    if(alpha<0||alpha>1)return false;
    for(unsigned i=0;i<3;i++)out[i]=a->position[i]+(b->position[i]-a->position[i])*alpha;
    return true;
}
bool pe_root_motion_sample(const PeRootMotion *m,double seconds,PeVec3 *out){
    double p[3];if(!out||!sample(m,seconds,p))return false;
    *out=(PeVec3){(float)p[0],(float)p[1],(float)p[2]};return true;
}
bool pe_root_motion_delta(const PeRootMotion *m,double from,double to,PeVec3 *out){
    double a[3],b[3];if(!out||from>to||!sample(m,from,a)||!sample(m,to,b))return false;
    *out=(PeVec3){(float)(b[0]-a[0]),(float)(b[1]-a[1]),(float)(b[2]-a[2])};return true;
}
