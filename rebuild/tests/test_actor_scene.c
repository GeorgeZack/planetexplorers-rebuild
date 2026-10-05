#define _POSIX_C_SOURCE 200809L
#include "pe_actor_scene.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"actor scene:%d: %s\n",__LINE__,#x);failures++;}}while(0)
static void u32(unsigned char *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void f32(unsigned char *p,float v){uint32_t bits;memcpy(&bits,&v,4);u32(p,bits);}
static void matrix(unsigned char *p,float x){float m[12]={1,0,0,x,0,1,0,0,0,0,1,0};for(int i=0;i<12;i++)f32(p+i*4,m[i]);}
static void fixture(unsigned char bytes[540],float x){
    memset(bytes,0,540);memcpy(bytes,"PEAC",4);u32(bytes+4,1);u32(bytes+8,3);u32(bytes+12,3);u32(bytes+16,1);u32(bytes+20,1);
    for(int i=0;i<3;i++){unsigned char *p=bytes+24+i*56;f32(p,i==1?1:0);f32(p+4,i==2?1:0);f32(p+16,1);f32(p+32,1);}
    u32(bytes+196,1);u32(bytes+200,2);matrix(bytes+204,0);
    memcpy(bytes+252,"arbitrary_capture",18);f32(bytes+284,1);u32(bytes+288,2);f32(bytes+292,1);matrix(bytes+300,x);matrix(bytes+348,x+1);
}
/* Geometry stays fixed while each clip has a distinct name, clock and pose. */
static size_t clips_fixture(unsigned char *bytes,size_t count,float x){
    unsigned char first[540];fixture(first,x);memcpy(bytes,first,252);u32(bytes+20,(uint32_t)count);
    for(size_t i=0;i<count;i++){
        unsigned char *clip=bytes+252+i*144;memcpy(clip,first+252,144);
        memset(clip,0,32);snprintf((char *)clip,32,"clip_%02u",(unsigned)i);
        f32(clip+32,(float)i+1);f32(clip+40,1.0f/((float)i+1));u32(clip+44,(uint32_t)(i%2));
        matrix(clip+48,x+10*(float)i);matrix(clip+96,x+10*(float)i+1);
    }
    return 252+count*144;
}
static bool save(const char *path,const void *data,size_t size){FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(data,1,size,f)==size;return fclose(f)==0&&ok;}
static void invalid(const char *manifest,const void *text,size_t size){
    CHECK(save(manifest,text,size));char error[256];PeActorSceneManifest *s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(!s&&error[0]);pe_actor_scene_manifest_free(s);
}
int pe_test_actor_scene(void){
    failures=0;char directory[]="/tmp/pe-actor-scene-XXXXXX";char *created=mkdtemp(directory);CHECK(created!=NULL);if(!created)return failures;
    char manifest[512],a[512],b[512],png[512];snprintf(manifest,sizeof manifest,"%s/recording.peas",directory);snprintf(a,sizeof a,"%s/a.peac",directory);snprintf(b,sizeof b,"%s/b.peac",directory);snprintf(png,sizeof png,"%s/diffuse.png",directory);
    unsigned char bytes[540];fixture(bytes,2);CHECK(save(a,bytes,396));fixture(bytes,12);CHECK(save(b,bytes,396));
    /* A real one-pixel PNG; loader's envelope validation is intentionally
     * distinct from full graphical PNG decode before opening a window. */
    const unsigned char image[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,4,0,0,0,181,28,12,2,0,0,0,11,73,68,65,84,120,218,99,100,248,15,0,1,5,1,1,39,24,227,102,0,0,0,0,73,69,78,68,174,66,96,130};
    CHECK(save(png,image,sizeof image));
    const char *valid="PEAS1\na.peac diffuse.png\nb.peac diffuse.png\n";CHECK(save(manifest,valid,strlen(valid)));char error[256];
    PeActorSceneManifest *s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(s&&!*error);
    if(s){CHECK(pe_actor_scene_part_count(s)==2);CHECK(!strcmp(pe_actor_scene_part(s,0)->actor,a));CHECK(!pe_actor_scene_part(s,2));
        const PeActorClip *clip=pe_actor_scene_clip(s);CHECK(!strcmp(clip->name,"arbitrary_capture")&&clip->frames==2&&clip->fps==1&&!clip->loop);
        float min[3],max[3];CHECK(pe_actor_scene_bounds(s,min,max));CHECK(min[0]==2&&max[0]==13&&min[1]==0&&max[1]==1);
        PeActor *actor=pe_actor_load(a,error,sizeof error);CHECK(actor&&pe_actor_scene_clip_matches(s,actor));pe_actor_free(actor);
        /* Regression: identical clip metadata and skin hash do not identify
         * animation palettes. Replace disk data after preflight; preview must
         * retain the original posed actor/PNG rather than reopen either path. */
        fixture(bytes,200);CHECK(save(a,bytes,396));
        actor=pe_actor_load(a,error,sizeof error);CHECK(actor&&pe_actor_scene_clip_matches(s,actor));
        PeActorVertex replaced,retained;
        CHECK(pe_actor_pose_vertex(actor,0,0,0,&replaced)&&replaced.position[0]==200);
        CHECK(pe_actor_skeleton_hash(actor)==pe_actor_skeleton_hash(pe_actor_scene_actor(s,0)));pe_actor_free(actor);
        CHECK(unlink(png)==0);size_t png_size=0;const unsigned char *retained_png=pe_actor_scene_png(s,0,&png_size);
        CHECK(retained_png&&png_size==sizeof image&&!memcmp(retained_png,image,sizeof image));
        CHECK(pe_actor_pose_vertex(pe_actor_scene_actor(s,0),0,0,0,&retained)&&retained.position[0]==2);
        CHECK(pe_actor_scene_bounds(s,min,max)&&min[0]==2&&max[0]==13);
        PeActor *owned=pe_actor_scene_take_actor(s,0);CHECK(owned&&!pe_actor_scene_actor(s,0)&&!pe_actor_scene_take_actor(s,0));
        pe_actor_scene_manifest_free(s);
        CHECK(pe_actor_pose_vertex(owned,0,0,0,&retained)&&retained.position[0]==2);pe_actor_free(owned);
        CHECK(save(png,image,sizeof image));fixture(bytes,2);CHECK(save(a,bytes,396));fixture(bytes,12);
    }
    /* Different clips within one actor are valid; every material must match
     * the complete ordered set, not just clip zero. */
    unsigned char multi[252+33*144];size_t multi_size=clips_fixture(multi,2,2);CHECK(save(a,multi,multi_size));
    clips_fixture(multi,2,12);CHECK(save(b,multi,multi_size));
    s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(s&&!*error);
    if(s){
        CHECK(pe_actor_scene_clip_count(s)==2&&pe_actor_scene_clip(s)==pe_actor_scene_clip_at(s,0));
        CHECK(pe_actor_scene_find_clip(s,"clip_01")==1&&pe_actor_scene_find_clip(s,"missing")==PE_ACTOR_BIND_POSE);
        CHECK(!pe_actor_scene_clip_at(s,2)&&!pe_actor_scene_clip_at(s,PE_ACTOR_BIND_POSE));
        const PeActorClip *second=pe_actor_scene_clip_at(s,1);CHECK(second&&second->fps==2&&second->duration==.5f&&second->loop);
        PeActorVertex pose;CHECK(pe_actor_pose_vertex(pe_actor_scene_actor(s,0),1,.25,0,&pose)&&pose.position[0]==12.5f);
        CHECK(pe_actor_pose_vertex(pe_actor_scene_actor(s,1),1,.25,0,&pose)&&pose.position[0]==22.5f);
        float min[3],max[3];CHECK(pe_actor_scene_bounds(s,min,max)&&min[0]==2&&max[0]==13);
        pe_actor_scene_manifest_free(s);
    }
    /* Same clips in a different order are not a synchronized scene. */
    unsigned char swap[144];memcpy(swap,multi+252,144);memcpy(multi+252,multi+396,144);memcpy(multi+396,swap,144);
    CHECK(save(b,multi,multi_size));s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(!s&&strstr(error,"timings"));pe_actor_scene_manifest_free(s);
    for(unsigned mismatch=0;mismatch<4;mismatch++){
        clips_fixture(multi,2,12);
        if(mismatch==0){memset(multi+396,0,32);memcpy(multi+396,"wrong",6);}
        if(mismatch==1)f32(multi+428,1.5f);
        if(mismatch==2)f32(multi+436,.4f);
        if(mismatch==3)u32(multi+440,0);
        CHECK(save(b,multi,multi_size));s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(!s&&strstr(error,"timings"));pe_actor_scene_manifest_free(s);
    }
    clips_fixture(multi,2,12);u32(multi+432,3);matrix(multi+540,24);
    CHECK(save(b,multi,588));s=pe_actor_scene_manifest_load(manifest,error,sizeof error);
    CHECK(!s&&strstr(error,"timings"));pe_actor_scene_manifest_free(s);
    /* All 32 clip records survive preflight and ownership transfer. */
    multi_size=clips_fixture(multi,32,2);u32(multi+4,2);CHECK(save(a,multi,multi_size));
    clips_fixture(multi,32,12);u32(multi+4,2);CHECK(save(b,multi,multi_size));
    s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(s&&pe_actor_scene_clip_count(s)==32);
    if(s){
        CHECK(pe_actor_scene_find_clip(s,"clip_31")==31&&!pe_actor_scene_clip_at(s,32));
        CHECK(pe_actor_scene_clip_at(s,31)->terminal_at_duration);
        PeActor *owned=pe_actor_scene_take_actor(s,1);CHECK(owned&&pe_actor_scene_clip_matches(s,owned));
        pe_actor_scene_manifest_free(s);PeActorVertex pose;
        CHECK(pe_actor_pose_vertex(owned,31,0,0,&pose)&&pose.position[0]==322);pe_actor_free(owned);
    }
    multi_size=clips_fixture(multi,33,12);CHECK(save(b,multi,multi_size));
    s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(!s);pe_actor_scene_manifest_free(s);
    fixture(bytes,2);CHECK(save(a,bytes,396));fixture(bytes,12);CHECK(save(b,bytes,396));
    const char *bad[]={"", "PEAS1\n", "PEAS2\na.peac diffuse.png\n", "PEAS1\n\n", "PEAS1\n../a.peac diffuse.png\n", "PEAS1\n/a.peac diffuse.png\n", "PEAS1\nsub/a.peac diffuse.png\n", "PEAS1\na.peac ../diffuse.png\n", "PEAS1\na.peac diffuse.png extra\n", "PEAS1\na.peac  diffuse.png\n", "PEAS1\na.peac\tdiffuse.png\n", "PEAS1\na.peac diffuse.jpg\n", "PEAS1\na.peac diffuse.png\na.peac diffuse.png\n", "PEAS1\nno.peac diffuse.png\n", "PEAS1\na.peac missing.png\n", "PEAS1\na.peac diffuse.png\n\n", "PEAS1\na.peac diff\303\251.png\n"};
    for(size_t i=0;i<sizeof bad/sizeof bad[0];i++)invalid(manifest,bad[i],strlen(bad[i]));
    const char embedded[]="PEAS1\na.peac diffuse.png\0junk\n";invalid(manifest,embedded,sizeof embedded-1);
    char longline[600];memset(longline,'a',sizeof longline);memcpy(longline,"PEAS1\n",6);invalid(manifest,longline,sizeof longline);
    CHECK(save(manifest,valid,strlen(valid)));
    /* Identical scalar clocks with different final-sample semantics differ. */
    fixture(bytes,12);u32(bytes+4,2);CHECK(save(b,bytes,396));s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(!s&&strstr(error,"timings"));pe_actor_scene_manifest_free(s);
    fixture(bytes,12);
    f32(bytes+292,.5f);CHECK(save(b,bytes,396));s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(!s&&strstr(error,"timings"));pe_actor_scene_manifest_free(s);
    fixture(bytes,12);u32(bytes+296,1);CHECK(save(b,bytes,396));s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(!s);pe_actor_scene_manifest_free(s);
    fixture(bytes,12);memcpy(bytes+252,"different_capture",18);CHECK(save(b,bytes,396));s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(!s);pe_actor_scene_manifest_free(s);
    fixture(bytes,12);u32(bytes+20,2);memcpy(bytes+396,bytes+252,144);memset(bytes+396,0,32);memcpy(bytes+396,"second",7);CHECK(save(b,bytes,540));s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(!s);pe_actor_scene_manifest_free(s);
    fixture(bytes,12);CHECK(save(b,bytes,396));CHECK(save(png,image,24));s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(!s);pe_actor_scene_manifest_free(s);CHECK(save(png,image,sizeof image));
    CHECK(unlink(b)==0);CHECK(symlink(a,b)==0);s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(!s);pe_actor_scene_manifest_free(s);CHECK(unlink(b)==0);CHECK(save(b,bytes,396));
    const char *crlf="PEAS1\r\na.peac diffuse.png\r\nb.peac diffuse.png";CHECK(save(manifest,crlf,strlen(crlf)));s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(s);pe_actor_scene_manifest_free(s);
    /* Count boundary, not just a tiny happy-path scene. */
    char text[4096]="PEAS1\n",part[512];size_t used=strlen(text);
    for(unsigned i=0;i<65;i++){snprintf(part,sizeof part,"%s/p%u.peac",directory,i);CHECK(save(part,bytes,396));used+=(size_t)snprintf(text+used,sizeof text-used,"p%u.peac diffuse.png\n",i);
        if(i==63){CHECK(save(manifest,text,used));s=pe_actor_scene_manifest_load(manifest,error,sizeof error);CHECK(s&&pe_actor_scene_part_count(s)==64);pe_actor_scene_manifest_free(s);}}
    invalid(manifest,text,used);
    for(unsigned i=0;i<65;i++){snprintf(part,sizeof part,"%s/p%u.peac",directory,i);unlink(part);}
    CHECK(!pe_actor_scene_manifest_load(NULL,error,sizeof error));CHECK(!pe_actor_scene_clip(NULL));CHECK(pe_actor_scene_part_count(NULL)==0);
    CHECK(pe_actor_scene_clip_count(NULL)==0&&!pe_actor_scene_clip_at(NULL,0));
    CHECK(pe_actor_scene_find_clip(NULL,"clip_00")==PE_ACTOR_BIND_POSE&&pe_actor_scene_find_clip(NULL,NULL)==PE_ACTOR_BIND_POSE);
    unlink(manifest);unlink(a);unlink(b);unlink(png);rmdir(directory);return failures;
}
