/* Headless presentation contract. Scene/GPU boundaries are mocked; actual
 * PEAC/PEAS parsing and skin arithmetic have separate real-file tests. */
#include "pe_sandbox_actor.h"
#include "pe_actor_scene_view.h"
#include "pe_player_locomotion.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do{if(!(e)){fprintf(stderr,"sandbox actor:%d: %s\n",__LINE__,#e);failures++;}}while(0)
enum { BODY,KNIFE,SHOVEL_STOWED,SHOVEL_HELD,AXE_HELD,WOOD_AXE_HELD,PICKAXE_STOWED,PICKAXE_HELD,
       DIG_BODY,DIG_TOOL,FELL_BODY,FELL_TOOL,WOOD_FELL_TOOL,MINE_BODY,MINE_TOOL,VIEWS };
static const char *const folders[]={"player","player-knife-stowed","player-shovel-stowed","player-shovel","player-axe","player-wood-axe",
    "player-pickaxe-stowed","player-pickaxe","player-dig","player-shovel-dig","player-cut","player-axe-cut","player-wood-axe-cut","player-mine","player-pickaxe-mine"};
static PeActorClip clips[VIEWS][PE_LOCOMOTION_COUNT];
static bool window=true,optional=true,draw_failure=false,missing_clip=false;
static int missing_view=-1,bad_clock_view=-1,bad_source_view=-1;
static unsigned pickaxe_mask=15;
static int wrong_count_view=-1;
static int allocated,uploads,fail_upload,draw_count,mode_depth;
typedef struct {int view;size_t clip;double seconds;Vector3 position;float yaw;} Draw;
static Draw draws[16];
static int path_view(const char *path) {
    for(int i=0;i<VIEWS;i++){char fragment[80];snprintf(fragment,sizeof fragment,"/%s/",folders[i]);if(strstr(path,fragment))return i;}
    return -1;
}
bool IsWindowReady(void){return window;}
bool FileExists(const char *path){
    int v=path_view(path),bit=v==PICKAXE_STOWED?0:v==PICKAXE_HELD?1:v==MINE_BODY?2:v==MINE_TOOL?3:-1;
    return optional&&v!=missing_view&&(bit<0||(pickaxe_mask&(1u<<bit)));
}
void BeginMode3D(Camera3D camera){(void)camera;mode_depth++;}
void EndMode3D(void){mode_depth--;}
bool pe_actor_scene_view_prepare(PeActorSceneView *v,const char *path,char *error,size_t cap) {
    (void)error;(void)cap;int id=path_view(path);if(id<0)return false;
    v->manifest=(PeActorSceneManifest*)(uintptr_t)(id+1);v->count=1;allocated++;return true;
}
void pe_actor_scene_view_unload(PeActorSceneView *v){if(v&&v->manifest){allocated--;memset(v,0,sizeof *v);}}
bool pe_actor_scene_view_upload(PeActorSceneView *v,char *error,size_t cap) {
    (void)error;(void)cap;uploads++;if(uploads==fail_upload){pe_actor_scene_view_unload(v);return false;}v->loaded=true;return true;
}
size_t pe_actor_scene_view_clip_count(const PeActorSceneView *v){
    int id=(int)(uintptr_t)v->manifest-1;
    return id==wrong_count_view?4:id>=DIG_BODY?3:PE_LOCOMOTION_COUNT;
}
size_t pe_actor_scene_view_find_clip(const PeActorSceneView *v,const char *name) {
    int id=(int)(uintptr_t)v->manifest-1;
    for(size_t i=0;i<pe_actor_scene_view_clip_count(v);i++)if(!strcmp(name,clips[id][i].name))
        return missing_clip&&i==PE_LOCOMOTION_FALL?PE_ACTOR_BIND_POSE:i;
    return PE_ACTOR_BIND_POSE;
}
const PeActorClip *pe_actor_scene_view_clip_at(const PeActorSceneView *v,size_t i) {
    if(i>=pe_actor_scene_view_clip_count(v))return NULL;
    int id=(int)(uintptr_t)v->manifest-1;
    clips[id][i].fps=bad_clock_view==id?29:30;
    clips[id][i].terminal_at_duration=bad_source_view!=id;
    return &clips[id][i];
}
bool pe_actor_scene_view_draw_clip(PeActorSceneView *v,size_t clip,double seconds,Vector3 p,float yaw,float scale,Color tint) {
    (void)scale;(void)tint;if(draw_count<16)draws[draw_count]=(Draw){(int)(uintptr_t)v->manifest-1,clip,seconds,p,yaw};draw_count++;return !draw_failure;
}
const PeSlot *pe_game_equipped_tool(const PeGame *g) {
    for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->equipped_instance_id&&g->inventory.slots[i].instance_id==g->equipped_instance_id)return &g->inventory.slots[i];
    return NULL;
}
static void draw(PeSandboxActor *a,PeGame *g){draw_count=0;pe_sandbox_actor_draw(a,g);}
static void init_clips(void) {
    for(int v=0;v<DIG_BODY;v++)for(int i=0;i<PE_LOCOMOTION_COUNT;i++) {
        snprintf(clips[v][i].name,sizeof clips[v][i].name,"%s",pe_player_locomotion_name(i));
        clips[v][i].fps=30;clips[v][i].duration=2;clips[v][i].frames=61;
        clips[v][i].loop=i<3||i==PE_LOCOMOTION_FALL;clips[v][i].terminal_at_duration=true;
    }
    static const float length[3][3]={{.6999969482421875f,1.3333358764648438f,.8333358764648438f},{.6000003814697266f,1.100006103515625f,.6000003814697266f},
        {.6999969482421875f,1.3333358764648438f,.8333358764648438f}};
    static const unsigned frames[3][3]={{22,42,27},{20,35,20},{22,42,27}};
    for(int v=DIG_BODY;v<VIEWS;v++)for(int i=0;i<3;i++) {
        int kind=v>=MINE_BODY?2:v>=FELL_BODY?1:0;snprintf(clips[v][i].name,sizeof clips[v][i].name,"%s%d",kind==2?"Mine":kind==1?"Felling":"Spade",i+1);
        clips[v][i].fps=30;clips[v][i].duration=length[kind][i];clips[v][i].frames=frames[kind][i];clips[v][i].loop=i==1;clips[v][i].terminal_at_duration=true;
    }
}
int main(void) {
    int failures=0;char error[256];init_clips();
    window=false;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0);window=true;
    missing_clip=true;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);missing_clip=false;
    bad_clock_view=KNIFE;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);bad_clock_view=-1;
    bad_clock_view=DIG_TOOL;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);bad_clock_view=-1;
    bad_source_view=FELL_BODY;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);bad_source_view=-1;
    missing_view=WOOD_FELL_TOOL;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);missing_view=-1;
    missing_view=DIG_TOOL;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);missing_view=-1;
    /* Every nonempty incomplete pickaxe group must fail before GPU upload,
     * including a lone stowed scene. An entirely absent group stays optional. */
    for(pickaxe_mask=1;pickaxe_mask<15;pickaxe_mask++){
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);
    }
    for(int v=PICKAXE_STOWED;v<VIEWS;v++)if(v==PICKAXE_STOWED||v==PICKAXE_HELD||v==MINE_BODY||v==MINE_TOOL){
        bad_clock_view=v;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);
    }
    bad_clock_view=-1;bad_source_view=MINE_TOOL;
    CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);bad_source_view=-1;
    wrong_count_view=MINE_BODY;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);wrong_count_view=-1;
    for(int field=0;field<4;field++){
        if(field==0)clips[MINE_BODY][1].frames=50; /* Reject male Mine2. */
        if(field==1)clips[MINE_TOOL][1].duration=1.6000022888183594f;
        if(field==2)clips[MINE_TOOL][0].loop=true;
        if(field==3)snprintf(clips[MINE_BODY][1].name,sizeof clips[MINE_BODY][1].name,"Spade2");
        CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0&&uploads==0);init_clips();
    }
    fail_upload=7;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0);fail_upload=0;uploads=0;
    fail_upload=VIEWS;CHECK(!pe_sandbox_actor_load("assets",error,sizeof error));CHECK(allocated==0);fail_upload=0;uploads=0;
    PeSandboxActor *a=pe_sandbox_actor_load("assets",error,sizeof error);CHECK(a&&allocated==VIEWS&&uploads==VIEWS&&pe_sandbox_actor_ok(a));
    CHECK(pe_sandbox_actor_action_ready(a,12)&&pe_sandbox_actor_action_ready(a,1527));
    CHECK(pe_sandbox_actor_action_ready(a,19)&&pe_sandbox_actor_action_ready(a,26));
    PeGame *g=calloc(1,sizeof *g);if(!a||!g){pe_sandbox_actor_unload(a);free(g);return failures+1;}
    g->player=(PeVec3){20,10,20};g->grounded=true;g->yaw=.5f;
    pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draw_count==1&&draws[0].clip==PE_LOCOMOTION_IDLE);
    CHECK(fabsf(draws[0].yaw-(180-.5f*RAD2DEG))<.001f&&draws[0].position.x==20);
    g->velocity.x=5;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_IDLE);
    g->player.x+=.2f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_WALK);
    g->player.x+=.5f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_RUN);
    g->inventory.slots[0]=(PeSlot){.item_id=33,.count=1,.instance_id=1};g->equipped_instance_id=1;draw(a,g);
    CHECK(draw_count==2&&draws[1].view==KNIFE&&draws[1].clip==draws[0].clip&&draws[1].seconds==draws[0].seconds&&draws[1].yaw==draws[0].yaw);
    g->inventory.slots[0].item_id=12;draw(a,g);CHECK(draw_count==2&&draws[1].view==SHOVEL_STOWED);
    PeSandboxActionPose pose={.item_id=12,.instance_id=1,.held=true};
    CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[0].view==BODY&&draws[1].view==SHOVEL_HELD&&draws[0].seconds==draws[1].seconds);
    for(int phase=0;phase<3;phase++) {
        pose.clip=phase==0?"Spade1":phase==1?"Spade2":"Spade3";pose.seconds=.123456789;
        CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);
        CHECK(draw_count==2&&draws[0].view==DIG_BODY&&draws[1].view==DIG_TOOL);
        CHECK(draws[0].clip==(size_t)phase&&draws[1].clip==(size_t)phase&&draws[0].seconds==pose.seconds&&draws[1].seconds==pose.seconds);
        CHECK(draws[0].position.x==draws[1].position.x&&draws[0].yaw==draws[1].yaw);
    }
    /* Neither locomotion time, redraws, nor pause can advance gameplay's clock. */
    pe_sandbox_actor_update(a,.2f,g);draw(a,g);CHECK(draws[0].seconds==pose.seconds);
    pe_sandbox_actor_update(a,0,g);draw(a,g);CHECK(draws[0].seconds==pose.seconds);
    pose.seconds=NAN;CHECK(!pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[0].seconds==.123456789);
    pose.seconds=5;CHECK(!pe_sandbox_actor_set_action(a,&pose));pose.seconds=.2;pose.clip="Pickaxe";CHECK(!pe_sandbox_actor_set_action(a,&pose));
    pose.clip=NULL;CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[0].view==BODY&&draws[1].view==SHOVEL_HELD);
    pose.held=false;CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[1].view==SHOVEL_STOWED);
    pose.held=true;pose.clip="Spade2";pose.seconds=.6035147905349731;CHECK(pe_sandbox_actor_set_action(a,&pose));
    g->inventory.slots[0].instance_id=2;g->equipped_instance_id=2;draw(a,g);
    CHECK(draws[0].view==BODY&&draws[1].view==SHOVEL_STOWED); /* no stale bound-instance action */
    pose=(PeSandboxActionPose){.item_id=1527,.instance_id=2,.held=true,.clip="Felling2",.seconds=.612334668636322};
    g->inventory.slots[0].item_id=1527;CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);
    CHECK(draws[0].view==FELL_BODY&&draws[1].view==FELL_TOOL&&draws[0].clip==1&&draws[1].clip==1);
    CHECK(draws[0].seconds==pose.seconds&&draws[1].seconds==pose.seconds);
    draw_count=0;pe_sandbox_actor_preview(a,g);CHECK(mode_depth==0&&draws[0].view==BODY&&draws[1].view==AXE_HELD&&draws[0].clip==PE_LOCOMOTION_IDLE&&draws[0].position.x==0&&draws[0].yaw==0);
    draw(a,g);CHECK(draws[0].view==FELL_BODY&&draws[0].seconds==pose.seconds);
    CHECK(pe_sandbox_actor_set_action(a,NULL));draw(a,g);CHECK(draw_count==1&&draws[0].view==BODY); /* no invented axe stow */
    g->inventory.slots[0].item_id=26;pose.item_id=26;
    CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);
    CHECK(draw_count==2&&draws[0].view==FELL_BODY&&draws[1].view==WOOD_FELL_TOOL);
    CHECK(draws[0].seconds==pose.seconds&&draws[1].seconds==pose.seconds);
    pose.clip=NULL;CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);
    CHECK(draws[0].view==BODY&&draws[1].view==WOOD_AXE_HELD);
    CHECK(pe_sandbox_actor_set_action(a,NULL));draw(a,g);CHECK(draw_count==1);
    g->inventory.slots[0].item_id=19;draw(a,g);CHECK(draw_count==2&&draws[1].view==PICKAXE_STOWED);
    pose=(PeSandboxActionPose){.item_id=19,.instance_id=2,.held=true};
    CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);
    CHECK(draw_count==2&&draws[0].view==BODY&&draws[1].view==PICKAXE_HELD&&draws[0].seconds==draws[1].seconds);
    const char *mine_names[]={"Mine1","Mine2","Mine3"};
    for(int phase=0;phase<3;phase++){
        pose.clip=mine_names[phase];pose.seconds=phase==1?5.123456789:.123456789;
        CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);
        CHECK(draw_count==2&&draws[0].view==MINE_BODY&&draws[1].view==MINE_TOOL);
        CHECK(draws[0].clip==(size_t)phase&&draws[1].clip==(size_t)phase&&draws[0].seconds==pose.seconds&&draws[1].seconds==pose.seconds);
        CHECK(draws[0].yaw==draws[1].yaw&&draws[0].position.x==draws[1].position.x);
        pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].seconds==pose.seconds);
        pe_sandbox_actor_update(a,0,g);draw(a,g);CHECK(draws[0].seconds==pose.seconds);
    }
    pose.clip="Spade3";CHECK(!pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[0].view==MINE_BODY&&draws[0].seconds==.123456789);
    pose.clip="Mine3";pose.seconds=.8333358764648438;
    CHECK(pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draws[0].seconds==pose.seconds);
    pose.seconds+=.01;CHECK(!pe_sandbox_actor_set_action(a,&pose));
    draw_count=0;pe_sandbox_actor_preview(a,g);
    CHECK(mode_depth==0&&draws[0].view==BODY&&draws[1].view==PICKAXE_HELD&&draws[0].clip==PE_LOCOMOTION_IDLE&&draws[0].seconds==draws[1].seconds);
    draw(a,g);CHECK(draws[0].view==MINE_BODY&&draws[0].seconds==.8333358764648438);
    g->inventory.slots[0].instance_id=3;g->equipped_instance_id=3;draw(a,g);
    CHECK(draws[0].view==BODY&&draws[1].view==PICKAXE_STOWED); /* Replaced item19 instance is not held. */
    g->equipped_instance_id=0;draw(a,g);CHECK(draw_count==1&&draws[0].view==BODY);
    g->equipped_instance_id=3;CHECK(pe_sandbox_actor_set_action(a,NULL));draw(a,g);CHECK(draws[1].view==PICKAXE_STOWED);
    double before=draws[0].seconds;pe_sandbox_actor_update(a,0,g);draw(a,g);CHECK(draws[0].seconds==before);
    g->grounded=false;g->jumping=true;g->velocity.y=7;g->player.y+=.1f;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_JUMP&&draws[0].seconds==0);
    pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].seconds>.09);
    g->player=(PeVec3){40,15,40};g->grounded=true;g->jumping=false;pe_sandbox_actor_update(a,.1f,g);draw(a,g);CHECK(draws[0].clip==PE_LOCOMOTION_IDLE);
    before=draws[0].seconds;pe_sandbox_actor_update(a,NAN,g);draw(a,g);CHECK(draws[0].seconds==before);
    draw_failure=true;draw(a,g);CHECK(!pe_sandbox_actor_ok(a));draw_failure=false;draw(a,g);CHECK(draw_count==0);
    pe_sandbox_actor_unload(a);CHECK(allocated==0);
    pickaxe_mask=0;uploads=0;a=pe_sandbox_actor_load("assets",error,sizeof error);
    CHECK(a&&allocated==VIEWS-4&&!pe_sandbox_actor_action_ready(a,19)&&pe_sandbox_actor_action_ready(a,12));
    pose=(PeSandboxActionPose){.item_id=19,.instance_id=3,.held=true,.clip="Mine2",.seconds=.2};
    CHECK(!pe_sandbox_actor_set_action(a,&pose));draw(a,g);CHECK(draw_count==1&&draws[0].view==BODY);
    pe_sandbox_actor_unload(a);CHECK(allocated==0);pickaxe_mask=15;
    optional=false;a=pe_sandbox_actor_load("assets",error,sizeof error);CHECK(a&&allocated==1);
    CHECK(!pe_sandbox_actor_action_ready(a,12)&&!pe_sandbox_actor_action_ready(a,1527)&&!pe_sandbox_actor_action_ready(a,19));
    pe_sandbox_actor_unload(a);CHECK(allocated==0);free(g);
    if(!failures)puts("sandbox actor: exact source tool groups, shared action clocks, instance binding, pause/cancel and cleanup passed");
    return failures;
}
