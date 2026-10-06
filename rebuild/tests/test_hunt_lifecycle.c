#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_adventure_hunt.h"
#include "pe_adventure_medicine.h"
#include "pe_hunt_encounter.h"
#include "pe_settlement.h"
#include <errno.h>
#include <math.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#ifndef PE_SOURCE_ROOT
#define PE_SOURCE_ROOT ".."
#endif
/* A persistence/transaction fixture, not a gameplay bot: player contact poses
 * are arranged to overlap recovered hare capsules. All hare HP/defence stay at
 * their captured values, and only real knife resolver deaths advance progress.
 * No AI, navigation, earned survival or original partial reload is claimed. */
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hunt lifecycle:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static PeSandboxMeleeGate gate(uint32_t instance){return (PeSandboxMeleeGate){.alive=true,.weapon_owned=true,.grounded=true,
    .third_person=true,.action_allowed=true,.equipped_item_id=33,.equipped_instance_id=instance,.drawn_instance_id=instance};}
static bool swing(PeGame *g,PeHuntEncounter *h,PeSandboxMelee *melee,unsigned target,bool *finished){
    if(target>=6||!h->actors[target].present||h->actors[target].hp<=0)return false;
    const PeSlot *knife=pe_game_equipped_tool(g);if(!knife||knife->item_id!=33)return false;
    PeSandboxMeleeFrame f={.gate=gate(knife->instance_id),.player_scale=1,.attacker_eligible=true};
    PeSandboxMelee probe=*melee;float energy=g->stamina;
    if(!pe_sandbox_melee_begin(&probe,f.gate,&energy))return false;
    PeSandboxMeleeUpdate sample={0};unsigned ticks=0;
    while(!probe.action.contact_open&&ticks++<120)
        if(!pe_sandbox_melee_step(&probe,&f,NULL,0,&energy,&sample))return false;
    if(!probe.action.contact_open||!sample.weapon_valid)return false;
    PeHitboxCapsule capsule;bool found=false;
    PeHuntEncounterActor *a=&h->actors[target];
    for(size_t i=0;i<pe_hitboxes_count(h->defence);i++){
        const PeHitboxDefinition *d=pe_hitboxes_get(h->defence,i);
        if(d->kind!=PE_HITBOX_DEFENCE||d->joint==PE_HITBOX_DISABLED)continue;
        found=pe_hitbox_pose(h->defence,h->actor,i,a->clip,a->seconds,a->motion.position,a->motion.yaw_degrees,a->motion.scale,&capsule);break;
    }
    if(!found)return false;
    f.player_position=(PeVec3){(capsule.a.x+capsule.b.x-sample.weapon.a.x-sample.weapon.b.x)*.5f,
        (capsule.a.y+capsule.b.y-sample.weapon.a.y-sample.weapon.b.y)*.5f,
        (capsule.a.z+capsule.b.z-sample.weapon.a.z-sample.weapon.b.z)*.5f};
    bool eligible[6]={0};eligible[target]=true;float hp=a->hp;
    unsigned kills=g->hunt_kills,hits=0,deaths=0;*finished=false;
    if(!pe_sandbox_melee_begin(melee,f.gate,&g->stamina))return false;
    ticks=0;
    while(melee->action.active&&ticks++<120){
        PeSandboxMeleeUpdate update;bool complete=false;
        if(!pe_hunt_encounter_melee(h,melee,&f,eligible,&g->stamina,&g->hunt_kills,&update,&complete))return false;
        for(size_t i=0;i<update.target_count;i++)if(update.status[i]==PE_KNIFE_CONTACT_HIT){
            CHECK(i==target);hits++;deaths+=update.contact[i].died;
            CHECK(update.contact[i].damage>0&&update.contact[i].applied_damage>0);
        }
        if(complete){*finished=true;g->hunt_state=PE_HUNT_RETURN_PENDING;}
    }
    CHECK(!melee->action.active&&ticks<120&&hits==1&&h->actors[target].hp<hp);
    CHECK(g->hunt_kills==kills+deaths&&deaths<=1);
    return hits==1&&!melee->action.active;
}
static bool defeat(PeGame *g,PeHuntEncounter *h,PeSandboxMelee *melee,unsigned target){
    unsigned count=g->hunt_kills,swings=0;bool complete=false;float hp=h->actors[target].hp;
    while(h->actors[target].hp>0&&swings++<100)if(!swing(g,h,melee,target,&complete))return false;
    CHECK(swings>1&&swings<100&&h->actors[target].hp==0&&g->hunt_kills==count+1);
    CHECK(complete==(g->hunt_kills==4));
    fprintf(stderr,"hunt lifecycle: source HP %.6f defeated by %u real knife guides, progress %u/4\n",hp,swings,g->hunt_kills);
    return h->actors[target].hp==0&&g->hunt_kills==count+1;
}
static bool checkpoint(PeGame *g,PeGame *loaded,const char *path,unsigned count){
    char error[256];PeInventory inventory=g->inventory;PeVec3 position=g->player;float hp=g->hp,stamina=g->stamina;
    uint32_t instance=g->equipped_instance_id;float cooldown=g->medpack_cooldown;
    if(!pe_game_save(g,path,error,sizeof error)||!pe_game_load(loaded,path,error,sizeof error)){
        fprintf(stderr,"hunt lifecycle checkpoint: %s\n",error);return false;
    }
    CHECK(loaded->hunt_kills==count&&loaded->hunt_state==g->hunt_state);
    CHECK(!memcmp(&loaded->inventory,&inventory,sizeof inventory)&&loaded->equipped_instance_id==instance);
    CHECK(!memcmp(&loaded->player,&position,sizeof position)&&loaded->hp==hp&&loaded->stamina==stamina&&loaded->medpack_cooldown==cooldown);
    CHECK(pe_world_hash(&loaded->world)==pe_world_hash(&g->world));
    pe_game_destroy(g);pe_game_move(g,loaded);return true;
}
static void actual_lifecycle(void){
    const char *root=PE_SOURCE_ROOT "/rebuild/build/reference-assets";
    const char *names[]={"adventure-route/terrain.pevx","hare.peac","hare.pehb","player/player-part-0-material-0.peac",
        "player-knife-drawn/knife-drawn-part-0-material-0.peac","player-knife-drawn/knife-drawn.pehb"};
    char paths[6][2048],error[256],save[]=PE_SOURCE_ROOT "/rebuild/build/hunt-lifecycle-XXXXXX";
    for(unsigned i=0;i<6;i++){
        int n=snprintf(paths[i],sizeof paths[i],"%s/%s",root,names[i]);CHECK(n>0&&(size_t)n<sizeof paths[i]);
        FILE *f=fopen(paths[i],"rb");if(!f){fprintf(stderr,"hunt lifecycle: SKIP recovered fixture missing: %s\n",paths[i]);return;}fclose(f);
    }
    int fd=mkstemp(save);CHECK(fd>=0);if(fd<0)return;close(fd);
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);PeSandboxCollision *collision=pe_sandbox_collision_create();
    PeActor *hare=pe_actor_load(paths[1],error,sizeof error),*body=pe_actor_load(paths[3],error,sizeof error),*knife=pe_actor_load(paths[4],error,sizeof error);
    PeHitboxes *hb=hare?pe_hitboxes_load(paths[2],hare,error,sizeof error):NULL,*kb=knife?pe_hitboxes_load(paths[5],knife,error,sizeof error):NULL;
    CHECK(g&&loaded&&collision&&hare&&body&&knife&&hb&&kb);if(!g||!loaded||!collision||!hare||!body||!knife||!hb||!kb)goto done;
    CHECK(pe_settlement_configure(root,error,sizeof error));CHECK(pe_game_init_mode(g,917,PE_GAME_ADVENTURE));
    if(!pe_settlement_start(g,error,sizeof error)){fprintf(stderr,"hunt lifecycle start: %s\n",error);CHECK(false);goto done;}
    CHECK(pe_settlement_is_route(&g->world)&&g->hp==500&&g->max_hp==500);
    CHECK(pe_test_ack_welcome(g)&&pe_game_arrival_complete(g)&&pe_game_accept_supplies(g));g->annot_complete=true;
    /* Bounded prerequisite fixture; inventory contents come through public
     * supply/medicine/item APIs, never fabricated kill or reduced-HP state. */
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};g->equipped_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    CHECK(pe_adventure_medicine_accept(g)&&pe_inventory_add(&g->inventory,917,5)&&pe_adventure_medicine_complete(g));
    CHECK(pe_inventory_count(&g->inventory,925)==10&&pe_inventory_add(&g->inventory,33,1));
    int knife_slot=-1;for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].item_id==33)knife_slot=i;
    CHECK(knife_slot>=0&&pe_game_equip_slot(g,knife_slot));
    CHECK(pe_adventure_hunt_accept(g));
    /* Supported explicit fixture relocation, not proof of walking the route. */
    g->player=(PeVec3){g->hunt_destination.x-g->world.origin_x,g->hunt_destination.y-g->world.origin_y,g->hunt_destination.z-g->world.origin_z};
    CHECK(pe_game_collision_update(g,collision,error,sizeof error)&&pe_game_collision_settle(g,collision));
    CHECK(pe_adventure_hunt_step(g)&&g->hunt_state==PE_HUNT_ENCOUNTER_PENDING&&g->hunt_kills==0);
    PeHuntEncounter h={0};PeSandboxMelee melee={0};
    PeSandboxMeleeSetup setup={.weapon_item_id=33,.body=body,.drawn_weapon=knife,.drawn_capsule=kb,.gender=PE_COMBAT_FEMALE,.player_id=1,
        .attack=pe_player_defaults.attack+pe_item_def(33)->attack,.stamina_reduce_percent=pe_player_defaults.stamina_reduce_percent};
    CHECK(pe_sandbox_melee_init(&melee,&setup));
    bool ready=pe_hunt_encounter_prepare(&h,&g->world,collision,hare,hb,g->hunt_kills,error,sizeof error);CHECK(ready);if(!ready)goto done;
    float initial_hp[6];for(unsigned i=0;i<6;i++){initial_hp[i]=h.actors[i].hp;CHECK(initial_hp[i]==h.actors[i].motion.max_hp);}
    if(!defeat(g,&h,&melee,0))goto done;
    bool finished=false;CHECK(swing(g,&h,&melee,1,&finished)&&!finished&&h.actors[1].hp<initial_hp[1]);
    if(!checkpoint(g,loaded,save,1)){CHECK(false);goto done;}
    pe_sandbox_melee_cancel(&melee);pe_hunt_encounter_clear(&h);
    CHECK(pe_game_collision_update(g,collision,error,sizeof error));
    CHECK(pe_hunt_encounter_prepare(&h,&g->world,collision,hare,hb,g->hunt_kills,error,sizeof error));
    CHECK(h.starting_kills==1&&h.credited_count==0);
    for(unsigned i=0;i<6;i++)CHECK(h.actors[i].present&&h.actors[i].hp==initial_hp[i]);
    if(!defeat(g,&h,&melee,0)||!defeat(g,&h,&melee,5))goto done;
    if(!checkpoint(g,loaded,save,3)){CHECK(false);goto done;}
    pe_sandbox_melee_cancel(&melee);pe_hunt_encounter_clear(&h);
    CHECK(pe_game_collision_update(g,collision,error,sizeof error));
    CHECK(pe_hunt_encounter_prepare(&h,&g->world,collision,hare,hb,g->hunt_kills,error,sizeof error));
    CHECK(h.starting_kills==3&&h.credited_count==0);
    for(unsigned i=0;i<6;i++)CHECK(h.actors[i].present&&h.actors[i].hp==initial_hp[i]);
    if(!defeat(g,&h,&melee,0))goto done;
    CHECK(!h.active&&g->hunt_state==PE_HUNT_RETURN_PENDING&&g->hunt_kills==4);
    for(unsigned i=0;i<6;i++)CHECK(!h.actors[i].present);
    CHECK(!pe_hunt_encounter_prepare(&h,&g->world,collision,hare,hb,4,error,sizeof error));
    if(!checkpoint(g,loaded,save,4)){CHECK(false);goto done;}
    CHECK(!pe_adventure_hunt_return_step(g));
    PeVec3 destination;CHECK(pe_adventure_hunt_return_reference(&destination));
    g->player=(PeVec3){destination.x-g->world.origin_x,destination.y-g->world.origin_y,destination.z-g->world.origin_z};
    CHECK(pe_game_collision_update(g,collision,error,sizeof error)&&pe_game_collision_settle(g,collision));
    CHECK(pe_adventure_hunt_return_step(g)&&pe_adventure_hunt_return_talk(g)==4105);
    for(int id=592;id<=595;id++)CHECK(pe_inventory_count(&g->inventory,id)==1);
    for(unsigned state=PE_HUNT_RETURN_TALK1;state<=PE_HUNT_COMPLETE;state++){
        CHECK(g->hunt_state==state);if(!checkpoint(g,loaded,save,4)){CHECK(false);goto done;}
        CHECK(!pe_adventure_hunt_return_step(g));
        for(int id=592;id<=595;id++)CHECK(pe_inventory_count(&g->inventory,id)==1);
        int talk=pe_adventure_hunt_return_talk(g);
        if(state<PE_HUNT_COMPLETE){CHECK(talk==(state==PE_HUNT_RETURN_TALK1?4105:4106));CHECK(pe_adventure_hunt_return_advance(g,talk));CHECK(!pe_adventure_hunt_return_advance(g,talk));}
        else CHECK(!talk&&!pe_adventure_hunt_return_advance(g,4106));
    }
    CHECK(g->hp==500&&pe_inventory_count(&g->inventory,925)==10);
    if(!failures)fprintf(stderr,"hunt lifecycle: fullHP deaths -> saved1/3 fresh cohorts -> fourth cleanup -> rewards/pages reload passed (arranged contact fixture)\n");
 done:unlink(save);pe_hitboxes_free(hb);pe_hitboxes_free(kb);pe_actor_free(hare);pe_actor_free(body);pe_actor_free(knife);
    pe_sandbox_collision_free(collision);if(g)pe_game_destroy(g);if(loaded)pe_game_destroy(loaded);free(g);free(loaded);
}
int pe_test_hunt_lifecycle(void){
    /* Flush before forking so child exit can run LeakSanitizer/stdio cleanup
     * without duplicating earlier suite output. _exit would bypass leak checks
     * for all allocations inside this otherwise isolated fixture. */
    failures=0;fflush(NULL);pid_t pid=fork();CHECK(pid>=0);if(pid<0)return failures;
    if(pid==0){actual_lifecycle();exit(failures?1:0);}
    int status=0;pid_t waited;do{waited=waitpid(pid,&status,0);}while(waited<0&&errno==EINTR);
    CHECK(waited==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);return failures;
}
