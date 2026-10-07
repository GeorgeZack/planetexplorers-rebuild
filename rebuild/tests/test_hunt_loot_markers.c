/* Exercise the real read-only host marker path, real reflected projection and
 * real collision snapshot. Only display dimensions and shared icon draw stop
 * at recording mocks; no window, graphics context or desktop input. */
#include "../src/pe_sandbox_view.c"
static unsigned failures,draw_count;
static int screen_width=1280,screen_height=720;
static const PeSandboxUI *expected_ui;
typedef struct {int item,count;PeTrainingUIRect rect;} Marker;
static Marker drawn[PE_HUNT_LOOT_CAPACITY];
#define CHECK(c) do{if(!(c)){fprintf(stderr,"hunt loot markers:%d %s\n",__LINE__,#c);failures++;}}while(0)
int GetScreenWidth(void){return screen_width;}
int GetScreenHeight(void){return screen_height;}
void pe_ui_draw_item(const PeUiCanvas *canvas,int id,int count,PeTrainingUIRect rect,bool box,Color tint){
    CHECK(canvas&&canvas->assets==expected_ui->assets&&canvas->effects==expected_ui->effects&&canvas->layout==&expected_ui->layout);
    CHECK(box&&tint.r==255&&tint.g==255&&tint.b==255&&tint.a==255);
    CHECK(draw_count<PE_HUNT_LOOT_CAPACITY);
    if(draw_count<PE_HUNT_LOOT_CAPACITY)drawn[draw_count++]=(Marker){id,count,rect};
}
static void render(PeGame *g,PeSandboxCollision *collision,Camera3D camera,PeSandboxUI *ui){
    PeGame *before=malloc(sizeof *before);CHECK(before!=NULL);if(!before)return;
    memcpy(before,g,sizeof *g);PeSandboxUI ui_before=*ui;Camera3D camera_before=camera;
    PeSandboxCollisionStats stats=pe_sandbox_collision_stats(collision);
    draw_count=0;memset(drawn,0,sizeof drawn);expected_ui=ui;
    sandbox_loot_markers(g,collision,camera,ui);
    CHECK(!memcmp(before,g,sizeof *g)&&!memcmp(&ui_before,ui,sizeof *ui)&&!memcmp(&camera_before,&camera,sizeof camera));
    PeSandboxCollisionStats after=pe_sandbox_collision_stats(collision);
    CHECK(after.generation==stats.generation&&after.terrain_rebuilds==stats.terrain_rebuilds&&after.construction_rebuilds==stats.construction_rebuilds&&after.triangles==stats.triangles);
    free(before);
}
static bool collision_update(PeSandboxCollision *c,PeGame *g,const PeSandboxBox *box,const PeTreeCapsule *tree){
    char error[256];bool okay=pe_sandbox_collision_update_with_trees(c,&g->world,NULL,0,box,box?1:0,tree,tree?1:0,error,sizeof error);
    if(!okay)fprintf(stderr,"marker collision fixture: %s\n",error);
    CHECK(okay);return okay;
}
static void one(PeGame *g,PeVec3 position){
    memset(&g->hunt_loot,0,sizeof g->hunt_loot);g->hunt_loot.count=1;
    g->hunt_loot.drops[0]=(PeHuntLootDrop){.instance_id=100,.item_id=229,.count=27,.position=position,.created_seconds=123};
}
static bool overlap(PeTrainingUIRect a,PeTrainingUIRect b){return a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y;}
int main(void){
    PeGame *g=calloc(1,sizeof *g);PeSandboxCollision *collision=pe_sandbox_collision_create();
    CHECK(g&&collision);if(!g||!collision){free(g);pe_sandbox_collision_free(collision);return 1;}
    g->world.revision=1;g->player=(PeVec3){20,5,20};g->rng_state=0xabcdefu;g->inventory.next_instance_id=108;
    PeSandboxUI ui={0};ui.layout.scale=1;
    Camera3D camera={.position={20,5.22f,10},.target={20,5.22f,20},.up={0,1,0},.fovy=60,.projection=CAMERA_PERSPECTIVE};
    CHECK(collision_update(collision,g,NULL,NULL));
    one(g,(PeVec3){20,5,20});render(g,collision,camera,&ui);
    CHECK(draw_count==1&&drawn[0].item==229&&drawn[0].count==27&&drawn[0].rect.w==28&&drawn[0].rect.h==28);
    /* Six drops from one death form disjoint rows at the projected root. */
    static const int items[]={229,1011,1008,236,1026,1228};
    for(unsigned i=1;i<6;i++){g->hunt_loot.drops[i]=g->hunt_loot.drops[0];g->hunt_loot.drops[i].instance_id+=i;g->hunt_loot.drops[i].item_id=items[i];g->hunt_loot.drops[i].count=(int)i;}
    g->hunt_loot.count=6;render(g,collision,camera,&ui);CHECK(draw_count==6);
    for(unsigned i=0;i<6;i++){
        CHECK(drawn[i].item==items[i]);
        for(unsigned j=0;j<i;j++)CHECK(!overlap(drawn[i].rect,drawn[j].rect));
    }
    CHECK(drawn[0].rect.y==drawn[1].rect.y&&drawn[1].rect.y==drawn[2].rect.y);
    CHECK(drawn[3].rect.y<drawn[0].rect.y&&drawn[0].rect.x==drawn[3].rect.x);
    /* A new root starts its own cluster, independent of preceding row count. */
    g->hunt_loot.drops[1].position.x+=2;g->hunt_loot.count=2;render(g,collision,camera,&ui);
    CHECK(draw_count==2&&drawn[0].rect.y==drawn[1].rect.y&&drawn[0].rect.x<drawn[1].rect.x);
    /* Marker range is root/player distance, not camera distance or anchor Y. */
    one(g,(PeVec3){20,5,29.99f});render(g,collision,camera,&ui);CHECK(draw_count==1);
    one(g,(PeVec3){20,5,30});render(g,collision,camera,&ui);CHECK(draw_count==0);
    one(g,(PeVec3){20,5,30.01f});render(g,collision,camera,&ui);CHECK(draw_count==0);
    one(g,(PeVec3){20,15.01f,20});render(g,collision,camera,&ui);CHECK(draw_count==0);
    one(g,(PeVec3){20,5,20});Camera3D reverse=camera;reverse.target.z=0;
    render(g,collision,reverse,&ui);CHECK(draw_count==0); /* Behind camera, still near player. */
    Camera3D close=camera;close.position.z=19;close.target.z=20;
    one(g,(PeVec3){29,5,20});render(g,collision,close,&ui);CHECK(draw_count==0); /* Offscreen but within10m. */
    g->player.z=15;
    one(g,(PeVec3){20,5,10});render(g,collision,camera,&ui);CHECK(draw_count==0); /* Anchor at camera, within marker range. */
    g->player.z=20;
    one(g,(PeVec3){20,5,20});
    PeSandboxBox wall={{19,4,14},{21,7,16}};
    CHECK(collision_update(collision,g,&wall,NULL));render(g,collision,camera,&ui);CHECK(draw_count==0);
    wall.minimum.z=21;wall.maximum.z=23;
    CHECK(collision_update(collision,g,&wall,NULL));render(g,collision,camera,&ui);CHECK(draw_count==1); /* Behind drop. */
    PeTreeCapsule tree={.center={20,5.22f,15},.radius=.5f,.height=3};
    CHECK(collision_update(collision,g,NULL,&tree));render(g,collision,camera,&ui);CHECK(draw_count==0);
    tree.center.x=23;CHECK(collision_update(collision,g,NULL,&tree));render(g,collision,camera,&ui);CHECK(draw_count==1);
    CHECK(collision_update(collision,g,NULL,NULL));
    screen_width=800;screen_height=450;ui.layout.scale=.625f;
    render(g,collision,camera,&ui);CHECK(draw_count==1&&drawn[0].rect.w==22&&drawn[0].rect.h==22);
    screen_width=2560;screen_height=1440;ui.layout.scale=2;
    render(g,collision,camera,&ui);CHECK(draw_count==1&&drawn[0].rect.w==56&&drawn[0].rect.h==56);
    g->hunt_loot.count=0;render(g,collision,camera,&ui);CHECK(draw_count==0);
    pe_sandbox_collision_free(collision);pe_game_destroy(g);free(g);
    printf("hunt loot marker host: %s (%u failures; real projection/collision)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
