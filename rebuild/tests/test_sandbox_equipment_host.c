/* Actual sandbox host helpers, CPU only. Actor presentation/asset callbacks are
 * mocked; equipped-slot lookup, tool pose validation, outfit ownership, source
 * item metadata and combat clocks use their production implementations.
 * Compile with function/data sections and --gc-sections; never runs the host. */
#include "../src/pe_sandbox_view.c"
static unsigned errors,asset_calls,load_calls,init_calls;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"sandbox equipment host:%d %s\n",__LINE__,#c);errors++;}}while(0)
struct PeSandboxActor {
    bool sword,knife,torch,ground,fail_action,fail_melee,fail_body,blend,bad_clock;
    unsigned action_calls,melee_calls,ground_calls;
    PeSandboxActionPose pose;
    PeCombat combat;
    const PeCombat *clock_pointer;
    char order[32];unsigned order_count;
};
static PeActorClip definitions[]={
    {.name="NormalIdle",.duration=1,.loop=true},
    {.name="AttackIdle",.duration=2,.loop=true},
    {.name="Attack6",.duration=1.2f},
    {.name="SwordWF",.duration=.8f,.loop=true},
    {.name="SwordRF",.duration=.7f,.loop=true}
};
static unsigned defence_identity,weapon_identity,box_identity;
static void record(PeSandboxActor *a,char code){if(a->order_count+1<sizeof a->order){a->order[a->order_count++]=code;a->order[a->order_count]=0;}}
bool pe_sandbox_actor_hold_ready(const PeSandboxActor *a,int item){return a&&(item==1?a->sword:item==33?a->knife:item==10?a->torch:false);}
bool pe_sandbox_actor_sword_ground_ready(const PeSandboxActor *a,const PeGame *g){
    ((PeSandboxActor*)a)->ground_calls++;return a->ground&&g->grounded&&!g->jumping&&!g->sprinting;
}
bool pe_sandbox_actor_set_action(PeSandboxActor *a,const PeSandboxActionPose *pose){
    if(!a||a->fail_action)return false;
    record(a,'A');a->action_calls++;a->pose=*pose;memset(&a->combat,0,sizeof a->combat);a->clock_pointer=NULL;return true;
}
bool pe_sandbox_actor_set_melee_item(PeSandboxActor *a,int item,uint32_t instance,bool held,const PeCombat *clock){
    if(!a||a->fail_melee)return false;
    /* The real melee setter owns its action binding; the host no longer clears it first. */
    PeSandboxActionPose pose={.item_id=item,.instance_id=instance,.held=held};
    if(!pe_sandbox_actor_set_action(a,&pose))return false;
    record(a,'M');a->melee_calls++;
    a->clock_pointer=clock;if(clock)a->combat=*clock;else memset(&a->combat,0,sizeof a->combat);return true;
}
bool pe_sandbox_actor_body_pose(const PeSandboxActor *a,const PeGame *g,PeSandboxBodyPose *out){
    (void)g;if(!a||a->fail_body||!out)return false;
    size_t clip=a->combat.active?2:a->pose.held&&a->pose.item_id==1?1:0;
    *out=(PeSandboxBodyPose){.clip=definitions[clip].name,.seconds=a->combat.active?a->combat.clip_time:.375,
        .duration=definitions[clip].duration,.loop=definitions[clip].loop,.yaw=37};
    if(a->blend){
        out->blend_count=2;
        for(unsigned i=0;i<2;i++){
            const PeActorClip *d=&definitions[3+i];
            out->blend[i].clip=d->name;out->blend[i].duration=d->duration;
            out->blend[i].loop=d->loop;out->blend[i].seconds=.1+i*.2;
            out->blend[i].weight=i?.75f:.25f;
        }
        if(a->bad_clock)out->blend[1].duration+=.01f;
    }else if(a->bad_clock)out->duration+=.01f;
    return true;
}
size_t pe_actor_find_clip(const PeActor *a,const char *name){
    if(a!=(const PeActor*)&defence_identity||!name)return PE_ACTOR_BIND_POSE;
    for(size_t i=0;i<sizeof definitions/sizeof definitions[0];i++)if(!strcmp(name,definitions[i].name))return i;
    return PE_ACTOR_BIND_POSE;
}
const PeActorClip *pe_actor_clip(const PeActor *a,size_t i){return a==(const PeActor*)&defence_identity&&i<sizeof definitions/sizeof definitions[0]?&definitions[i]:NULL;}
bool pe_sandbox_actor_melee_assets_for_item(const PeSandboxActor *a,int item,const PeActor **body,const PeActor **weapon){
    asset_calls++;if(!pe_sandbox_actor_hold_ready(a,item))return false;
    *body=(const PeActor*)&defence_identity;*weapon=(const PeActor*)&weapon_identity;return true;
}
PeHitboxes *pe_hitboxes_load(const char *path,const PeActor *a,char *error,size_t capacity){
    (void)error;(void)capacity;load_calls++;CHECK(a==(const PeActor*)&weapon_identity);
    CHECK(strstr(path,"player-wood-sword-drawn/wood-sword-drawn.pehb")||strstr(path,"player-knife-drawn/knife-drawn.pehb"));
    return (PeHitboxes*)&box_identity;
}
bool pe_sandbox_melee_init(PeSandboxMelee *out,const PeSandboxMeleeSetup *setup){init_calls++;*out=(PeSandboxMelee){.ready=true,.setup=*setup};pe_combat_init(&out->action);return true;}
void pe_sandbox_melee_cancel(PeSandboxMelee *h){pe_combat_stop(&h->action);h->instance_id=0;h->tick_pending=false;h->effected_count=0;}
static void reset_callbacks(PeSandboxActor *a){a->action_calls=a->melee_calls=a->ground_calls=a->order_count=0;a->order[0]=0;}
static void sword_poses(PeGame *g){
    PeSandboxActor a={.sword=true,.knife=true,.ground=true};PeSandboxActions tool;pe_sandbox_actions_init(&tool,false);
    PeSandboxMelee melee={.ready=true,.setup={.weapon_item_id=1}};pe_combat_init(&melee.action);
    PeHuntCombatPlayer queried={0};PeCombat original=melee.action;PeSandboxActions old_tool=tool;
    CHECK(sandbox_action_pose(&a,g,&tool,&melee,true));
    CHECK(!strcmp(a.order,"AM")&&a.pose.held&&a.pose.item_id==1&&a.pose.instance_id==91);
    CHECK(a.clock_pointer==&melee.action&&!memcmp(&melee.action,&original,sizeof original)&&!memcmp(&tool,&old_tool,sizeof tool));
    CHECK(hunt_player_pose(&a,g,(const PeActor*)&defence_identity,&melee,&queried));
    CHECK(queried.clip==1&&queried.seconds==.375&&queried.yaw==37);
    CHECK(queried.weapon==&g->inventory.slots[0]&&queried.base_attack==pe_player_defaults.attack&&queried.outfit==&g->outfit);
    /* Final rendering consumes the same helper; repeat does not advance either
     * action clock or change the body/defence callback's selected held state. */
    reset_callbacks(&a);CHECK(sandbox_action_pose(&a,g,&tool,&melee,true));
    PeHuntCombatPlayer rendered={0};CHECK(hunt_player_pose(&a,g,(const PeActor*)&defence_identity,&melee,&rendered));
    CHECK(!memcmp(&queried,&rendered,sizeof queried)&&!memcmp(&melee.action,&original,sizeof original));
    a.blend=true;CHECK(hunt_player_pose(&a,g,(const PeActor*)&defence_identity,&melee,&queried));
    CHECK(queried.blend_count==2&&queried.blend[0].clip==3&&queried.blend[1].clip==4&&
        queried.blend[0].seconds==.1&&queried.blend[1].seconds==.1+.2&&
        queried.blend[0].weight==.25f&&queried.blend[1].weight==.75f);
    for(unsigned blended=0;blended<2;blended++){
        a.blend=blended!=0;a.bad_clock=true;PeHuntCombatPlayer before=queried;
        CHECK(!hunt_player_pose(&a,g,(const PeActor*)&defence_identity,&melee,&queried));
        CHECK(!memcmp(&before,&queried,sizeof before));
    }
    a.blend=a.bad_clock=false;
    for(unsigned condition=0;condition<5;condition++){
        a.ground=condition!=0;g->grounded=condition!=1;g->jumping=condition==2;g->sprinting=condition==3;
        g->hp=condition==4?0:500;reset_callbacks(&a);
        CHECK(sandbox_action_pose(&a,g,&tool,&melee,true));
        CHECK(!a.pose.held&&!strcmp(a.order,"AM"));
        CHECK(hunt_player_pose(&a,g,(const PeActor*)&defence_identity,&melee,&queried)&&queried.clip==0);
        CHECK(!memcmp(&melee.action,&original,sizeof original));
    }
    a.ground=true;g->grounded=true;g->jumping=g->sprinting=false;g->hp=500;
    /* Active authored action overrides the passive-ground fallback. The host's
     * simulation action gate owns airborne cancellation, not this reader. */
    float stamina=100;CHECK(pe_combat_begin(&melee.action,PE_COMBAT_FEMALE,PE_COMBAT_THIRD_PERSON,1,&stamina));
    CHECK(pe_combat_advance(&melee.action,.375,.35,&stamina)!=PE_COMBAT_INVALID);original=melee.action;
    a.ground=false;reset_callbacks(&a);CHECK(sandbox_action_pose(&a,g,&tool,&melee,true));
    CHECK(a.pose.held&&!a.ground_calls&&a.clock_pointer==&melee.action&&!memcmp(&a.combat,&original,sizeof original));
    CHECK(hunt_player_pose(&a,g,(const PeActor*)&defence_identity,&melee,&queried)&&queried.clip==2&&queried.seconds==original.clip_time);
    CHECK(!memcmp(&melee.action,&original,sizeof original)&&stamina==95);
    melee.tick_cancelled=true;reset_callbacks(&a);CHECK(sandbox_action_pose(&a,g,&tool,&melee,true));
    CHECK(!a.clock_pointer&&!a.combat.active&&!memcmp(&melee.action,&original,sizeof original));
    melee.tick_cancelled=false;
    pe_combat_stop(&melee.action);reset_callbacks(&a);CHECK(sandbox_action_pose(&a,g,&tool,&melee,true)&&!a.pose.held);
    /* Input draw/stow affects binding without adding elapsed time. */
    a.ground=true;CHECK(sandbox_action_pose(&a,g,&tool,&melee,false)&&!a.pose.held);
    a.fail_action=true;reset_callbacks(&a);CHECK(!sandbox_action_pose(&a,g,&tool,&melee,true)&&!a.melee_calls);
    a.fail_action=false;a.fail_melee=true;CHECK(!sandbox_action_pose(&a,g,&tool,&melee,true));a.fail_melee=false;
    tool.seconds=NAN;reset_callbacks(&a);CHECK(!sandbox_action_pose(&a,g,&tool,&melee,true)&&!a.action_calls&&!a.melee_calls);
}
static void old_save_and_eligibility(PeGame *g){
    PeSandboxActor a={.knife=true,.ground=true};PeSandboxActions tool;pe_sandbox_actions_init(&tool,false);
    PeSandboxMelee absent={0},knife={.ready=true,.setup={.weapon_item_id=33}},sword={.ready=true,.setup={.weapon_item_id=1}};
    pe_combat_init(&knife.action);pe_combat_init(&sword.action);
    PeHitboxes *boxes=NULL;char error[80]={0};
    CHECK(sandbox_blade_prepare(&a,"absent-assets",1,&absent,&boxes,error,sizeof error));
    CHECK(!absent.ready&&!boxes&&!asset_calls&&!load_calls&&!init_calls);
    CHECK(sandbox_action_pose(&a,g,&tool,&knife,true)&&!a.pose.held&&a.melee_calls==0);
    PeHuntCombatPlayer p;CHECK(hunt_player_pose(&a,g,(const PeActor*)&defence_identity,&knife,&p));
    CHECK(!p.weapon&&p.clip==0&&!sandbox_blade_owned(&g->inventory.slots[0],&knife));
    CHECK(sandbox_blade_owned(&g->inventory.slots[0],&sword));
    g->inventory.slots[0].durability=0;CHECK(sandbox_blade_owned(&g->inventory.slots[0],&sword));
    CHECK(pe_game_equip_slot(g,0));PeToolStats stats;CHECK(pe_game_tool_stats(g,&stats)&&stats.attack==10);
    PeSlot saved=g->inventory.slots[0];
    for(unsigned invalid=0;invalid<4;invalid++){
        g->inventory.slots[0]=saved;if(invalid==0)g->inventory.slots[0].durability=NAN;
        if(invalid==1)g->inventory.slots[0].durability=-1;
        if(invalid==2)g->inventory.slots[0].durability=2370;
        if(invalid==3)g->inventory.slots[0].instance_id=0;
        CHECK(!sandbox_blade_owned(&g->inventory.slots[0],&sword));
    }
    g->inventory.slots[0]=(PeSlot){33,1,91,0};CHECK(!sandbox_blade_owned(&g->inventory.slots[0],&knife));
    g->inventory.slots[0].durability=1;CHECK(sandbox_blade_owned(&g->inventory.slots[0],&knife));
    reset_callbacks(&a);CHECK(sandbox_action_pose(&a,g,&tool,&knife,true)&&a.pose.held&&a.pose.item_id==33);
    CHECK(hunt_player_pose(&a,g,(const PeActor*)&defence_identity,&knife,&p)&&!p.weapon);
    g->inventory.slots[0]=saved;a.sword=true;
    CHECK(sandbox_blade_prepare(&a,"source-assets",1,&sword,&boxes,error,sizeof error));
    CHECK(asset_calls==1&&load_calls==1&&init_calls==1&&sword.ready&&sword.setup.weapon_item_id==1&&sword.setup.attack==20);
    CHECK(sword.setup.history_enabled&&sword.setup.playback_enabled&&sword.setup.history_local_center.x==0&&
        sword.setup.history_local_center.y==.875f&&sword.setup.history_local_center.z==0);
    knife.playback=(PeAttackPlayback){.last_contact=99,.time=99.1f,.initialized=true};
    PeAttackPlayback character_clock=knife.playback;
    sandbox_blade_rebind(&knife,&sword);
    CHECK(knife.setup.weapon_item_id==1&&!memcmp(&knife.playback,&character_clock,sizeof character_clock));
    /* Stable slot binding is reacquired from the current game after load. */
    g->inventory.slots[3]=g->inventory.slots[0];memset(&g->inventory.slots[0],0,sizeof(PeSlot));g->inventory.slots[3].instance_id=92;g->equipped_instance_id=92;
    CHECK(sandbox_action_pose(&a,g,&tool,&sword,true)&&a.pose.instance_id==92);
    CHECK(hunt_player_pose(&a,g,(const PeActor*)&defence_identity,&sword,&p)&&p.weapon==&g->inventory.slots[3]);
    PeHuntCombatPlayer before;memset(&p,0x5a,sizeof p);before=p;a.fail_body=true;
    CHECK(!hunt_player_pose(&a,g,(const PeActor*)&defence_identity,&sword,&p)&&!memcmp(&p,&before,sizeof p));
}
int main(void){
    PeGame *g=calloc(1,sizeof *g);CHECK(g);if(!g)return 1;
    g->mode=PE_GAME_ADVENTURE;g->hp=g->max_hp=500;g->grounded=true;
    g->inventory.next_instance_id=200;g->inventory.slots[0]=(PeSlot){1,1,91,2369};g->equipped_instance_id=91;
    CHECK(pe_outfit_init(g));sword_poses(g);old_save_and_eligibility(g);free(g);
    if(!errors)puts("sandbox equipment host: actual pose helpers, source clocks, old-save assets and depleted Sword passed (mock presentation; no graphics)");
    return errors?1:0;
}
