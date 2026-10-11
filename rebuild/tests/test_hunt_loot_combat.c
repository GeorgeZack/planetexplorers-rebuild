/* Real recovered blade/defence contacts and the actual route collision. Target
 * positions/HP are arranged to isolate transactions, not difficulty parity. */
#include "pe_test_paths.h"
#include "pe_hunt_loot_combat.h"
#include "pe_hunt_loot.h"
#include "pe_settlement.h"
#include "../generated/pe_player_collider_center_source.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    PeGame *game;
    PeSandboxCollision *collision;
    PeActor *hare,*player,*body,*sword;
    PeHitboxes *hare_boxes,*player_boxes,*sword_boxes;
    PeHuntCombat combat;
    PeSandboxMelee blade;
    int weapon_slot,shield_slot,weapon_item;
} Fixture;
static int slot_for(const PeGame *g,int item){for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==item)return i;return -1;}
static PeHuntCombatPlayer player(Fixture *f){
    return (PeHuntCombatPlayer){.position=f->game->player,.radius=.5f,.height=1.8f,
        .max_hp=f->game->max_hp,.alive=f->game->hp>0,.grounded=true,
        .clip=pe_actor_find_clip(f->player,"NormalIdle"),.base_attack=10,
        .weapon=f->weapon_item==1?&f->game->inventory.slots[f->weapon_slot]:NULL,
        .shield=&f->game->inventory.slots[f->shield_slot],.outfit=&f->game->outfit};
}
static PeSandboxMeleeFrame attack_frame(Fixture *f){
    uint32_t id=f->game->inventory.slots[f->weapon_slot].instance_id;
    return (PeSandboxMeleeFrame){.gate={.alive=true,.weapon_owned=true,.grounded=true,
        .third_person=true,.action_allowed=true,.equipped_item_id=f->weapon_item,
        .equipped_instance_id=id,.drawn_instance_id=id},.player_position=f->game->player,
        .player_scale=1,.attacker_eligible=true};
}
static void load_fixture(Fixture *f,int weapon_item){
    memset(f,0,sizeof *f);f->weapon_item=weapon_item;char error[256];f->game=calloc(1,sizeof *f->game);assert(f->game);
    assert(pe_game_init_mode(f->game,1,PE_GAME_ADVENTURE));
    assert(pe_settlement_start(f->game,error,sizeof error));
    f->collision=pe_sandbox_collision_create();assert(f->collision);
    assert(pe_game_collision_update(f->game,f->collision,error,sizeof error));
    f->hare=pe_actor_load(pe_test_path("rebuild/build/reference-assets/hare.peac"),error,sizeof error);
    f->player=pe_actor_load(pe_test_path("rebuild/build/reference-assets/player-defence/player-defence.peac"),error,sizeof error);
    f->body=pe_actor_load(weapon_item==1?pe_test_path("rebuild/build/starter-pair-unified-03/player/player-part-0-material-0.peac"):
        pe_test_path("rebuild/build/reference-assets/player/player-part-0-material-0.peac"),error,sizeof error);
    f->sword=pe_actor_load(weapon_item==1?pe_test_path("rebuild/build/starter-pair-unified-03/player-wood-sword-drawn/player-wood-sword-drawn-part-0-material-0.peac"):
        pe_test_path("rebuild/build/reference-assets/player-knife-drawn/knife-drawn-part-0-material-0.peac"),error,sizeof error);
    if(!f->hare||!f->player||!f->body||!f->sword){fprintf(stderr,"Required loot combat assets: %s\n",error);abort();}
    f->hare_boxes=pe_hitboxes_load(pe_test_path("rebuild/build/reference-assets/hare.pehb"),f->hare,error,sizeof error);
    f->player_boxes=pe_hitboxes_load(pe_test_path("rebuild/build/reference-assets/player-defence/player-defence.pehb"),f->player,error,sizeof error);
    f->sword_boxes=pe_hitboxes_load(weapon_item==1?pe_test_path("rebuild/build/starter-pair-unified-03/player-wood-sword-drawn/wood-sword-drawn.pehb"):
        pe_test_path("rebuild/build/reference-assets/player-knife-drawn/knife-drawn.pehb"),f->sword,error,sizeof error);
    assert(f->hare_boxes&&f->player_boxes&&f->sword_boxes);
    assert(pe_hunt_combat_prepare(&f->combat,&f->game->world,f->collision,f->hare,f->hare_boxes,f->player,f->player_boxes,0,error,sizeof error));
    assert(pe_inventory_add(&f->game->inventory,weapon_item,1)&&pe_inventory_add(&f->game->inventory,34,1));
    f->weapon_slot=slot_for(f->game,weapon_item);f->shield_slot=slot_for(f->game,34);assert(f->weapon_slot>=0&&f->shield_slot>=0);
    assert(pe_game_equip_slot(f->game,f->weapon_slot));f->game->shield_instance_id=f->game->inventory.slots[f->shield_slot].instance_id;
    f->game->max_hp=f->game->hp=5000;f->game->stamina=f->game->stamina_max=100;
    PeSandboxMeleeSetup setup={.history_enabled=true,
        .history_local_center={pe_player_collider_center_source[0].local_center[0],pe_player_collider_center_source[0].local_center[1],pe_player_collider_center_source[0].local_center[2]},
        .weapon_item_id=weapon_item,.body=f->body,.drawn_weapon=f->sword,.drawn_capsule=f->sword_boxes,
        .gender=PE_COMBAT_FEMALE,.player_id=1,.attack=weapon_item==1?20:19,.stamina_reduce_percent=1};
    assert(pe_sandbox_melee_init(&f->blade,&setup));assert(pe_hunt_loot_valid(f->game));
}
static void destroy_fixture(Fixture *f){
    pe_hitboxes_free(f->hare_boxes);pe_hitboxes_free(f->player_boxes);pe_hitboxes_free(f->sword_boxes);
    pe_actor_free(f->hare);pe_actor_free(f->player);pe_actor_free(f->body);pe_actor_free(f->sword);
    pe_sandbox_collision_free(f->collision);pe_game_destroy(f->game);free(f->game);
}
static PeHitboxCapsule first_defence(Fixture *f,const PeHuntEncounterActor *a){
    for(size_t i=0;i<pe_hitboxes_count(f->hare_boxes);i++){
        const PeHitboxDefinition *d=pe_hitboxes_get(f->hare_boxes,i);
        if(d->kind==PE_HITBOX_DEFENCE&&d->joint!=PE_HITBOX_DISABLED){PeHitboxCapsule capsule;
            assert(pe_hitbox_pose(f->hare_boxes,f->hare,i,a->clip,a->seconds,a->motion.position,a->motion.yaw_degrees,a->motion.scale,&capsule));return capsule;}
    }
    abort();
}
static void arrange_deaths(Fixture *f){
    for(unsigned i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++){
        f->combat.encounter.actors[i].motion.position=f->combat.encounter.actors[0].motion.position;
        f->combat.encounter.actors[i].motion.yaw_degrees=0;f->combat.encounter.actors[i].hp=.01f;
    }
    PeHitboxCapsule target=first_defence(f,&f->combat.encounter.actors[0]);
    PeSandboxMeleeFrame frame=attack_frame(f);frame.player_position=(PeVec3){0};
    assert(pe_sandbox_melee_begin(&f->blade,frame.gate,&f->game->stamina));
    PeSandboxMeleeUpdate sample={0};
    for(unsigned i=0;i<80&&!f->blade.action.contact_open;i++)assert(pe_sandbox_melee_step(&f->blade,&frame,NULL,0,&f->game->stamina,&sample));
    assert(f->blade.action.contact_open&&sample.weapon_valid);
    f->game->player=(PeVec3){(target.a.x+target.b.x-sample.weapon.a.x-sample.weapon.b.x)*.5f,
        (target.a.y+target.b.y-sample.weapon.a.y-sample.weapon.b.y)*.5f,
        (target.a.z+target.b.z-sample.weapon.a.z-sample.weapon.b.z)*.5f};
}
static void expect_rollback(Fixture *f,PeHuntCombat combat,PeSandboxMelee blade,PeSandboxMeleeFrame frame){
    PeGame *snapshot=malloc(sizeof *snapshot);assert(snapshot);memcpy(snapshot,f->game,sizeof *snapshot);
    PeHuntCombat before=combat;PeSandboxMelee old_blade=blade;
    PeSandboxMeleeUpdate update;memset(&update,0x5a,sizeof update);PeSandboxMeleeUpdate old_update=update;bool complete=true;
    assert(!pe_hunt_loot_combat_step(f->game,&combat,f->collision,player(f),&blade,&frame,&update,&complete));
    assert(!memcmp(f->game,snapshot,sizeof *snapshot)&&!memcmp(&combat,&before,sizeof combat));
    assert(!memcmp(&blade,&old_blade,sizeof blade)&&!memcmp(&update,&old_update,sizeof update)&&complete);
    free(snapshot);
}
static void deaths_and_failures(Fixture *f){
    arrange_deaths(f);PeSandboxMeleeFrame frame=attack_frame(f);
    unsigned expected_deaths=0;
    /* Knife's narrower capsule need not contact all six differently scaled
     * actors together. Find its first actual fatal interval; Sword's broader
     * arranged contact separately qualifies the six-death completion batch. */
    for(unsigned tick=0;tick<90&&!expected_deaths;tick++){
        PeHuntCombat trial=f->combat;PeSandboxMelee trial_blade=f->blade;
        PeOutfit outfit=f->game->outfit;PeSlot shield=f->game->inventory.slots[f->shield_slot],weapon=f->game->inventory.slots[f->weapon_slot];
        PeHuntCombatPlayer p=player(f);p.outfit=&outfit;p.shield=&shield;if(p.weapon)p.weapon=&weapon;
        float hp=f->game->hp,energy=f->game->stamina;unsigned kills=0;bool complete=false;PeSandboxMeleeUpdate update;
        assert(pe_hunt_combat_step(&trial,&f->game->world,f->collision,p,&trial_blade,&frame,&hp,&energy,&kills,&update,&complete));
        for(unsigned i=0;i<6;i++)expected_deaths+=update.status[i]==PE_KNIFE_CONTACT_HIT&&update.contact[i].died;
        if(!expected_deaths){f->combat=trial;f->blade=trial_blade;}
    }
    assert(expected_deaths>0&&(f->weapon_item!=1||expected_deaths==6));
    PeHuntCombat initial=f->combat;PeSandboxMelee blade=f->blade;
    uint32_t counter=f->game->inventory.next_instance_id;
    uint32_t rng=f->game->rng_state;float wear=f->game->inventory.slots[f->weapon_slot].durability;
    /* Each failure occurs only after actual fatal contacts and sword wear were
     * staged by the real combat core, so wrapper rollback is not a mock claim. */
    for(unsigned i=0;i<PE_HUNT_LOOT_CAPACITY;i++)f->game->hunt_loot.drops[f->game->hunt_loot.count++]=
        (PeHuntLootDrop){f->game->inventory.next_instance_id++,229,22,f->game->player,f->game->game_seconds};
    assert(pe_hunt_loot_valid(f->game));expect_rollback(f,initial,blade,frame);
    pe_hunt_loot_clear(f->game);f->game->inventory.next_instance_id=counter;f->game->rng_state=0;
    expect_rollback(f,initial,blade,frame);f->game->rng_state=rng;
    f->game->inventory.next_instance_id=UINT32_MAX;expect_rollback(f,initial,blade,frame);
    f->game->inventory.next_instance_id=counter;PeSandboxMeleeFrame bad=frame;bad.player_scale=NAN;
    expect_rollback(f,initial,blade,bad);
    for(unsigned previous=0;previous<=3;previous+=3){
        PeHuntCombat combat=initial;PeSandboxMelee weapon=blade;combat.encounter.starting_kills=previous;
        f->game->hunt_kills=previous;f->game->rng_state=rng;f->game->inventory.next_instance_id=counter;
        f->game->inventory.slots[f->weapon_slot].durability=wear;pe_hunt_loot_clear(f->game);
        PeSandboxMeleeUpdate update={0};bool complete=false;
        assert(pe_hunt_loot_combat_step(f->game,&combat,f->collision,player(f),&weapon,&frame,&update,&complete));
        unsigned deaths=0,meat=0;for(unsigned i=0;i<6;i++)deaths+=update.status[i]==PE_KNIFE_CONTACT_HIT&&update.contact[i].died;
        for(unsigned i=0;i<f->game->hunt_loot.count;i++)meat+=f->game->hunt_loot.drops[i].item_id==229;
        unsigned credit=previous+expected_deaths;if(credit>4)credit=4;
        assert(deaths==expected_deaths&&meat==expected_deaths&&complete==(credit==4)&&f->game->hunt_kills==credit);
        assert(combat.encounter.active==(credit<4));
        assert(f->game->inventory.slots[f->weapon_slot].durability==wear-(f->weapon_item==1?5*expected_deaths:0)&&pe_hunt_loot_valid(f->game));
        for(unsigned i=0;i<6;i++)if(update.status[i]==PE_KNIFE_CONTACT_HIT&&update.contact[i].died)assert(combat.encounter.actors[i].hp==0);
        if(!complete)continue;
        PeHuntLootState drops=f->game->hunt_loot;uint32_t after_rng=f->game->rng_state,after_id=f->game->inventory.next_instance_id;
        assert(pe_hunt_loot_combat_step(f->game,&combat,f->collision,player(f),&weapon,&frame,&update,&complete));
        assert(!complete&&f->game->hunt_kills==4&&!memcmp(&drops,&f->game->hunt_loot,sizeof drops));
        assert(f->game->rng_state==after_rng&&f->game->inventory.next_instance_id==after_id);
    }
    printf("hunt loot combat item%d: %u real deaths yield %u loot rolls across fourth-credit cleanup; allocation/RNG/capacity failures roll back whole transaction\n",f->weapon_item,expected_deaths,expected_deaths);fflush(stdout);
}
static void incoming_and_death(Fixture *f){
    PeHuntCombat combat=f->combat;PeSandboxMelee blade=f->blade;
    for(unsigned i=2;i<6;i++)combat.encounter.actors[i].present=false;
    combat.encounter.actors[0].motion.yaw_degrees=0;
    combat.encounter.actors[1].motion.position=combat.encounter.actors[0].motion.position;
    combat.encounter.actors[1].motion.yaw_degrees=0;
    f->game->player=combat.encounter.actors[0].motion.position;f->game->player.z+=.6f;
    for(unsigned i=0;i<30;i++)assert(pe_hunt_combat_sense(&combat,f->collision,player(f)));
    bool found=false;
    for(unsigned tick=0;tick<900&&!found;tick++){
        /* Restore target HP in this arranged timing search so earlier contacts
         * cannot end the fixture before a simultaneous incoming/outgoing hit. */
        combat.encounter.actors[0].hp=combat.encounter.actors[0].motion.max_hp;
        combat.encounter.actors[1].hp=combat.encounter.actors[1].motion.max_hp;
        PeSandboxMeleeFrame frame=attack_frame(f);
        if(!blade.action.active){f->game->stamina=100;assert(pe_sandbox_melee_begin(&blade,frame.gate,&f->game->stamina));}
        PeHuntCombat trial=combat;trial.encounter.actors[1].hp=.01f;PeSandboxMelee trial_blade=blade;
        PeOutfit outfit=f->game->outfit;PeSlot shield=f->game->inventory.slots[f->shield_slot],weapon=f->game->inventory.slots[f->weapon_slot];
        PeHuntCombatPlayer p=player(f);p.outfit=&outfit;p.shield=&shield;p.weapon=&weapon;
        float life=f->game->hp,energy=f->game->stamina;unsigned kills=0;bool complete=false;PeSandboxMeleeUpdate update;
        assert(pe_hunt_combat_step(&trial,&f->game->world,f->collision,p,&trial_blade,&frame,&life,&energy,&kills,&update,&complete));
        if(update.status[1]==PE_KNIFE_CONTACT_HIT&&update.contact[1].died&&life<f->game->hp){
            found=true;combat.encounter.actors[1].hp=.01f;
            assert(kills==1&&!complete&&weapon.durability<f->game->inventory.slots[f->weapon_slot].durability);
            assert(shield.durability<f->game->inventory.slots[f->shield_slot].durability&&outfit.items[0].durability<f->game->outfit.items[0].durability);
            uint32_t rng=f->game->rng_state;f->game->rng_state=0;
            expect_rollback(f,combat,blade,frame);f->game->rng_state=rng;
            assert(pe_hunt_loot_combat_step(f->game,&combat,f->collision,player(f),&blade,&frame,&update,&complete));
            assert(f->game->hp==life&&f->game->stamina==energy&&f->game->hunt_kills==1&&!complete);
            assert(!memcmp(&f->game->outfit,&outfit,sizeof outfit));
            assert(!memcmp(&f->game->inventory.slots[f->shield_slot],&shield,sizeof shield));
            assert(!memcmp(&f->game->inventory.slots[f->weapon_slot],&weapon,sizeof weapon));
            unsigned meat=0;for(unsigned i=0;i<f->game->hunt_loot.count;i++)meat+=f->game->hunt_loot.drops[i].item_id==229;
            assert(meat==1&&pe_hunt_loot_valid(f->game));
        }else{
            /* Baseline has no fatal target: exercise the real wrapper while
             * retaining its generated AI/contact clocks and equipment wear. */
            assert(pe_hunt_loot_combat_step(f->game,&combat,f->collision,player(f),&blade,&frame,&update,&complete));
            assert(!f->game->hunt_kills&&!f->game->hunt_loot.count&&!complete);
        }
    }
    if(!found)fprintf(stderr,"No simultaneous incoming/outgoing loot fixture contact found\n");
    assert(found);
    puts("hunt loot combat: real incoming HP/outfit/shield wear plus outgoing death/sword wear roll back on loot failure and commit together on retry");
}
int pe_test_hunt_loot_combat(void){
    Fixture f;load_fixture(&f,33);deaths_and_failures(&f);destroy_fixture(&f);
    load_fixture(&f,1);deaths_and_failures(&f);destroy_fixture(&f);
    load_fixture(&f,1);incoming_and_death(&f);destroy_fixture(&f);return 0;
}
#ifdef PE_HUNT_LOOT_COMBAT_TEST_MAIN
int main(void){return pe_test_hunt_loot_combat();}
#endif
