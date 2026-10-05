#include "pe_hitbox.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct PeHitboxes { size_t count;uint32_t joints;uint64_t fingerprint;PeHitboxDefinition *definitions; };
static uint32_t u32(const unsigned char *p){return(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static float f32(const unsigned char *p){uint32_t b=u32(p);float f;memcpy(&f,&b,4);return f;}
static void error_set(char *e,size_t c,const char*m){if(e&&c)snprintf(e,c,"%s",m);}
void pe_hitboxes_free(PeHitboxes*b){if(b){free(b->definitions);free(b);}}
size_t pe_hitboxes_count(const PeHitboxes*b){return b?b->count:0;}
const PeHitboxDefinition *pe_hitboxes_get(const PeHitboxes*b,size_t i){return b&&i<b->count?b->definitions+i:NULL;}
PeHitboxes *pe_hitboxes_load(const char *path,const PeActor *actor,char *error,size_t capacity){
    error_set(error,capacity,"");if(!path||!actor){error_set(error,capacity,"Missing hitbox path/actor");return NULL;}
    FILE*f=fopen(path,"rb");if(!f){error_set(error,capacity,"Cannot open hitboxes");return NULL;}
    unsigned char h[32];PeHitboxes*b=NULL;
    if(fread(h,1,32,f)!=32||memcmp(h,"PEHB",4)||u32(h+4)!=1||u32(h+24)||u32(h+28))goto bad;
    size_t count=u32(h+8);uint32_t joints=u32(h+12);uint64_t hash=(uint64_t)u32(h+16)|(uint64_t)u32(h+20)<<32;
    if(!count||count>1024||joints!=pe_actor_stats(actor).joints||hash!=pe_actor_skeleton_hash(actor))goto bad;
    b=calloc(1,sizeof(*b));if(!b)goto bad;b->count=count;b->joints=joints;b->fingerprint=hash;
    b->definitions=calloc(count,sizeof(*b->definitions));if(!b->definitions)goto bad;
    for(size_t i=0;i<count;i++){
        unsigned char bytes[104];if(fread(bytes,1,sizeof(bytes),f)!=sizeof(bytes))goto bad;
        PeHitboxDefinition*d=b->definitions+i;memcpy(d->name,bytes,32);bool nul=false;
        for(int k=0;k<32;k++){unsigned char c=(unsigned char)d->name[k];if(!c)nul=true;else if(nul||c<32||c>126)goto bad;}
        if(!nul||!d->name[0])goto bad;
        for(size_t k=0;k<i;k++)if(!strcmp(d->name,b->definitions[k].name))goto bad;
        d->kind=u32(bytes+32);d->group=u32(bytes+36);d->joint=u32(bytes+40);d->form=u32(bytes+44);d->material=u32(bytes+48);d->damage_scale=f32(bytes+52);
        if(d->kind>PE_HITBOX_ATTACK||(d->kind==PE_HITBOX_DEFENCE?d->group!=0:(d->group!=PE_HITBOX_LEFT&&d->group!=PE_HITBOX_RIGHT))||
           (d->joint!=PE_HITBOX_DISABLED&&d->joint>=joints)||d->form>128||d->material>128||!isfinite(d->damage_scale)||d->damage_scale<=0||d->damage_scale>1000)goto bad;
        for(int p=0;p<4;p++)for(int k=0;k<3;k++){
            float value=f32(bytes+56+(p*3+k)*4);if(!isfinite(value)||fabsf(value)>10000||(d->joint==PE_HITBOX_DISABLED&&value!=0))goto bad;
            d->bind_points[p][k]=value;
        }
        if(d->joint!=PE_HITBOX_DISABLED){double length=0;for(int k=0;k<3;k++){double q=(double)d->bind_points[3][k]-d->bind_points[2][k];length+=q*q;}if(length<1e-20)goto bad;}
    }
    if(fgetc(f)!=EOF||ferror(f))goto bad;
    fclose(f);return b;
bad:
    fclose(f);pe_hitboxes_free(b);error_set(error,capacity,"Invalid PEHB or mismatched actor skeleton");return NULL;
}
static bool finite_point(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z)&&fabsf(p.x)<=1000000&&fabsf(p.y)<=1000000&&fabsf(p.z)<=1000000;}
bool pe_hitbox_pose(const PeHitboxes *b,const PeActor *a,size_t index,size_t clip,double seconds,PeVec3 root,float yaw,float scale,PeHitboxCapsule*out){
    const PeHitboxDefinition*d=pe_hitboxes_get(b,index);
    if(!d||!a||!out||d->joint==PE_HITBOX_DISABLED||b->joints!=pe_actor_stats(a).joints||b->fingerprint!=pe_actor_skeleton_hash(a)||
       !finite_point(root)||!isfinite(yaw)||!isfinite(scale)||scale<=0||scale>1000)return false;
    float points[4][3];
    for(int p=0;p<4;p++)if(!pe_actor_pose_joint_point(a,clip,seconds,d->joint,d->bind_points[p],points[p]))return false;
    double angle=remainder((double)yaw,360)*3.14159265358979323846/180,c=cos(angle),s=sin(angle),radius=0;
    PeVec3 transformed[2];
    for(int p=0;p<2;p++){
        transformed[p]=(PeVec3){root.x+(float)(scale*(c*points[p][0]+s*points[p][2])),root.y+scale*points[p][1],root.z+(float)(scale*(-s*points[p][0]+c*points[p][2]))};
        if(!finite_point(transformed[p]))return false;
    }
    for(int k=0;k<3;k++){double q=(double)points[3][k]-points[2][k];radius+=q*q;}
    radius=sqrt(radius)*scale;if(!isfinite(radius)||radius<=0||radius>1000000)return false;
    *out=(PeHitboxCapsule){transformed[0],transformed[1],(float)radius};return true;
}
static double clamp01(double x){return fmin(1,fmax(0,x));}
static bool capsule_valid(PeHitboxCapsule c){return finite_point(c.a)&&finite_point(c.b)&&isfinite(c.radius)&&c.radius>0&&c.radius<=1000000;}
bool pe_hitbox_contact(PeHitboxCapsule self,PeHitboxCapsule target,PeHitboxContact*out){
    if(!capsule_valid(self)||!capsule_valid(target))return false;
    double p[3]={self.a.x,self.a.y,self.a.z},q[3]={target.a.x,target.a.y,target.a.z};
    double d[3]={(double)self.b.x-self.a.x,(double)self.b.y-self.a.y,(double)self.b.z-self.a.z};
    double e[3]={(double)target.b.x-target.a.x,(double)target.b.y-target.a.y,(double)target.b.z-target.a.z};
    double a=0,b=0,c=0,f=0,g=0;
    for(int k=0;k<3;k++){a+=d[k]*d[k];b+=d[k]*e[k];c+=e[k]*e[k];f+=d[k]*(p[k]-q[k]);g+=e[k]*(p[k]-q[k]);}
    double u=0,v=0;
    if(a<=1e-24){if(c>1e-24)v=clamp01(g/c);}
    else if(c<=1e-24)u=clamp01(-f/a);
    else{
        double den=a*c-b*b;u=den>1e-24?clamp01((b*g-c*f)/den):0;
        v=(b*u+g)/c;
        if(v<0){v=0;u=clamp01(-f/a);}else if(v>1){v=1;u=clamp01((b-f)/a);}
    }
    double x[3],y[3],dist2=0;
    for(int k=0;k<3;k++){x[k]=p[k]+d[k]*u;y[k]=q[k]+e[k]*v;double delta=y[k]-x[k];dist2+=delta*delta;}
    double radius=(double)self.radius+target.radius;
    if(!(dist2<radius*radius))return false;
    if(out){
        double distance=sqrt(dist2);PeHitboxContact result={0};
        result.self_axis=(PeVec3){(float)x[0],(float)x[1],(float)x[2]};result.target_axis=(PeVec3){(float)y[0],(float)y[1],(float)y[2]};
        result.point=(PeVec3){(float)(x[0]+(y[0]-x[0])*self.radius/radius),(float)(x[1]+(y[1]-x[1])*self.radius/radius),(float)(x[2]+(y[2]-x[2])*self.radius/radius)};
        if(distance>1e-12)result.normal=(PeVec3){(float)((y[0]-x[0])/distance),(float)((y[1]-x[1])/distance),(float)((y[2]-x[2])/distance)};
        result.axis_distance=(float)distance;*out=result;
    }
    return true;
}
