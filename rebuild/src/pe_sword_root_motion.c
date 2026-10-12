#include "pe_sword_root_motion.h"
#include "pe_sword_motion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exact currently qualified source profile. Exporter-generated metadata can
 * replace this table without widening the accepted binary contract. */
typedef struct {uint32_t id,count;double duration,begin;} Profile;
static const Profile profiles[]={
    {231,34,1.0666667222976685,-1.4901161193847656e-8},
    {240,35,1.100000262260437,0},
    {247,40,1.2666668891906738,0},
    {254,31,.9666671752929688,0}
};
#define HUMAN_SCALE .9932681322097778
#define SOURCE_RATE 30.0
struct PeSwordRootMotion {double raw[4][40][2];};
static uint32_t u32(const unsigned char *p){
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static double f64(const unsigned char *p){
    uint64_t bits=0;for(unsigned i=0;i<8;i++)bits|=(uint64_t)p[i]<<(8*i);
    double value;memcpy(&value,&bits,sizeof value);return value;
}
static int index_for(uint32_t id){for(unsigned i=0;i<4;i++)if(profiles[i].id==id)return (int)i;return -1;}
PeSwordRootMotion *pe_sword_root_motion_load(const char *path,char *error,size_t capacity){
    if(error&&capacity)error[0]=0;
    FILE *file=path?fopen(path,"rb"):NULL;PeSwordRootMotion *motion=NULL;
    unsigned char header[24],clip[32],pair[16];
    if(!file)goto bad;
    if(fread(header,1,sizeof header,file)!=sizeof header||memcmp(header,"PESR",4)||
       u32(header+4)!=1||u32(header+8)!=777||u32(header+12)!=4||f64(header+16)!=HUMAN_SCALE)goto bad;
    motion=calloc(1,sizeof *motion);if(!motion)goto bad;
    for(unsigned i=0;i<4;i++){
        const Profile *p=&profiles[i];
        if(fread(clip,1,sizeof clip,file)!=sizeof clip||u32(clip)!=p->id||u32(clip+4)!=p->count||
           f64(clip+8)!=p->duration||f64(clip+16)!=p->begin||f64(clip+24)!=SOURCE_RATE)goto bad;
        /* Both endpoint phases must lie inside actual stored dense knots. */
        double first=-p->begin*SOURCE_RATE,last=(p->duration-p->begin)*SOURCE_RATE;
        if(first<0||last>(double)p->count-1||last<first)goto bad;
        for(unsigned j=0;j<p->count;j++){
            if(fread(pair,1,sizeof pair,file)!=sizeof pair)goto bad;
            for(unsigned axis=0;axis<2;axis++){
                double value=f64(pair+8*axis);
                if(!isfinite(value)||fabs(value)>10000)goto bad;
                motion->raw[i][j][axis]=value;
            }
        }
    }
    if(fgetc(file)!=EOF||ferror(file))goto bad;
    if(fclose(file)!=0){file=NULL;goto bad;}
    return motion;
bad:
    if(file)fclose(file);
    free(motion);
    if(error&&capacity)snprintf(error,capacity,"Invalid or unreadable PESR1 female sword source curves");
    return NULL;
}
void pe_sword_root_motion_free(PeSwordRootMotion *motion){free(motion);}
double pe_sword_root_motion_duration(const PeSwordRootMotion *motion,uint32_t id){
    int i=index_for(id);return motion&&i>=0?profiles[i].duration:0;
}
static bool raw_sample(const PeSwordRootMotion *motion,uint32_t id,double time,double out[2]){
    int i=index_for(id);
    if(!motion||i<0||!isfinite(time)||time<0||time>profiles[i].duration)return false;
    double phase=(time-profiles[i].begin)*SOURCE_RATE;
    if(!isfinite(phase)||phase<0||phase>(double)profiles[i].count-1)return false;
    unsigned lo=(unsigned)phase,hi=lo+1<profiles[i].count?lo+1:lo;
    double alpha=phase-lo;
    for(unsigned axis=0;axis<2;axis++)
        out[axis]=motion->raw[i][lo][axis]+(motion->raw[i][hi][axis]-motion->raw[i][lo][axis])*alpha;
    return true;
}
bool pe_sword_root_motion_sample(const PeSwordRootMotion *motion,uint32_t id,double time,PeVec3 *out){
    double start[2],end[2];
    if(!out||!raw_sample(motion,id,0,start)||!raw_sample(motion,id,time,end))return false;
    *out=(PeVec3){(float)((end[0]-start[0])*HUMAN_SCALE),0,(float)((end[1]-start[1])*HUMAN_SCALE)};
    return true;
}
bool pe_sword_root_motion_delta(const PeSwordRootMotion *motion,uint32_t id,double from,double to,PeVec3 *out){
    double start[2],end[2];
    if(!out||from>to||!raw_sample(motion,id,from,start)||!raw_sample(motion,id,to,end))return false;
    *out=(PeVec3){(float)((end[0]-start[0])*HUMAN_SCALE),0,(float)((end[1]-start[1])*HUMAN_SCALE)};
    return true;
}
bool pe_sword_root_motion_world_delta(const PeSwordRootMotion *motion,const PeCombatInterval *interval,
                                      double body_yaw_degrees,double world_delta[3]){
    if(!motion||!interval||!world_delta||!interval->valid||interval->gender!=PE_COMBAT_FEMALE||
       interval->view!=PE_COMBAT_THIRD_PERSON||!isfinite(body_yaw_degrees)||
       (interval->variant!=PE_COMBAT_STATIONARY&&interval->stage==0))return false;
    const PeCombatClip *clip=pe_combat_variant_clip(interval->gender,interval->view,interval->variant,interval->stage);
    PeVec3 delta;
    if(!clip||!pe_sword_root_motion_delta(motion,(uint32_t)clip->path_id,interval->from,interval->to,&delta))return false;
    const double radians=remainder(body_yaw_degrees,360.0)*.017453292519943295769;
    const double c=cos(radians),s=sin(radians);
    world_delta[0]=c*delta.x+s*delta.z;world_delta[1]=0;world_delta[2]=c*delta.z-s*delta.x;
    return true;
}
bool pe_sword_root_motion_request(const PeSwordRootMotion *motion,
                                  const PeCombatInterval *interval,
                                  PeVec3 current_velocity,double body_yaw_degrees,
                                  double dt,PeVec3 *request){
    double world_delta[3];
    if(!request||!pe_sword_root_motion_world_delta(motion,interval,body_yaw_degrees,world_delta))return false;
    return pe_sword_motion_request(world_delta,current_velocity,dt,request);
}
