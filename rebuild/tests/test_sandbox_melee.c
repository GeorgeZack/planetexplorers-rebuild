#define _POSIX_C_SOURCE 200809L
#include "pe_sandbox_melee.h"
#include "pe_running_attack_motion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT "."
#endif
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sandbox melee:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static void u32(unsigned char*p,uint32_t v){for(int i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static void f32(unsigned char*p,float v){uint32_t bits;memcpy(&bits,&v,4);u32(p,bits);}
static void matrix(unsigned char*p,float x){float v[]={1,0,0,x,0,1,0,0,0,0,1,0};for(int i=0;i<12;i++)f32(p+4*i,v[i]);}
static bool save(const char*path,const void*data,size_t size){FILE*f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(data,1,size,f)==size;return fclose(f)==0&&ok;}
static PeActor*actor_fixture(const char*path,unsigned version,bool missing,bool running){
    unsigned char b[1692]={0};memcpy(b,"PEAC",4);u32(b+4,version);u32(b+8,3);u32(b+12,3);u32(b+16,1);u32(b+20,running?5:4);
    for(int i=0;i<3;i++){f32(b+24+i*56,(float)i);f32(b+40+i*56,1);f32(b+56+i*56,1);}
    u32(b+196,1);u32(b+200,2);matrix(b+204,0);
    for(unsigned c=0;c<(running?5u:4u);c++){
        unsigned char*p=b+252+c*288;if(c==4)snprintf((char*)p,32,"RunSwordAttack");else snprintf((char*)p,32,"Attack%u",c+6+(missing?10:0));f32(p+32,2);u32(p+36,5);f32(p+40,2);
        for(unsigned frame=0;frame<5;frame++)matrix(p+48+frame*48,(float)frame*.1f);
    }
    CHECK(save(path,b,running?sizeof b:1404));char error[128];return pe_actor_load(path,error,sizeof error);
}
static PeHitboxes*boxes_fixture(const char*path,const PeActor*actor,bool attack){
    unsigned char b[136]={0};memcpy(b,"PEHB",4);u32(b+4,1);u32(b+8,1);u32(b+12,1);uint64_t hash=pe_actor_skeleton_hash(actor);u32(b+16,(uint32_t)hash);u32(b+20,(uint32_t)(hash>>32));
    unsigned char*p=b+32;memcpy(p,attack?"SingleHandSword":"Head",attack?16:5);u32(p+32,attack?1:0);u32(p+36,attack?1:0);f32(p+52,attack?1:2);f32(p+72,1);f32(p+92,attack?.3f:.5f);
    CHECK(save(path,b,sizeof b));char error[128];return pe_hitboxes_load(path,actor,error,sizeof error);
}
static PeSandboxMeleeGate gate(void){return (PeSandboxMeleeGate){.alive=true,.weapon_owned=true,.grounded=true,.third_person=true,.action_allowed=true,.equipped_item_id=33,.equipped_instance_id=91,.drawn_instance_id=91};}
static PeSandboxMeleeFrame frame(void){return (PeSandboxMeleeFrame){.gate=gate(),.player_scale=1,.attacker_eligible=true};}
static void targets_init(PeKnifeContactTarget*t,const PeActor*a,const PeHitboxes*d,float hp){
    for(unsigned i=0;i<6;i++)t[i]=(PeKnifeContactTarget){.entity_id=10+i,.actor=a,.hitboxes=d,.clip=0,.scale=1,.defense=29,.hp=hp,.max_hp=212,.eligible=true};
}
static unsigned run(PeSandboxMelee*h,PeSandboxMeleeFrame f,PeKnifeContactTarget*t,float*stamina,bool combos,unsigned*deaths){
    unsigned hits=0;
    for(unsigned i=0;i<600&&h->action.active;i++){
        if(combos)pe_sandbox_melee_queue(h,f.gate);
        PeSandboxMeleeUpdate u;bool ok=pe_sandbox_melee_step(h,&f,t,6,stamina,&u);CHECK(ok);if(!ok)break;
        CHECK(u.target_count==6);
        if(u.weapon_valid)CHECK(fabs(u.weapon.a.x-(float)(h->action.clip_time*.2))<.00001);
        for(unsigned k=0;k<6;k++)if(u.status[k]==PE_KNIFE_CONTACT_HIT){hits++;if(u.contact[k].died)(*deaths)++;}
    }
    CHECK(!h->action.active&&h->instance_id==0);return hits;
}
static void actual_assets(void){
    const char*paths[]={PE_SOURCE_ROOT "/rebuild/build/animator-bake/player-female/all/player-part-0-material-0.peac",
        PE_SOURCE_ROOT "/rebuild/build/animator-bake/player-female/drawn-all/knife-drawn-part-0-material-0.peac",
        PE_SOURCE_ROOT "/rebuild/build/animator-bake/player-female/drawn-all/knife-drawn.pehb",
        PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.peac",PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.pehb"};
    for(unsigned i=0;i<5;i++){FILE*f=fopen(paths[i],"rb");if(!f){fprintf(stderr,"sandbox melee: optional authored actor fixture absent, not tested\n");return;}fclose(f);}
    char error[160];PeActor*body=pe_actor_load(paths[0],error,sizeof error),*knife=pe_actor_load(paths[1],error,sizeof error),*hare=pe_actor_load(paths[3],error,sizeof error);
    PeHitboxes*w=pe_hitboxes_load(paths[2],knife,error,sizeof error),*d=pe_hitboxes_load(paths[4],hare,error,sizeof error);
    CHECK(body&&knife&&hare&&w&&d);
    if(body&&knife&&hare&&w&&d){
        PeSandboxMeleeSetup setup={.weapon_item_id=33,.history_enabled=true,.history_local_center={0,.875f,0},.body=body,.drawn_weapon=knife,.drawn_capsule=w,.gender=PE_COMBAT_FEMALE,.player_id=1,.attack=19,.stamina_reduce_percent=1};
        PeSandboxMelee h;bool ready=pe_sandbox_melee_init(&h,&setup);CHECK(ready);
        if(ready){
            PeKnifeContactTarget t[6];targets_init(t,hare,d,212);for(unsigned i=0;i<6;i++){t[i].clip=pe_actor_find_clip(hare,"idle");t[i].position.x=100+i;}
            float stamina=100;CHECK(pe_sandbox_melee_begin(&h,gate(),&stamina));PeSandboxMeleeFrame f=frame();unsigned frames=0;
            while(h.action.active&&frames++<600){
                pe_sandbox_melee_queue(&h,f.gate);PeSandboxMeleeUpdate u;bool ok=pe_sandbox_melee_step(&h,&f,t,6,&stamina,&u);CHECK(ok);if(!ok)break;
                for(unsigned i=0;i<6;i++)CHECK(u.status[i]!=PE_KNIFE_CONTACT_HIT&&t[i].hp==212);
            }
            CHECK(!h.action.active&&h.action.stage==3&&stamina==80);
        }
    }
    pe_hitboxes_free(w);pe_hitboxes_free(d);pe_actor_free(body);pe_actor_free(knife);pe_actor_free(hare);
}
static void running_tests(const char *path){
    PeActor *a=actor_fixture(path,2,false,true),*old=actor_fixture(path,2,false,false);
    if(!a||!old){CHECK(false);pe_actor_free(a);pe_actor_free(old);return;}
    PeHitboxes *w=boxes_fixture(path,a,true),*d=boxes_fixture(path,a,false);
    if(!w||!d){CHECK(false);goto done;}
    PeSandboxMeleeSetup setup={.weapon_item_id=33,.body=a,.drawn_weapon=a,.drawn_capsule=w,.gender=PE_COMBAT_FEMALE,.player_id=1,.attack=19,.stamina_reduce_percent=1};
    PeSandboxMelee h;CHECK(pe_sandbox_melee_init(&h,&setup));PeSandboxMelee before=h;
    PeSandboxMeleeGate g=gate();g.velocity.x=5;g.move_requested=true;float stamina=100;
    CHECK(!pe_sandbox_melee_begin(&h,g,&stamina)&&stamina==100&&!memcmp(&h,&before,sizeof h));
    setup.running_motion_ready=true;setup.body=old;
    CHECK(!pe_sandbox_melee_init(&h,&setup)&&!memcmp(&h,&before,sizeof h));
    setup.body=a;setup.gender=PE_COMBAT_MALE;CHECK(!pe_sandbox_melee_init(&h,&setup));
    setup.gender=PE_COMBAT_FEMALE;CHECK(pe_sandbox_melee_init(&h,&setup));before=h;
    /* Strict source branch boundaries. Exact4.5 selects the stationary branch,
     * whose moving starts deliberately remain unsupported in this increment. */
    g.velocity.x=4.5f;CHECK(!pe_sandbox_melee_begin(&h,g,&stamina));
    g.velocity.x=nextafterf(4.5f,0);CHECK(!pe_sandbox_melee_begin(&h,g,&stamina));
    g.velocity.x=nextafterf(7,INFINITY);CHECK(!pe_sandbox_melee_begin(&h,g,&stamina));
    CHECK(stamina==100&&!memcmp(&h,&before,sizeof h));
    const float speeds[]={nextafterf(4.5f,INFINITY),7};
    for(unsigned k=0;k<2;k++){
        g.velocity.x=speeds[k];CHECK(pe_sandbox_melee_begin(&h,g,&stamina));
        CHECK(h.action.variant==PE_COMBAT_RUNNING&&!strcmp(pe_combat_pose_name(&h.action),"RunSwordAttack"));
        pe_sandbox_melee_cancel(&h);
    }
    g.velocity=(PeVec3){0,4.6f,0};CHECK(pe_sandbox_melee_begin(&h,g,&stamina));pe_sandbox_melee_cancel(&h);
    g.velocity=(PeVec3){5,0,0};g.in_water=true;before=h;float energy=stamina;
    CHECK(!pe_sandbox_melee_begin(&h,g,&stamina)&&stamina==energy&&!memcmp(&h,&before,sizeof h));g.in_water=false;
    CHECK(pe_sandbox_melee_begin(&h,g,&stamina));
    PeSandboxMeleeFrame f=frame();f.gate=g;PeKnifeContactTarget t[6];targets_init(t,a,d,212);
    PeSandboxMeleeUpdate out;PeCombatInterval span;
    /* Split path uses POST-sweep world position; changing running speed does
     * not reclassify the skill. A pending tick cannot advance twice. */
    f.player_position.x=25;for(unsigned i=0;i<6;i++)t[i].position.x=25;
    while(!h.action.contact_open){
        g.velocity.x=h.action.update_index%2?0:20;
        CHECK(pe_sandbox_melee_advance(&h,g,&stamina));CHECK(pe_sandbox_melee_interval(&h,&span));
        CHECK(span.valid&&span.variant==PE_COMBAT_RUNNING&&span.stage==0&&span.to>=span.from);
        before=h;energy=stamina;CHECK(!pe_sandbox_melee_advance(&h,g,&stamina));
        CHECK(!pe_sandbox_melee_queue(&h,g));CHECK(!memcmp(&h,&before,sizeof h)&&stamina==energy);
        if(h.action.contact_open)break;
        CHECK(pe_sandbox_melee_resolve(&h,&f,t,6,&out));
    }
    PeKnifeContactTarget saved_t[6];memcpy(saved_t,t,sizeof t);t[5].seconds=NAN;
    memset(&out,0x5a,sizeof out);PeSandboxMeleeUpdate saved_out=out;before=h;
    CHECK(!pe_sandbox_melee_resolve(&h,&f,t,6,&out)&&!memcmp(&h,&before,sizeof h)&&!memcmp(&out,&saved_out,sizeof out));
    for(unsigned i=0;i<6;i++)CHECK(t[i].hp==saved_t[i].hp);
    memcpy(t,saved_t,sizeof t);uint64_t tick=h.action.update_index;
    CHECK(pe_sandbox_melee_resolve(&h,&f,t,6,&out));CHECK(h.action.update_index==tick&&!h.tick_pending);
    CHECK(fabs(out.weapon.a.x-(25+h.action.clip_time*.2))<.00001);
    for(unsigned i=0;i<6;i++)CHECK(out.status[i]==PE_KNIFE_CONTACT_HIT&&t[i].hp<212);
    before=h;saved_out=out;CHECK(!pe_sandbox_melee_resolve(&h,&f,t,6,&out));CHECK(!memcmp(&h,&before,sizeof h)&&!memcmp(&out,&saved_out,sizeof out));
    CHECK(!pe_sandbox_melee_interval(&h,&span));
    /* Identity/modal cancellation between motion and contact suppresses hits. */
    for(unsigned mode=0;mode<3;mode++){
        pe_sandbox_melee_cancel(&h);g=gate();g.velocity.x=5;g.move_requested=true;
        CHECK(pe_sandbox_melee_begin(&h,g,&stamina));f=frame();f.gate=g;
        targets_init(t,a,d,212);
        while(!h.action.contact_open){CHECK(pe_sandbox_melee_advance(&h,g,&stamina));if(h.action.contact_open)break;CHECK(pe_sandbox_melee_resolve(&h,&f,t,6,&out));}
        if(mode==0)f.gate.action_allowed=false;
        if(mode==1)f.gate.equipped_instance_id=f.gate.drawn_instance_id=92;
        if(mode==2)f.gate.grounded=false;
        CHECK(pe_sandbox_melee_resolve(&h,&f,t,6,&out)&&out.cancelled&&!h.action.active&&!h.tick_pending);
        for(unsigned i=0;i<6;i++)CHECK(t[i].hp==212);
    }
    /* Existing four-stage contacts and charges stay shared across six targets. */
    g=gate();g.velocity.x=5;g.move_requested=true;f=frame();f.gate=g;targets_init(t,a,d,212);stamina=100;
    CHECK(pe_sandbox_melee_begin(&h,g,&stamina));unsigned deaths=0;
    CHECK(run(&h,f,t,&stamina,true,&deaths)==24&&stamina==80&&deaths==0);
 done:pe_hitboxes_free(w);pe_hitboxes_free(d);pe_actor_free(a);pe_actor_free(old);
}
static void actual_running_playback(PeSandboxMeleeSetup setup){
    const char *paths[]={PE_SOURCE_ROOT "/rebuild/build/running-motion-20261006/running-knife.perm",
        PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.peac",PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.pehb"};
    for(unsigned i=0;i<3;i++){FILE *f=fopen(paths[i],"rb");if(!f){fprintf(stderr,"sandbox melee playback: SKIP qualified408 root/hare fixture %s\n",paths[i]);return;}fclose(f);}
    char error[200];PeRootMotion *curve=pe_root_motion_load(paths[0],error,sizeof error);
    PeActor *hare=pe_actor_load(paths[1],error,sizeof error);PeHitboxes *boxes=hare?pe_hitboxes_load(paths[2],hare,error,sizeof error):NULL;
    CHECK(curve&&hare&&boxes);if(!curve||!hare||!boxes)goto done;
    size_t idle=pe_actor_find_clip(hare,"idle"),defence=SIZE_MAX;
    for(size_t i=0;i<pe_hitboxes_count(boxes);i++){const PeHitboxDefinition *d=pe_hitboxes_get(boxes,i);
        if(d->kind==PE_HITBOX_DEFENCE&&d->joint!=PE_HITBOX_DISABLED){defence=i;break;}}
    CHECK(idle!=PE_ACTOR_BIND_POSE&&defence!=SIZE_MAX);if(idle==PE_ACTOR_BIND_POSE||defence==SIZE_MAX)goto done;
    setup.playback_enabled=true;setup.history_enabled=true;
    PeSandboxMeleeGate g=gate();g.velocity.z=5;g.move_requested=true;g.scaled_time=10;
    PeHitboxCapsule target_pose;CHECK(pe_hitbox_pose(boxes,hare,defence,idle,.37,(PeVec3){0},27,1,&target_pose));
    PeSandboxMelee measured;CHECK(pe_sandbox_melee_init(&measured,&setup));float energy=100;
    CHECK(pe_sandbox_melee_begin(&measured,g,&energy));PeVec3 fixed_target={0},position={0},velocity=g.velocity;unsigned opening=0;
    /* Arrange a fixed whole-hare pose from a no-target run's first open blade.
     * Both compared actions subsequently start afresh with this same fixture;
     * no target follows the blade. This is geometry testing, not earned combat. */
    for(unsigned tick=1;tick<120&&measured.action.active;tick++){
        g.scaled_time=(float)(10+tick*PE_SANDBOX_MELEE_DT);
        CHECK(pe_sandbox_melee_advance(&measured,g,&energy));PeCombatInterval span;CHECK(pe_sandbox_melee_interval(&measured,&span));
        CHECK(pe_running_attack_motion_request(curve,&span,velocity,0,PE_SANDBOX_MELEE_DT,&velocity));
        position.x+=velocity.x*(float)PE_SANDBOX_MELEE_DT;position.z+=velocity.z*(float)PE_SANDBOX_MELEE_DT;
        PeSandboxMeleeFrame f=frame();f.gate=g;f.player_position=position;PeSandboxMeleeUpdate out;
        CHECK(pe_sandbox_melee_resolve(&measured,&f,NULL,0,&out));
        if(measured.action.contact_open){
            fixed_target=(PeVec3){(out.weapon.a.x+out.weapon.b.x-target_pose.a.x-target_pose.b.x)*.5f,
                (out.weapon.a.y+out.weapon.b.y-target_pose.a.y-target_pose.b.y)*.5f,
                (out.weapon.a.z+out.weapon.b.z-target_pose.a.z-target_pose.b.z)*.5f};opening=tick;break;
        }
    }
    CHECK(opening>0);if(!opening)goto done;
    PeSandboxMelee hosts[2];PeKnifeContactTarget targets[2];float stamina[2]={100,100};PeVec3 roots[2]={{0}},velocities[2]={{0,0,5},{0,0,5}};
    unsigned hits[2]={0},notifications[2]={0},ended[2]={0};double final_span[2]={0};bool slowed=false,overshoot=false,recovered=false,root_changed=false,body_changed=false,weapon_changed=false;
    g.scaled_time=10;
    for(unsigned i=0;i<2;i++){
        CHECK(pe_sandbox_melee_init(&hosts[i],&setup)&&pe_sandbox_melee_begin(&hosts[i],g,&stamina[i]));
        targets[i]=(PeKnifeContactTarget){.entity_id=700,.actor=hare,.hitboxes=boxes,.clip=idle,.seconds=.37,
            .position=fixed_target,.yaw=27,.scale=1,.defense=29,.hp=212,.max_hp=212,.eligible=true,.trigger_candidate=true,.trigger_can_damage=true};
        if(i)targets[i].position.x+=100; /* Real miss control, same playback opt-in. */
    }
    float contact_stamp=0;unsigned ticks=0;
    while((hosts[0].action.active||hosts[1].action.active)&&ticks++<240){
        g.scaled_time=(float)(10+ticks*PE_SANDBOX_MELEE_DT);PeCombatInterval spans[2]={{0}};
        PeVec3 deltas[2]={{0}};PeSandboxMeleeUpdate outputs[2]={{0}};
        for(unsigned i=0;i<2;i++)if(hosts[i].action.active){
            PeSandboxMelee *h=&hosts[i];double wall=h->action.wall_time,clip=h->action.clip_time;
            PeAttackPlayback clock=h->playback;PeAttackPlaybackSample sample;float value=1,rate=0;
            CHECK(pe_attack_playback_sample(&clock,g.scaled_time,33,stamina[i],&sample));
            if(sample.curve_active)CHECK(pe_attack_playback_curve(sample.curve_time,&value));
            CHECK(pe_attack_playback_speed(sample,value,&rate));
            CHECK(pe_sandbox_melee_advance(h,g,&stamina[i])&&pe_sandbox_melee_interval(h,&spans[i]));
            CHECK(spans[i].valid&&spans[i].variant==PE_COMBAT_RUNNING&&spans[i].stage==0&&spans[i].from==clip);
            CHECK(fabs(h->action.wall_time-wall-PE_SANDBOX_MELEE_DT)<1e-12);
            CHECK(fabs(h->action.wall_time-ticks*PE_SANDBOX_MELEE_DT)<1e-12&&h->playback.time==g.scaled_time);
            if(h->action.active){CHECK(fabs(spans[i].to-clip-rate*PE_SANDBOX_MELEE_DT)<1e-9);CHECK(spans[i].to==h->action.clip_time);}
            CHECK(pe_root_motion_delta(curve,spans[i].from,spans[i].to,&deltas[i]));
            CHECK(pe_running_attack_motion_request(curve,&spans[i],velocities[i],0,PE_SANDBOX_MELEE_DT,&velocities[i]));
            roots[i].x+=velocities[i].x*(float)PE_SANDBOX_MELEE_DT;roots[i].z+=velocities[i].z*(float)PE_SANDBOX_MELEE_DT;
            PeSandboxMeleeFrame f=frame();f.gate=g;f.player_position=roots[i];uint64_t updates=h->action.update_index;
            CHECK(pe_sandbox_melee_resolve(h,&f,&targets[i],1,&outputs[i]));
            CHECK(h->action.update_index==updates&&!h->tick_pending&&h->action.wall_time==wall+PE_SANDBOX_MELEE_DT);
            CHECK(!memcmp(&outputs[i].interval,&spans[i],sizeof spans[i]));
            if(outputs[i].trigger_contact[0])CHECK(i==0&&ticks==opening);
            if(outputs[i].playback_notified[0]){notifications[i]++;CHECK(i==0&&ticks==opening);contact_stamp=g.scaled_time;}
            if(outputs[i].status[0]==PE_KNIFE_CONTACT_HIT){hits[i]++;CHECK(i==0&&ticks==opening&&targets[i].hp<212);}
            if(h->action.active){
                PeHitboxCapsule posed;CHECK(!strcmp(pe_combat_pose_name(&h->action),"RunSwordAttack"));
                CHECK(pe_hitbox_pose(setup.drawn_capsule,setup.drawn_weapon,0,h->running_weapon_clip,h->action.clip_time,roots[i],0,1,&posed));
                CHECK(outputs[i].weapon_valid&&!memcmp(&posed,&outputs[i].weapon,sizeof posed));
            }else{ended[i]=ticks;final_span[i]=spans[i].to;}
            if(i==0&&contact_stamp&&ticks>opening){
                CHECK(h->playback.last_contact==contact_stamp&&!outputs[i].playback_notified[0]);
                float base=pe_combat_item_animation_speed(33,stamina[i]);slowed|=rate<base*.1f;overshoot|=rate>base;
                if(!sample.curve_active){CHECK(rate==base);recovered=true;}
            }
        }
        if(contact_stamp&&ticks>opening&&hosts[0].action.active&&hosts[1].action.active){
            root_changed|=fabsf(deltas[0].z-deltas[1].z)>.00001f;
            PeHitboxCapsule a,b;CHECK(pe_hitbox_pose(setup.drawn_capsule,setup.drawn_weapon,0,hosts[0].running_weapon_clip,hosts[0].action.clip_time,(PeVec3){0},0,1,&a));
            CHECK(pe_hitbox_pose(setup.drawn_capsule,setup.drawn_weapon,0,hosts[1].running_weapon_clip,hosts[1].action.clip_time,(PeVec3){0},0,1,&b));
            weapon_changed|=memcmp(&a,&b,sizeof a)!=0;
            for(size_t v=0;v<pe_actor_stats(setup.body).vertices;v+=97){PeActorVertex a,b;
                CHECK(pe_actor_pose_vertex(setup.body,hosts[0].running_body_clip,hosts[0].action.clip_time,v,&a));
                CHECK(pe_actor_pose_vertex(setup.body,hosts[1].running_body_clip,hosts[1].action.clip_time,v,&b));
                body_changed|=memcmp(a.position,b.position,sizeof a.position)!=0;
            }
        }
        if(ticks==opening){
            CHECK(hosts[0].action.clip_time==hosts[1].action.clip_time); /* Contact affects the next display. */
            /* A second fixed tick in this SAME display frame must retain the
             * pre-contact rate; catch-up cannot trigger a fresh response phase.
             * Exercise detached copies so the paired wall-clock run stays exact. */
            PeSandboxMelee catchup=hosts[0],control=hosts[1];float energy_a=stamina[0],energy_b=stamina[1];
            PeCombatInterval ca,cb;double old=catchup.action.wall_time;
            CHECK(pe_sandbox_melee_advance(&catchup,g,&energy_a)&&pe_sandbox_melee_advance(&control,g,&energy_b));
            CHECK(pe_sandbox_melee_interval(&catchup,&ca)&&pe_sandbox_melee_interval(&control,&cb));
            CHECK(ca.from==cb.from&&ca.to==cb.to&&fabs(ca.to-ca.from-pe_combat_item_animation_speed(33,energy_a)*PE_SANDBOX_MELEE_DT)<1e-9);
            CHECK(fabs(catchup.action.wall_time-old-PE_SANDBOX_MELEE_DT)<1e-12&&catchup.playback.time==g.scaled_time);
            PeVec3 da,db;CHECK(pe_root_motion_delta(curve,ca.from,ca.to,&da)&&pe_root_motion_delta(curve,cb.from,cb.to,&db));
            CHECK(!memcmp(&da,&db,sizeof da));
            PeSandboxMeleeFrame f=frame();f.gate=g;f.player_position=roots[0];PeKnifeContactTarget t=targets[0];PeSandboxMeleeUpdate out;
            CHECK(pe_sandbox_melee_resolve(&catchup,&f,&t,1,&out)&&!out.playback_notified[0]&&!out.trigger_contact[0]);
            CHECK(t.hp==targets[0].hp&&catchup.playback.last_contact==contact_stamp);
        }
    }
    CHECK(ticks<240&&hits[0]==1&&notifications[0]==1&&hits[1]==0&&notifications[1]==0&&targets[1].hp==212);
    CHECK(slowed&&overshoot&&recovered&&root_changed&&body_changed&&weapon_changed);
    CHECK(ended[0]>ended[1]&&stamina[0]==95&&stamina[1]==95);
    CHECK(final_span[0]==1.1680725812911987&&final_span[1]==final_span[0]);
    printf("sandbox melee playback: recovered408 hit at tick%u, response/miss ended%u/%u; shared root/body/blade intervals and scaled wall clocks passed\n",opening,ended[0],ended[1]);
 done:pe_hitboxes_free(boxes);pe_actor_free(hare);pe_root_motion_free(curve);
}
static void actual_running_assets(void){
    const char *paths[]={PE_SOURCE_ROOT "/rebuild/build/running-player-20261006/player/player-part-0-material-0.peac",
        PE_SOURCE_ROOT "/rebuild/build/running-player-20261006/player-knife-drawn/knife-drawn-part-0-material-0.peac",
        PE_SOURCE_ROOT "/rebuild/build/running-player-20261006/player-knife-drawn/knife-drawn.pehb"};
    for(unsigned i=0;i<3;i++){FILE *f=fopen(paths[i],"rb");if(!f){fprintf(stderr,"sandbox melee: optional staged running actor fixture absent, not tested\n");return;}fclose(f);}
    char error[160];PeActor *body=pe_actor_load(paths[0],error,sizeof error),*knife=pe_actor_load(paths[1],error,sizeof error);
    PeHitboxes *boxes=knife?pe_hitboxes_load(paths[2],knife,error,sizeof error):NULL;CHECK(body&&knife&&boxes);
    if(body&&knife&&boxes){
        PeSandboxMeleeSetup setup={.weapon_item_id=33,.history_enabled=true,.history_local_center={0,.875f,0},.body=body,.drawn_weapon=knife,.drawn_capsule=boxes,.gender=PE_COMBAT_FEMALE,
            .player_id=1,.attack=19,.stamina_reduce_percent=1,.running_motion_ready=true};
        PeSandboxMelee h;bool ready=pe_sandbox_melee_init(&h,&setup);CHECK(ready);
        if(ready)actual_running_playback(setup);
        if(ready)for(unsigned combo=0;combo<2;combo++){
            PeSandboxMeleeGate g=gate();g.velocity.z=5;g.move_requested=true;float stamina=100;
            CHECK(pe_sandbox_melee_begin(&h,g,&stamina));PeSandboxMeleeFrame f=frame();f.gate=g;
            PeSandboxMeleeUpdate out={0};unsigned stages=1;
            for(unsigned tick=0;tick<600&&h.action.active;tick++){
                if(combo)pe_sandbox_melee_queue(&h,g);
                CHECK(pe_sandbox_melee_advance(&h,g,&stamina));PeCombatInterval span;
                CHECK(pe_sandbox_melee_interval(&h,&span));
                CHECK(span.valid&&span.from<=span.to);
                CHECK(pe_sandbox_melee_resolve(&h,&f,NULL,0,&out));
                if(out.advance==PE_COMBAT_NEXT_STAGE)stages++;
                if(h.action.active)CHECK(out.weapon_valid);
            }
            CHECK(!h.action.active&&!h.tick_pending&&stages==(combo?4u:1u)&&stamina==(combo?80:95));
            if(!combo)CHECK(out.interval.to==1.1680725812911987&&out.interval.stage==0);
        }
    }
    pe_hitboxes_free(boxes);pe_actor_free(body);pe_actor_free(knife);
}
static void weapon_clock_tests(const PeActor *a,const PeHitboxes *w){
    unsigned first_hit[2]={0},ended[2]={0};
    const int items[]={33,1};
    for(unsigned k=0;k<2;k++){
        PeSandboxMeleeSetup setup={.weapon_item_id=items[k],.body=a,.drawn_weapon=a,.drawn_capsule=w,
            .gender=PE_COMBAT_FEMALE,.player_id=1,.attack=k?20:19,.stamina_reduce_percent=1};
        PeSandboxMelee h={0};CHECK(pe_sandbox_melee_init(&h,&setup));
        PeSandboxMeleeFrame f=frame();f.gate.equipped_item_id=items[k];float stamina=100;
        PeSandboxMeleeGate wrong=f.gate;wrong.equipped_item_id=items[1-k];
        CHECK(!pe_sandbox_melee_begin(&h,wrong,&stamina)&&stamina==100);
        CHECK(pe_sandbox_melee_begin(&h,f.gate,&stamina)&&stamina==95);
        for(unsigned tick=1;tick<100&&h.action.active;tick++){
            PeSandboxMeleeUpdate u;
            CHECK(pe_sandbox_melee_step(&h,&f,NULL,0,&stamina,&u));
            if(h.action.contact_open&&!first_hit[k])first_hit[k]=tick;
            if(!h.action.active)ended[k]=tick;
        }
        CHECK(stamina==95&&ended[k]>0);
        CHECK(pe_sandbox_melee_begin(&h,f.gate,&stamina));
        f.gate=wrong;PeSandboxMeleeUpdate cancelled;
        CHECK(pe_sandbox_melee_step(&h,&f,NULL,0,&stamina,&cancelled));
        CHECK(cancelled.cancelled&&!h.action.active&&stamina==90);
        setup.weapon_item_id=34;PeSandboxMelee unchanged=h;
        CHECK(!pe_sandbox_melee_init(&h,&setup)&&!memcmp(&h,&unchanged,sizeof h));
    }
    /* Same authored Attack6; item-specific rate moves the opening/end in wall
     * ticks without changing event times or multiplying them a second time. */
    CHECK(first_hit[0]==4&&first_hit[1]==5);
    CHECK(ended[1]>ended[0]);
}
static void history_tests(const PeActor *a,const PeHitboxes *w,const PeHitboxes *d){
    PeSandboxMeleeSetup setup={.weapon_item_id=33,.body=a,.drawn_weapon=a,.drawn_capsule=w,
        .gender=PE_COMBAT_FEMALE,.player_id=1,.attack=19,.stamina_reduce_percent=1,.history_enabled=true};
    PeSandboxMelee h;CHECK(pe_sandbox_melee_init(&h,&setup));
    PeSandboxMeleeFrame f=frame();f.player_position.x=-2;
    PeKnifeContactTarget t[6];targets_init(t,a,d,212);float energy=100;
    CHECK(pe_sandbox_melee_begin(&h,f.gate,&energy));PeSandboxMeleeUpdate out;
    unsigned ticks=0;
    while(!h.action.contact_open&&ticks++<60)CHECK(pe_sandbox_melee_step(&h,&f,t,6,&energy,&out));
    CHECK(h.history_active&&h.history.previous_a.x==h.history.current.a.x);
    for(unsigned i=0;i<6;i++)CHECK(t[i].hp==212);
    /* No candidate query is necessary to advance a continuously active part. */
    PeHitboxCapsule previous=h.history.current;
    CHECK(pe_sandbox_melee_step(&h,&f,NULL,0,&energy,&out));
    CHECK(h.history.previous_a.x==previous.a.x&&h.history.current.a.x!=previous.a.x);
    /* An active displacement is sampled; all six targets use one snapshot. */
    f.player_position.x=2;CHECK(pe_sandbox_melee_step(&h,&f,t,6,&energy,&out));
    for(unsigned i=0;i<6;i++){
        CHECK(out.status[i]==PE_KNIFE_CONTACT_HIT&&out.contact[i].historical&&t[i].hp<212);
        CHECK(out.contact[i].history_contact.sample_index==out.contact[0].history_contact.sample_index);
    }
    /* Dedupe rejects another damage dispatch but must not freeze the pose. */
    previous=h.history.current;CHECK(pe_sandbox_melee_step(&h,&f,t,6,&energy,&out));
    CHECK(h.history.previous_a.x==previous.a.x&&h.history.current.a.x!=previous.a.x);
    for(unsigned i=0;i<6;i++)CHECK(out.status[i]==PE_KNIFE_CONTACT_IGNORED);
    /* Eligibility-list reset alone is not a trigger activation or pose reset. */
    h.action.target_count=0;memset(h.action.targets,0,sizeof h.action.targets);
    previous=h.history.current;for(unsigned i=0;i<6;i++)t[i].position.x=2;
    CHECK(pe_sandbox_melee_step(&h,&f,t,6,&energy,&out));
    CHECK(h.history.previous_a.x==previous.a.x&&h.history.previous_a.x!=h.history.current.a.x);
    for(unsigned i=0;i<6;i++)CHECK(out.status[i]==PE_KNIFE_CONTACT_HIT);
    /* Invalid prior geometry cannot publish action time, stamina or contacts. */
    h.history.current.a.x=NAN;PeSandboxMelee before=h;float before_energy=energy;
    PeSandboxMeleeUpdate before_out=out;
    CHECK(!pe_sandbox_melee_step(&h,&f,t,6,&energy,&out));
    CHECK(!memcmp(&h,&before,sizeof h)&&energy==before_energy&&!memcmp(&out,&before_out,sizeof out));
    pe_sandbox_melee_cancel(&h);CHECK(!h.history_active);
    /* Reopening an exact new instance at a distant pose pins the activation;
     * it must not sweep through the stale/corrupt prior trigger window. */
    f.gate.equipped_instance_id=f.gate.drawn_instance_id=92;f.player_position.x=50;
    targets_init(t,a,d,212);CHECK(pe_sandbox_melee_begin(&h,f.gate,&energy));ticks=0;
    while(!h.action.contact_open&&ticks++<60)CHECK(pe_sandbox_melee_step(&h,&f,t,6,&energy,&out));
    CHECK(h.history_active&&h.history.previous_a.x==h.history.current.a.x);
    for(unsigned i=0;i<6;i++)CHECK(t[i].hp==212);
    PeCapsuleHistory frozen=h.history;ticks=0;
    while(h.action.contact_open&&ticks++<120){
        frozen=h.history;CHECK(pe_sandbox_melee_step(&h,&f,NULL,0,&energy,&out));
    }
    CHECK(!h.history_active&&!memcmp(&frozen,&h.history,sizeof frozen));
    pe_sandbox_melee_cancel(&h);f.player_position.x=-50; /* Same instance, new window. */
    CHECK(pe_sandbox_melee_begin(&h,f.gate,&energy));ticks=0;
    while(!h.action.contact_open&&ticks++<60)CHECK(pe_sandbox_melee_step(&h,&f,NULL,0,&energy,&out));
    CHECK(h.history_active&&h.history.previous_a.x==h.history.current.a.x);
}
static void playback_gate_tests(PeSandboxMeleeSetup setup){
    PeSandboxMelee h;CHECK(pe_sandbox_melee_init(&h,&setup));
    PeAttackPlaybackSample sample;CHECK(pe_attack_playback_sample(&h.playback,10,33,100,&sample));
    PeSandboxMeleeGate g=gate();g.scaled_time=10;float energy=100;
    const float invalid[]={NAN,INFINITY,-1,9};
    for(unsigned i=0;i<4;i++){
        PeSandboxMelee before=h;PeSandboxMeleeGate bad=g;bad.scaled_time=invalid[i];
        CHECK(!pe_sandbox_melee_begin(&h,bad,&energy)&&!memcmp(&h,&before,sizeof h)&&energy==100);
    }
    CHECK(pe_sandbox_melee_begin(&h,g,&energy));PeSandboxMeleeFrame f=frame();f.gate=g;
    unsigned ticks=0;PeSandboxMeleeUpdate out;
    while(h.action.active&&!h.action.combo_window&&ticks++<200){
        f.gate.scaled_time=(float)(10+ticks*PE_SANDBOX_MELEE_DT);
        CHECK(pe_sandbox_melee_step(&h,&f,NULL,0,&energy,&out));
    }
    CHECK(h.action.combo_window&&!h.action.combo_queued&&energy==95);
    for(unsigned i=0;i<4;i++){
        PeSandboxMelee before=h;PeSandboxMeleeGate bad=f.gate;bad.scaled_time=i==3?f.gate.scaled_time-.01f:invalid[i];
        CHECK(!pe_sandbox_melee_queue(&h,bad)&&!memcmp(&h,&before,sizeof h));
    }
    CHECK(pe_sandbox_melee_queue(&h,f.gate)&&h.action.combo_queued&&energy==95);
}
static void playback_tests(const PeActor *a,const PeHitboxes *w,const PeHitboxes *d){
    PeSandboxMeleeSetup setup={.weapon_item_id=33,.body=a,.drawn_weapon=a,.drawn_capsule=w,
        .gender=PE_COMBAT_FEMALE,.player_id=1,.attack=19,.stamina_reduce_percent=1,
        .history_enabled=true,.playback_enabled=true};
    playback_gate_tests(setup);
    PeSandboxMelee h;CHECK(pe_sandbox_melee_init(&h,&setup));PeSandboxMeleeFrame f=frame();
    f.gate.scaled_time=10;float energy=100;PeKnifeContactTarget t[6];targets_init(t,a,d,212);
    for(unsigned i=0;i<6;i++)t[i].trigger_candidate=t[i].trigger_can_damage=true;
    t[1].eligible=false;t[2].trigger_can_damage=false;t[4].position.x=100;
    CHECK(pe_sandbox_melee_begin(&h,f.gate,&energy));h.action.targets[0]=t[3].entity_id;h.action.target_count=1;
    PeSandboxMeleeUpdate out;unsigned ticks=0;
    while(!h.action.contact_open&&ticks++<100){f.gate.scaled_time+=1.f/60;CHECK(pe_sandbox_melee_step(&h,&f,t,6,&energy,&out));}
    CHECK(h.action.contact_open&&h.effected_count==5&&h.playback.last_contact==f.gate.scaled_time);
    for(unsigned i=0;i<6;i++){
        CHECK(out.trigger_contact[i]==(i!=4));CHECK(out.playback_notified[i]==(i!=2&&i!=4));
        CHECK((out.status[i]==PE_KNIFE_CONTACT_HIT)==(i==0||i==5));
    }
    CHECK(t[1].hp==212&&t[2].hp==212&&t[3].hp==212);
    float stamp=h.playback.last_contact,life=t[0].hp;
    /* Skill-list reset/permission changes do not reset the trigger's effected list. */
    h.action.target_count=0;memset(h.action.targets,0,sizeof h.action.targets);
    t[1].eligible=true;t[2].trigger_can_damage=true;f.gate.scaled_time+=.1f;
    double old_clip=h.action.clip_time,old_wall=h.action.wall_time;
    PeAttackPlayback clock=h.playback;PeAttackPlaybackSample sample;float curve,rate;
    CHECK(pe_attack_playback_sample(&clock,f.gate.scaled_time,33,energy,&sample));
    CHECK(pe_attack_playback_curve(sample.curve_time,&curve)&&pe_attack_playback_speed(sample,curve,&rate));
    CHECK(pe_sandbox_melee_step(&h,&f,t,6,&energy,&out));
    CHECK(h.playback.last_contact==stamp&&t[0].hp==life&&t[1].hp==212&&t[2].hp==212);
    CHECK(fabs(h.action.clip_time-old_clip-rate*PE_SANDBOX_MELEE_DT)<1e-8);
    CHECK(fabs(h.action.wall_time-old_wall-PE_SANDBOX_MELEE_DT)<1e-8);
    for(unsigned i=0;i<6;i++)CHECK(!out.trigger_contact[i]&&!out.playback_notified[i]);
    t[4].position.x=0;f.gate.scaled_time+=1.f/60;
    CHECK(pe_sandbox_melee_step(&h,&f,t,6,&energy,&out));
    CHECK(out.playback_notified[4]&&h.playback.last_contact==f.gate.scaled_time&&h.effected_count==6);
    /* Native capacity failure after the first prospective new contact must
     * roll back the earlier notification, damage, clock and stamina together. */
    h.effected_count=PE_COMBAT_TARGET_CAPACITY-1;
    for(unsigned i=0;i<h.effected_count;i++)h.effected[i]=1000+i;
    h.action.target_count=0;memset(h.action.targets,0,sizeof h.action.targets);
    PeSandboxMelee before=h;PeKnifeContactTarget saved[6];memcpy(saved,t,sizeof t);
    PeSandboxMeleeUpdate saved_out=out;float saved_energy=energy;f.gate.scaled_time+=1.f/60;
    CHECK(!pe_sandbox_melee_step(&h,&f,t,6,&energy,&out));
    CHECK(!memcmp(&h,&before,sizeof h)&&!memcmp(t,saved,sizeof t)&&
        !memcmp(&out,&saved_out,sizeof out)&&energy==saved_energy);
    stamp=h.playback.last_contact;pe_sandbox_melee_cancel(&h);
    CHECK(h.playback.last_contact==stamp&&h.effected_count==0);
    f.gate.equipped_instance_id=f.gate.drawn_instance_id=92;
    CHECK(pe_sandbox_melee_begin(&h,f.gate,&energy));
    CHECK(pe_sandbox_melee_step(&h,&f,NULL,0,&energy,&out)&&h.playback.last_contact==stamp);
    CHECK(h.action.clip_time<pe_combat_animation_speed(energy)*PE_SANDBOX_MELEE_DT);
}
int pe_test_sandbox_melee(void){
    failures=0;char path[]="/tmp/pe-sandbox-melee-XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return failures;close(fd);
    PeActor*a=actor_fixture(path,2,false,false),*wrong=actor_fixture(path,1,false,false),*missing=actor_fixture(path,2,true,false);
    CHECK(a&&wrong&&missing);if(!a||!wrong||!missing)goto done;
    PeHitboxes*w=boxes_fixture(path,a,true),*d=boxes_fixture(path,a,false);CHECK(w&&d);if(!w||!d){pe_hitboxes_free(w);pe_hitboxes_free(d);goto done;}
    PeSandboxMeleeSetup setup={.weapon_item_id=33,.body=a,.drawn_weapon=a,.drawn_capsule=w,.gender=PE_COMBAT_FEMALE,.player_id=1,.attack=19,.stamina_reduce_percent=1};
    weapon_clock_tests(a,w);
    history_tests(a,w,d);
    playback_tests(a,w,d);
    PeSandboxMelee h,before;CHECK(pe_sandbox_melee_init(&h,&setup));before=h;
    PeSandboxMeleeSetup bad=setup;bad.body=wrong;CHECK(!pe_sandbox_melee_init(&h,&bad));CHECK(!memcmp(&h,&before,sizeof h));
    bad=setup;bad.drawn_weapon=missing;CHECK(!pe_sandbox_melee_init(&h,&bad));bad=setup;bad.drawn_capsule=d;CHECK(!pe_sandbox_melee_init(&h,&bad));CHECK(!memcmp(&h,&before,sizeof h));
    float stamina=100;PeSandboxMeleeGate g=gate();
    g.third_person=false;CHECK(!pe_sandbox_melee_begin(&h,g,&stamina));g=gate();g.move_requested=true;CHECK(!pe_sandbox_melee_begin(&h,g,&stamina));
    g=gate();g.velocity.x=.1f;CHECK(!pe_sandbox_melee_begin(&h,g,&stamina));g=gate();g.drawn_instance_id=90;CHECK(!pe_sandbox_melee_begin(&h,g,&stamina));
    g=gate();g.equipped_item_id=10;CHECK(!pe_sandbox_melee_begin(&h,g,&stamina));g=gate();g.grounded=false;CHECK(!pe_sandbox_melee_begin(&h,g,&stamina));
    CHECK(stamina==100&&!memcmp(&h,&before,sizeof h));
    PeKnifeContactTarget t[6];targets_init(t,a,d,212);PeSandboxMeleeFrame f=frame();unsigned deaths=0;
    CHECK(pe_sandbox_melee_begin(&h,gate(),&stamina));CHECK(stamina==95&&h.instance_id==91);
    PeSandboxMeleeUpdate first_update;CHECK(pe_sandbox_melee_step(&h,&f,t,6,&stamina,&first_update));
    CHECK(h.action.update_index==1&&fabs(h.action.clip_time-pe_combat_animation_speed(stamina)*PE_SANDBOX_MELEE_DT)<1e-9);
    CHECK(stamina==95&&!h.action.contact_open);for(unsigned i=0;i<6;i++)CHECK(t[i].hp==212);
    CHECK(run(&h,f,t,&stamina,false,&deaths)==6&&deaths==0&&stamina==95);
    for(unsigned i=0;i<6;i++)CHECK(fabsf(t[i].hp-(212-361.f/48*2))<.0001f);
    /* One debit and one clock for six targets, then stage-local dedup resets. */
    CHECK(pe_sandbox_melee_init(&h,&setup));targets_init(t,a,d,212);stamina=100;CHECK(pe_sandbox_melee_begin(&h,gate(),&stamina));
    CHECK(run(&h,f,t,&stamina,true,&deaths)==24&&deaths==0&&stamina==80&&h.action.stage==3);
    for(unsigned i=0;i<6;i++)CHECK(fabsf(t[i].hp-(212-4*361.f/48*2))<.0001f);
    /* Per-target eligibility/geometry/death, independent of attacker eligibility. */
    targets_init(t,a,d,212);t[1].eligible=false;t[2].position.x=100;t[3].hp=0;
    CHECK(pe_sandbox_melee_begin(&h,gate(),&stamina));CHECK(run(&h,f,t,&stamina,false,&deaths)==3);
    targets_init(t,a,d,212);f.attacker_eligible=false;CHECK(pe_sandbox_melee_begin(&h,gate(),&stamina));CHECK(run(&h,f,t,&stamina,false,&deaths)==0);f=frame();
    /* Current HP belongs to the caller. Repeated swings never repeat a death. */
    targets_init(t,a,d,1);stamina=0;CHECK(pe_sandbox_melee_begin(&h,gate(),&stamina));CHECK(run(&h,f,t,&stamina,true,&deaths)==6&&deaths==6&&stamina==0);
    for(unsigned i=0;i<6;i++)CHECK(t[i].hp==0);
    CHECK(pe_sandbox_melee_begin(&h,gate(),&stamina));CHECK(run(&h,f,t,&stamina,true,&deaths)==0&&deaths==6);
    /* Stop exactly before a contact frame; a bad LAST target must roll back
     * the entire batch, including potential earlier hits and clock advancement. */
    targets_init(t,a,d,212);stamina=100;CHECK(pe_sandbox_melee_begin(&h,gate(),&stamina));
    double first=pe_combat_clip(PE_COMBAT_FEMALE,PE_COMBAT_THIRD_PERSON,0)->events[0].time;
    PeSandboxMeleeUpdate u;
    while(h.action.clip_time+pe_combat_animation_speed(stamina)*PE_SANDBOX_MELEE_DT<first)CHECK(pe_sandbox_melee_step(&h,&f,t,6,&stamina,&u));
    before=h;float saved_stamina=stamina;PeKnifeContactTarget saved_t[6];memcpy(saved_t,t,sizeof t);memset(&u,0x5a,sizeof u);PeSandboxMeleeUpdate saved_u=u;
    t[5].seconds=NAN;PeKnifeContactTarget invalid_t[6];memcpy(invalid_t,t,sizeof t);
    CHECK(!pe_sandbox_melee_step(&h,&f,t,6,&stamina,&u));CHECK(!memcmp(&h,&before,sizeof h)&&stamina==saved_stamina&&!memcmp(&u,&saved_u,sizeof u)&&!memcmp(t,invalid_t,sizeof t));
    memcpy(t,saved_t,sizeof t);t[5].entity_id=t[0].entity_id;CHECK(!pe_sandbox_melee_step(&h,&f,t,6,&stamina,&u));CHECK(!memcmp(&h,&before,sizeof h)&&stamina==saved_stamina);
    memcpy(t,saved_t,sizeof t);t[5].hitboxes=w;CHECK(!pe_sandbox_melee_step(&h,&f,t,6,&stamina,&u));CHECK(!memcmp(&h,&before,sizeof h));
    memcpy(t,saved_t,sizeof t);CHECK(!pe_sandbox_melee_step(&h,&f,t,7,&stamina,&u));CHECK(!pe_sandbox_melee_step(&h,&f,NULL,1,&stamina,&u));
    /* Modal opening, unequip, ownership loss and another Knife33 instance
     * cancel before the very next would-be hit, with no extra stamina debit. */
    for(unsigned mode=0;mode<5;mode++){
        h=before;stamina=saved_stamina;memcpy(t,saved_t,sizeof t);f=frame();
        if(mode==0)f.gate.action_allowed=false;
        if(mode==1)f.gate.equipped_item_id=0;
        if(mode==2)f.gate.weapon_owned=false;
        if(mode==3)f.gate.equipped_instance_id=f.gate.drawn_instance_id=92;
        if(mode==4)f.gate.drawn_instance_id=0;
        CHECK(!pe_sandbox_melee_queue(&h,f.gate));CHECK(pe_sandbox_melee_step(&h,&f,t,6,&stamina,&u));
        CHECK(u.cancelled&&!h.action.active&&!h.action.contact_open&&h.instance_id==0&&stamina==saved_stamina&&!memcmp(t,saved_t,sizeof t));
    }
    h=before;f=frame();CHECK(pe_sandbox_melee_step(&h,&f,NULL,0,&stamina,&u));CHECK(u.target_count==0&&h.action.update_index==before.action.update_index+1);
    pe_sandbox_melee_cancel(&h);CHECK(!h.action.active&&h.instance_id==0);
    pe_hitboxes_free(w);pe_hitboxes_free(d);
 done:pe_actor_free(a);pe_actor_free(wrong);pe_actor_free(missing);running_tests(path);unlink(path);actual_assets();actual_running_assets();return failures;
}
