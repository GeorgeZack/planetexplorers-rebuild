#define _POSIX_C_SOURCE 200809L
#include "pe_actor_scene.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
struct PeActorSceneManifest {
    size_t count;
    PeActorScenePart parts[PE_ACTOR_SCENE_MAX_PARTS];
    PeActor *actors[PE_ACTOR_SCENE_MAX_PARTS];
    unsigned char *png[PE_ACTOR_SCENE_MAX_PARTS];
    size_t png_size[PE_ACTOR_SCENE_MAX_PARTS];
    size_t clip_count;
    PeActorClip clips[PE_ACTOR_SCENE_MAX_CLIPS];
    float min[3],max[3];
};
static void fail(char *error,size_t cap,const char *message){if(error&&cap)snprintf(error,cap,"%s",message);}
static bool regular_size(const char *path,uint64_t limit,uint64_t *size){
    struct stat s;if(lstat(path,&s)||!S_ISREG(s.st_mode)||s.st_size<0||(uint64_t)s.st_size>limit)return false;
    *size=(uint64_t)s.st_size;return true;
}
static bool basename_valid(const char *s,const char *suffix){
    size_t n=strlen(s),e=strlen(suffix);if(n<=e||n>255||s[0]=='.'||strstr(s,"..")||strcmp(s+n-e,suffix))return false;
    for(size_t i=0;i<n;i++){unsigned char c=(unsigned char)s[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;}
    return true;
}
static uint32_t be32(const unsigned char *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static unsigned char *png_snapshot(const char *path,size_t *length,uint64_t *pixels){
    uint64_t size;if(!regular_size(path,64u*1024u*1024u,&size)||size<57)return NULL;
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    unsigned char *h=malloc((size_t)size);if(!h){fclose(f);return NULL;}
    bool ok=fread(h,1,(size_t)size,f)==size&&fgetc(f)==EOF&&!ferror(f);fclose(f);
    const unsigned char signature[]={137,80,78,71,13,10,26,10};
    if(!ok||memcmp(h,signature,8)||be32(h+8)!=13||memcmp(h+12,"IHDR",4)){free(h);return NULL;}
    uint32_t w=be32(h+16),height=be32(h+20);
    if(!w||!height||w>8192||height>8192||(uint64_t)w*height>16777216){free(h);return NULL;}
    *pixels=(uint64_t)w*height;*length=(size_t)size;return h;
}
void pe_actor_scene_manifest_free(PeActorSceneManifest *s){
    if(!s)return;
    for(size_t i=0;i<s->count;i++){pe_actor_free(s->actors[i]);free(s->png[i]);}
    free(s);
}
const PeActor *pe_actor_scene_actor(const PeActorSceneManifest *s,size_t p){return s&&p<s->count?s->actors[p]:NULL;}
PeActor *pe_actor_scene_take_actor(PeActorSceneManifest *s,size_t p){
    if(!s||p>=s->count)return NULL;
    PeActor *actor=s->actors[p];s->actors[p]=NULL;return actor;
}
const unsigned char *pe_actor_scene_png(const PeActorSceneManifest *s,size_t p,size_t *length){
    if(!s||p>=s->count||!length)return NULL;
    *length=s->png_size[p];return s->png[p];
}
size_t pe_actor_scene_part_count(const PeActorSceneManifest *s){return s?s->count:0;}
const PeActorScenePart *pe_actor_scene_part(const PeActorSceneManifest *s,size_t p){return s&&p<s->count?s->parts+p:NULL;}
size_t pe_actor_scene_clip_count(const PeActorSceneManifest *s){return s?s->clip_count:0;}
const PeActorClip *pe_actor_scene_clip_at(const PeActorSceneManifest *s,size_t clip){return s&&clip<s->clip_count?s->clips+clip:NULL;}
const PeActorClip *pe_actor_scene_clip(const PeActorSceneManifest *s){return pe_actor_scene_clip_at(s,0);}
size_t pe_actor_scene_find_clip(const PeActorSceneManifest *s,const char *name){
    if(s&&name)for(size_t i=0;i<s->clip_count;i++)if(!strcmp(name,s->clips[i].name))return i;
    return PE_ACTOR_BIND_POSE;
}
bool pe_actor_scene_bounds(const PeActorSceneManifest *s,float min[3],float max[3]){if(!s||!min||!max)return false;memcpy(min,s->min,sizeof s->min);memcpy(max,s->max,sizeof s->max);return true;}
bool pe_actor_scene_clip_matches(const PeActorSceneManifest *s,const PeActor *a){
    if(!s||!a||pe_actor_stats(a).clips!=s->clip_count)return false;
    for(size_t i=0;i<s->clip_count;i++){
        const PeActorClip *c=pe_actor_clip(a,i),*expected=s->clips+i;
        if(strcmp(c->name,expected->name)||c->fps!=expected->fps||c->frames!=expected->frames||
           c->duration!=expected->duration||c->loop!=expected->loop||
           c->terminal_at_duration!=expected->terminal_at_duration)return false;
    }
    return true;
}
PeActorSceneManifest *pe_actor_scene_manifest_load(const char *path,char *error,size_t cap){
    fail(error,cap,"");uint64_t size;
    if(!path||strlen(path)>=PE_ACTOR_SCENE_PATH_CAPACITY||!regular_size(path,65536,&size)||size<6){fail(error,cap,"invalid PEAS path or size");return NULL;}
    FILE *f=fopen(path,"rb");if(!f){fail(error,cap,"cannot open PEAS manifest");return NULL;}
    char *bytes=malloc((size_t)size+1);PeActorSceneManifest *s=calloc(1,sizeof *s);
    if(!bytes||!s){fclose(f);free(bytes);free(s);fail(error,cap,"scene allocation failed");return NULL;}
    bool read=fread(bytes,1,(size_t)size,f)==size&&fgetc(f)==EOF&&!ferror(f);fclose(f);bytes[size]=0;
    if(!read||memchr(bytes,0,(size_t)size)){fail(error,cap,"invalid PEAS text");goto bad;}
    for(size_t i=0;i<size;i++)if((unsigned char)bytes[i]>126||((unsigned char)bytes[i]<32&&bytes[i]!='\n'&&bytes[i]!='\r')){fail(error,cap,"PEAS requires ASCII lines");goto bad;}
    size_t parent=0;const char *slash=strrchr(path,'/');if(slash)parent=(size_t)(slash-path)+1;
    char *line=bytes;size_t number=0;
    while(*line){
        char *next=strchr(line,'\n');if(next)*next++=0;else next=line+strlen(line);
        size_t length=strlen(line);if(length&&line[length-1]=='\r')line[--length]=0;
        if(number++==0){if(strcmp(line,"PEAS1")){fail(error,cap,"unsupported PEAS version");goto bad;}line=next;continue;}
        if(s->count>=PE_ACTOR_SCENE_MAX_PARTS){fail(error,cap,"too many scene parts");goto bad;}
        char *space=strchr(line,' ');if(!space||space==line||strchr(space+1,' ')){fail(error,cap,"PEAS requires two basenames per line");goto bad;}
        *space++=0;if(!basename_valid(line,".peac")||!basename_valid(space,".png")){fail(error,cap,"invalid PEAS basename");goto bad;}
        if(parent+strlen(line)>=PE_ACTOR_SCENE_PATH_CAPACITY||parent+strlen(space)>=PE_ACTOR_SCENE_PATH_CAPACITY){fail(error,cap,"scene path too long");goto bad;}
        PeActorScenePart *p=&s->parts[s->count];memcpy(p->actor,path,parent);strcpy(p->actor+parent,line);memcpy(p->diffuse,path,parent);strcpy(p->diffuse+parent,space);
        for(size_t i=0;i<s->count;i++)if(!strcmp(s->parts[i].actor,p->actor)){fail(error,cap,"duplicate scene actor");goto bad;}
        s->count++;line=next;
    }
    if(!s->count){fail(error,cap,"empty PEAS scene");goto bad;}
    uint64_t total_bytes=0,total_pixels=0,total_vertices=0,total_indices=0;
    bool first=true;
    for(size_t i=0;i<s->count;i++){
        uint64_t file_size,pixels;
        if(!regular_size(s->parts[i].actor,256u*1024u*1024u,&file_size)||
           !(s->png[i]=png_snapshot(s->parts[i].diffuse,&s->png_size[i],&pixels))){fail(error,cap,"missing/invalid required actor or PNG");goto bad;}
        PeActor *actor=pe_actor_load(s->parts[i].actor,error,cap);if(!actor)goto bad;
        PeActorStats stats=pe_actor_stats(actor);total_vertices+=stats.vertices;total_indices+=stats.indices;
        if(!stats.clips||stats.clips>PE_ACTOR_SCENE_MAX_CLIPS||total_vertices>1000000||total_indices>3000000){pe_actor_free(actor);fail(error,cap,"scene requires 1..64 clips per part within geometry budget");goto bad;}
        /* Exact validated PEAC v1/v2 encoded size from the retained object. A
         * concurrent replacement after stat cannot under-report its budget. */
        total_bytes+=24+(uint64_t)stats.vertices*56+(uint64_t)stats.indices*4+
            (uint64_t)stats.joints*48+s->png_size[i];
        for(size_t clip=0;clip<stats.clips;clip++)
            total_bytes+=48+(uint64_t)pe_actor_clip(actor,clip)->frames*stats.joints*48;
        total_pixels+=pixels;
        if(total_bytes>512u*1024u*1024u||total_pixels>64u*1024u*1024u){pe_actor_free(actor);fail(error,cap,"scene file/texture budget exceeded");goto bad;}
        if(i==0){
            s->clip_count=stats.clips;
            for(size_t clip=0;clip<stats.clips;clip++)s->clips[clip]=*pe_actor_clip(actor,clip);
        }
        if(!pe_actor_scene_clip_matches(s,actor)){pe_actor_free(actor);fail(error,cap,"scene part clip timings differ");goto bad;}
        PeActorVertex *vertices=malloc(stats.vertices*sizeof *vertices);
        if(!vertices||!pe_actor_pose(actor,0,0,vertices,stats.vertices)){free(vertices);pe_actor_free(actor);fail(error,cap,"scene initial pose failed");goto bad;}
        for(size_t v=0;v<stats.vertices;v++)for(int axis=0;axis<3;axis++){
            float p=vertices[v].position[axis];if(!isfinite(p)){free(vertices);pe_actor_free(actor);fail(error,cap,"nonfinite scene bounds");goto bad;}
            if(first&&v==0){s->min[axis]=p;s->max[axis]=p;}else{s->min[axis]=fminf(s->min[axis],p);s->max[axis]=fmaxf(s->max[axis],p);}
        }
        first=false;free(vertices);s->actors[i]=actor;
    }
    free(bytes);return s;
bad:free(bytes);pe_actor_scene_manifest_free(s);return NULL;
}
