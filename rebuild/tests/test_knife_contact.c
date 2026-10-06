#define _POSIX_C_SOURCE 200809L
#include "pe_knife_contact.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"knife contact:%d: %s\n",__LINE__,#x);failures++;}}while(0)
static void u32(unsigned char *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void f32(unsigned char *p,float v){uint32_t b;memcpy(&b,&v,4);u32(p,b);}
static void matrix(unsigned char *p,float x){float m[12]={1,0,0,x,0,1,0,0,0,0,1,0};for(int i=0;i<12;i++)f32(p+i*4,m[i]);}
static bool save(const char *path,const void *p,size_t size){FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(p,1,size,f)==size;return fclose(f)==0&&ok;}
static PeActor *actor_fixture(const char *path){
    unsigned char b[396]={0};memcpy(b,"PEAC",4);u32(b+4,1);u32(b+8,3);u32(b+12,3);u32(b+16,1);u32(b+20,1);
    for(int i=0;i<3;i++){f32(b+24+i*56+16,1);f32(b+24+i*56+32,1);}u32(b+196,1);u32(b+200,2);matrix(b+204,0);
    memcpy(b+252,"test_pose",10);f32(b+284,1);u32(b+288,2);f32(b+292,1);matrix(b+300,0);matrix(b+348,2);
    CHECK(save(path,b,sizeof b));char error[128];return pe_actor_load(path,error,sizeof error);
}
static void record(unsigned char *p,const char *name,uint32_t kind,uint32_t joint,float x,float radius,float damage){
    memcpy(p,name,strlen(name)+1);u32(p+32,kind);u32(p+36,kind==PE_HITBOX_ATTACK?PE_HITBOX_RIGHT:0);u32(p+40,joint);f32(p+52,damage);
    if(joint!=PE_HITBOX_DISABLED){f32(p+56,x);f32(p+68,x);f32(p+72,1);f32(p+92,radius);}
}
static PeHitboxes *boxes_fixture(const char *path,const PeActor *actor,bool late_invalid){
    unsigned char b[32+104*4]={0};memcpy(b,"PEHB",4);u32(b+4,1);u32(b+8,4);u32(b+12,1);uint64_t hash=pe_actor_skeleton_hash(actor);u32(b+16,(uint32_t)hash);u32(b+20,(uint32_t)(hash>>32));
    record(b+32,"Disabled",PE_HITBOX_DEFENCE,PE_HITBOX_DISABLED,0,0,2);
    record(b+136,"AttackNotDefence",PE_HITBOX_ATTACK,0,0,.5f,100);
    record(b+240,"HeadFirst",PE_HITBOX_DEFENCE,0,.4f,.25f,2);
    record(b+344,"BodyDeeper",PE_HITBOX_DEFENCE,0,late_invalid?10000:0,.5f,1);
    CHECK(save(path,b,sizeof b));char error[128];return pe_hitboxes_load(path,actor,error,sizeof error);
}
static PeCombat opened(void){
    PeCombat a;pe_combat_init(&a);float stamina=100;CHECK(pe_combat_begin(&a,PE_COMBAT_FEMALE,PE_COMBAT_THIRD_PERSON,1,&stamina));
    const PeCombatClip *clip=pe_combat_clip(a.gender,a.view,0);double start=0;
    for(unsigned i=0;i<5;i++)if(clip->events[i].type==PE_COMBAT_HIT_START)start=clip->events[i].time;
    CHECK(pe_combat_advance(&a,start,start,&stamina)==PE_COMBAT_ADVANCED&&a.contact_open);return a;
}
static void unchanged(PeCombat *a,PeKnifeContactAttack *attack,PeKnifeContactTarget *t,PeKnifeContactStatus expected){
    PeCombat before=*a;PeKnifeContactTarget old=*t;PeKnifeContactResult r;memset(&r,0x5a,sizeof r);PeKnifeContactResult saved=r;
    CHECK(pe_knife_contact_resolve(a,attack,t,&r)==expected);
    CHECK(!memcmp(a,&before,sizeof before)&&!memcmp(t,&old,sizeof old)&&!memcmp(&r,&saved,sizeof r));
}
int pe_test_knife_contact(void){
    failures=0;char path[]="/tmp/pe-knife-contact-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return failures;close(fd);
    PeActor *actor=actor_fixture(path);CHECK(actor);if(!actor){unlink(path);return failures;}PeHitboxes *boxes=boxes_fixture(path,actor,false);CHECK(boxes);if(!boxes){pe_actor_free(actor);unlink(path);return failures;}
    PeKnifeContactAttack attack={.entity_id=1,.weapon={{0,0,0},{0,1,0},.3f},.attack=19,.eligible=true};
    PeKnifeContactTarget target={.entity_id=2,.actor=actor,.hitboxes=boxes,.clip=0,.scale=1,.defense=29,.hp=100,.max_hp=100,.eligible=true};
    PeCombat a=opened();PeKnifeContactResult r;
    /* Geometry dispatch precedes both permissions and skill/HP decisions. */
    attack.eligible=false;target.eligible=false;PeKnifeContactTarget old_target=target;
    CHECK(pe_knife_contact_probe(&attack,&target,&r)==PE_KNIFE_CONTACT_HIT);
    CHECK(r.defence_index==2&&r.damage==0&&r.hp_before==0&&!r.died&&
        !memcmp(&target,&old_target,sizeof target));
    unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_IGNORED);
    attack.eligible=true;target.eligible=true;
    /* Endpoint snapshots miss on both sides; original sampled history crosses
     * the target and retains the first defence record and guide dedupe. */
    PeCapsuleHistory history={.previous_a={-2,0,0},.previous_b={-2,1,0},
        .current={{2,0,0},{2,1,0},.3f}};
    PeKnifeContactAttack swept=attack;swept.weapon=history.current;swept.history=&history;
    CHECK(!pe_hitbox_contact(history.current,(PeHitboxCapsule){{.4f,0,0},{.4f,1,0},.25f},NULL));
    CHECK(pe_knife_contact_resolve(&a,&swept,&target,&r)==PE_KNIFE_CONTACT_HIT);
    CHECK(r.historical&&r.defence_index==2&&r.history_contact.sample_count==7&&r.history_contact.sample_index>0);
    unchanged(&a,&swept,&target,PE_KNIFE_CONTACT_IGNORED);
    a=opened();target.hp=100;history.previous_a.x=NAN;
    unchanged(&a,&swept,&target,PE_KNIFE_CONTACT_INVALID);history.previous_a.x=-2;
    swept.weapon.radius=.2f;unchanged(&a,&swept,&target,PE_KNIFE_CONTACT_INVALID);
    /* Count1 tests previous endpoints, even if current would overlap. */
    history=(PeCapsuleHistory){.previous_a={-.9f,0,0},.previous_b={-.9f,1,0},
        .current={{-.4f,0,0},{-.4f,1,0},.3f}};swept.weapon=history.current;
    unchanged(&a,&swept,&target,PE_KNIFE_CONTACT_MISS);
    a=opened();target.hp=100;
    CHECK(pe_knife_contact_resolve(&a,&attack,&target,&r)==PE_KNIFE_CONTACT_HIT);
    CHECK(r.defence_index==2&&r.contact_scale==2&&fabsf(r.damage-361.0f/48*2)<.00001f&&r.target_id==2&&!r.died);
    CHECK(r.contact.axis_distance>.39f); /* Head wins although body is deeper. */
    CHECK(fabsf(target.hp-(100-r.damage))<.00001f&&a.target_count==1);
    unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_IGNORED);
    a=opened();target.hp=100;attack.weapon.a.x=attack.weapon.b.x=-.25f;attack.weapon.radius=.1f;
    CHECK(pe_knife_contact_resolve(&a,&attack,&target,&r)==PE_KNIFE_CONTACT_HIT&&r.defence_index==3&&r.contact_scale==1);
    CHECK(fabsf(r.damage-361.0f/48)<.00001f);
    a=opened();target.hp=100;attack.weapon.a.x=attack.weapon.b.x=-.8f;attack.weapon.radius=.3f;
    unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_MISS); /* Exact tangency. */
    attack.weapon.a.x=attack.weapon.b.x=nextafterf(-.8f,0);
    CHECK(pe_knife_contact_resolve(&a,&attack,&target,&r)==PE_KNIFE_CONTACT_HIT&&r.defence_index==3);
    a=opened();attack.weapon.a.x=attack.weapon.b.x=0;target.hp=1;
    CHECK(pe_knife_contact_resolve(&a,&attack,&target,&r)==PE_KNIFE_CONTACT_HIT&&r.died&&r.hp_after==0&&r.applied_damage==1&&target.hp==0);
    a=opened();unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_IGNORED); /* No second death from fresh stage. */
    target.hp=100;target.shield_block=true;target.shield_protection=1;
    CHECK(pe_knife_contact_resolve(&a,&attack,&target,&r)==PE_KNIFE_CONTACT_HIT&&r.damage==0&&target.hp==100&&a.target_count==1);
    a=opened();target.shield_protection=.25f;attack.critical=true;
    CHECK(pe_knife_contact_resolve(&a,&attack,&target,&r)==PE_KNIFE_CONTACT_HIT&&fabsf(r.damage-(361.0f/48*2*.75f*2))<.00001f);
    a=opened();target.hp=100;target.shield_block=false;target.shield_protection=0;attack.critical=false;
    target.seconds=1;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_MISS);target.seconds=0;
    target.position=(PeVec3){10,20,30};target.yaw=90;target.scale=2;attack.weapon=(PeHitboxCapsule){{10,20,30},{10,22,30},.6f};
    CHECK(pe_knife_contact_resolve(&a,&attack,&target,&r)==PE_KNIFE_CONTACT_HIT&&r.defence_index==2);
    target.position=(PeVec3){0};target.yaw=0;target.scale=1;attack.weapon=(PeHitboxCapsule){{0,0,0},{0,1,0},.3f};a=opened();target.hp=100;
    attack.eligible=false;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_IGNORED);attack.eligible=true;
    target.eligible=false;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_IGNORED);target.eligible=true;
    target.entity_id=attack.entity_id;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_IGNORED);target.entity_id=2;
    a.contact_open=false;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_IGNORED);a=opened();
    a.variant=(PeCombatVariant)99;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_INVALID);a=opened();
    target.seconds=NAN;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_INVALID);target.seconds=0;
    target.scale=0;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_INVALID);target.scale=1;
    target.hp=NAN;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_INVALID);target.hp=100;
    target.max_hp=1;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_INVALID);target.max_hp=100;
    target.clip=99;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_INVALID);target.clip=0;
    attack.weapon.radius=-1;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_INVALID);attack.weapon.radius=.3f;
    attack.attack=FLT_MAX;target.defense=0;attack.critical=true;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_INVALID);attack.critical=false;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_INVALID);attack.attack=19;target.defense=29;attack.critical=false;
    PeHitboxes *bad_boxes=boxes_fixture(path,actor,true);CHECK(bad_boxes);
    if(bad_boxes){target.hitboxes=bad_boxes;target.scale=1000;attack.weapon.radius=500;
        unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_INVALID); /* First head overlaps, later body pose exceeds bounds. */
        target.hitboxes=boxes;target.scale=1;attack.weapon.radius=.3f;pe_hitboxes_free(bad_boxes);}
    a.target_count=PE_COMBAT_TARGET_CAPACITY;for(unsigned i=0;i<a.target_count;i++)a.targets[i]=100+i;
    unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_IGNORED);a.target_count++;unchanged(&a,&attack,&target,PE_KNIFE_CONTACT_INVALID);
    pe_hitboxes_free(boxes);pe_actor_free(actor);unlink(path);return failures;
}
