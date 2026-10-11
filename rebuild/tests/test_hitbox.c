#define _POSIX_C_SOURCE 200809L
#include "pe_test_paths.h"
#include "pe_hitbox.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hitbox:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static bool near(float a,float b){return fabsf(a-b)<.0001f;}
static void u32(unsigned char*p,uint32_t v){for(int i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static void f32(unsigned char*p,float v){uint32_t b;memcpy(&b,&v,4);u32(p,b);}
static bool write_file(const char*p,const unsigned char*b,size_t n){FILE*f=fopen(p,"wb");if(!f)return false;bool ok=fwrite(b,1,n,f)==n;return fclose(f)==0&&ok;}
static void matrix(unsigned char*p,float x,float y){float m[12]={x,0,0,0,0,y,0,0,0,0,1,0};for(int i=0;i<12;i++)f32(p+i*4,m[i]);}
static PeActor *fixture_actor(const char*path,bool rotate){
    unsigned char b[396]={0};memcpy(b,"PEAC",4);u32(b+4,1);u32(b+8,3);u32(b+12,3);u32(b+16,1);u32(b+20,1);
    for(int i=0;i<3;i++){f32(b+24+i*56+16,1);f32(b+24+i*56+32,1);}u32(b+196,1);u32(b+200,2);matrix(b+204,1,1);
    memcpy(b+252,"pose",5);f32(b+284,1);u32(b+288,2);f32(b+292,1);matrix(b+300,1,1);matrix(b+348,2,3);
    if(rotate){f32(b+348,0);f32(b+352,-3);f32(b+364,2);f32(b+368,0);}
    CHECK(write_file(path,b,sizeof b));char error[128];return pe_actor_load(path,error,sizeof(error));
}
static void fixture_boxes(unsigned char*b,uint64_t hash){
    memset(b,0,136);memcpy(b,"PEHB",4);u32(b+4,1);u32(b+8,1);u32(b+12,1);u32(b+16,(uint32_t)hash);u32(b+20,(uint32_t)(hash>>32));
    memcpy(b+32,"Head1",6);f32(b+84,2);f32(b+88+4*4,1);f32(b+88+9*4,.5f);
}
static void contact_tests(void){
    PeHitboxCapsule a={{0,0,0},{0,2,0},.5f},b={{1,0,0},{1,2,0},.5f};PeHitboxContact c={.axis_distance=99},old=c;
    CHECK(!pe_hitbox_contact(a,b,&c));CHECK(!memcmp(&c,&old,sizeof(c))); /* Strict tangent. */
    b.a.x=b.b.x=.999f;CHECK(pe_hitbox_contact(a,b,&c));CHECK(near(c.axis_distance,.999f)&&near(c.point.x,.4995f)&&near(c.normal.x,1));
    b=(PeHitboxCapsule){{-1,1,0},{1,1,0},.1f};CHECK(pe_hitbox_contact(a,b,&c));CHECK(near(c.axis_distance,0)&&near(c.point.y,1));
    b=(PeHitboxCapsule){{0,3.1f,0},{0,4,0},.5f};CHECK(!pe_hitbox_contact(a,b,NULL));b.a.y=2.9f;CHECK(pe_hitbox_contact(a,b,NULL));
    a=(PeHitboxCapsule){{0,0,0},{0,0,0},1};b=(PeHitboxCapsule){{0,0,0},{0,0,0},.5f};CHECK(pe_hitbox_contact(a,b,&c));CHECK(c.axis_distance==0&&c.normal.x==0&&c.normal.y==0&&c.normal.z==0);
    b.a.x=b.b.x=1.5f;CHECK(!pe_hitbox_contact(a,b,NULL));b.a.x=b.b.x=1.4f;CHECK(pe_hitbox_contact(a,b,&c));CHECK(near(c.point.x,1.4f/1.5f));
    b=(PeHitboxCapsule){{-2,1.01f,0},{2,1.01f,0},.001f};CHECK(!pe_hitbox_contact(a,b,NULL));b.a.y=b.b.y=1;CHECK(pe_hitbox_contact(a,b,NULL));
    a=(PeHitboxCapsule){{-1,0,0},{1,0,0},.1f};b=(PeHitboxCapsule){{0,-1,.19f},{0,1,.19f},.1f};CHECK(pe_hitbox_contact(a,b,&c));CHECK(near(c.axis_distance,.19f));
    PeHitboxContact inverse;CHECK(pe_hitbox_contact(b,a,&inverse));CHECK(near(c.axis_distance,inverse.axis_distance)&&near(c.point.z,inverse.point.z)&&near(c.normal.z,-inverse.normal.z));
    b.radius=-1;old=c;CHECK(!pe_hitbox_contact(a,b,&c));CHECK(!memcmp(&old,&c,sizeof(c)));b.radius=.1f;b.a.x=NAN;CHECK(!pe_hitbox_contact(a,b,NULL));
}
int pe_test_hitbox(void){
    failures=0;contact_tests();char path[]="/tmp/pe-hitbox-test-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return failures;close(fd);
    PeActor*a=fixture_actor(path,false);CHECK(a);if(!a){unlink(path);return failures;}
    unsigned char bytes[137];fixture_boxes(bytes,pe_actor_skeleton_hash(a));CHECK(write_file(path,bytes,136));char error[128];PeHitboxes*b=pe_hitboxes_load(path,a,error,sizeof(error));CHECK(b&&!*error);
    if(b){
        CHECK(pe_hitboxes_count(b)==1&&pe_hitboxes_get(b,0)->damage_scale==2&&!pe_hitboxes_get(b,1));
        PeHitboxCapsule posed;CHECK(pe_hitbox_pose(b,a,0,0,.5,(PeVec3){10,20,30},90,2,&posed));
        CHECK(near(posed.a.x,10)&&near(posed.a.y,20)&&near(posed.a.z,30)&&near(posed.b.y,24)&&near(posed.radius,1.5f));
        PeActorBlendSample blend[]={{0,0,1},{0,1,3}};PeHitboxCapsule blended;
        CHECK(pe_hitbox_pose_blend(b,a,0,blend,2,(PeVec3){10,20,30},90,2,&blended));
        CHECK(near(blended.b.y,25)&&near(blended.radius,1.75f));
        blend[0].weight=0;blend[1].seconds=.5;
        CHECK(pe_hitbox_pose_blend(b,a,0,blend,2,(PeVec3){10,20,30},90,2,&blended));CHECK(!memcmp(&blended,&posed,sizeof posed));
        PeHitboxCapsule before=blended;blend[0].seconds=NAN;
        CHECK(!pe_hitbox_pose_blend(b,a,0,blend,2,(PeVec3){0},0,1,&blended));CHECK(!memcmp(&before,&blended,sizeof blended));
        PeHitboxCapsule old=posed;CHECK(!pe_hitbox_pose(b,a,0,0,NAN,(PeVec3){0},0,1,&posed));CHECK(!memcmp(&old,&posed,sizeof(posed)));
        CHECK(!pe_hitbox_pose(b,a,1,0,0,(PeVec3){0},0,1,&posed));CHECK(!pe_hitbox_pose(b,a,0,0,0,(PeVec3){0},0,-1,&posed));pe_hitboxes_free(b);
    }
    for(size_t n=0;n<136;n++){CHECK(write_file(path,bytes,n));b=pe_hitboxes_load(path,a,error,sizeof(error));CHECK(!b);pe_hitboxes_free(b);}
    bytes[136]=0;CHECK(write_file(path,bytes,137));b=pe_hitboxes_load(path,a,error,sizeof(error));CHECK(!b);pe_hitboxes_free(b);
    size_t offsets[]={0,4,8,12,16,24,32,64,68,72,84,88};uint32_t values[]={0,2,1025,2,0,1,0,2,3,1,0x7fc00000,0x7f800000};
    for(size_t i=0;i<sizeof offsets/sizeof offsets[0];i++){unsigned char old[4];memcpy(old,bytes+offsets[i],4);u32(bytes+offsets[i],values[i]);CHECK(write_file(path,bytes,136));b=pe_hitboxes_load(path,a,error,sizeof(error));CHECK(!b);pe_hitboxes_free(b);memcpy(bytes+offsets[i],old,4);}
    fixture_boxes(bytes,pe_actor_skeleton_hash(a));u32(bytes+72,PE_HITBOX_DISABLED);memset(bytes+88,0,48);CHECK(write_file(path,bytes,136));b=pe_hitboxes_load(path,a,error,sizeof(error));CHECK(b);
    if(b){PeHitboxCapsule c={.radius=42},old=c;CHECK(!pe_hitbox_pose(b,a,0,0,0,(PeVec3){0},0,1,&c));CHECK(!memcmp(&c,&old,sizeof(c)));pe_hitboxes_free(b);}
    pe_actor_free(a);
    /* Radius comes from the blended radius vector, not the average of the
     * independently posed radii. A quarter-turn makes those observably differ. */
    a=fixture_actor(path,true);CHECK(a);
    if(a){fixture_boxes(bytes,pe_actor_skeleton_hash(a));CHECK(write_file(path,bytes,136));b=pe_hitboxes_load(path,a,error,sizeof error);CHECK(b);
        if(b){PeActorBlendSample mix[]={{0,0,1},{0,1,3}};PeHitboxCapsule c;
            CHECK(pe_hitbox_pose_blend(b,a,0,mix,2,(PeVec3){0},0,1,&c));
            CHECK(near(c.radius,sqrtf(.125f*.125f+.75f*.75f))&&!near(c.radius,.875f));
            CHECK(near(c.b.x,-2.25f)&&near(c.b.y,.25f));pe_hitboxes_free(b);}
        pe_actor_free(a);}
    unlink(path);
    const char*actual=pe_test_path("rebuild/build/reference-assets/hare.pehb");FILE*f=fopen(actual,"rb");if(f){fclose(f);
        a=pe_actor_load(pe_test_path("rebuild/build/reference-assets/hare.peac"),error,sizeof(error));CHECK(a);b=pe_hitboxes_load(actual,a,error,sizeof(error));CHECK(b);
        if(b){CHECK(pe_hitboxes_count(b)==19);unsigned enabled=0,disabled=0,attacks=0;size_t clip=pe_actor_find_clip(a,"attack2");CHECK(clip!=PE_ACTOR_BIND_POSE);
            for(size_t i=0;i<pe_hitboxes_count(b);i++){const PeHitboxDefinition*d=pe_hitboxes_get(b,i);PeHitboxCapsule c;
                bool ok=pe_hitbox_pose(b,a,i,clip,.3,(PeVec3){15,2,12},20,1.3f,&c);
                if(d->joint==PE_HITBOX_DISABLED){disabled++;CHECK(!ok);}else{CHECK(ok&&c.radius>0&&c.radius<2);if(d->kind==PE_HITBOX_DEFENCE)enabled++;else{attacks++;CHECK(d->group==(strstr(d->name,"R Forearm")?PE_HITBOX_RIGHT:PE_HITBOX_LEFT));}CHECK(pe_hitbox_contact(c,c,NULL));}}
            CHECK(enabled==12&&disabled==3&&attacks==4);pe_hitboxes_free(b);}pe_actor_free(a);
    }
    return failures;
}
