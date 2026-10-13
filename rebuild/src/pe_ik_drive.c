#include "pe_ik_drive.h"
#include "pe_ik_pose.h"
#include "pe_ik_skin.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE (1024u*1024u)
#define MAX_ENTRIES 256u
#define MAX_JOINTS 256u
#define NAME 48u
typedef struct {char group[NAME]; uint32_t part,joints; uint64_t skeleton; unsigned char *driver;} Entry;
struct PeIkDrive {
    PeIkPose *pose;
    Entry *entries; size_t count;
    /* IKCmpt sets both arm mapping weights to 0 while equipment is held. */
    PeFbbikRig unmapped;
    PeFbbik solver[2];
};
static void error_set(char *error,size_t capacity,const char *message){if(error&&capacity)snprintf(error,capacity,"%s",message);}
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static bool path(char *out,size_t capacity,const char *root,const char *tail){
    int n=snprintf(out,capacity,"%s/%s",root,tail);return n>0&&(size_t)n<capacity;
}
static bool exists(const char *root,const char *tail){
    char name[4096];if(!root||!path(name,sizeof name,root,tail))return false;
    FILE *f=fopen(name,"rb");if(f)fclose(f);return f!=NULL;
}
bool pe_ik_drive_present(const char *root){
    return exists(root,"player-ik-pose/player-ik-pose.peik")||exists(root,"player-ik-drivers/player-ik-drivers.peid");
}
void pe_ik_drive_free(PeIkDrive *d){
    if(!d)return;
    if(d->entries)for(size_t i=0;i<d->count;i++)free(d->entries[i].driver);
    free(d->entries);pe_ik_pose_free(d->pose);free(d);
}
static bool load_drivers(PeIkDrive *d,const char *name,const PeFbbikRig *rig){
    FILE *f=fopen(name,"rb");if(!f)return false;
    unsigned char *bytes=malloc(MAX_FILE+1);if(!bytes){fclose(f);return false;}
    size_t size=fread(bytes,1,MAX_FILE+1,f);bool ok=!ferror(f)&&size<=MAX_FILE;fclose(f);
    size_t at=12+(size_t)rig->bone_count*4+4;
    ok=ok&&size>=at&&!memcmp(bytes,"PEID",4)&&u32(bytes+4)==1&&u32(bytes+8)==rig->bone_count;
    for(unsigned i=0;ok&&i<rig->bone_count;i++)ok=u32(bytes+12+i*4)==pe_fbbik_female_bone_id(i);
    size_t count=ok?u32(bytes+at-4):0;ok=ok&&count&&count<=MAX_ENTRIES;
    if(ok){d->entries=calloc(count,sizeof *d->entries);ok=d->entries!=NULL;}
    for(size_t i=0;ok&&i<count;i++){
        Entry *e=d->entries+i;d->count=i+1;
        if(size-at<NAME+16){ok=false;break;}
        memcpy(e->group,bytes+at,NAME);e->part=u32(bytes+at+NAME);
        e->skeleton=(uint64_t)u32(bytes+at+NAME+4)|(uint64_t)u32(bytes+at+NAME+8)<<32;e->joints=u32(bytes+at+NAME+12);at+=NAME+16;
        const char *end=memchr(e->group,0,NAME);size_t length=end?(size_t)(end-e->group):NAME,padded=((size_t)e->joints+3)&~(size_t)3;
        ok=length&&length<NAME&&e->joints&&e->joints<=MAX_JOINTS&&e->part<64&&size-at>=padded;
        for(size_t k=0;ok&&k<NAME;k++){unsigned char c=(unsigned char)e->group[k];ok=k<length?(c=='-'||(c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')):!c;}
        for(size_t k=0;ok&&k<i;k++)ok=strcmp(e->group,d->entries[k].group)||e->part!=d->entries[k].part;
        if(!ok)break;
        e->driver=malloc(e->joints);if(!e->driver){ok=false;break;}
        memcpy(e->driver,bytes+at,e->joints);
        for(size_t k=0;ok&&k<padded;k++)ok=k<e->joints?bytes[at+k]<rig->bone_count:!bytes[at+k];
        at+=padded;
    }
    ok=ok&&at==size;free(bytes);return ok;
}
PeIkDrive *pe_ik_drive_load(const char *root,char *error,size_t capacity){
    error_set(error,capacity,"");
    char name[4096];const PeFbbikRig *rig=pe_fbbik_female_rig();PeFbbikTransform rest[PE_FBBIK_MAX_BONES];
    PeIkDrive *d=calloc(1,sizeof *d);
    if(!d||!root||!rig||!path(name,sizeof name,root,"player-ik-pose/player-ik-pose.peik")){error_set(error,capacity,"IK drive allocation or path failed");free(d);return NULL;}
    d->pose=pe_ik_pose_load(name,rig,error,capacity);if(!d->pose)goto fail;
    if(!path(name,sizeof name,root,"player-ik-drivers/player-ik-drivers.peid")||!load_drivers(d,name,rig)){
        error_set(error,capacity,"missing, invalid or mismatched PEID driver table");goto fail;
    }
    d->unmapped=*rig;d->unmapped.limb_weight[0]=d->unmapped.limb_weight[1]=0;
    if(!pe_fbbik_female_rest(rest)||!pe_fbbik_init(&d->solver[0],&d->unmapped,rest)||!pe_fbbik_init(&d->solver[1],rig,rest)){
        error_set(error,capacity,"IK solver initiation failed");goto fail;
    }
    return d;
fail:
    pe_ik_drive_free(d);return NULL;
}
const unsigned char *pe_ik_drive_drivers(const PeIkDrive *d,const char *group,size_t part,const PeActor *actor){
    if(!d||!group||!actor)return NULL;
    for(size_t i=0;i<d->count;i++){
        const Entry *e=d->entries+i;
        if(e->part==part&&!strcmp(e->group,group))
            return e->joints==pe_actor_stats(actor).joints&&e->skeleton==pe_actor_skeleton_hash(actor)?e->driver:NULL;
    }
    return NULL;
}
bool pe_ik_drive_solve(const PeIkDrive *d,const PeIkDriveSample *samples,size_t count,bool arms_mapped,PeIkDriveFrame *out){
    if(!d||!samples||!out||!count||count>PE_ACTOR_BLEND_MAX)return false;
    const PeFbbik *solver=&d->solver[arms_mapped?1:0];unsigned bones=solver->rig->bone_count;
    double total=0,position[PE_FBBIK_MAX_BONES][3]={{0}},rotation[PE_FBBIK_MAX_BONES][4]={{0}};
    for(size_t i=0;i<count;i++){
        if(!isfinite(samples[i].weight)||samples[i].weight<0||!isfinite(samples[i].seconds)||samples[i].seconds<0)return false;
        total+=samples[i].weight;
    }
    if(!isfinite(total)||total<=0)return false;
    PeFbbikTransform local[PE_FBBIK_MAX_BONES]={0},first[PE_FBBIK_MAX_BONES];bool have=false;
    for(size_t i=0;i<count;i++)if(samples[i].weight>0){
        size_t clip=samples[i].clip?pe_ik_pose_find_clip(d->pose,samples[i].clip):PE_ACTOR_BIND_POSE;
        if(clip==PE_ACTOR_BIND_POSE||!pe_ik_pose_sample(d->pose,clip,samples[i].seconds,local))return false;
        if(!have){memcpy(first,local,sizeof first);have=true;}
        double w=samples[i].weight/total;
        for(unsigned b=0;b<bones;b++){
            PeFbbikQuat q=local[b].rotation,f=first[b].rotation;
            /* Each rotation joins the sum in the first sample's hemisphere. */
            double sign=(double)q.x*f.x+(double)q.y*f.y+(double)q.z*f.z+(double)q.w*f.w<0?-w:w;
            position[b][0]+=w*local[b].position.x;position[b][1]+=w*local[b].position.y;position[b][2]+=w*local[b].position.z;
            rotation[b][0]+=sign*q.x;rotation[b][1]+=sign*q.y;rotation[b][2]+=sign*q.z;rotation[b][3]+=sign*q.w;
        }
    }
    for(unsigned b=0;b<bones;b++){
        double n=sqrt(rotation[b][0]*rotation[b][0]+rotation[b][1]*rotation[b][1]+rotation[b][2]*rotation[b][2]+rotation[b][3]*rotation[b][3]);
        if(!isfinite(n)||n<1e-6)return false;
        local[b].position=(PeVec3){(float)position[b][0],(float)position[b][1],(float)position[b][2]};
        local[b].rotation=(PeFbbikQuat){(float)(rotation[b][0]/n),(float)(rotation[b][1]/n),(float)(rotation[b][2]/n),(float)(rotation[b][3]/n)};
    }
    PeFbbikInput input;PeFbbikOutput solved;PeIkDriveFrame frame;
    if(!pe_fbbik_input(solver,local,&input))return false;
    input.weight=1;
    if(!pe_fbbik_solve(solver,&input,&solved)||!pe_ik_skin_deltas(solver->rig,local,solved.local,frame.delta))return false;
    for(unsigned b=bones;b<PE_FBBIK_MAX_BONES;b++)memset(frame.delta[b],0,sizeof frame.delta[b]);
    *out=frame;return true;
}
