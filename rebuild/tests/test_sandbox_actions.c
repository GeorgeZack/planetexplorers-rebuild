#include "pe_sandbox_actions.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sandbox actions:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static void recovery_frame(PeGame *g,PeAttributeClock *clock,double dt){
    PeAttributeFrame frame;PePassiveRecovery before=g->passive_recovery;
    CHECK(pe_attribute_clock_step(clock,dt,&frame));
    CHECK(pe_passive_recovery_frame(&g->passive_recovery,frame,&g->hp,g->max_hp));
    PeStaminaVitals vitals={g->stamina,g->stamina_max,g->stamina_regen,g->stamina_recover_delay,g->comfort};
    CHECK(pe_stamina_recovery_frame(&g->stamina_recovery,&before,&g->passive_recovery,frame,&vitals));
    g->stamina=vitals.stamina;g->comfort=vitals.comfort;
}
static PeSandboxActionInput input(void){
    return(PeSandboxActionInput){.enabled=true,.held=true,.tool_drawn=true,
        .ray_origin={32.1f,11.58f,32.1f},.ray_direction={0,-1,0},.camera_tree=-1,.forward_tree=-1};
}
static void setup(PeGame *g,PeSandboxActions *a,int item,bool male){
    pe_game_init(g,1);g->mode=PE_GAME_ADVENTURE;g->adventure_mission=PE_ADVENTURE_ARRIVAL;
    memset(&g->world,0,sizeof g->world);g->world.source=1;g->world.revision=1;
    for(int z=0;z<PE_WORLD_Z;z++)for(int x=0;x<PE_WORLD_X;x++)for(int y=0;y<10;y++)
        pe_world_set(&g->world,x,y,z,(PeVoxel){255,1});
    g->player=(PeVec3){32.1f,10,32.1f};g->velocity=(PeVec3){0};g->grounded=true;
    g->inventory=(PeInventory){.next_instance_id=1,.categorized=true};g->tree_count=2;
    g->trees[0]=(PeTree){{33,10,32},8,255};g->trees[1]=(PeTree){{32,10,33},8,255};
    CHECK(pe_inventory_add(&g->inventory,item,1));CHECK(pe_game_equip_slot(g,PE_INVENTORY_CATEGORY_SLOTS));
    pe_sandbox_actions_init(a,male);
}
static PeSandboxActionUpdate tick(PeSandboxActions *a,PeGame *g,PeSandboxActionInput in){
    PeSandboxActionUpdate update={0};CHECK(pe_sandbox_actions_step(a,g,in,&update));return update;
}
static void shovel_timing(PeGame *g,PeSandboxActions *a){
    setup(g,a,12,false);PeSandboxActionInput in=input();uint64_t hash=pe_world_hash(&g->world);
    PeSandboxActionUpdate u=tick(a,g,in);CHECK(u.started&&!u.swung&&a->stage==PE_SANDBOX_ACTION_ENTRY&&g->stamina==96);
    PeSandboxActionPose pose;CHECK(pe_sandbox_actions_pose(a,g,&pose));
    CHECK(pose.item_id==12&&pose.instance_id==g->equipped_instance_id&&pose.held&&strcmp(pose.clip,"Spade1")==0);
    CHECK(fabs(pose.seconds-2.0/60)<1e-8);
    for(int i=1;i<12;i++){u=tick(a,g,in);CHECK(!u.swung);}
    CHECK(a->stage==PE_SANDBOX_ACTION_ENTRY);u=tick(a,g,in);CHECK(a->stage==PE_SANDBOX_ACTION_LOOP&&!u.swung);
    for(int i=13;i<48;i++){u=tick(a,g,in);CHECK(!u.swung);}
    CHECK(pe_world_hash(&g->world)==hash&&pe_game_equipped_tool(g)->durability==3553);
    u=tick(a,g,in);CHECK(u.swung&&u.changed&&!u.removed&&u.x==32&&u.y==9&&u.z==32);
    CHECK(pe_world_get(&g->world,32,9,32).volume==221&&g->stamina==92&&pe_game_equipped_tool(g)->durability==3548);
    CHECK(g->tool_cooldown==0&&a->swings==1&&a->stamina_lock);
    unsigned removed=0;
    for(int i=0;i<280&&!removed;i++){u=tick(a,g,in);removed+=u.removed;}
    CHECK(removed==1&&a->swings==4&&pe_inventory_count(&g->inventory,268)==2&&g->gathered==1);
    CHECK(pe_game_equipped_tool(g)->durability==3533);
    /* A normal release exits through the loop gate; re-press does not undo it. */
    in.held=false;tick(a,g,in);CHECK(a->ending);
    in.held=true;unsigned events=0;bool ended=false;
    for(int i=0;i<200&&!ended;i++){u=tick(a,g,in);events+=u.swung;ended=u.finished;}
    CHECK(ended&&events==0&&a->stage==PE_SANDBOX_ACTION_IDLE&&a->stamina_lock);
    CHECK(pe_sandbox_actions_pose(a,g,&pose)&&pose.clip==NULL&&pose.held);
    /* Male override uses its own hit time; female timing must not leak in. */
    setup(g,a,12,true);in=input();for(int i=0;i<51;i++){u=tick(a,g,in);CHECK(!u.swung);}
    u=tick(a,g,in);CHECK(u.swung&&a->swings==1);
}
static void shovel_targets(PeGame *g,PeSandboxActions *a){
    setup(g,a,12,false);PeSandboxActionInput in=input();tick(a,g,in);
    in.ray_origin.x=33.1f;
    PeSandboxActionUpdate u={0};for(int i=1;i<49;i++)u=tick(a,g,in);
    CHECK(u.swung&&u.x==32&&pe_world_get(&g->world,33,9,32).volume==255);
    for(int i=0;i<81;i++){u=tick(a,g,in);if(u.swung)break;}
    CHECK(u.swung&&u.x==33&&pe_world_get(&g->world,33,9,32).volume==221);
    setup(g,a,12,false);in=input();tick(a,g,in);in.ray_direction=(PeVec3){0,1,0};
    for(int i=1;i<49;i++)u=tick(a,g,in);
    CHECK(u.swung&&u.x==32);float wear=pe_game_equipped_tool(g)->durability,stamina=g->stamina;
    for(int i=0;i<100;i++)CHECK(!tick(a,g,in).swung);
    CHECK(pe_game_equipped_tool(g)->durability==wear&&g->stamina==stamina);
    /* A locked empty first sample still wears the source tool, without yield. */
    setup(g,a,12,false);in=input();tick(a,g,in);pe_world_set(&g->world,32,9,32,(PeVoxel){0});
    for(int i=1;i<49;i++)u=tick(a,g,in);
    CHECK(u.swung&&!u.changed&&!u.removed&&pe_game_equipped_tool(g)->durability==3548&&g->stamina==92);
}
static void conservation(PeGame *g,PeSandboxActions *a){
    setup(g,a,12,false);PeSandboxActionInput in=input();
    CHECK(pe_inventory_add(&g->inventory,279,pe_item_def(279)->stack_limit*PE_INVENTORY_CATEGORY_SLOTS));
    pe_world_set(&g->world,32,9,32,(PeVoxel){130,1});PeSandboxActionUpdate u={0};
    for(int i=0;i<48;i++)u=tick(a,g,in);
    PeInventory inventory=g->inventory;uint32_t rng=g->rng_state;uint64_t hash=pe_world_hash(&g->world);float stamina=g->stamina;
    u=tick(a,g,in);CHECK(u.capacity_blocked&&!u.swung&&!u.changed&&a->hit_count==1);
    CHECK(memcmp(&g->inventory,&inventory,sizeof inventory)==0&&g->rng_state==rng&&pe_world_hash(&g->world)==hash&&g->stamina==stamina);
    in.held=false;bool ended=false;for(int i=0;i<200&&!ended;i++)ended=tick(a,g,in).finished;
    CHECK(ended&&a->stage==PE_SANDBOX_ACTION_IDLE);
    setup(g,a,1527,false);in=input();in.camera_tree=in.forward_tree=0;
    g->trees[0].health=10;CHECK(pe_inventory_add(&g->inventory,279,pe_item_def(279)->stack_limit*PE_INVENTORY_CATEGORY_SLOTS));
    for(int i=0;i<56;i++)u=tick(a,g,in);
    inventory=g->inventory;stamina=g->stamina;rng=g->rng_state;
    u=tick(a,g,in);CHECK(u.capacity_blocked&&!u.swung&&g->trees[0].health==10&&g->trees[0].remaining==8);
    CHECK(memcmp(&inventory,&g->inventory,sizeof inventory)==0&&g->stamina==stamina&&g->rng_state==rng);
}
static void felling(PeGame *g,PeSandboxActions *a){
    setup(g,a,1527,false);PeSandboxActionInput in=input();in.camera_tree=0;in.forward_tree=1;
    CHECK(!tick(a,g,in).started&&g->stamina==100);
    in.forward_tree=0;PeSandboxActionUpdate u=tick(a,g,in);CHECK(u.started&&fabsf(a->speed-1.22f)<1e-6&&g->stamina==96);
    PeSandboxActionPose pose;CHECK(pe_sandbox_actions_pose(a,g,&pose)&&strcmp(pose.clip,"Felling1")==0);
    /* Source locks the selected tree; later camera/forward mismatch does not
     * silently change it or suppress its already-running authored events. */
    in.camera_tree=in.forward_tree=1;
    for(int i=1;i<56;i++){u=tick(a,g,in);CHECK(!u.swung);}
    u=tick(a,g,in);CHECK(u.swung&&u.tree_index==0&&fabsf(g->trees[0].health-(255-28*.48571f))<.001f&&g->trees[1].health==255);
    unsigned hits=1;bool removed=false;
    for(int i=0;i<2000&&!removed;i++){u=tick(a,g,in);hits+=u.swung;removed=u.removed;}
    CHECK(removed&&hits==19&&g->trees[0].remaining==0&&g->trees[0].health==0&&g->trees[1].remaining==8);
    CHECK(pe_inventory_count(&g->inventory,279)==16&&g->gathered==1&&g->stamina==24);
    CHECK(pe_game_equipped_tool(g)->durability==100); /* Safety axe's source wear is0. */
    bool ended=false;for(int i=0;i<150&&!ended;i++)ended=tick(a,g,in).finished;
    CHECK(ended&&pe_inventory_count(&g->inventory,279)==16);
    setup(g,a,1527,true);in=input();in.camera_tree=in.forward_tree=0;
    for(int i=0;i<42;i++)CHECK(!tick(a,g,in).swung);
    CHECK(tick(a,g,in).swung); /* Male override first event frame43. */
    setup(g,a,26,false);in=input();in.camera_tree=in.forward_tree=0;
    for(int i=0;i<69;i++)CHECK(!tick(a,g,in).swung);
    u=tick(a,g,in);CHECK(u.swung&&a->speed==1&&fabsf(g->trees[0].health-(255-46*.48571f))<.001f&&g->stamina==92);
    hits=1;removed=false;
    for(int i=0;i<1200&&!removed;i++){u=tick(a,g,in);hits+=u.swung;removed=u.removed;}
    CHECK(removed&&hits==12&&pe_inventory_count(&g->inventory,279)==16&&g->stamina==52);
    CHECK(pe_game_equipped_tool(g)->durability==3493); /* Twelve source5-wear attacks. */
}
static void pickaxe(PeGame *g,PeSandboxActions *a){
    PeSandboxActionInput in=input();PeSandboxActionPose pose;PeSandboxActionUpdate u;
    CHECK(pe_sandbox_actions_supported(19)&&!pe_sandbox_actions_supported(20));
    for(int male=0;male<2;male++){
        setup(g,a,19,male!=0);uint64_t hash=pe_world_hash(&g->world);
        u=tick(a,g,in);CHECK(u.started&&a->kind==PE_SANDBOX_ACTION_DIG&&g->stamina==96);
        CHECK(pe_sandbox_actions_pose(a,g,&pose)&&pose.item_id==19&&pose.instance_id==g->equipped_instance_id&&strcmp(pose.clip,"Mine1")==0);
        CHECK(fabs(pose.seconds-1.5/60)<1e-9); /* Mine entry speed1.5, not Spade2. */
        int first=male?74:68,change=male?28:27;
        for(int frame=2;frame<first;frame++){
            u=tick(a,g,in);CHECK(!u.swung);
            if(frame==16)CHECK(a->stage==PE_SANDBOX_ACTION_ENTRY);
            if(frame==17)CHECK(a->stage==PE_SANDBOX_ACTION_LOOP);
            if(frame==change-1)CHECK(a->change_count==0);
            if(frame==change)CHECK(a->change_count==1);
        }
        CHECK(pe_world_hash(&g->world)==hash&&pe_game_equipped_tool(g)->durability==3553);
        u=tick(a,g,in);CHECK(u.swung&&u.changed&&a->swings==1&&g->stamina==92&&g->tool_cooldown==0);
        CHECK(pe_game_equipped_tool(g)->durability==3548&&pe_world_get(&g->world,32,9,32).volume==176);
        CHECK(pe_sandbox_actions_pose(a,g,&pose)&&strcmp(pose.clip,"Mine2")==0);
        CHECK(pose.seconds>=(male?.9432708024978638:.838300883769989));
        if(male){
            for(int frame=75;frame<170;frame++){
                u=tick(a,g,in);CHECK(!u.swung);
                if(frame==113){ /* Exact Mine male loop, not near-equal Spade duration. */
                    CHECK(pe_sandbox_actions_pose(a,g,&pose));
                    CHECK(fabs(pose.seconds-(a->seconds-1.6000022888183594))<1e-10);
                }
            }
            u=tick(a,g,in);CHECK(u.swung&&u.removed&&a->swings==2&&a->hit_count==2&&g->stamina==88);
            CHECK(pe_world_get(&g->world,32,9,32).volume==0&&pe_inventory_count(&g->inventory,268)==2&&g->gathered==1);
            CHECK(pe_game_equipped_tool(g)->durability==3543);
        }
    }
    setup(g,a,19,false);g->stamina=54;tick(a,g,in);
    CHECK(g->stamina==50&&fabsf(a->speed-.9f)<1e-6);
    CHECK(pe_sandbox_actions_pose(a,g,&pose)&&fabs(pose.seconds-1.5*.9/60)<1e-8);
    setup(g,a,19,false);g->stamina=24;tick(a,g,in);CHECK(g->stamina==20&&fabsf(a->speed-.81f)<1e-6);
    /* Target is locked through the first Mine event; subsequent hits retarget. */
    setup(g,a,19,false);in=input();tick(a,g,in);in.ray_origin.x=33.1f;
    for(int i=1;i<68;i++)u=tick(a,g,in);
    CHECK(u.swung&&u.x==32&&pe_world_get(&g->world,33,9,32).volume==255);
    for(int i=0;i<100;i++){u=tick(a,g,in);if(u.swung)break;}
    CHECK(u.swung&&u.x==33&&pe_world_get(&g->world,33,9,32).volume<255);
    /* Release/repress is latched; Mine3 remains the exit pose, with no second hit. */
    setup(g,a,19,false);in=input();tick(a,g,in);in.held=false;tick(a,g,in);in.held=true;
    bool ended=false,saw_exit=false;unsigned hits=0;
    for(int i=0;i<180&&!ended;i++){
        u=tick(a,g,in);hits+=u.swung;ended=u.finished;
        if(a->stage==PE_SANDBOX_ACTION_EXIT){saw_exit=true;CHECK(pe_sandbox_actions_pose(a,g,&pose)&&strcmp(pose.clip,"Mine3")==0);}
    }
    CHECK(ended&&saw_exit&&hits==1&&a->swings==1&&a->stamina_lock&&g->stamina==96);
    /* Capacity rejection preserves terrain, inventory, RNG, wear and followup cost. */
    setup(g,a,19,false);in=input();CHECK(pe_inventory_add(&g->inventory,279,pe_item_def(279)->stack_limit*PE_INVENTORY_CATEGORY_SLOTS));
    pe_world_set(&g->world,32,9,32,(PeVoxel){130,1});
    for(int i=0;i<67;i++)tick(a,g,in);
    PeInventory inventory=g->inventory;uint64_t hash=pe_world_hash(&g->world);uint32_t rng=g->rng_state;
    u=tick(a,g,in);CHECK(u.capacity_blocked&&!u.swung&&!u.changed&&g->stamina==96);
    CHECK(pe_world_hash(&g->world)==hash&&g->rng_state==rng&&memcmp(&inventory,&g->inventory,sizeof inventory)==0);
    in.held=false;ended=false;for(int i=0;i<180&&!ended;i++)ended=tick(a,g,in).finished;CHECK(ended);
    /* Empty locked first attempt wears once; explicit cancellation adds no hit. */
    setup(g,a,19,false);in=input();tick(a,g,in);pe_world_set(&g->world,32,9,32,(PeVoxel){0});
    for(int i=1;i<68;i++)u=tick(a,g,in);
    CHECK(u.swung&&!u.changed&&!u.removed&&g->gathered==0&&pe_game_equipped_tool(g)->durability==3548);
    setup(g,a,19,false);in=input();tick(a,g,in);hash=pe_world_hash(&g->world);in.enabled=false;
    CHECK(tick(a,g,in).finished&&a->stage==PE_SANDBOX_ACTION_IDLE&&a->swings==0&&g->stamina==96);
    CHECK(pe_world_hash(&g->world)==hash&&pe_game_equipped_tool(g)->durability==3553);
    setup(g,a,19,false);in=input();tick(a,g,in);CHECK(pe_inventory_add(&g->inventory,19,1));CHECK(pe_game_equip_slot(g,PE_INVENTORY_CATEGORY_SLOTS+1));
    CHECK(tick(a,g,in).finished&&a->stage==PE_SANDBOX_ACTION_IDLE&&g->stamina==96&&pe_game_equipped_tool(g)->durability==3553);
}
static void guards(PeGame *g,PeSandboxActions *a){
    setup(g,a,26,false);PeSandboxActionInput axe=input();axe.camera_tree=axe.forward_tree=0;g->stamina=54;
    CHECK(tick(a,g,axe).started&&g->stamina==50&&fabsf(a->speed-.9f)<1e-6);
    setup(g,a,1527,false);g->stamina=24;
    CHECK(tick(a,g,axe).started&&g->stamina==20&&fabsf(a->speed-1.22f*.81f)<1e-6);
    setup(g,a,12,false);PeSandboxActionInput in=input();g->stamina=0;
    PeAttributeClock attributes={.seconds=100};recovery_frame(g,&attributes,0);
    CHECK(tick(a,g,in).started&&fabsf(a->speed-.81f)<1e-6&&g->stamina==0);
    float before=g->stamina;pe_game_step(g,(PeInput){.stamina_action=a->stamina_lock},PE_FIXED_DT);
    CHECK(g->stamina==before&&g->stamina_idle==0);
    in.enabled=false;CHECK(tick(a,g,in).finished&&a->stage==PE_SANDBOX_ACTION_IDLE);
    for(int i=0;i<6;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    CHECK(g->stamina==0);pe_game_step(g,(PeInput){0},PE_FIXED_DT);CHECK(g->stamina==0);
    CHECK(g->stamina_recovery.last_consumption==100);
    /* Fixed ticks cannot recover Adventure stamina. The real shared display
     * scheduler admits a full40 pulse only at its next eight-frame pass. */
    for(int i=0;i<6;i++){recovery_frame(g,&attributes,1.0/60);CHECK(g->stamina==0);}
    recovery_frame(g,&attributes,1.0/60);CHECK(g->stamina==40);
    setup(g,a,12,false);in=input();tick(a,g,in);CHECK(pe_inventory_add(&g->inventory,12,1));CHECK(pe_game_equip_slot(g,PE_INVENTORY_CATEGORY_SLOTS+1));
    CHECK(tick(a,g,in).finished&&a->stage==PE_SANDBOX_ACTION_IDLE&&pe_game_equipped_tool(g)->durability==3553);
    setup(g,a,20,false);in=input();CHECK(tick(a,g,in).unsupported&&a->stage==PE_SANDBOX_ACTION_IDLE&&g->stamina==100);
    setup(g,a,12,false);in=input();g->inventory.slots[PE_INVENTORY_CATEGORY_SLOTS].durability=5;
    for(int i=0;i<49;i++)tick(a,g,in);
    CHECK(a->ending&&a->stage==PE_SANDBOX_ACTION_LOOP&&g->inventory.slots[PE_INVENTORY_CATEGORY_SLOTS].durability==0&&g->stamina==96);
    bool ended=false;for(int i=0;i<200&&!ended;i++)ended=tick(a,g,in).finished;
    CHECK(ended&&a->swings==1);
    setup(g,a,12,false);in=input();tick(a,g,in);
    PeGame *copy=malloc(sizeof *copy);CHECK(copy!=NULL);if(!copy)return;*copy=*g;
    PeSandboxActions action=*a;PeSandboxActionUpdate sentinel={.changed=true,.x=97},output=sentinel;
    in.ray_direction.x=NAN;CHECK(!pe_sandbox_actions_step(a,g,in,&output));
    CHECK(memcmp(g,copy,sizeof *g)==0&&memcmp(a,&action,sizeof action)==0&&memcmp(&output,&sentinel,sizeof output)==0);
    in=input();a->seconds=NAN;action=*a;CHECK(!pe_sandbox_actions_step(a,g,in,&output));
    CHECK(memcmp(g,copy,sizeof *g)==0&&memcmp(a,&action,sizeof action)==0&&memcmp(&output,&sentinel,sizeof output)==0);
    pe_sandbox_actions_init(a,false);a->speed=1;in=input();g->hp=0;CHECK(!tick(a,g,in).started&&a->stage==PE_SANDBOX_ACTION_IDLE);
    setup(g,a,12,false);in=input();for(int i=0;i<48;i++)tick(a,g,in);
    pe_world_set(&g->world,32,9,32,(PeVoxel){255,255});*copy=*g;action=*a;output=sentinel;
    CHECK(!pe_sandbox_actions_step(a,g,in,&output));
    CHECK(memcmp(g,copy,sizeof *g)==0&&memcmp(a,&action,sizeof action)==0&&memcmp(&output,&sentinel,sizeof output)==0);
    pe_world_set(&g->world,32,9,32,(PeVoxel){255,1});g->world.revision=UINT32_MAX;*copy=*g;
    CHECK(!pe_sandbox_actions_step(a,g,in,&output));
    CHECK(memcmp(g,copy,sizeof *g)==0&&memcmp(a,&action,sizeof action)==0&&memcmp(&output,&sentinel,sizeof output)==0);
    free(copy);
}
int pe_test_sandbox_actions(void){
    failures=0;PeGame *game=malloc(sizeof *game);if(!game)return 1;PeSandboxActions action;
    shovel_timing(game,&action);shovel_targets(game,&action);conservation(game,&action);felling(game,&action);pickaxe(game,&action);guards(game,&action);
    free(game);return failures;
}
