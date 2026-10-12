#include "pe_sprint_root_motion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DURATION 1.8000068664550781
#define SCALE .9932681322097778
struct PeSprintRootMotion {double knots[56][2];};
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static double f64(const unsigned char *p){uint64_t bits=0;for(unsigned i=0;i<8;i++)bits|=(uint64_t)p[i]<<(8*i);double value;memcpy(&value,&bits,8);return value;}
PeSprintRootMotion *pe_sprint_root_motion_load(const char *path,char *error,size_t cap){
    if(error&&cap)*error=0;
    FILE *file=path?fopen(path,"rb"):NULL;PeSprintRootMotion *motion=NULL;
    unsigned char header[56],pair[16];
    if(!file||fread(header,1,56,file)!=56||memcmp(header,"PESR",4)||u32(header+4)!=1||
       u32(header+8)!=777||u32(header+12)!=1||f64(header+16)!=SCALE||u32(header+24)!=401||
       u32(header+28)!=56||f64(header+32)!=DURATION||f64(header+40)!=0||f64(header+48)!=30)goto bad;
    motion=calloc(1,sizeof *motion);if(!motion)goto bad;
    for(unsigned i=0;i<56;i++){
        if(fread(pair,1,16,file)!=16)goto bad;
        for(unsigned axis=0;axis<2;axis++){
            double value=f64(pair+axis*8);if(!isfinite(value)||fabs(value)>10000)goto bad;
            motion->knots[i][axis]=value;
        }
    }
    if(fgetc(file)!=EOF||ferror(file))goto bad;
    if(fclose(file)!=0){file=NULL;goto bad;}
    return motion;
bad:
    if(file)fclose(file);
    free(motion);
    if(error&&cap)snprintf(error,cap,"Invalid or unreadable PESR1 female sprint401 source curve");
    return NULL;
}
void pe_sprint_root_motion_free(PeSprintRootMotion *motion){free(motion);}
double pe_sprint_root_motion_duration(const PeSprintRootMotion *motion){return motion?DURATION:0;}
static bool sample(const PeSprintRootMotion *m,double time,double out[2]){
    if(!m||!isfinite(time)||time<0||time>DURATION)return false;
    double phase=time*30;unsigned lo=(unsigned)phase,hi=lo+1;double alpha=phase-lo;
    if(hi>=56)return false;
    for(unsigned axis=0;axis<2;axis++)out[axis]=(m->knots[lo][axis]+alpha*(m->knots[hi][axis]-m->knots[lo][axis])-m->knots[0][axis])*SCALE;
    return true;
}
bool pe_sprint_root_motion_sample(const PeSprintRootMotion *m,double time,PeVec3 *out){
    double value[2];if(!out||!sample(m,time,value))return false;
    *out=(PeVec3){(float)value[0],0,(float)value[1]};return true;
}
bool pe_sprint_root_motion_world_delta(const PeSprintRootMotion *m,const PeCombatInterval *span,double yaw,double out[3]){
    if(!out||!span||!span->valid||span->variant!=PE_COMBAT_SPRINT||span->stage!=0||
       span->gender!=PE_COMBAT_FEMALE||span->view!=PE_COMBAT_THIRD_PERSON||!isfinite(yaw)||span->from>span->to)return false;
    double a[2],b[2];if(!sample(m,span->from,a)||!sample(m,span->to,b))return false;
    const double angle=remainder(yaw,360)*.017453292519943295769,c=cos(angle),s=sin(angle);
    const double x=b[0]-a[0],z=b[1]-a[1];
    out[0]=c*x+s*z;out[1]=0;out[2]=c*z-s*x;return true;
}
