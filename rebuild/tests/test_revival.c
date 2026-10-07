#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_adventure_hunt.h"
#include "pe_adventure_medicine.h"
#include "pe_revival.h"
#include "../generated/pe_stamina_recovery_source.h"
#include <math.h>
#include <string.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"revival:%d %s\n",__LINE__,#c);failures++;}}while(0)
static int slot(const PeGame *g,int id){
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==id&&g->inventory.slots[i].count)return i;
    return -1;
}
static PeRevivalGate admitted(void){
    return (PeRevivalGate){.death_mask=true,.getup_ready=true,.item_skill_ready=true};
}
static PeAttributeFrame current(void){return (PeAttributeFrame){100,800,false};}
static PeGame *dead_game(void){
    PeGame *g=calloc(1,sizeof *g);CHECK(g);if(!g)return NULL;
    if(!pe_game_init_mode(g,938,PE_GAME_ADVENTURE)){CHECK(false);free(g);return NULL;}
    g->hp=0;g->stamina=7;g->comfort=123;
    g->passive_recovery=(PePassiveRecovery){.created=20,.mounted_at=20,
        .last_hurt=99,.last_time=100,.frame=800,.initialized=true,
        .mounted=false,.initialization_checked=true};
    g->stamina_recovery=(PeStaminaRecovery){.last_consumption=98,.summed_delay=1.5f,
        .frame=800,.initialized=true};
    return g;
}
static void reject(PeGame *g,PeAttributeFrame frame,PeRevivalGate gate){
    unsigned char *before=malloc(sizeof *g);CHECK(before);if(!before)return;
    memcpy(before,g,sizeof *g);uint64_t world=pe_world_hash(&g->world);
    CHECK(!pe_game_revive(g,frame,gate));
    CHECK(!memcmp(before,g,sizeof *g)&&pe_world_hash(&g->world)==world);free(before);
}
static void transaction(void){
    PeGame *g=dead_game();if(!g)return;
    CHECK(pe_inventory_count(&g->inventory,937)==10);
    PeInventory expected=g->inventory;CHECK(pe_inventory_remove(&expected,937,1));
    PeOutfit outfit=g->outfit;PeVec3 position=g->player;uint32_t equipped=g->equipped_instance_id;
    g->medpack_cooldown=4;g->juice_cooldown=3;
    g->velocity=(PeVec3){2,-3,4};g->sprinting=g->jumping=true;g->jump_action_elapsed=.2f;g->tool_cooldown=.4f;
    CHECK(pe_game_revive(g,current(),admitted()));
    CHECK(g->hp==g->max_hp*.333f&&g->hp<g->max_hp&&g->stamina==g->stamina_max);
    CHECK(g->comfort==98&&g->medpack_cooldown==4&&g->juice_cooldown==3);
    CHECK(!memcmp(&expected,&g->inventory,sizeof expected)&&!memcmp(&outfit,&g->outfit,sizeof outfit));
    CHECK(!memcmp(&position,&g->player,sizeof position)&&equipped==g->equipped_instance_id);
    CHECK(g->velocity.x==0&&g->velocity.y==0&&g->velocity.z==0&&!g->sprinting&&!g->jumping&&g->jump_action_elapsed==0&&g->tool_cooldown==0);
    CHECK(g->revival.phase==PE_REVIVAL_GETTING_UP&&pe_revival_frozen(&g->revival)&&pe_revival_damage_disabled(&g->revival));
    CHECK(g->passive_recovery.mounted&&g->passive_recovery.mounted_at==100&&g->passive_recovery.evaluations==0);
    CHECK(g->passive_recovery.last_hurt==99&&g->passive_recovery.last_time==100&&g->passive_recovery.frame==800);
    CHECK(g->stamina_recovery.last_consumption==98&&g->stamina_recovery.frame==800);
    CHECK(!g->stamina_recovery.stamina_ticks&&!g->stamina_recovery.comfort_ticks&&!g->stamina_recovery.delay_ticks);
    reject(g,current(),admitted()); /* Repeated death-button request cannot consume again. */
    pe_game_destroy(g);free(g);
}
static void admission(void){
    PeGame *g=dead_game();if(!g)return;
    for(unsigned i=0;i<5;i++){
        PeRevivalGate gate=admitted();
        if(i==0)gate.death_mask=false;
        if(i==1)gate.fall_mask=true;
        if(i==2)gate.ride_mask=true;
        if(i==3)gate.getup_ready=false;
        if(i==4)gate.item_skill_ready=false;
        reject(g,current(),gate);
    }
    g->hp=1;reject(g,current(),admitted());g->hp=0;
    g->mode=PE_GAME_LEGACY;reject(g,current(),admitted());g->mode=PE_GAME_ADVENTURE;
    const PeAttributeFrame invalid[]={{NAN,800,false},{INFINITY,800,false},{-1,800,false},
        {99,800,false},{101,800,false},{100,799,false},{100,801,false}};
    for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++)reject(g,invalid[i],admitted());
    CHECK(pe_inventory_remove(&g->inventory,937,10));reject(g,current(),admitted());
    CHECK(pe_inventory_add(&g->inventory,937,1));CHECK(pe_game_quickslot_assign(g,8,slot(g,937)));
    CHECK(pe_game_revive(g,current(),admitted())&&pe_inventory_count(&g->inventory,937)==0);
    /* Stack shortcuts intentionally survive exhaustion; ordinary use remains unavailable. */
    CHECK(g->quickslots[8].item_id==937&&g->quickslots[8].instance_id==0&&!pe_game_quickslot_use(g,8));
    pe_game_destroy(g);free(g);
}
static void grace(void){
    PeGame *g=dead_game();if(!g)return;
    CHECK(pe_game_revive(g,current(),admitted()));
    PeRevival before=g->revival;
    CHECK(!pe_revival_frame(&g->revival,current(),true)&&!memcmp(&before,&g->revival,sizeof before));
    CHECK(!pe_revival_frame(&g->revival,(PeAttributeFrame){99,801,false},true)&&!memcmp(&before,&g->revival,sizeof before));
    CHECK(!pe_revival_frame(&g->revival,(PeAttributeFrame){NAN,801,false},true)&&!memcmp(&before,&g->revival,sizeof before));
    /* The next display frame can have the same scaled timestamp. */
    CHECK(pe_revival_frame(&g->revival,(PeAttributeFrame){100,801,false},true));
    CHECK(g->revival.phase==PE_REVIVAL_GRACE&&!pe_revival_frozen(&g->revival));
    CHECK(g->revival.enable_at==101.5f&&pe_revival_damage_disabled(&g->revival));
    CHECK(pe_revival_frame(&g->revival,(PeAttributeFrame){nextafterf(101.5f,0),802,false},true));
    CHECK(pe_revival_damage_disabled(&g->revival));
    CHECK(pe_revival_frame(&g->revival,(PeAttributeFrame){101.5f,803,false},true));
    CHECK(g->revival.phase==PE_REVIVAL_IDLE&&!pe_revival_damage_disabled(&g->revival)&&!pe_revival_frozen(&g->revival));
    PeRevival active={.phase=PE_REVIVAL_GRACE,.last_time=110,.enable_at=111.5f,.frame=900};
    CHECK(pe_revival_frame(&active,(PeAttributeFrame){110,901,false},false));
    CHECK(active.phase==PE_REVIVAL_IDLE&&!pe_revival_damage_disabled(&active));
    pe_game_destroy(g);free(g);
}
static void malformed_owners(void){
    for(unsigned i=0;i<9;i++){
        PeGame *g=dead_game();if(!g)return;
        if(i==0)g->revival.phase=(PeRevivalPhase)99;
        if(i==1)g->revival=(PeRevival){.phase=PE_REVIVAL_GRACE,.last_time=100,.enable_at=99,.frame=800};
        if(i==2)g->passive_recovery.last_hurt=NAN;
        if(i==3)g->passive_recovery.mounted=true;
        if(i==4)g->stamina_recovery.last_consumption=101;
        if(i==5){g->stamina_recovery.comfort_pending=true;g->stamina_recovery.pending_raw_comfort=NAN;}
        if(i==6)g->inventory.slots[slot(g,937)].count=-1;
        if(i==7)g->inventory.slots[slot(g,937)].durability=1;
        if(i==8)g->inventory.slots[slot(g,33)].instance_id=g->inventory.next_instance_id;
        reject(g,current(),admitted());pe_game_destroy(g);free(g);
    }
    PeRevival bad[]={
        {.phase=(PeRevivalPhase)99},
        {.phase=PE_REVIVAL_GRACE,.last_time=100,.enable_at=99,.frame=800},
        {.phase=PE_REVIVAL_GETTING_UP,.last_time=100,.enable_at=101,.frame=800},
        {.phase=PE_REVIVAL_IDLE,.last_time=NAN,.frame=800}};
    for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){
        PeRevival before;memcpy(&before,&bad[i],sizeof before);
        CHECK(pe_revival_frozen(&bad[i])&&pe_revival_damage_disabled(&bad[i]));
        CHECK(!pe_revival_frame(&bad[i],(PeAttributeFrame){101,801,false},true)&&!memcmp(&before,&bad[i],sizeof before));
    }
}
static bool attributes(PeGame *g,PeAttributeFrame frame){
    PePassiveRecovery before=g->passive_recovery;
    if(!pe_passive_recovery_frame(&g->passive_recovery,frame,&g->hp,g->max_hp))return false;
    PeStaminaVitals v={g->stamina,g->stamina_max,g->stamina_regen,g->stamina_recover_delay,g->comfort};
    if(!pe_stamina_recovery_frame(&g->stamina_recovery,&before,&g->passive_recovery,frame,&v))return false;
    g->stamina=v.stamina;g->comfort=v.comfort;return true;
}
static void recovery_remount(void){
    PeGame *g=dead_game();if(!g)return;
    g->comfort=20;PeAttributeClock clock={.seconds=100,.frame=800,.attribute_frame=800};
    CHECK(pe_game_revive(g,current(),admitted()));
    CHECK(clock.seconds==100&&clock.frame==800&&clock.attribute_frame==800);
    CHECK(g->comfort==0&&g->stamina_recovery.comfort_pending&&g->stamina_recovery.pending_raw_comfort==-5);
    CHECK(g->passive_recovery.created==20&&g->passive_recovery.initialization_checked);
    float hp=g->hp;PeAttributeFrame frame={0};
    for(unsigned i=0;i<8;i++){
        CHECK(pe_attribute_clock_step(&clock,1./64,&frame));CHECK(attributes(g,frame));
        CHECK(g->hp==hp&&g->stamina==g->stamina_max);
        CHECK(g->passive_recovery.last_hurt==99&&g->stamina_recovery.last_consumption==98);
        if(i<7)CHECK(!frame.attributes_due&&g->stamina_recovery.comfort_pending&&g->passive_recovery.evaluations==0);
    }
    CHECK(frame.attributes_due&&frame.frame==808&&frame.now==100.125f);
    CHECK(!g->stamina_recovery.comfort_pending&&g->stamina_recovery.pending_raw_comfort==0&&g->comfort==0);
    float raw=-5+pe_stamina_recovery_source.comfort_recovery_speed*
        pe_stamina_recovery_source.comfort_reduction_percent*(float)pe_stamina_recovery_source.comfort_interval_seconds;
    float delay=g->stamina_recover_delay+(1-raw/500)*2;
    CHECK(fabsf(g->stamina_recovery.summed_delay-delay)<1e-6f&&g->stamina_recovery.summed_delay>2);
    CHECK(g->passive_recovery.evaluations==1&&g->stamina_recovery.stamina_ticks==1&&g->stamina_recovery.comfort_ticks==1);
    PePassiveRecovery parent=g->passive_recovery;PeStaminaRecovery stamina=g->stamina_recovery;
    CHECK(!attributes(g,frame)&&!memcmp(&parent,&g->passive_recovery,sizeof parent)&&!memcmp(&stamina,&g->stamina_recovery,sizeof stamina));
    pe_game_destroy(g);free(g);
}
static void repeated_death(void){
    PeGame *g=dead_game();if(!g)return;
    g->comfort=20;
    CHECK(pe_game_revive(g,current(),admitted()));
    CHECK(pe_inventory_count(&g->inventory,937)==9&&g->stamina_recovery.pending_raw_comfort==-5);
    /* A second death before the next attribute pass cannot erase the first
     * pending raw cost by reading already-clamped display/save comfort. */
    g->hp=0;pe_passive_recovery_hurt(&g->passive_recovery,g->hp);
    PeAttributeFrame next={100,801,false};CHECK(attributes(g,next));
    CHECK(pe_revival_frame(&g->revival,next,false));
    CHECK(pe_game_revive(g,next,admitted()));
    CHECK(pe_inventory_count(&g->inventory,937)==8&&g->comfort==0);
    CHECK(g->stamina_recovery.comfort_pending&&g->stamina_recovery.pending_raw_comfort==-30);
    CHECK(g->passive_recovery.last_hurt==100&&g->passive_recovery.frame==801);
    reject(g,next,admitted());CHECK(pe_inventory_count(&g->inventory,937)==8);
    pe_game_destroy(g);free(g);
}
static void save_progress(void){
    PeGame *g=dead_game(),*loaded=calloc(1,sizeof *loaded);CHECK(loaded);
    char path[]="/tmp/pe-revival-XXXXXX",error[256];int fd=mkstemp(path);CHECK(fd>=0);
    if(fd>=0)close(fd);
    if(!g||!loaded||fd<0)goto done;
    g->hp=g->max_hp;
    CHECK(pe_test_ack_welcome(g)&&pe_game_arrival_complete(g)&&pe_game_accept_supplies(g));g->annot_complete=true;
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id,.categorized=g->mode==PE_GAME_ADVENTURE};
    g->equipped_instance_id=g->shield_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    CHECK(pe_adventure_medicine_accept(g)&&pe_inventory_add(&g->inventory,917,5)&&pe_adventure_medicine_complete(g));
    CHECK(pe_inventory_add(&g->inventory,937,3)&&pe_adventure_hunt_reference(&g->hunt_destination));
    /* Explicit partial-progress persistence precondition, not earned combat. */
    g->hunt_state=PE_HUNT_ENCOUNTER_PENDING;g->hunt_kills=2;CHECK(pe_adventure_hunt_state_valid(g));
    CHECK(pe_outfit_wear(&g->outfit,123));g->hp=0;
    CHECK(pe_game_revive(g,current(),admitted())&&pe_inventory_count(&g->inventory,937)==2);
    CHECK(g->hunt_state==PE_HUNT_ENCOUNTER_PENDING&&g->hunt_kills==2);
    CHECK(pe_game_save(g,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->hp==g->hp&&loaded->stamina==g->stamina&&loaded->comfort==g->comfort);
    CHECK(loaded->hunt_state==g->hunt_state&&loaded->hunt_kills==2);
    CHECK(!memcmp(&loaded->inventory,&g->inventory,sizeof g->inventory)&&!memcmp(&loaded->outfit,&g->outfit,sizeof g->outfit));
    CHECK(!memcmp(&loaded->revival,&(PeRevival){0},sizeof loaded->revival));
    CHECK(loaded->passive_recovery.loaded&&!loaded->passive_recovery.initialized);
    CHECK(!memcmp(&loaded->stamina_recovery,&(PeStaminaRecovery){0},sizeof loaded->stamina_recovery));
    CHECK(!pe_revival_damage_disabled(&loaded->revival)&&!pe_revival_frozen(&loaded->revival));
    /* No second debit when the same saved post-revival life is reopened. */
    CHECK(attributes(loaded,(PeAttributeFrame){200,1600,false}));
    reject(loaded,(PeAttributeFrame){200,1600,false},admitted());
    CHECK(pe_inventory_count(&loaded->inventory,937)==2);
    CHECK(pe_game_save(loaded,path,error,sizeof error)&&pe_game_load(loaded,path,error,sizeof error));
    CHECK(pe_inventory_count(&loaded->inventory,937)==2&&loaded->hunt_kills==2);
    FILE *file=fopen(path,"rb");CHECK(file);if(file){unsigned char h[8];CHECK(fread(h,1,8,file)==8&&h[4]==20&&h[5]==0);fclose(file);}
    /* Invalid loads leave current recovery/game/world state untouched. */
    unsigned char *snapshot=malloc(sizeof *loaded);CHECK(snapshot);
    if(snapshot){
        memcpy(snapshot,loaded,sizeof *loaded);uint64_t world=pe_world_hash(&loaded->world);
        file=fopen(path,"wb");CHECK(file);if(file){CHECK(fwrite("PECS",1,4,file)==4);fclose(file);}
        CHECK(!pe_game_load(loaded,path,error,sizeof error)&&!memcmp(snapshot,loaded,sizeof *loaded)&&pe_world_hash(&loaded->world)==world);
        free(snapshot);
    }
done:
    if(fd>=0)unlink(path);
    pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);
}
static void ordinary_use_rejected(void){
    PeGame *g=calloc(1,sizeof *g);CHECK(g);if(!g)return;
    CHECK(pe_game_init_mode(g,938,PE_GAME_ADVENTURE));
    int shot=slot(g,937);CHECK(shot>=0&&pe_inventory_count(&g->inventory,937)==10);
    if(shot<0)goto done;
    CHECK(pe_game_quickslot_assign(g,8,shot));
    PeInventory bag=g->inventory;PeQuickslot shortcut=g->quickslots[8];
    const float health[]={500,17,0};
    for(unsigned i=0;i<sizeof health/sizeof health[0];i++){
        g->hp=health[i];g->comfort=123;g->stamina=7;
        CHECK(!pe_game_use_slot(g,shot)&&!pe_game_quickslot_use(g,8));
        CHECK(g->hp==health[i]&&g->comfort==123&&g->stamina==7);
        CHECK(!memcmp(&g->inventory,&bag,sizeof bag));
        CHECK(!memcmp(&g->quickslots[8],&shortcut,sizeof shortcut));
    }
done:
    pe_game_destroy(g);free(g);
}

int pe_test_revival(void){
    failures=0;
    ordinary_use_rejected();
    transaction();admission();grace();malformed_owners();recovery_remount();repeated_death();save_progress();
    return failures;
}
