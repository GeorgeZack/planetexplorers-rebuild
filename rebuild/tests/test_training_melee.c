#define _POSIX_C_SOURCE 200809L
#include "pe_training_melee.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT "."
#endif
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"training melee:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static void u32(unsigned char*p,uint32_t v){for(int i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void f32(unsigned char*p,float v){uint32_t bits;memcpy(&bits,&v,4);u32(p,bits);}
static void matrix(unsigned char*p,float x){float v[]={1,0,0,x,0,1,0,0,0,0,1,0};for(int i=0;i<12;i++)f32(p+4*i,v[i]);}
static bool save(const char*path,const void*data,size_t size){FILE*f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(data,1,size,f)==size;return fclose(f)==0&&ok;}
static PeActor*actor_fixture(const char*path,unsigned version,bool missing){
    unsigned char b[1404]={0};memcpy(b,"PEAC",4);u32(b+4,version);u32(b+8,3);u32(b+12,3);u32(b+16,1);u32(b+20,4);
    for(int i=0;i<3;i++){f32(b+24+i*56,(float)i);f32(b+40+i*56,1);f32(b+56+i*56,1);}
    u32(b+196,1);u32(b+200,2);matrix(b+204,0);
    for(unsigned c=0;c<4;c++){
        unsigned char*p=b+252+c*288;snprintf((char*)p,32,"Attack%u",c+6+(missing?10:0));f32(p+32,2);u32(p+36,5);f32(p+40,2);
        for(unsigned frame=0;frame<5;frame++)matrix(p+48+frame*48,(float)frame*.1f);
    }
    CHECK(save(path,b,sizeof b));char error[128];return pe_actor_load(path,error,sizeof error);
}
static PeHitboxes*boxes_fixture(const char*path,const PeActor*actor,bool attack){
    unsigned char b[136]={0};memcpy(b,"PEHB",4);u32(b+4,1);u32(b+8,1);u32(b+12,1);uint64_t hash=pe_actor_skeleton_hash(actor);u32(b+16,(uint32_t)hash);u32(b+20,(uint32_t)(hash>>32));
    unsigned char*p=b+32;memcpy(p,attack?"SingleHandSword":"Head",attack?16:5);u32(p+32,attack?1:0);u32(p+36,attack?1:0);f32(p+52,attack?1:2);f32(p+72,1);f32(p+92,attack?.3f:.5f);
    CHECK(save(path,b,sizeof b));char error[128];return pe_hitboxes_load(path,actor,error,sizeof error);
}
static PeTrainingMeleeGate gate(void){return (PeTrainingMeleeGate){.alive=true,.knife_owned=true,.knife_equipped=true,.knife_drawn=true,.grounded=true,.third_person=true,.action_allowed=true};}
static PeTrainingMeleeFrame frame(void){return (PeTrainingMeleeFrame){.gate=gate(),.player_scale=1,.target_scale=1,.attacker_eligible=true,.target_eligible=true};}
static unsigned run_action(PeTrainingMelee*h,PeTrainingMeleeFrame f,float*stamina,bool combos,unsigned*deaths){
    unsigned hits=0;
    for(unsigned i=0;i<600&&h->action.active;i++){
        if(combos)pe_training_melee_queue(h);
        PeTrainingMeleeUpdate u;CHECK(pe_training_melee_step(h,&f,stamina,&u));
        if(u.weapon_valid)CHECK(fabs(u.weapon.a.x-(float)(h->action.clip_time*.2))<.00001);
        if(u.contact_status==PE_KNIFE_CONTACT_HIT){hits++;if(u.contact.died)(*deaths)++;}
    }
    CHECK(!h->action.active);return hits;
}
static void actual_assets(void){
    const char*body_path=PE_SOURCE_ROOT "/rebuild/build/animator-bake/player-female/all/player-part-0-material-0.peac";
    const char*knife_path=PE_SOURCE_ROOT "/rebuild/build/animator-bake/player-female/drawn-all/knife-drawn-part-0-material-0.peac";
    const char*weapon_path=PE_SOURCE_ROOT "/rebuild/build/animator-bake/player-female/drawn-all/knife-drawn.pehb";
    const char*target_path=PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.peac";
    const char*defence_path=PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.pehb";
    const char*paths[]={body_path,knife_path,weapon_path,target_path,defence_path};
    for(unsigned i=0;i<5;i++){FILE*f=fopen(paths[i],"rb");if(!f)return;fclose(f);}
    char error[128];PeActor*body=pe_actor_load(body_path,error,sizeof error),*knife=pe_actor_load(knife_path,error,sizeof error),*target=pe_actor_load(target_path,error,sizeof error);
    PeHitboxes*weapon=pe_hitboxes_load(weapon_path,knife,error,sizeof error),*defence=pe_hitboxes_load(defence_path,target,error,sizeof error);
    CHECK(body&&knife&&target&&weapon&&defence);
    if(body&&knife&&target&&weapon&&defence){
        PeTrainingMeleeSetup setup={.body=body,.drawn_knife=knife,.target_actor=target,.drawn_capsule=weapon,.target_defence=defence,.gender=PE_COMBAT_FEMALE,.player_id=1,.target_id=261,.attack=19,.stamina_reduce_percent=1,.target_hp=212,.target_max_hp=212,.target_defense=29};
        PeTrainingMelee host;bool ready=pe_training_melee_init(&host,&setup);CHECK(ready);
        if(ready){
            float stamina=100;CHECK(pe_training_melee_begin(&host,gate(),&stamina));PeTrainingMeleeFrame f=frame();f.target_position.x=100;f.target_clip=pe_actor_find_clip(target,"idle");
            unsigned frames=0;PeHitboxCapsule first={0};bool have_first=false,moved=false;
            while(host.action.active&&frames++<600){
                pe_training_melee_queue(&host);PeTrainingMeleeUpdate u;bool ok=pe_training_melee_step(&host,&f,&stamina,&u);CHECK(ok);if(!ok)break;
                CHECK(u.contact_status!=PE_KNIFE_CONTACT_HIT&&host.target_hp==212);
                if(u.weapon_valid){if(!have_first){first=u.weapon;have_first=true;}else if(fabsf(u.weapon.a.x-first.a.x)+fabsf(u.weapon.a.y-first.a.y)+fabsf(u.weapon.a.z-first.a.z)>.01f)moved=true;}
            }
            CHECK(!host.action.active&&host.action.stage==3&&stamina==80&&have_first&&moved);
        }
    }
    pe_hitboxes_free(weapon);pe_hitboxes_free(defence);pe_actor_free(body);pe_actor_free(knife);pe_actor_free(target);
}
int pe_test_training_melee(void){
    failures=0;char path[]="/tmp/pe-training-melee-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return failures;close(fd);
    PeActor*a=actor_fixture(path,2,false),*wrong=actor_fixture(path,1,false),*missing=actor_fixture(path,2,true);
    CHECK(a&&wrong&&missing);if(!a||!wrong||!missing)goto done;
    PeHitboxes*w=boxes_fixture(path,a,true),*d=boxes_fixture(path,a,false);CHECK(w&&d);if(!w||!d){pe_hitboxes_free(w);pe_hitboxes_free(d);goto done;}
    PeTrainingMeleeSetup setup={.body=a,.drawn_knife=a,.target_actor=a,.drawn_capsule=w,.target_defence=d,.gender=PE_COMBAT_FEMALE,.player_id=1,.target_id=2,.attack=19,.stamina_reduce_percent=1,.target_hp=212,.target_max_hp=212,.target_defense=29};
    PeTrainingMelee h,before;CHECK(pe_training_melee_init(&h,&setup));before=h;
    PeTrainingMeleeSetup bad=setup;bad.body=wrong;CHECK(!pe_training_melee_init(&h,&bad));CHECK(!memcmp(&h,&before,sizeof h));
    bad=setup;bad.drawn_knife=missing;CHECK(!pe_training_melee_init(&h,&bad));bad=setup;bad.drawn_capsule=d;CHECK(!pe_training_melee_init(&h,&bad));
    bad=setup;bad.target_defence=w;CHECK(!pe_training_melee_init(&h,&bad));bad=setup;bad.target_id=1;CHECK(!pe_training_melee_init(&h,&bad));
    PeTrainingMeleeGate g=gate();float stamina=100;g.third_person=false;CHECK(!pe_training_melee_begin(&h,g,&stamina));g=gate();g.move_requested=true;CHECK(!pe_training_melee_begin(&h,g,&stamina));
    g=gate();g.velocity.x=.1f;CHECK(!pe_training_melee_begin(&h,g,&stamina));g=gate();g.knife_drawn=false;CHECK(!pe_training_melee_begin(&h,g,&stamina));g=gate();g.grounded=false;CHECK(!pe_training_melee_begin(&h,g,&stamina));
    CHECK(stamina==100&&!memcmp(&h,&before,sizeof h));
    CHECK(pe_training_melee_begin(&h,gate(),&stamina));CHECK(stamina==95);CHECK(!pe_training_melee_begin(&h,gate(),&stamina));CHECK(!pe_training_melee_queue(&h));
    PeTrainingMeleeFrame f=frame();PeTrainingMeleeUpdate u;
    CHECK(pe_training_melee_step(&h,&f,&stamina,&u));CHECK(u.contact_status==PE_KNIFE_CONTACT_IGNORED&&h.target_hp==212&&!h.action.contact_open);
    CHECK(fabs(h.action.clip_time-pe_combat_animation_speed(stamina)*PE_TRAINING_MELEE_DT)<1e-9);
    unsigned deaths=0;CHECK(run_action(&h,f,&stamina,false,&deaths)==1&&deaths==0);
    CHECK(fabsf(h.target_hp-(212-361.f/48*2))<.0001f&&stamina==95);
    /* Four real contacts, one per skill guide; chained stages charge once. */
    CHECK(pe_training_melee_init(&h,&setup));stamina=100;CHECK(pe_training_melee_begin(&h,gate(),&stamina));
    CHECK(run_action(&h,f,&stamina,true,&deaths)==4&&deaths==0);CHECK(stamina==80&&h.action.stage==3);
    CHECK(fabsf(h.target_hp-(212-4*361.f/48*2))<.0001f);
    /* Actual separated target and explicit permissions suppress HP changes. */
    CHECK(pe_training_melee_init(&h,&setup));stamina=100;CHECK(pe_training_melee_begin(&h,gate(),&stamina));f.target_position.x=100;
    CHECK(run_action(&h,f,&stamina,false,&deaths)==0&&h.target_hp==212);
    f=frame();f.target_eligible=false;CHECK(pe_training_melee_begin(&h,gate(),&stamina));CHECK(run_action(&h,f,&stamina,false,&deaths)==0&&h.target_hp==212);
    /* Positive-to-zero only; later swings on that same dead target can't
     * re-emit death. Host is responsible for objective matching/dispatch. */
    setup.target_hp=1;CHECK(pe_training_melee_init(&h,&setup));stamina=0;CHECK(pe_training_melee_begin(&h,gate(),&stamina));f=frame();
    CHECK(run_action(&h,f,&stamina,false,&deaths)==1&&deaths==1&&h.target_hp==0&&stamina==0);
    CHECK(pe_training_melee_begin(&h,gate(),&stamina));CHECK(run_action(&h,f,&stamina,true,&deaths)==0&&deaths==1);
    setup.target_hp=212;CHECK(pe_training_melee_init(&h,&setup));stamina=100;CHECK(pe_training_melee_begin(&h,gate(),&stamina));
    f=frame();f.gate.move_requested=true;CHECK(pe_training_melee_step(&h,&f,&stamina,&u));CHECK(u.cancelled&&!h.action.active&&!h.action.contact_open&&h.target_hp==212&&stamina==95);
    CHECK(pe_training_melee_begin(&h,gate(),&stamina));f=frame();f.gate.third_person=false;CHECK(pe_training_melee_step(&h,&f,&stamina,&u));CHECK(u.cancelled&&!h.action.active);
    CHECK(pe_training_melee_begin(&h,gate(),&stamina));f=frame();before=h;float old_stamina=stamina;memset(&u,0x5a,sizeof u);PeTrainingMeleeUpdate saved=u;
    f.target_seconds=NAN;CHECK(!pe_training_melee_step(&h,&f,&stamina,&u));CHECK(!memcmp(&h,&before,sizeof h)&&stamina==old_stamina&&!memcmp(&u,&saved,sizeof u));
    f=frame();f.target_clip=99;CHECK(!pe_training_melee_step(&h,&f,&stamina,&u));CHECK(!memcmp(&h,&before,sizeof h)&&stamina==old_stamina);
    pe_training_melee_cancel(&h);CHECK(!h.action.active&&!h.action.contact_open&&h.target_hp==before.target_hp);
    pe_hitboxes_free(w);pe_hitboxes_free(d);
done:pe_actor_free(a);pe_actor_free(wrong);pe_actor_free(missing);unlink(path);actual_assets();return failures;
}
