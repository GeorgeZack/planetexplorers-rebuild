#define _POSIX_C_SOURCE 200809L
#include "pe_test_paths.h"
#include "pe_hare_damage.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hare damage:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static bool near(float a,float b){return fabsf(a-b)<.0001f;}
static void u32(unsigned char *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static void f32(unsigned char *p,float v){uint32_t n;memcpy(&n,&v,4);u32(p,n);}
static void matrix(unsigned char *p,float x){float m[12]={1,0,0,x,0,1,0,0,0,0,1,0};for(int i=0;i<12;i++)f32(p+i*4,m[i]);}
static bool save(const char *p,const void *b,size_t n){FILE *f=fopen(p,"wb");if(!f)return false;bool ok=fwrite(b,1,n,f)==n;return fclose(f)==0&&ok;}
static PeActor *actor_fixture(const char *path,bool wrong_duration){
    unsigned char b[252+5*144]={0};memcpy(b,"PEAC",4);u32(b+4,1);u32(b+8,3);u32(b+12,3);u32(b+16,1);u32(b+20,5);
    for(int i=0;i<3;i++){f32(b+24+i*56+16,1);f32(b+24+i*56+32,1);}u32(b+196,1);u32(b+200,2);matrix(b+204,0);
    for(unsigned i=0;i<5;i++){
        unsigned char *p=b+252+i*144;snprintf((char *)p,32,"attack%u",i);
        float duration=(float)pe_hare_attack_def(i)->clip_duration+(wrong_duration?.1f:0);
        f32(p+32,1/duration);u32(p+36,2);f32(p+40,duration);matrix(p+48,0);matrix(p+96,0);
    }
    CHECK(save(path,b,sizeof(b)));char error[256];return pe_actor_load(path,error,sizeof(error));
}
static PeActor *blend_actor_fixture(const char *path){
    PeActor *initial=actor_fixture(path,false);if(!initial)return NULL;pe_actor_free(initial);
    unsigned char b[252+5*144];FILE *f=fopen(path,"rb");if(!f)return NULL;
    bool ok=fread(b,1,sizeof b,f)==sizeof b;fclose(f);if(!ok)return NULL;
    for(unsigned clip=1;clip<=2;clip++)for(unsigned frame=0;frame<2;frame++)
        matrix(b+252+clip*144+48+frame*48,clip==1?4:-4);
    CHECK(save(path,b,sizeof b));char error[256];return pe_actor_load(path,error,sizeof error);
}
static void record(unsigned char *p,const char *name,unsigned kind,unsigned group,unsigned joint,float x,float radius,float damage){
    memcpy(p,name,strlen(name)+1);u32(p+32,kind);u32(p+36,group);u32(p+40,joint);f32(p+52,damage);
    if(joint!=PE_HITBOX_DISABLED){f32(p+56,x);f32(p+68,x);f32(p+72,1);f32(p+92,radius);}
}
static PeHitboxes *boxes_fixture(const char *path,const PeActor *actor,bool late_bad){
    unsigned char b[32+104*7]={0};memcpy(b,"PEHB",4);u32(b+4,1);u32(b+8,7);u32(b+12,1);
    uint64_t hash=pe_actor_skeleton_hash(actor);u32(b+16,(uint32_t)hash);u32(b+20,(uint32_t)(hash>>32));
    record(b+32,"disabled",PE_HITBOX_DEFENCE,0,PE_HITBOX_DISABLED,0,0,2);
    record(b+136,"head",PE_HITBOX_DEFENCE,0,0,.4f,.25f,2);
    record(b+240,"body",PE_HITBOX_DEFENCE,0,0,late_bad?10000:0,.5f,1);
    record(b+344,"right_first",PE_HITBOX_ATTACK,PE_HITBOX_RIGHT,0,0,.3f,1);
    record(b+448,"right_second",PE_HITBOX_ATTACK,PE_HITBOX_RIGHT,0,.4f,.3f,1);
    record(b+552,"left_first",PE_HITBOX_ATTACK,PE_HITBOX_LEFT,0,0,.3f,1);
    record(b+656,"left_second",PE_HITBOX_ATTACK,PE_HITBOX_LEFT,0,.4f,.3f,1);
    CHECK(save(path,b,sizeof(b)));char error[256];return pe_hitboxes_load(path,actor,error,sizeof(error));
}
static PeHareAttack opened(unsigned index,double time){
    PeHareAttack a;pe_hare_attack_init(&a);CHECK(pe_hare_attack_begin(&a,2,true,(uint8_t)(1u<<index),0));
    PeHareAttackEvents e;CHECK(pe_hare_attack_update(&a,time,&e));return a;
}
static void unchanged(PeHareAttack *state,const PeHareDamageAttack *a,const PeHareDamageTarget *t,float *hp,PeHareDamageStatus status){
    PeHareAttack before=*state;float old=*hp;PeHareDamageResult r;memset(&r,0x5a,sizeof(r));PeHareDamageResult saved=r;
    CHECK(pe_hare_damage_resolve(state,a,t,hp,&r)==status);
    CHECK(!memcmp(state,&before,sizeof(before))&&!memcmp(&r,&saved,sizeof(r))&&*hp==old);
}
static void blend_tests(const char *path,const PeActor *attacker,const PeHitboxes *boxes){
    PeActor *actor=blend_actor_fixture(path);CHECK(actor);if(!actor)return;
    PeHareDamageAttack a={.entity_id=1,.pose={attacker,boxes,{0},0,1},.attack=64,.bone=PE_HARE_RIGHT_FOREARM,.eligible=true};
    PeHareDamageTarget t={.entity_id=2,.pose={actor,boxes,{0},0,1},.clip=1,.max_hp=500,.eligible=true};
    PeHareAttack state=opened(0,.4);float hp=500;PeHareDamageResult result;
    unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_MISS); /* +4m single pose. */
    t.clip=2;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_MISS); /* -4m single pose. */
    t.blend_count=2;t.blend[0]=(PeActorBlendSample){1,0,2};t.blend[1]=(PeActorBlendSample){2,0,2};
    t.clip=SIZE_MAX;t.seconds=NAN; /* Legacy pose is deliberately unused. */
    CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&result)==PE_HARE_DAMAGE_HIT&&hp==372&&result.defence_index==1);
    CHECK(state.target_count[0]==1);unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED);
    for(unsigned bad=0;bad<7;bad++){
        state=opened(0,.4);hp=500;PeHareDamageTarget invalid=t;
        if(bad==0)invalid.blend_count=PE_ACTOR_BLEND_MAX+1;
        if(bad==1)invalid.blend[0].weight=NAN;
        if(bad==2)invalid.blend[0].weight=-1;
        if(bad==3)invalid.blend[0].weight=invalid.blend[1].weight=0;
        if(bad==4)invalid.blend[1].seconds=NAN;
        if(bad==5)invalid.blend[1].seconds=-1;
        if(bad==6){invalid.blend[1].weight=0;invalid.blend[1].clip=pe_actor_stats(actor).clips;}
        unchanged(&state,&a,&invalid,&hp,PE_HARE_DAMAGE_INVALID);
        invalid.eligible=false;unchanged(&state,&a,&invalid,&hp,PE_HARE_DAMAGE_INVALID);
    }
    pe_actor_free(actor);
}
static void history_tests(const PeActor *actor,const PeHitboxes *boxes){
    PeHareDamagePose pose={actor,boxes,{-2,0,0},0,1};
    PeHareAttack state=opened(0,.4);PeHareDamageHistory history={0};
    CHECK(pe_hare_damage_history_step(&history,&state,pose,(PeVec3){0}));
    CHECK(history.active_bones==1&&history.parts[0].index==3&&history.parts[3].index==6);
    CHECK(history.parts[0].capsule.previous_a.x==history.parts[0].capsule.current.a.x);
    PeHareDamageAttack a={.entity_id=1,.pose=pose,.attack=64,.bone=1,.eligible=true,.history=&history};
    PeHareDamageTarget t={.entity_id=2,.pose={actor,boxes,{0},0,1},.clip=PE_ACTOR_BIND_POSE,.max_hp=500,.eligible=true};
    float hp=500;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_MISS);
    PeHareAttackEvents events;CHECK(pe_hare_attack_update(&state,.42,&events));pose.position.x=2;
    CHECK(pe_hare_damage_history_step(&history,&state,pose,(PeVec3){0}));a.pose=pose;
    PeHareDamageAttack instant=a;instant.history=NULL;
    unchanged(&state,&instant,&t,&hp,PE_HARE_DAMAGE_MISS);
    PeHareDamageResult out;PeHareDamageHistory before=history;
    CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&out)==PE_HARE_DAMAGE_HIT&&hp<500);
    CHECK(out.attack_index==3&&out.defence_index==1&&!memcmp(&before,&history,sizeof history));
    /* Dedupe never advances history; the next explicit active update does. */
    unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED);
    CHECK(pe_hare_attack_update(&state,.44,&events));pose.position.x=2.1f;
    CHECK(pe_hare_damage_history_step(&history,&state,pose,(PeVec3){1,.75f,2}));
    CHECK(history.parts[0].capsule.previous_a.x==2&&history.parts[0].capsule.parent_center.y==.75f);
    a.pose=pose;a.eligible=false;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED);a.eligible=true;
    /* Even ignored damage validates supplied history; bad later part is atomic. */
    before=history;history.parts[1].capsule.previous_a.x=NAN;
    unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_INVALID);
    PeHareDamageHistory corrupt=history;
    CHECK(!pe_hare_damage_history_step(&history,&state,pose,(PeVec3){0})&&!memcmp(&history,&corrupt,sizeof history));history=before;
    history.normalized_time-=.01;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_INVALID);history=before;
    history.parts[1].index++;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_INVALID);history=before;
    /* Inactive movement must not become a sweep on the next activation. */
    CHECK(pe_hare_attack_update(&state,.8,&events));pose.position.x=100;
    CHECK(pe_hare_damage_history_step(&history,&state,pose,(PeVec3){0})&&history.active_bones==0);
    CHECK(history.parts[0].capsule.current.a.x==2.1f);
    pe_hare_attack_end(&state);state=opened(0,.4);state.last_melee_start=20;
    CHECK(pe_hare_damage_history_step(&history,&state,pose,(PeVec3){0}));
    CHECK(history.parts[0].capsule.previous_a.x==100&&history.parts[0].capsule.current.a.x==100);
    /* Attack2 opens each distinct forearm once, never reuses the other history. */
    history=(PeHareDamageHistory){0};state=opened(2,.25);pose.position.x=-10;
    CHECK(pe_hare_damage_history_step(&history,&state,pose,(PeVec3){0}));
    CHECK(pe_hare_attack_update(&state,.55,&events));pose.position.x=10;
    CHECK(pe_hare_damage_history_step(&history,&state,pose,(PeVec3){0})&&history.active_bones==2);
    CHECK(history.parts[0].capsule.current.a.x==-10&&history.parts[2].capsule.previous_a.x==10);
}
int pe_test_hare_damage(void){
    failures=0;char path[]="/tmp/pe-hare-damage-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return failures;close(fd);
    PeActor *actor=actor_fixture(path,false);CHECK(actor);if(!actor){unlink(path);return failures;}
    PeHitboxes *boxes=boxes_fixture(path,actor,false);CHECK(boxes);if(!boxes){pe_actor_free(actor);unlink(path);return failures;}
    history_tests(actor,boxes);
    PeHareDamagePose pose={.actor=actor,.hitboxes=boxes,.scale=1};
    PeHareDamageAttack a={.entity_id=1,.pose=pose,.attack=64,.bone=PE_HARE_RIGHT_FOREARM,.eligible=true};
    PeHareDamageTarget t={.entity_id=2,.pose=pose,.clip=PE_ACTOR_BIND_POSE,.max_hp=500,.eligible=true};
    float hp=500;PeHareAttack state=opened(0,.4);PeHareDamageResult r;
    CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&r)==PE_HARE_DAMAGE_HIT);
    CHECK(r.attack_index==3&&r.defence_index==1&&r.contact_scale==2&&r.damage==128&&hp==372&&!r.died);
    CHECK(state.target_count[0]==1&&r.target_id==2);unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED);
    /* Both attack4 forearms share one successful guide target list. */
    state=opened(4,.5);hp=500;CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&r)==PE_HARE_DAMAGE_HIT);
    a.bone=PE_HARE_LEFT_FOREARM;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED);
    /* Attack2's second window can hit only when the host supplies its next guide. */
    state=opened(2,.25);hp=500;a.bone=PE_HARE_RIGHT_FOREARM;CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&r)==PE_HARE_DAMAGE_HIT);
    PeHareAttackEvents events;CHECK(pe_hare_attack_update(&state,.55,&events));a.window=1;a.bone=PE_HARE_LEFT_FOREARM;
    unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED);a.guide=1;
    CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&r)==PE_HARE_DAMAGE_HIT&&hp==244&&state.target_count[1]==1);
    a.window=a.guide=0;a.bone=PE_HARE_RIGHT_FOREARM;
    /* Target pose, strict contact and actual HP rather than max-HP substitution. */
    state=opened(0,.4);hp=20;CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&r)==PE_HARE_DAMAGE_HIT&&r.died&&hp==0&&r.applied_damage==20);
    state=opened(0,.4);unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED);hp=500;
    t.shield_block=true;t.shield_protection=1;CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&r)==PE_HARE_DAMAGE_HIT&&hp==500&&r.damage==0);
    state=opened(0,.4);t.shield_protection=.25f;a.critical=true;t.defense=64;
    CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&r)==PE_HARE_DAMAGE_HIT&&near(r.damage,96));
    t.shield_block=false;t.shield_protection=0;t.defense=0;a.critical=false;hp=500;
    state=opened(0,.4);t.pose.position.x=1.2f;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_MISS);
    t.pose.position.x=nextafterf(1.2f,0);CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&r)==PE_HARE_DAMAGE_HIT&&r.defence_index==2);
    t.pose.position=(PeVec3){10,20,30};t.pose.yaw=90;t.pose.scale=2;a.pose=t.pose;state=opened(0,.4);hp=500;
    CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&r)==PE_HARE_DAMAGE_HIT);
    a.pose=t.pose=pose;hp=500;state=opened(0,.4);
    for(unsigned attack=0;attack<PE_HARE_ATTACK_COUNT;attack++){
        const PeHareAttackDef *d=pe_hare_attack_def(attack);
        state=opened(attack,(d->windows[0].start+d->windows[0].end)/2);hp=500;
        a.bone=(d->windows[0].bones&PE_HARE_RIGHT_FOREARM)?PE_HARE_RIGHT_FOREARM:PE_HARE_LEFT_FOREARM;
        CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&r)==PE_HARE_DAMAGE_HIT&&r.damage==128);
    }
    a.bone=PE_HARE_RIGHT_FOREARM;state=opened(0,.4);hp=500;
    a.eligible=false;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED);a.eligible=true;
    t.eligible=false;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED);t.eligible=true;
    t.entity_id=1;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED);t.entity_id=2;
    a.bone=PE_HARE_LEFT_FOREARM;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED);a.bone=PE_HARE_RIGHT_FOREARM;
    state=opened(0,.1);state.open_windows=1;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED); /* Cannot forge a clock-open window. */
    state=opened(0,.4);a.guide=1;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_INVALID);a.guide=0;
    t.seconds=NAN;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_INVALID);t.seconds=0;
    a.attack=FLT_MAX;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_INVALID);a.attack=64;
    state.target_count[0]=PE_HARE_ATTACK_TARGET_CAPACITY;for(unsigned i=0;i<PE_HARE_ATTACK_TARGET_CAPACITY;i++)state.targets[0][i]=100+i;
    unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_IGNORED);state.target_count[0]++;
    unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_INVALID);state=opened(0,.4);
    PeActor *wrong=actor_fixture(path,true);CHECK(wrong);if(wrong){a.pose.actor=wrong;unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_INVALID);a.pose.actor=actor;pe_actor_free(wrong);}
    PeHitboxes *bad=boxes_fixture(path,actor,true);CHECK(bad);if(bad){t.pose.hitboxes=bad;t.pose.scale=a.pose.scale=1000;
        unchanged(&state,&a,&t,&hp,PE_HARE_DAMAGE_INVALID);t.pose=a.pose=pose;pe_hitboxes_free(bad);}
    blend_tests(path,actor,boxes);
    /* Optional real authored forearm clips/capsules; player fixture is explicit. */
    const char *hare_path=pe_test_path("rebuild/build/reference-assets/hare.peac");FILE *f=fopen(hare_path,"rb");
    if(f){fclose(f);char error[256];PeActor *hare=pe_actor_load(hare_path,error,sizeof(error));CHECK(hare);
        PeHitboxes *hb=hare?pe_hitboxes_load(pe_test_path("rebuild/build/reference-assets/hare.pehb"),hare,error,sizeof(error)):NULL;CHECK(hb);
        if(hb){a.pose=(PeHareDamagePose){.actor=hare,.hitboxes=hb,.scale=1};state=opened(0,.4);a.bone=PE_HARE_RIGHT_FOREARM;
            size_t ci=pe_actor_find_clip(hare,"attack0");PeHitboxCapsule c={0};bool found=false;
            for(size_t i=0;i<pe_hitboxes_count(hb);i++){const PeHitboxDefinition *d=pe_hitboxes_get(hb,i);if(d->kind==PE_HITBOX_ATTACK&&d->group==a.bone&&d->joint!=PE_HITBOX_DISABLED){CHECK(pe_hitbox_pose(hb,hare,i,ci,.4,(PeVec3){0},0,1,&c));found=true;break;}}
            CHECK(found);t.pose.position=c.a;hp=500;CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&r)==PE_HARE_DAMAGE_HIT);
            const char *player_path=pe_test_path("rebuild/build/player-defence/player-defence.peac");FILE *pf=fopen(player_path,"rb");
            if(pf){fclose(pf);PeActor *player=pe_actor_load(player_path,error,sizeof(error));CHECK(player);
                PeHitboxes *pb=player?pe_hitboxes_load(pe_test_path("rebuild/build/player-defence/player-defence.pehb"),player,error,sizeof(error)):NULL;CHECK(pb);
                if(pb){CHECK(pe_hitboxes_count(pb)==16);t.pose=(PeHareDamagePose){.actor=player,.hitboxes=pb,.scale=1};
                    t.clip=pe_actor_find_clip(player,"NormalIdle");CHECK(t.clip!=PE_ACTOR_BIND_POSE);PeHitboxCapsule defence;
                    CHECK(pe_hitbox_pose(pb,player,0,t.clip,0,(PeVec3){0},0,1,&defence));
                    t.pose.position=(PeVec3){c.a.x-defence.a.x,c.a.y-defence.a.y,c.a.z-defence.a.z};
                    state=opened(0,.4);hp=500;CHECK(pe_hare_damage_resolve(&state,&a,&t,&hp,&r)==PE_HARE_DAMAGE_HIT);
                    CHECK(r.damage>0&&hp<500&&r.defence_index==0);pe_hitboxes_free(pb);}
                pe_actor_free(player);
            }else fprintf(stderr,"hare damage: SKIP optional recovered player defence assets\n");
            pe_hitboxes_free(hb);}
        pe_actor_free(hare);
    }else fprintf(stderr,"hare damage: SKIP optional recovered hare assets\n");
    pe_hitboxes_free(boxes);pe_actor_free(actor);unlink(path);return failures;
}
