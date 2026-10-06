#define _POSIX_C_SOURCE 200809L
#include "pe_hunt_combat.h"
#include "pe_settlement.h"
#include "../generated/pe_starter_outfit_source.h"
#include "../generated/pe_hare_perception_source.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT "."
#endif
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hunt combat:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static PeSandboxMeleeGate gate(void){return (PeSandboxMeleeGate){.alive=true,.weapon_owned=true,.grounded=true,.third_person=true,.action_allowed=true,.equipped_item_id=33,.equipped_instance_id=91,.drawn_instance_id=91};}
static PeSandboxMeleeFrame frame(PeHuntCombatPlayer p){return (PeSandboxMeleeFrame){.gate=gate(),.player_position=p.position,.player_yaw=p.yaw,.player_scale=1,.attacker_eligible=true};}
static unsigned hostiles(const PeHuntCombat*h){unsigned count=0;for(unsigned i=0;i<6;i++)count+=h->hostile[i];return count;}
static PeVec3 point(const PeHare*h,const float p[3]){float s=sinf(h->yaw_degrees*.01745329251994329577f),c=cosf(h->yaw_degrees*.01745329251994329577f);return(PeVec3){h->position.x+h->scale*(c*p[0]+s*p[2]),h->position.y+h->scale*p[1],h->position.z+h->scale*(-s*p[0]+c*p[2])};}
static void sense_tests(const PeHuntCombat *initial,PeWorld*w,PeSandboxCollision*c,const PeSceneCollision*buildings,PeHuntCombatPlayer p){
    PeHuntCombat h=*initial;
    p.position.x+=100;for(unsigned i=0;i<60;i++)CHECK(pe_hunt_combat_sense(&h,c,p));
    CHECK(hostiles(&h)==0);
    h=*initial;p.position=h.encounter.actors[0].motion.position;p.position.z+=.5f;
    for(unsigned i=0;i<29;i++)CHECK(pe_hunt_combat_sense(&h,c,p));
    CHECK(h.sensors[0].heard&&!h.hostile[0]);CHECK(pe_hunt_combat_sense(&h,c,p)&&h.hostile[0]);
    h=*initial;PeHuntEncounterActor*a=&h.encounter.actors[0];float origin[3],axis[3];
    CHECK(pe_actor_pose_joint_point(h.encounter.actor,a->clip,0,PE_HARE_SENSOR_JOINT,pe_hare_sensor_bind_origin,origin));
    CHECK(pe_actor_pose_joint_point(h.encounter.actor,a->clip,0,PE_HARE_SENSOR_JOINT,pe_hare_sensor_bind_axis_point,axis));
    PeVec3 o=point(&a->motion,origin),v=point(&a->motion,axis);
    p.position=(PeVec3){o.x+3*(v.x-o.x),o.y+3*(v.y-o.y)-p.height*.5f,o.z+3*(v.z-o.z)};
    for(unsigned i=0;i<30;i++)CHECK(pe_hunt_combat_sense(&h,c,p));
    CHECK(h.sensors[0].visible&&h.hostile[0]);
    PeVec3 center={p.position.x,p.position.y+p.height*.5f,p.position.z},mid={(o.x+center.x)/2,(o.y+center.y)/2,(o.z+center.z)/2};
    PeSandboxBox wall={{mid.x-.2f,mid.y-.2f,mid.z-.2f},{mid.x+.2f,mid.y+.2f,mid.z+.2f}};char error[256];
    CHECK(pe_sandbox_collision_update_with_static(c,w,NULL,0,&wall,1,NULL,0,buildings,error,sizeof error));
    h=*initial;for(unsigned i=0;i<30;i++)CHECK(pe_hunt_combat_sense(&h,c,p));
    CHECK(!h.sensors[0].visible&&!h.sensors[0].heard&&!h.hostile[0]);
    CHECK(pe_sandbox_collision_update_with_static(c,w,NULL,0,NULL,0,NULL,0,buildings,error,sizeof error));
    h=*initial;h.encounter.actors[5].seconds=NAN;PeHuntCombat before=h;
    CHECK(!pe_hunt_combat_sense(&h,c,p)&&!memcmp(&h,&before,sizeof h));
}
static void blade_blend_override_tests(PeHuntCombat initial,PeWorld*w,PeSandboxCollision*c,PeHuntCombatPlayer p,PeSandboxMelee blade){
    float hp=p.max_hp,stamina=100;unsigned kills=0,active_checks=0,active_hits=0;
    PeHuntCombat h=initial;PeHuntCombatPlayer mixed=p;mixed.blend_count=2;
    mixed.blend[0]=(PeActorBlendSample){pe_actor_find_clip(h.player_actor,"NormalWL"),.7,.7f};
    mixed.blend[1]=(PeActorBlendSample){pe_actor_find_clip(h.player_actor,"NormalRB"),.3,.3f};
    mixed.clip=SIZE_MAX;mixed.seconds=NAN;
    for(unsigned tick=0;tick<180&&hp>0;tick++){
        if(!blade.action.active)CHECK(pe_sandbox_melee_begin(&blade,gate(),&stamina));
        PeHuntCombat other=h;PeSandboxMelee other_blade=blade;float other_hp=hp,other_stamina=stamina;unsigned other_kills=kills;
        PeSandboxMeleeFrame f=frame(p);f.attacker_eligible=false;
        PeSandboxMeleeUpdate a={0},b={0};bool ca=false,cb=false;uint64_t hits=h.player_hits;
        CHECK(pe_hunt_combat_step(&h,w,c,p,&blade,&f,&hp,&stamina,&kills,&a,&ca));
        CHECK(pe_hunt_combat_step(&other,w,c,mixed,&other_blade,&f,&other_hp,&other_stamina,&other_kills,&b,&cb));
        if(blade.action.active){
            active_checks++;active_hits+=(unsigned)(h.player_hits-hits);
            CHECK(!memcmp(&h,&other,sizeof h)&&!memcmp(&blade,&other_blade,sizeof blade));
            CHECK(hp==other_hp&&stamina==other_stamina&&kills==other_kills&&ca==cb&&!memcmp(&a,&b,sizeof a));
        }
    }
    CHECK(active_checks>100&&active_hits>0);
}
/* Same real posed forearm contacts against base defense, intact clothing and
 * clothes that break on the first hit. Geometry is arranged, not survival AI. */
static void outfit_contact_tests(const PeHuntCombat *initial,PeWorld *w,PeSandboxCollision *c,PeHuntCombatPlayer p,PeSandboxMelee blade){
    PeHuntCombat bare=*initial;for(unsigned i=1;i<6;i++)bare.encounter.actors[i].present=false;
    bare.encounter.actors[0].motion.yaw_degrees=0;
    p.position=bare.encounter.actors[0].motion.position;p.position.z+=.6f;
    for(unsigned i=0;i<30;i++)CHECK(pe_hunt_combat_sense(&bare,c,p));
    PeOutfit intact={0};
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)intact.items[i]=(PeWornItem){pe_starter_outfit_sources[i].item_id,101+i,pe_starter_outfit_sources[i].durability};
    PeOutfit pristine=intact,fragile=intact;
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)fragile.items[i].durability=.01f;
    PeHuntCombat armored=bare,breaking=bare;PeSandboxMelee ab=blade,bb=blade;
    PeHuntCombatPlayer ap=p,bp=p;ap.outfit=&intact;bp.outfit=&fragile;
    float hp=p.max_hp,ahp=hp,bhp=hp,energy=100,ae=100,be=100;
    unsigned kills=0,ak=0,bk=0;PeSandboxMeleeUpdate update={0};bool completed=false;
    unsigned hits=0;
    for(unsigned tick=0;tick<360&&hits<2;tick++){
        float old=hp,aold=ahp,bold=bhp;uint64_t previous=bare.player_hits;
        PeHuntCombat before=armored;PeSandboxMelee before_blade=ab;
        PeSandboxMeleeFrame f=frame(p);
        CHECK(pe_hunt_combat_step(&bare,w,c,p,&blade,&f,&hp,&energy,&kills,&update,&completed));
        CHECK(pe_hunt_combat_step(&armored,w,c,ap,&ab,&f,&ahp,&ae,&ak,&update,&completed));
        CHECK(pe_hunt_combat_step(&breaking,w,c,bp,&bb,&f,&bhp,&be,&bk,&update,&completed));
        CHECK(bare.player_hits==armored.player_hits&&bare.player_hits==breaking.player_hits);
        if(bare.player_hits==previous)continue;
        CHECK(bare.player_hits==previous+1);hits++;
        float naked=old-hp,protected_hit=aold-ahp;
        float attack=bare.encounter.actors[0].motion.attack;
        CHECK(naked>0&&protected_hit>0&&protected_hit<naked);
        CHECK(fabsf(protected_hit-naked*attack/(attack+12))<.002f);
        CHECK(fabsf((bold-bhp)-(hits==1?protected_hit:naked))<.002f);
        CHECK(pe_outfit_defense(&fragile)==0);
        if(hits==1){
            /* Wear follows calculated damage even when only.01HP remains. */
            PeOutfit overkill=pristine;PeHuntCombatPlayer op=p;op.outfit=&overkill;
            float life=.01f,stamina=100;unsigned credit=0;
            CHECK(pe_hunt_combat_step(&before,w,c,op,&before_blade,&f,&life,&stamina,&credit,&update,&completed));
            CHECK(life==0&&pe_outfit_defense(&overkill)==12);
            for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)CHECK(fabsf(overkill.items[i].durability-intact.items[i].durability)<.001f);
        }
    }
    CHECK(hits==2&&pe_outfit_defense(&intact)==12);
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)CHECK(fabsf(intact.items[i].durability-(pristine.items[i].durability-(p.max_hp-ahp)*.1f))<.001f);
    PeHuntCombat saved=armored;PeOutfit saved_outfit=intact;float oldhp=ahp;
    PeSandboxMeleeFrame bad=frame(ap);bad.player_scale=NAN;
    CHECK(!pe_hunt_combat_step(&armored,w,c,ap,&ab,&bad,&ahp,&ae,&ak,&update,&completed));
    CHECK(!memcmp(&saved,&armored,sizeof saved)&&!memcmp(&saved_outfit,&intact,sizeof intact)&&ahp==oldhp);
    fprintf(stderr,"hunt outfit: source protection, per-hit breakage, overkill wear and rollback passed\n");
}

/* Replays a real sensor/clock-derived forearm contact with arranged geometry.
 * Shield pose qualification is a host responsibility; these tests supply its
 * transient block request and retain the real recovered player defence pose. */
static void shield_contact_tests(const PeHuntCombat *initial,PeWorld *w,PeSandboxCollision *c,PeHuntCombatPlayer p,PeSandboxMelee blade){
    PeHuntCombat h=*initial;for(unsigned i=1;i<6;i++)h.encounter.actors[i].present=false;
    h.encounter.actors[0].motion.yaw_degrees=0;p.position=h.encounter.actors[0].motion.position;p.position.z+=.6f;
    for(unsigned i=0;i<30;i++)CHECK(pe_hunt_combat_sense(&h,c,p));
    PeHuntCombat checkpoint=h;PeSandboxMelee checkpoint_blade=blade;
    float hp=p.max_hp,energy=100,naked=0;unsigned kills=0;
    PeSandboxMeleeFrame f=frame(p);PeSandboxMeleeUpdate update={0};bool complete=false;
    for(unsigned tick=0;tick<360&&naked==0;tick++){
        checkpoint=h;checkpoint_blade=blade;float old=hp;
        CHECK(pe_hunt_combat_step(&h,w,c,p,&blade,&f,&hp,&energy,&kills,&update,&complete));
        naked=old-hp;
    }
    CHECK(naked>0&&h.player_hits==checkpoint.player_hits+1);if(naked<=0)return;
    float attack=h.encounter.actors[0].motion.attack,loss[5]={0};
    PeSlot full={.item_id=34,.count=1,.instance_id=201,.durability=4312};
    /* Passive9, active9*.78, depleted0, absent0, outfit12+shield9. */
    for(unsigned variant=0;variant<5;variant++){
        PeHuntCombat trial=checkpoint;PeSandboxMelee tb=checkpoint_blade;PeSlot shield=full;
        PeOutfit outfit={0};PeHuntCombatPlayer tp=p;tp.shield=variant==3?NULL:&shield;tp.shield_block=variant!=0;
        if(variant==2)shield.durability=0;
        if(variant==4){for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)outfit.items[i]=(PeWornItem){pe_starter_outfit_sources[i].item_id,101+i,pe_starter_outfit_sources[i].durability};tp.outfit=&outfit;}
        float life=p.max_hp,stamina=100;unsigned credit=0;
        CHECK(pe_hunt_combat_step(&trial,w,c,tp,&tb,&f,&life,&stamina,&credit,&update,&complete));
        CHECK(trial.player_hits==checkpoint.player_hits+1);loss[variant]=p.max_hp-life;
        float defense=variant==2||variant==3?0:variant==4?21:9;
        float expected=naked*attack/(attack+defense)*(variant==1||variant==4?.78f:1);
        CHECK(fabsf(loss[variant]-expected)<.002f);
        if(variant!=2&&variant!=3)CHECK(fabsf(shield.durability-(4312-loss[variant]*.1f))<.001f);
        if(variant==4)for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)CHECK(fabsf(outfit.items[i].durability-(pe_starter_outfit_sources[i].durability-loss[variant]*.1f))<.001f);
        if(variant==1){
            PeHuntCombat lethal=checkpoint;PeSandboxMelee lb=checkpoint_blade;PeSlot overkill=full;tp.shield=&overkill;
            life=.01f;stamina=100;credit=0;
            CHECK(pe_hunt_combat_step(&lethal,w,c,tp,&lb,&f,&life,&stamina,&credit,&update,&complete));
            CHECK(life==0&&fabsf(overkill.durability-shield.durability)<.001f);
        }
    }
    CHECK(loss[1]<loss[0]&&loss[0]<loss[2]&&loss[2]==loss[3]&&loss[4]<loss[1]);
    /* Two actual posed actors at the same supported location and clock, with
     * distinct source identities. First damage breaks shield; second must use
     * zero defence/protection within this very same transaction. */
    PeHuntCombat pair=checkpoint;uint64_t second_id=pair.encounter.actors[1].entity_id;
    pair.encounter.actors[1]=pair.encounter.actors[0];pair.encounter.actors[1].entity_id=second_id;
    pair.attacks[1]=pair.attacks[0];pair.skills[1]=pair.skills[0];pair.attack_seconds[1]=pair.attack_seconds[0];pair.hostile[1]=true;
    PeHuntCombat pair_before=pair;PeSlot fragile=full;fragile.durability=.01f;
    PeHuntCombatPlayer tp=p;tp.shield=&fragile;tp.shield_block=true;
    PeSandboxMelee tb=checkpoint_blade;float life=p.max_hp,stamina=100;unsigned credit=0;
    CHECK(pe_hunt_combat_step(&pair,w,c,tp,&tb,&f,&life,&stamina,&credit,&update,&complete));
    CHECK(pair.player_hits==pair_before.player_hits+2&&fragile.durability==0);
    CHECK(fabsf((p.max_hp-life)-(loss[1]+naked))<.002f);
    /* Invalid second incoming contact is deliberately late: its clock and
     * geometry remain valid, but its guide dedupe is malformed. First actor's
     * real contact would already have staged shield/outfit wear and HP loss. */
    pair=pair_before;pair.attacks[1].target_count[0]=PE_HARE_ATTACK_TARGET_CAPACITY+1;
    PeHuntCombat invalid_before=pair;PeOutfit outfit={0};
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)outfit.items[i]=(PeWornItem){pe_starter_outfit_sources[i].item_id,101+i,pe_starter_outfit_sources[i].durability};
    PeOutfit outfit_before=outfit;fragile=full;tp.outfit=&outfit;tb=checkpoint_blade;
    life=p.max_hp;stamina=100;credit=0;complete=true;memset(&update,0x5a,sizeof update);PeSandboxMeleeUpdate old_update=update;
    CHECK(!pe_hunt_combat_step(&pair,w,c,tp,&tb,&f,&life,&stamina,&credit,&update,&complete));
    CHECK(!memcmp(&pair,&invalid_before,sizeof pair)&&!memcmp(&tb,&checkpoint_blade,sizeof tb));
    CHECK(!memcmp(&fragile,&full,sizeof full)&&!memcmp(&outfit,&outfit_before,sizeof outfit));
    CHECK(life==p.max_hp&&stamina==100&&credit==0&&complete&&!memcmp(&update,&old_update,sizeof update));
    /* Exact-slot validation precedes both display sensor and fixed ticks. */
    for(unsigned bad=0;bad<5;bad++){
        fragile=full;if(bad==0)fragile.item_id=33;if(bad==1)fragile.count=2;if(bad==2)fragile.instance_id=0;
        if(bad==3)fragile.durability=NAN;
        if(bad==4)fragile.durability=4313;
        CHECK(!pe_hunt_combat_sense(&pair,c,tp)&&!memcmp(&pair,&invalid_before,sizeof pair));
        CHECK(!pe_hunt_combat_step(&pair,w,c,tp,&tb,&f,&life,&stamina,&credit,&update,&complete));
        CHECK(!memcmp(&pair,&invalid_before,sizeof pair)&&life==p.max_hp);
    }
    /* Existing active attacks and their ending tick suppress stale block.
     * Use natural source action advancement, not fabricated clip/event clocks. */
    PeSandboxMelee ending=checkpoint_blade;float se=100;
    CHECK(pe_sandbox_melee_begin(&ending,gate(),&se));bool found_end=false;
    for(unsigned tick=0;tick<120;tick++){
        PeSandboxMelee next=ending;PeSandboxMeleeUpdate u;float ne=se;
        CHECK(pe_sandbox_melee_step(&next,&f,NULL,0,&ne,&u));
        if(!next.action.active){found_end=true;break;}ending=next;se=ne;
    }
    CHECK(found_end&&ending.action.active);
    for(unsigned split=0;split<2;split++)for(unsigned block=0;block<2;block++){
        PeHuntCombat trial=checkpoint;PeSandboxMelee eb=ending;PeSlot shield=full;
        PeHuntCombatPlayer ep=p;ep.shield=&shield;ep.shield_block=block;
        life=p.max_hp;stamina=100;credit=0;
        if(split){CHECK(pe_sandbox_melee_advance(&eb,f.gate,&stamina));CHECK(!eb.action.active&&eb.tick_pending&&eb.tick_interval.valid);}
        CHECK(pe_hunt_combat_step(&trial,w,c,ep,&eb,&f,&life,&stamina,&credit,&update,&complete));
        CHECK(!eb.action.active&&trial.player_hits==checkpoint.player_hits+1&&fabsf((p.max_hp-life)-loss[0])<.002f);
    }
    h=checkpoint;blade=checkpoint_blade;hp=p.max_hp;energy=100;kills=0;unsigned active_hits=0;
    PeSlot passive=full;PeHuntCombatPlayer passive_player=p;passive_player.shield=&passive;
    for(unsigned tick=0;tick<180;tick++){
        if(!blade.action.active)CHECK(pe_sandbox_melee_begin(&blade,gate(),&energy));
        PeHuntCombat other=h;PeSandboxMelee ob=blade;PeSlot blocked=passive;
        PeHuntCombatPlayer blocked_player=passive_player;blocked_player.shield=&blocked;blocked_player.shield_block=true;
        float ohp=hp,oe=energy;unsigned ok=kills;PeSandboxMeleeUpdate ou;bool oc=false;uint64_t hits=h.player_hits;
        PeSandboxMeleeFrame no_outgoing=f;no_outgoing.attacker_eligible=false;
        CHECK(pe_hunt_combat_step(&h,w,c,passive_player,&blade,&no_outgoing,&hp,&energy,&kills,&update,&complete));
        CHECK(pe_hunt_combat_step(&other,w,c,blocked_player,&ob,&no_outgoing,&ohp,&oe,&ok,&ou,&oc));
        CHECK(hp==ohp&&!memcmp(&passive,&blocked,sizeof passive)&&!memcmp(&h,&other,sizeof h));
        if(blade.action.active)active_hits+=(unsigned)(h.player_hits-hits);
    }
    CHECK(active_hits>0);
    fprintf(stderr,"hunt shield: real posed passive/block/broken contacts, same-tick breakage, overkill, active/ending blade and late rollback passed\n");
}

static void retaliation_tests(const PeHuntCombat*initial,PeWorld*w,PeSandboxCollision*c,PeHuntCombatPlayer p,PeSandboxMelee blade){
    PeHuntCombat h=*initial;for(unsigned i=1;i<6;i++)h.encounter.actors[i].present=false;
    h.encounter.actors[0].motion.yaw_degrees=0;p.position=h.encounter.actors[0].motion.position;p.position.z+=.6f;
    for(unsigned i=0;i<30;i++)CHECK(pe_hunt_combat_sense(&h,c,p));
    CHECK(h.hostile[0]);
    float hp=p.max_hp,stamina=100;unsigned kills=0;PeSandboxMeleeUpdate update={0};bool completed=false;unsigned ticks=0;
    while(h.player_hits<2&&ticks++<360){PeSandboxMeleeFrame f=frame(p);CHECK(pe_hunt_combat_step(&h,w,c,p,&blade,&f,&hp,&stamina,&kills,&update,&completed));}
    CHECK(h.player_hits>=2&&hp<p.max_hp&&hp>0&&kills==0&&!completed);
    fprintf(stderr,"hunt combat: real forearm contacts %llu, player HP %.3f after %u ticks\n",(unsigned long long)h.player_hits,hp,ticks);
    blade_blend_override_tests(h,w,c,p,blade);
    /* Malformed late blade input rolls back already advanced AI, stamina/HP,
     * dedupe, progress and caller outputs as one controller transaction. */
    PeHuntCombat saved=h;PeSandboxMelee saved_blade=blade;PeSandboxMeleeUpdate saved_update=update;float oldhp=hp,oldstamina=stamina;unsigned oldkills=kills;bool oldcomplete=completed;
    PeSandboxMeleeFrame bad=frame(p);bad.player_scale=NAN;
    CHECK(!pe_hunt_combat_step(&h,w,c,p,&blade,&bad,&hp,&stamina,&kills,&update,&completed));
    CHECK(!memcmp(&h,&saved,sizeof h)&&!memcmp(&blade,&saved_blade,sizeof blade)&&!memcmp(&update,&saved_update,sizeof update));
    CHECK(hp==oldhp&&stamina==oldstamina&&kills==oldkills&&completed==oldcomplete);
    /* Blend metadata is validated before sensor or fixed-tick mutation, even
     * for a zero-weight entry. Caller outputs and dedupe all stay untouched. */
    for(unsigned kind=0;kind<6;kind++){
        PeHuntCombatPlayer bad_player=p;bad_player.blend_count=1;
        bad_player.blend[0]=(PeActorBlendSample){p.clip,p.seconds,1};
        if(kind==0)bad_player.blend_count=PE_ACTOR_BLEND_MAX+1;
        if(kind==1)bad_player.blend[0].weight=NAN;
        if(kind==2)bad_player.blend[0].weight=-1;
        if(kind==3)bad_player.blend[0].weight=0;
        if(kind==4)bad_player.blend[0].seconds=NAN;
        if(kind==5){bad_player.blend_count=2;bad_player.blend[1]=(PeActorBlendSample){pe_actor_stats(h.player_actor).clips,0,0};}
        CHECK(!pe_hunt_combat_sense(&h,c,bad_player)&&!memcmp(&h,&saved,sizeof h));
        PeSandboxMeleeFrame live=frame(p);
        CHECK(!pe_hunt_combat_step(&h,w,c,bad_player,&blade,&live,&hp,&stamina,&kills,&update,&completed));
        CHECK(!memcmp(&h,&saved,sizeof h)&&!memcmp(&blade,&saved_blade,sizeof blade)&&!memcmp(&update,&saved_update,sizeof update));
        CHECK(hp==oldhp&&stamina==oldstamina&&kills==oldkills&&completed==oldcomplete);
    }
    /* Incoming contacts during the next death run use real two-clip defence,
     * with no valid legacy clip available as an accidental fallback. */
    p.blend_count=2;p.blend[0]=(PeActorBlendSample){p.clip,0,.4f};
    p.blend[1]=(PeActorBlendSample){pe_actor_find_clip(h.player_actor,"NormalWL"),.25,.6f};
    p.clip=SIZE_MAX;p.seconds=NAN;
    /* A later real forearm hit, not a fabricated death notification, kills
     * this explicitly low-current-HP fixture and cancels clocks immediately. */
    hp=.01f;unsigned death_ticks=0;
    while(hp>0&&death_ticks++<240){PeSandboxMeleeFrame live=frame(p);CHECK(pe_hunt_combat_step(&h,w,c,p,&blade,&live,&hp,&stamina,&kills,&update,&completed));}
    CHECK(hp==0&&!hostiles(&h)&&!blade.action.active);
    for(unsigned i=0;i<6;i++)CHECK(!h.attacks[i].active&&!h.skills[i].active&&!h.skills[i].contact_enabled);
    /* A stale UI/action gate cannot override authoritative zero HP. */
    p.alive=false;PeSandboxMeleeFrame f=frame(p);
    CHECK(f.gate.alive&&pe_sandbox_melee_begin(&blade,f.gate,&stamina));
    CHECK(pe_hunt_combat_step(&h,w,c,p,&blade,&f,&hp,&stamina,&kills,&update,&completed));
    CHECK(!hostiles(&h)&&!blade.action.active&&hp==0&&update.cancelled);
    for(unsigned i=0;i<6;i++)CHECK(!h.attacks[i].active&&!h.skills[i].active&&!h.skills[i].contact_enabled);
    uint64_t hits=h.player_hits;
    for(unsigned i=0;i<60;i++)CHECK(pe_hunt_combat_step(&h,w,c,p,&blade,&f,&hp,&stamina,&kills,&update,&completed));
    CHECK(h.player_hits==hits&&hp==0);
}
static void outgoing_tests(const PeHuntCombat*initial,PeWorld*w,PeSandboxCollision*c,PeHuntCombatPlayer p,PeSandboxMelee blade){
    PeHuntCombat h=*initial;
    /* Explicit test arrangement, not runtime spawning: six real authored
     * targets share supported placement and low currentHP, preserving IDs. */
    for(unsigned i=0;i<6;i++){
        h.encounter.actors[i].motion.position=h.encounter.actors[0].motion.position;
        h.encounter.actors[i].motion.yaw_degrees=0;h.encounter.actors[i].hp=.01f;
    }
    PeHuntEncounterActor*a=&h.encounter.actors[0];PeHitboxCapsule target={0};bool found=false;
    for(size_t i=0;i<pe_hitboxes_count(h.encounter.defence);i++){
        const PeHitboxDefinition*d=pe_hitboxes_get(h.encounter.defence,i);
        if(d->kind==PE_HITBOX_DEFENCE&&d->joint!=PE_HITBOX_DISABLED){CHECK(pe_hitbox_pose(h.encounter.defence,h.encounter.actor,i,a->clip,0,a->motion.position,0,a->motion.scale,&target));found=true;break;}}
    CHECK(found);PeSandboxMelee probe=blade;float energy=100;CHECK(pe_sandbox_melee_begin(&probe,gate(),&energy));
    PeSandboxMeleeFrame f=frame(p);f.player_position=(PeVec3){0};PeSandboxMeleeUpdate sample={0};
    for(unsigned i=0;i<40&&!sample.weapon_valid;i++)CHECK(pe_sandbox_melee_step(&probe,&f,NULL,0,&energy,&sample));
    CHECK(sample.weapon_valid);
    p.position=(PeVec3){(target.a.x+target.b.x-sample.weapon.a.x-sample.weapon.b.x)*.5f,(target.a.y+target.b.y-sample.weapon.a.y-sample.weapon.b.y)*.5f,(target.a.z+target.b.z-sample.weapon.a.z-sample.weapon.b.z)*.5f};
    f=frame(p);float hp=p.max_hp,stamina=100;unsigned kills=0;PeSandboxMeleeUpdate update={0};bool completed=false;
    CHECK(pe_sandbox_melee_begin(&blade,f.gate,&stamina));
    unsigned ticks=0;while(!completed&&ticks++<90){CHECK(pe_hunt_combat_step(&h,w,c,p,&blade,&f,&hp,&stamina,&kills,&update,&completed));}
    CHECK(completed&&kills==4&&!h.encounter.active&&h.hare_hits>=4&&stamina==95);
    for(unsigned i=0;i<6;i++)CHECK(!h.encounter.actors[i].present&&!h.attacks[i].active&&!h.skills[i].contact_enabled);
    CHECK(pe_hunt_combat_step(&h,w,c,p,&blade,&f,&hp,&stamina,&kills,&update,&completed));CHECK(kills==4&&!completed);
}
/* Actual modern Wood Sword blade/hand poses against recovered hare capsules.
 * All source cohorts are full HP; geometry is explicitly arranged for overlap. */
static void sword_wear_tests(const PeHuntCombat *initial,PeWorld*w,PeSandboxCollision*c,PeHuntCombatPlayer p){
    const char *paths[]={PE_SOURCE_ROOT "/rebuild/build/starter-pair-unified-03/player/player-part-0-material-0.peac",
        PE_SOURCE_ROOT "/rebuild/build/starter-pair-unified-03/player-wood-sword-drawn/player-wood-sword-drawn-part-0-material-0.peac",
        PE_SOURCE_ROOT "/rebuild/build/starter-pair-unified-03/player-wood-sword-drawn/wood-sword-drawn.pehb"};
    for(unsigned i=0;i<3;i++){FILE*f=fopen(paths[i],"rb");if(!f){fprintf(stderr,"hunt sword wear: SKIP staged source Sword fixture %s\n",paths[i]);return;}fclose(f);}
    char error[160];PeActor *body=pe_actor_load(paths[0],error,sizeof error),*sword=pe_actor_load(paths[1],error,sizeof error);
    PeHitboxes *capsule=pe_hitboxes_load(paths[2],sword,error,sizeof error);CHECK(body&&sword&&capsule);if(!body||!sword||!capsule)goto done;
    PeSandboxMelee blade={0};PeSandboxMeleeSetup setup={.weapon_item_id=1,.body=body,.drawn_weapon=sword,.drawn_capsule=capsule,
        .gender=PE_COMBAT_FEMALE,.player_id=1,.attack=999,.stamina_reduce_percent=1};
    CHECK(pe_sandbox_melee_init(&blade,&setup));PeSandboxMeleeFrame f=frame(p);f.gate.equipped_item_id=1;
    float stamina=100;CHECK(pe_sandbox_melee_begin(&blade,f.gate,&stamina));
    f.player_position=(PeVec3){0};PeSandboxMeleeUpdate sample={0};
    for(unsigned tick=0;tick<80&&!blade.action.contact_open;tick++)CHECK(pe_sandbox_melee_step(&blade,&f,NULL,0,&stamina,&sample));
    CHECK(blade.action.contact_open&&sample.weapon_valid);
    PeHuntCombat arranged=*initial;
    for(unsigned i=0;i<6;i++){arranged.encounter.actors[i].motion.position=arranged.encounter.actors[0].motion.position;
        arranged.encounter.actors[i].motion.yaw_degrees=0;arranged.encounter.actors[i].hp=arranged.encounter.actors[i].motion.max_hp;}
    const PeHuntEncounterActor *a=&arranged.encounter.actors[0];PeHitboxCapsule target={0};bool found=false;
    for(size_t i=0;i<pe_hitboxes_count(arranged.encounter.defence);i++){
        const PeHitboxDefinition*d=pe_hitboxes_get(arranged.encounter.defence,i);
        if(d->kind==PE_HITBOX_DEFENCE&&d->joint!=PE_HITBOX_DISABLED){CHECK(pe_hitbox_pose(arranged.encounter.defence,arranged.encounter.actor,i,a->clip,0,a->motion.position,0,a->motion.scale,&target));found=true;break;}}
    CHECK(found);p.position=(PeVec3){(target.a.x+target.b.x-sample.weapon.a.x-sample.weapon.b.x)*.5f,
        (target.a.y+target.b.y-sample.weapon.a.y-sample.weapon.b.y)*.5f,(target.a.z+target.b.z-sample.weapon.a.z-sample.weapon.b.z)*.5f};
    f.player_position=p.position;p.base_attack=10;
    PeSlot pristine={.item_id=1,.count=1,.instance_id=91,.durability=2369};
    for(unsigned variant=0;variant<3;variant++){
        PeHuntCombat h=arranged;PeSandboxMelee b=blade;PeSlot weapon=pristine;p.weapon=&weapon;
        if(variant==1)weapon.durability=5;
        if(variant==2)weapon.durability=0;
        float initial_wear=weapon.durability,hp=p.max_hp,energy=95;unsigned kills=0;bool complete=false;PeSandboxMeleeUpdate u={0};
        CHECK(pe_hunt_combat_step(&h,w,c,p,&b,&f,&hp,&energy,&kills,&u,&complete));
        unsigned hits=0;
        for(size_t i=0;i<u.target_count;i++)if(u.status[i]==PE_KNIFE_CONTACT_HIT){
            float attack=(variant==0||(variant==1&&hits==0))?20:10,expected;
            CHECK(pe_combat_damage(attack,arranged.encounter.actors[i].motion.defense,false,0,false,u.contact[i].contact_scale,&expected));
            CHECK(fabsf(u.contact[i].damage-expected)<.00001f);hits++;
        }
        CHECK(hits>=2&&kills==0&&!complete&&h.hare_hits==hits);
        CHECK(weapon.durability==fmaxf(0,initial_wear-5*hits));CHECK(b.action.active);
        /* Next tick reuses real guide dedupe: no second wear for same targets. */
        float worn=weapon.durability;uint64_t old_hits=h.hare_hits;
        CHECK(pe_hunt_combat_step(&h,w,c,p,&b,&f,&hp,&energy,&kills,&u,&complete));
        CHECK(h.hare_hits==old_hits&&weapon.durability==worn&&b.action.active);
    }
    /* Misses cost no wear, and a subsequent valid hit can still claim its guide. */
    {
        PeHuntCombat h=arranged;PeSandboxMelee b=blade;PeSlot weapon=pristine;p.weapon=&weapon;
        PeHuntCombatPlayer far=p;far.position.x+=100;PeSandboxMeleeFrame ff=f;ff.player_position=far.position;
        float hp=p.max_hp,energy=95;unsigned kills=0;bool complete=false;PeSandboxMeleeUpdate u={0};
        CHECK(pe_hunt_combat_step(&h,w,c,far,&b,&ff,&hp,&energy,&kills,&u,&complete));
        CHECK(!h.hare_hits&&weapon.durability==2369);
    }
    /* Overkill still dispatches one successful attack notification per target. */
    {
        PeHuntCombat h=arranged;for(unsigned i=1;i<6;i++)h.encounter.actors[i].present=false;h.encounter.actors[0].hp=.01f;
        PeSandboxMelee b=blade;PeSlot weapon=pristine;p.weapon=&weapon;
        float hp=p.max_hp,energy=95;unsigned kills=0;bool complete=false;PeSandboxMeleeUpdate u={0};
        CHECK(pe_hunt_combat_step(&h,w,c,p,&b,&f,&hp,&energy,&kills,&u,&complete));
        CHECK(kills==1&&weapon.durability==2364&&u.contact[0].damage>u.contact[0].applied_damage);
    }
    /* A valid source attack clock with corrupt guide bookkeeping is checked
     * after outgoing hits. It must roll back all staged outgoing sword wear. */
    {
        PeHuntCombat h=arranged;h.clock=2;CHECK(pe_hare_attack_begin(&h.attacks[5],2,true,1,0));CHECK(pe_hare_skill_begin(&h.skills[5],0));
        PeHareAttackEvents events;
        for(unsigned tick=0;tick<24;tick++){
            h.attack_seconds[5]+=PE_HARE_SKILL_DT;CHECK(pe_hare_skill_step(&h.skills[5],0));
            CHECK(pe_hare_attack_update(&h.attacks[5],h.attack_seconds[5]/pe_hare_attack_def(0)->clip_duration,&events));
        }
        CHECK(h.skills[5].contact_enabled&&h.attacks[5].open_windows);
        PeHuntCombat control=h;PeSandboxMelee cb=blade;PeSlot control_weapon=pristine;PeHuntCombatPlayer cp=p;cp.weapon=&control_weapon;
        float chp=p.max_hp,ce=95;unsigned ck=0;bool cc=false;PeSandboxMeleeUpdate cu={0};
        CHECK(pe_hunt_combat_step(&control,w,c,cp,&cb,&f,&chp,&ce,&ck,&cu,&cc));
        CHECK(control.hare_hits>=2&&control_weapon.durability<pristine.durability);
        h.attacks[5].target_count[0]=PE_HARE_ATTACK_TARGET_CAPACITY+1;
        PeHuntCombat old=h;PeSandboxMelee b=blade;PeSlot weapon=pristine;p.weapon=&weapon;
        float hp=p.max_hp,energy=95;unsigned kills=0;bool complete=true;PeSandboxMeleeUpdate u;memset(&u,0x5a,sizeof u);PeSandboxMeleeUpdate before=u;
        CHECK(!pe_hunt_combat_step(&h,w,c,p,&b,&f,&hp,&energy,&kills,&u,&complete));
        CHECK(!memcmp(&h,&old,sizeof h)&&!memcmp(&b,&blade,sizeof b)&&!memcmp(&weapon,&pristine,sizeof weapon));
        CHECK(hp==p.max_hp&&energy==95&&kills==0&&complete&&!memcmp(&u,&before,sizeof u));
    }
    for(unsigned invalid=0;invalid<15;invalid++){
        PeHuntCombat h=arranged;PeSandboxMelee b=blade;PeSlot weapon=pristine,shield={.item_id=34,.count=1,.instance_id=91,.durability=4312};
        PeHuntCombatPlayer bad=p;bad.weapon=&weapon;PeSandboxMeleeFrame ff=f;
        if(invalid==0)weapon.item_id=33;
        if(invalid==1)weapon.durability=NAN;
        if(invalid==2)weapon.durability=2370;
        if(invalid==3)weapon.count=2;
        if(invalid==4)weapon.instance_id=92;
        if(invalid==5)bad.base_attack=NAN;
        if(invalid==6)ff.gate.equipped_item_id=33;
        if(invalid==7)ff.gate.drawn_instance_id=92;
        if(invalid==8)bad.shield=&shield;
        if(invalid==9)b.setup.weapon_item_id=33;
        if(invalid==10)weapon.instance_id=0;
        if(invalid==11)weapon.durability=-1;
        if(invalid==12)bad.base_attack=-1;
        if(invalid==13)b.instance_id=92;
        PeOutfit outfit={0};
        if(invalid==14){
            for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)outfit.items[i]=(PeWornItem){pe_starter_outfit_sources[i].item_id,91+i,pe_starter_outfit_sources[i].durability};
            bad.outfit=&outfit;
        }
        PeSandboxMelee old=b;float hp=p.max_hp,energy=95;unsigned kills=0;bool complete=true;PeSandboxMeleeUpdate u;memset(&u,0x5a,sizeof u);PeSandboxMeleeUpdate before=u;PeSlot old_weapon=weapon;
        CHECK(!pe_hunt_combat_step(&h,w,c,bad,&b,&ff,&hp,&energy,&kills,&u,&complete));
        CHECK(!memcmp(&h,&arranged,sizeof h)&&!memcmp(&b,&old,sizeof b)&&!memcmp(&weapon,&old_weapon,sizeof weapon));
        CHECK(hp==p.max_hp&&energy==95&&kills==0&&complete&&!memcmp(&u,&before,sizeof u));
    }
    fprintf(stderr,"hunt sword wear: real source blade multi-target break/depletion, no miss/dedupe wear, overkill and late rollback passed\n");
 done:pe_hitboxes_free(capsule);pe_actor_free(body);pe_actor_free(sword);
}

static uint32_t read_u32(const unsigned char*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static PeActor *missing_clip_fixture(const char*source,const PeActor*actor,const char*name){
    FILE*f=fopen(source,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long length=ftell(f);
    if(length<24||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    unsigned char*data=malloc((size_t)length);if(!data){fclose(f);return NULL;}
    bool ok=fread(data,1,(size_t)length,f)==(size_t)length;fclose(f);
    PeActorStats stats=pe_actor_stats(actor);size_t offset=24+stats.vertices*56+stats.indices*4+stats.joints*48;bool changed=false;
    for(size_t i=0;ok&&i<stats.clips;i++){
        if(offset+48>(size_t)length){ok=false;break;}
        if(!strcmp((const char*)data+offset,name)){data[offset]='X';changed=true;break;}
        offset+=48+(size_t)read_u32(data+offset+36)*stats.joints*48;
    }
    char path[]=PE_SOURCE_ROOT "/rebuild/build/hunt-missing-clip-XXXXXX";int fd=mkstemp(path);
    PeActor*result=NULL;
    if(fd>=0){f=fdopen(fd,"wb");if(f){ok=ok&&changed&&fwrite(data,1,(size_t)length,f)==(size_t)length;ok=fclose(f)==0&&ok;
            if(ok){char error[256];result=pe_actor_load(path,error,sizeof error);}}
        else close(fd);
        unlink(path);}
    free(data);return result;
}
static void running_pending_tests(const PeHuntCombat *initial,PeWorld *w,PeSandboxCollision *c,PeHuntCombatPlayer p){
    const char *root=getenv("PE_RUNNING_STAGE");if(!root)root=PE_SOURCE_ROOT "/rebuild/build/running-player-20261006";
    const char *names[]={"player/player-part-0-material-0.peac","player-knife-drawn/knife-drawn-part-0-material-0.peac",
        "player-knife-drawn/knife-drawn.pehb","player-defence/player-defence.peac","player-defence/player-defence.pehb"};
    char paths[5][1024],error[256];
    for(unsigned i=0;i<5;i++){
        int n=snprintf(paths[i],sizeof paths[i],"%s/%s",root,names[i]);CHECK(n>0&&(size_t)n<sizeof paths[i]);
        FILE *f=fopen(paths[i],"rb");if(!f){fprintf(stderr,"hunt combat: SKIP optional staged running pose %s\n",paths[i]);return;}fclose(f);
    }
    PeActor *body=pe_actor_load(paths[0],error,sizeof error),*knife=pe_actor_load(paths[1],error,sizeof error),*player=pe_actor_load(paths[3],error,sizeof error);
    PeHitboxes *kb=pe_hitboxes_load(paths[2],knife,error,sizeof error),*pb=pe_hitboxes_load(paths[4],player,error,sizeof error);
    CHECK(body&&knife&&player&&kb&&pb);if(!body||!knife||!player||!kb||!pb)goto done;
    /* Explicit headless opt-in tests dispatch, not root-motion qualification or
     * gameplay admission. Staged source poses remain outside installed assets. */
    PeSandboxMelee blade={0};PeSandboxMeleeSetup setup={.weapon_item_id=33,.body=body,.drawn_weapon=knife,.drawn_capsule=kb,.gender=PE_COMBAT_FEMALE,
        .player_id=1,.attack=19,.stamina_reduce_percent=1,.running_motion_ready=true};
    CHECK(pe_sandbox_melee_init(&blade,&setup));PeHuntCombat h=*initial;
    h.player_actor=player;h.player_defence=pb;for(unsigned i=1;i<6;i++)h.encounter.actors[i].present=false;
    h.encounter.actors[0].motion.yaw_degrees=0;p.position=h.encounter.actors[0].motion.position;p.position.z+=.6f;
    p.clip=pe_actor_find_clip(player,"NormalIdle");for(unsigned i=0;i<30;i++)CHECK(pe_hunt_combat_sense(&h,c,p));
    CHECK(h.hostile[0]);float hp=p.max_hp,stamina=100;unsigned kills=0,active_checks=0,wrong_pose_differences=0;
    PeSandboxMeleeFrame f=frame(p);f.gate.velocity.z=5;f.gate.move_requested=true;f.attacker_eligible=false;
    CHECK(pe_sandbox_melee_begin(&blade,f.gate,&stamina));CHECK(pe_sandbox_melee_advance(&blade,f.gate,&stamina));
    /* Missing variant defence cannot consume a pending tick or partially step AI. */
    PeActor *missing=missing_clip_fixture(paths[3],player,"RunSwordAttack");CHECK(missing);
    PeHuntCombat invalid=h;invalid.player_actor=missing;
    PeHuntCombat saved=invalid;PeSandboxMelee saved_blade=blade;PeSandboxMeleeUpdate untouched;memset(&untouched,0x5a,sizeof untouched);
    PeSandboxMeleeUpdate old_update=untouched;bool complete=true;float old_hp=hp,old_stamina=stamina;
    CHECK(!pe_hunt_combat_step(&invalid,w,c,p,&blade,&f,&hp,&stamina,&kills,&untouched,&complete));
    CHECK(!memcmp(&invalid,&saved,sizeof saved)&&!memcmp(&blade,&saved_blade,sizeof blade)&&!memcmp(&untouched,&old_update,sizeof untouched));
    CHECK(hp==old_hp&&stamina==old_stamina&&kills==0&&complete);
    pe_actor_free(missing);
    for(unsigned tick=0;tick<360&&hp>0;tick++){
        if(!blade.action.active)CHECK(pe_sandbox_melee_begin(&blade,f.gate,&stamina));
        if(!blade.tick_pending)CHECK(pe_sandbox_melee_advance(&blade,f.gate,&stamina));
        double time=blade.action.clip_time;uint64_t updates=blade.action.update_index;float energy=stamina;
        PeHuntCombat other=h,wrong=h;PeSandboxMelee idle=blade,wrong_idle;pe_sandbox_melee_cancel(&idle);wrong_idle=idle;
        PeHuntCombatPlayer explicit_pose=p;explicit_pose.clip=pe_actor_find_clip(player,blade.action.active?pe_combat_pose_name(&blade.action):"NormalIdle");
        explicit_pose.seconds=blade.action.active?time:0;PeHuntCombatPlayer wrong_pose=explicit_pose;
        if(blade.action.active)wrong_pose.clip=pe_actor_find_clip(player,"Attack6");
        float other_hp=hp,other_stamina=stamina,wrong_hp=hp,wrong_stamina=stamina;unsigned other_kills=kills,wrong_kills=kills;
        PeSandboxMeleeUpdate result,other_result,wrong_result;bool other_complete=false,wrong_complete=false;complete=false;
        CHECK(pe_hunt_combat_step(&h,w,c,p,&blade,&f,&hp,&stamina,&kills,&result,&complete));
        CHECK(pe_hunt_combat_step(&other,w,c,explicit_pose,&idle,&f,&other_hp,&other_stamina,&other_kills,&other_result,&other_complete));
        CHECK(pe_hunt_combat_step(&wrong,w,c,wrong_pose,&wrong_idle,&f,&wrong_hp,&wrong_stamina,&wrong_kills,&wrong_result,&wrong_complete));
        CHECK(!blade.tick_pending&&blade.action.clip_time==time&&blade.action.update_index==updates&&stamina==energy);
        CHECK(hp==other_hp&&kills==other_kills&&complete==other_complete&&!memcmp(&h,&other,sizeof h));
        if(blade.action.active){active_checks++;wrong_pose_differences+=(hp!=wrong_hp);}
    }
    CHECK(active_checks>200&&h.player_hits>0&&wrong_pose_differences>0);
 done:pe_hitboxes_free(kb);pe_hitboxes_free(pb);pe_actor_free(body);pe_actor_free(knife);pe_actor_free(player);
}
static void actual_assets(void){
    const char *paths[]={PE_SOURCE_ROOT "/rebuild/build/reference-assets/adventure-route/terrain.pevx",
        PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.peac",PE_SOURCE_ROOT "/rebuild/build/reference-assets/hare.pehb",
        PE_SOURCE_ROOT "/rebuild/build/reference-assets/player-defence/player-defence.peac",PE_SOURCE_ROOT "/rebuild/build/reference-assets/player-defence/player-defence.pehb",
        PE_SOURCE_ROOT "/rebuild/build/animator-bake/player-female/all/player-part-0-material-0.peac",
        PE_SOURCE_ROOT "/rebuild/build/animator-bake/player-female/drawn-all/knife-drawn-part-0-material-0.peac",
        PE_SOURCE_ROOT "/rebuild/build/animator-bake/player-female/drawn-all/knife-drawn.pehb"};
    for(unsigned i=0;i<8;i++){FILE*f=fopen(paths[i],"rb");if(!f){fprintf(stderr,"hunt combat: SKIP optional route/actor fixture %s\n",paths[i]);return;}fclose(f);}
    char error[256];PeWorld*w=calloc(1,sizeof *w);PeSandboxCollision*c=pe_sandbox_collision_create();
    PeActor *hare=pe_actor_load(paths[1],error,sizeof error),*player=pe_actor_load(paths[3],error,sizeof error),*body=pe_actor_load(paths[5],error,sizeof error),*knife=pe_actor_load(paths[6],error,sizeof error);
    PeHitboxes *hb=pe_hitboxes_load(paths[2],hare,error,sizeof error),*pb=pe_hitboxes_load(paths[4],player,error,sizeof error),*kb=pe_hitboxes_load(paths[7],knife,error,sizeof error);
    CHECK(w&&c&&hare&&player&&body&&knife&&hb&&pb&&kb);if(!w||!c||!hare||!player||!body||!knife||!hb||!pb||!kb)goto done;
    CHECK(pe_settlement_load_world(paths[0],w,error,sizeof error));const PeSceneCollision*buildings=pe_settlement_collision_for_world(w,error,sizeof error);CHECK(buildings);
    CHECK(pe_sandbox_collision_update_with_static(c,w,NULL,0,NULL,0,NULL,0,buildings,error,sizeof error));
    PeHuntCombat h={0};bool ready=pe_hunt_combat_prepare(&h,w,c,hare,hb,player,pb,0,error,sizeof error);CHECK(ready);if(!ready)goto done;
    PeHuntCombat saved=h;CHECK(!pe_hunt_combat_prepare(&h,w,c,hare,hb,player,hb,0,error,sizeof error)&&!memcmp(&h,&saved,sizeof h));
    /* A valid actor/PEHB pair can still lack one supported visible pose.
     * Rename only that clip in a disposable fixture, preserving skin identity. */
    const char *required[]={"Felling1","NormalWL","NormalWR","NormalWB","NormalRL","NormalRR","NormalRB"};
    for(unsigned i=0;i<sizeof required/sizeof required[0];i++){
        PeActor *missing=missing_clip_fixture(paths[3],player,required[i]);CHECK(missing);
        if(missing){CHECK(!pe_hunt_combat_prepare(&h,w,c,hare,hb,missing,pb,0,error,sizeof error));
            CHECK(!memcmp(&h,&saved,sizeof h)&&strstr(error,required[i]));pe_actor_free(missing);}
    }
    PeSandboxMelee blade={0};PeSandboxMeleeSetup setup={.weapon_item_id=33,.body=body,.drawn_weapon=knife,.drawn_capsule=kb,.gender=PE_COMBAT_FEMALE,.player_id=1,.attack=19,.stamina_reduce_percent=1};CHECK(pe_sandbox_melee_init(&blade,&setup));
    PeHuntCombatPlayer p={.position=h.encounter.actors[0].motion.position,.radius=.5f,.height=1.8f,.max_hp=5000,.alive=true,.grounded=true,.clip=pe_actor_find_clip(player,"NormalIdle")};
    sword_wear_tests(&h,w,c,p);shield_contact_tests(&h,w,c,p,blade);outfit_contact_tests(&h,w,c,p,blade);sense_tests(&h,w,c,buildings,p);retaliation_tests(&h,w,c,p,blade);outgoing_tests(&h,w,c,p,blade);running_pending_tests(&h,w,c,p);
 done:pe_hitboxes_free(hb);pe_hitboxes_free(pb);pe_hitboxes_free(kb);pe_actor_free(hare);pe_actor_free(player);pe_actor_free(body);pe_actor_free(knife);pe_sandbox_collision_free(c);pe_world_destroy(w);free(w);
}
int pe_test_hunt_combat(void){
    failures=0;fflush(NULL);pid_t pid=fork();CHECK(pid>=0);if(pid<0)return failures;
    if(pid==0){actual_assets();exit(failures?1:0);}
    int status=0;pid_t waited;do{waited=waitpid(pid,&status,0);}while(waited<0&&errno==EINTR);
    CHECK(waited==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);return failures;
}
