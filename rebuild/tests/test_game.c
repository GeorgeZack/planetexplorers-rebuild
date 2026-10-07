#define _POSIX_C_SOURCE 200809L
#include "adventure_test_helpers.h"
#include "pe_game.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int failures;
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL game %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
static PeVec3 eye(const PeGame *g){return (PeVec3){g->player.x,g->player.y+1.58f,g->player.z};}
static bool equal_cells(const PeB45Cell *a,const PeB45Cell *b,size_t count){
    for(size_t i=0;i<count;i++)if(a[i].x!=b[i].x||a[i].y!=b[i].y||a[i].z!=b[i].z||
        a[i].type!=b[i].type||a[i].material!=b[i].material)return false;
    return true;
}
static void flat(PeGame *g){
    pe_game_init(g,44);
    for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++)
        pe_world_set(&g->world,x,y,z,(PeVoxel){y<10?255:0,y<10?PE_MAT_DIRT:PE_MAT_AIR});
    g->player=(PeVec3){32,10.05f,32};g->velocity=(PeVec3){0,0,0};g->block_count=0;
    memset(&g->inventory,0,sizeof g->inventory);g->inventory.next_instance_id=1;memset(&g->craft,0,sizeof g->craft);
}
static void settle(PeGame *g){for(int i=0;i<120;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);}
static void advance(PeGame *g,float seconds){
    while(seconds>0){float dt=fminf(0.25f,seconds);pe_game_step(g,(PeInput){0},dt);seconds-=dt;}
}
static int add_tool(PeGame *g,int item){
    CHECK(pe_inventory_add(&g->inventory,item,1));
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==item){CHECK(pe_game_equip_slot(g,i));return i;}
    CHECK(false);return -1;
}
static bool mine_until_drop(PeGame *g,PeVec3 direction,int item){
    int before=pe_inventory_count(&g->inventory,item);
    for(int i=0;i<80;i++){
        advance(g,0.75f);
        if(!pe_game_mine(g,eye(g),direction))return false;
        if(pe_inventory_count(&g->inventory,item)>before)return true;
    }
    return false;
}
static void inventory_tests(void){
    PeInventory p={0},before;const PeItemDef *w=pe_item_def(PE_ITEM_WOOD);CHECK(w!=NULL);if(!w)return;
    CHECK(pe_inventory_add(&p,PE_ITEM_WOOD,w->stack_limit+3));
    CHECK(pe_inventory_count(&p,PE_ITEM_WOOD)==w->stack_limit+3);
    CHECK(p.slots[0].count==w->stack_limit&&p.slots[1].count==3);
    before=p;CHECK(!pe_inventory_remove(&p,PE_ITEM_WOOD,w->stack_limit+4));CHECK(memcmp(&p,&before,sizeof p)==0);
    CHECK(pe_inventory_remove(&p,PE_ITEM_WOOD,w->stack_limit+3));CHECK(pe_inventory_count(&p,PE_ITEM_WOOD)==0);
    CHECK(pe_inventory_add(&p,PE_ITEM_WOOD,w->stack_limit*PE_INVENTORY_SLOTS));
    before=p;CHECK(!pe_inventory_add(&p,PE_ITEM_STONE,1));CHECK(memcmp(&p,&before,sizeof p)==0);
    CHECK(!pe_inventory_add(&p,123456,1));CHECK(!pe_inventory_add(&p,PE_ITEM_WOOD,-1));
}
static void movement_tests(PeGame *g){
    flat(g);settle(g);CHECK(g->grounded);CHECK(g->player.y>9.49f&&g->player.y<9.65f);
    float start=g->player.y;pe_game_step(g,(PeInput){.jump=true},PE_FIXED_DT);CHECK(g->player.y>start);
    settle(g);CHECK(g->grounded);CHECK(fabsf(g->player.y-start)<0.1f);
    g->blocks[0]=(PeBlock){33,9,31,PE_ITEM_WOOD};g->blocks[1]=(PeBlock){33,9,32,PE_ITEM_WOOD};g->block_count=2;
    for(int i=0;i<120;i++)pe_game_step(g,(PeInput){.move_x=1},PE_FIXED_DT);
    CHECK(g->player.x<=32.701f);
    flat(g);settle(g);float x=g->player.x;
    for(int i=0;i<60;i++)pe_game_step(g,(PeInput){.move_x=1},PE_FIXED_DT);
    CHECK(g->player.x>x+4.9f&&g->player.x<x+5.1f);
    flat(g);settle(g);x=g->player.x;
    for(int i=0;i<60;i++)pe_game_step(g,(PeInput){.move_x=1,.walk=true},PE_FIXED_DT);
    CHECK(fabsf(g->player.x-x-2.0f)<0.05f);
    for(unsigned seed=1;seed<=4;seed++){
        pe_game_init(g,seed);CHECK(!pe_world_collides(&g->world,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));settle(g);x=g->player.x;
        for(int i=0;i<90;i++)pe_game_step(g,(PeInput){.move_x=1},PE_FIXED_DT);
        CHECK(g->player.x>x+4.0f);CHECK(!pe_world_collides(&g->world,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));
    }
}
static void movement_stamina_tests(PeGame *g){
    flat(g);settle(g);float x=g->player.x;
    for(int i=0;i<60;i++)pe_game_step(g,(PeInput){.move_x=1,.sprint=true,.walk=true},PE_FIXED_DT);
    CHECK(fabsf(g->player.x-x-8.0f)<0.05f);CHECK(fabsf(g->stamina-95.0f)<0.01f);CHECK(g->sprinting);
    /* A started sprint continues below the entry threshold, until exhausted. */
    g->stamina=1;pe_game_step(g,(PeInput){.move_x=1,.sprint=true},0.1f);
    CHECK(g->sprinting&&fabsf(g->stamina-0.5f)<0.001f);
    pe_game_step(g,(PeInput){.move_x=1,.sprint=true},0.1f);
    CHECK(!g->sprinting&&g->stamina==0);CHECK(g->velocity.x==PE_RUN_SPEED);
    pe_game_step(g,(PeInput){.move_x=1,.sprint=true},0.1f);
    CHECK(!g->sprinting&&g->stamina==0); /* Recovery delay. */
    pe_game_step(g,(PeInput){.move_x=1,.sprint=true},0.1f);
    CHECK(!g->sprinting&&fabsf(g->stamina-4)<0.001f);
    flat(g);settle(g);g->stamina=14.9f;
    pe_game_step(g,(PeInput){.move_x=1,.sprint=true},PE_FIXED_DT);
    CHECK(!g->sprinting&&g->velocity.x==PE_RUN_SPEED);
    g->stamina=15;pe_game_step(g,(PeInput){.move_x=1,.sprint=true},PE_FIXED_DT);
    CHECK(g->sprinting&&g->stamina<15);
    float stamina=g->stamina;
    pe_game_step(g,(PeInput){.sprint=true},0.1f);
    CHECK(!g->sprinting&&fabsf(g->stamina-stamina)<0.001f);
    pe_game_step(g,(PeInput){.sprint=true},0.1f);
    CHECK(fabsf(g->stamina-stamina-4)<0.001f); /* Stationary Shift does not drain. */
    flat(g);settle(g);g->stamina=9.9f;x=g->player.y;
    pe_game_step(g,(PeInput){.jump=true},PE_FIXED_DT);
    CHECK(!g->jumping&&g->grounded&&fabsf(g->player.y-x)<0.02f);
    g->stamina=10;pe_game_step(g,(PeInput){.jump=true},PE_FIXED_DT);
    CHECK(g->jumping&&!g->grounded&&g->stamina==0);
    pe_game_step(g,(PeInput){.jump=true,.sprint=true,.move_x=1},0.25f);
    CHECK(g->jumping&&g->stamina==0&&!g->sprinting);
    for(int i=0;i<120&&!g->grounded;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    CHECK(g->grounded&&!g->jumping);CHECK(g->stamina==0);
    /* Landing detection may have credited one fixed frame of idle time. */
    advance(g,0.05f);CHECK(g->stamina==0);
    advance(g,0.15f);CHECK(g->stamina>3.9f&&g->stamina<4.8f);
}
static void action_motion_tests(PeGame *g){
    PeSandboxCollision *c=pe_sandbox_collision_create();char error[160];
    unsigned char *before=malloc(sizeof *g);CHECK(c&&before);
    if(!c||!before){pe_sandbox_collision_free(c);free(before);return;}
    flat(g);CHECK(pe_game_collision_update(g,c,error,sizeof error));CHECK(pe_game_collision_settle(g,c));
    g->stamina=30;g->medpack_cooldown=5;
    PeVec3 start=g->player;double clock=g->game_seconds;
    PeInput held={.move_x=-1,.move_z=1,.jump=true,.sprint=true};
    for(int i=0;i<60;i++)CHECK(pe_game_step_collision_motion(g,held,PE_FIXED_DT,c,(PeVec3){3,0,0}));
    CHECK(fabsf(g->player.x-start.x-3)<.01f&&fabsf(g->player.z-start.z)<.001f);
    CHECK(g->grounded&&!g->jumping&&!g->sprinting&&g->stamina==30);
    CHECK(fabs(g->game_seconds-clock-12)<.001&&fabsf(g->medpack_cooldown-4)<.001f);
    memcpy(before,g,sizeof *g);
    PeVec3 invalid[]={{NAN,0,0},{0,1,0},{101,0,0},{0,0,INFINITY}};
    for(size_t i=0;i<sizeof invalid/sizeof *invalid;i++){
        CHECK(!pe_game_step_collision_motion(g,held,PE_FIXED_DT,c,invalid[i]));CHECK(!memcmp(before,g,sizeof *g));
    }
    g->velocity.y=-1000;memcpy(before,g,sizeof *g);
    CHECK(!pe_game_step_collision_motion(g,held,PE_FIXED_DT,c,(PeVec3){0}));CHECK(!memcmp(before,g,sizeof *g));
    g->player=(PeVec3){32,10,32};g->velocity=(PeVec3){0};CHECK(pe_game_collision_settle(g,c));
    g->blocks[0]=(PeBlock){33,9,32,PE_ITEM_FENCE};g->block_count=1;
    CHECK(pe_game_collision_update(g,c,error,sizeof error));
    for(int i=0;i<60;i++)CHECK(pe_game_step_collision_motion(g,held,PE_FIXED_DT,c,(PeVec3){12,0,0}));
    CHECK(g->player.x>32.65f&&g->player.x<32.71f);
    CHECK(!pe_sandbox_collision_overlap(c,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));
    start=g->player;g->block_count=0;CHECK(pe_game_collision_update(g,c,error,sizeof error));
    CHECK(pe_game_step_collision_motion(g,held,PE_FIXED_DT,c,(PeVec3){0}));
    CHECK(fabsf(g->player.x-start.x)<.001f); /* No retained blocked displacement or WASD addition. */
    g->player.y=20;g->velocity.y=0;start=g->player;
    CHECK(pe_game_step_collision_motion(g,held,.1f,c,(PeVec3){2,0,0}));
    CHECK(fabsf(g->player.x-start.x-.2f)<.001f&&g->player.y<start.y&&g->velocity.y<0);
    pe_sandbox_collision_free(c);free(before);
}
static void collision_game_tests(PeGame *g){
    PeSandboxCollision *collision=pe_sandbox_collision_create();PeGame *before=malloc(sizeof *before);char error[160];
    CHECK(collision&&before);if(!collision||!before){pe_sandbox_collision_free(collision);free(before);return;}
    flat(g);CHECK(pe_game_collision_update(g,collision,error,sizeof error));
    g->player.y=9.45f;g->velocity=(PeVec3){1,2,3};g->jumping=true;g->jump_action_elapsed=1;
    CHECK(pe_game_collision_settle(g,collision));CHECK(g->player.x==32&&g->player.z==32&&g->player.y>9.49f&&g->player.y<9.51f);
    CHECK(g->grounded&&!g->jumping&&g->velocity.y==0&&g->jump_action_elapsed==0);
    float start=g->player.x;
    for(int i=0;i<60;i++)CHECK(pe_game_step_collision(g,(PeInput){.move_x=1},PE_FIXED_DT,collision));
    CHECK(fabsf(g->player.x-start-5)<.01f&&g->grounded);
    CHECK(!pe_sandbox_collision_overlap(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));
    g->player=(PeVec3){32,2,32};*before=*g;
    CHECK(!pe_game_collision_settle(g,collision));CHECK(memcmp(g,before,sizeof *g)==0);
    CHECK(!pe_game_step_collision(g,(PeInput){.jump=true},PE_FIXED_DT,collision));CHECK(memcmp(g,before,sizeof *g)==0);
    g->player=(PeVec3){32,10,32};CHECK(pe_game_collision_settle(g,collision));*before=*g;
    CHECK(!pe_game_step_collision(g,(PeInput){0},NAN,collision));CHECK(memcmp(g,before,sizeof *g)==0);
    /* The bounded crop cannot sustain a three-second physical fall. Hold the
     * test body aloft between updates to isolate the original jump action clock. */
    g->jumping=true;g->jump_action_elapsed=0;g->stamina=30;g->stamina_idle=0;
    for(int i=0;i<12;i++){g->player.y=25;g->velocity.y=0;CHECK(pe_game_step_collision(g,(PeInput){0},.25f,collision));}
    CHECK(g->jumping&&g->jump_action_elapsed==3&&g->stamina==30);
    g->player.y=25;g->velocity.y=0;CHECK(pe_game_step_collision(g,(PeInput){0},.25f,collision));
    CHECK(!g->jumping&&g->stamina==30); /* The ending action frame still delays regeneration. */
    g->player.y=25;g->velocity.y=0;CHECK(pe_game_step_collision(g,(PeInput){0},.1f,collision));CHECK(g->stamina==30);
    g->player.y=25;g->velocity.y=0;CHECK(pe_game_step_collision(g,(PeInput){0},.1f,collision));CHECK(fabsf(g->stamina-34)<.001f);
    /* The adapter must include furniture and current cells, not only terrain. */
    g->player=(PeVec3){32,10,32};g->velocity=(PeVec3){0};CHECK(pe_game_collision_settle(g,collision));
    g->blocks[0]=(PeBlock){33,9,32,PE_ITEM_FENCE};g->block_count=1;
    CHECK(pe_game_collision_update(g,collision,error,sizeof error));
    for(int i=0;i<60;i++)CHECK(pe_game_step_collision(g,(PeInput){.move_x=1},PE_FIXED_DT,collision));
    CHECK(g->player.x<32.71f&&g->player.x>32.65f);
    g->block_count=0;g->cells[0]=(PeB45Cell){66,19,64,PE_B45_CUBE<<2,0};g->cells[1]=(PeB45Cell){66,20,64,PE_B45_CUBE<<2,0};g->cell_count=2;
    CHECK(pe_game_collision_update(g,collision,error,sizeof error));
    CHECK(pe_sandbox_collision_overlap(collision,(PeVec3){33.2f,9.6f,32.25f},PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));
    CHECK(pe_game_init_mode(g,101,PE_GAME_ADVENTURE));
    CHECK(pe_game_collision_update(g,collision,error,sizeof error));
    CHECK(!pe_sandbox_collision_overlap(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));
    *before=*g;CHECK(!pe_game_init_mode(g,1,(PeGameMode)99));CHECK(memcmp(g,before,sizeof *g)==0);
    PeWorld *imported=malloc(sizeof *imported);CHECK(imported!=NULL);
    if(imported){
        memset(imported,0,sizeof *imported);imported->source=1;imported->revision=1;
        for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<10;y++)for(int x=0;x<PE_WORLD_X;x++)pe_world_set(imported,x,y,z,(PeVoxel){255,1});
        CHECK(pe_test_ack_welcome(g));CHECK(pe_game_arrival_complete(g));CHECK(pe_game_set_world(g,imported,(PeVec3){32,10,32}));
        CHECK(g->mode==PE_GAME_ADVENTURE&&g->arrival_complete&&g->tree_count==0&&g->cell_count==0);
        CHECK(pe_inventory_count(&g->inventory,279)==200);
        CHECK(pe_game_collision_update(g,collision,error,sizeof error));
        CHECK(!pe_sandbox_collision_overlap(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));
        *before=*g;memset(imported->voxels,255,sizeof imported->voxels);
        CHECK(!pe_game_set_world(g,imported,(PeVec3){32,10,32}));CHECK(memcmp(g,before,sizeof *g)==0);
        free(imported);
    }
    pe_sandbox_collision_free(collision);free(before);
}
static void craft_tests(PeGame *g){
    flat(g);CHECK(pe_recipe_count>0);if(!pe_recipe_count)return;const PeRecipeDef *r=&pe_recipes[0];
    CHECK(!pe_game_start_craft(g,r->id,1));
    for(int i=0;i<r->ingredient_count;i++)CHECK(pe_inventory_add(&g->inventory,r->ingredients[i].item_id,r->ingredients[i].count*2));
    PeInventory before=g->inventory;CHECK(pe_game_start_craft(g,r->id,2));CHECK(!pe_game_start_craft(g,r->id,1));
    CHECK(memcmp(&before,&g->inventory,sizeof before)==0);
    int frames=(int)ceilf(2*r->seconds/PE_FIXED_DT)+4;
    for(int i=0;i<frames;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    CHECK(g->craft.batches_left==0);CHECK(pe_inventory_count(&g->inventory,r->output_id)==2*r->output_count);
    CHECK(g->crafted==2);
    /* A running job must neither lose inputs nor produce output when blocked. */
    flat(g);
    for(int i=0;i<r->ingredient_count;i++)CHECK(pe_inventory_add(&g->inventory,r->ingredients[i].item_id,r->ingredients[i].count));
    CHECK(pe_game_start_craft(g,r->id,1));
    const PeItemDef *wood=pe_item_def(PE_ITEM_WOOD);
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)g->inventory.slots[i]=(PeSlot){.item_id=PE_ITEM_WOOD,.count=wood->stack_limit};
    before=g->inventory;
    for(int i=0;i<frames;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    CHECK(g->craft.batches_left==1);CHECK(memcmp(&before,&g->inventory,sizeof before)==0);
}
static void interaction_tests(PeGame *g){
    flat(g);settle(g);
    g->tree_count=1;g->trees[0]=(PeTree){{33,g->player.y,32},1,255};
    CHECK(pe_game_gather(g));CHECK(!pe_game_gather(g));CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==1);
    CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,3));
    PeVec3 d={1,-1,0};int count=pe_inventory_count(&g->inventory,PE_ITEM_WOOD);
    CHECK(pe_game_place_shape(g,eye(g),d,PE_ITEM_WOOD,PE_B45_CUBE,0,4));CHECK(g->cell_count==4);CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==count-1);
    if(g->cell_count){PeB45Cell b=g->cells[0];PeVec3 o=eye(g);d=(PeVec3){b.x*0.5f+0.25f-o.x,b.y*0.5f+0.5f-o.y,b.z*0.5f+0.25f-o.z};
        CHECK(pe_game_remove_shape(g,o,d,4));CHECK(g->cell_count==0);CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==count);}
    PeInventory before=g->inventory;CHECK(!pe_game_place(g,eye(g),(PeVec3){0,1,0},PE_ITEM_WOOD));CHECK(memcmp(&before,&g->inventory,sizeof before)==0);
    uint64_t hash=pe_world_hash(&g->world);CHECK(!pe_game_mine(g,eye(g),(PeVec3){1,-1,0}));CHECK(pe_world_hash(&g->world)==hash);
    add_tool(g,19);CHECK(mine_until_drop(g,(PeVec3){1,-1,0},PE_ITEM_DIRT));CHECK(pe_world_hash(&g->world)!=hash);CHECK(pe_inventory_count(&g->inventory,PE_ITEM_DIRT)==2);
}
static void ceiling_mining_test(PeGame *g){
    flat(g);
    for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++)
        pe_world_set(&g->world,x,y,z,(PeVoxel){y>=8?255:0,y>=8?PE_MAT_STONE:PE_MAT_AIR});
    g->player=(PeVec3){20,5,20};
    add_tool(g,19);
    /* Keep the upward mining position fixed rather than falling out of range. */
    for(int i=0;i<20&&pe_inventory_count(&g->inventory,PE_ITEM_STONE)==0;i++){
        g->tool_cooldown=0;CHECK(pe_game_mine(g,eye(g),(PeVec3){0,1,0}));
    }
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_STONE)==2);
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_DIRT)==0);
}
static void dig_indicator_game_tests(PeGame *g){
    flat(g);g->player=(PeVec3){32,10.6f,32};add_tool(g,19);
    /* A supported 100-density boundary sample is the indicator target even
     * though it is below the mesh isovalue; its own material supplies yield. */
    pe_world_set(&g->world,32,9,32,(PeVoxel){255,PE_MAT_STONE});
    pe_world_set(&g->world,32,10,32,(PeVoxel){100,PE_MAT_DIRT});
    CHECK(pe_game_mine(g,eye(g),(PeVec3){0,-1,0}));
    CHECK(pe_world_get(&g->world,32,10,32).volume==0);
    CHECK(pe_world_get(&g->world,32,9,32).volume==255);
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_DIRT)==2);
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_STONE)==0);
    /* Reach is measured from feet+up to the snapped voxel, not the camera. */
    flat(g);g->player=(PeVec3){32,10.2f,32};add_tool(g,19);
    pe_world_set(&g->world,32,9,32,(PeVoxel){100,PE_MAT_DIRT});
    PeVec3 origin={32,13.4f,32};
    CHECK(!pe_world_raycast(&g->world,origin,(PeVec3){0,-1,0},3).hit);
    CHECK(pe_game_mine(g,origin,(PeVec3){0,-1,0}));
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_DIRT)==2);
    /* Exactly 3 m from body center is outside the source's strict bound. */
    flat(g);g->player=(PeVec3){32,11,32};add_tool(g,19);
    uint64_t hash=pe_world_hash(&g->world);float wear=pe_game_equipped_tool(g)->durability;
    CHECK(!pe_game_mine(g,eye(g),(PeVec3){0,-1,0}));
    CHECK(pe_world_hash(&g->world)==hash&&pe_game_equipped_tool(g)->durability==wear);
    CHECK(g->stamina==100);
    /* Insufficient inventory space must preserve selected boundary density. */
    flat(g);g->player=(PeVec3){32,10.6f,32};add_tool(g,19);
    pe_world_set(&g->world,32,10,32,(PeVoxel){100,PE_MAT_DIRT});
    CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,pe_item_def(PE_ITEM_WOOD)->stack_limit*(PE_INVENTORY_SLOTS-1)));
    hash=pe_world_hash(&g->world);wear=pe_game_equipped_tool(g)->durability;
    CHECK(!pe_game_mine(g,eye(g),(PeVec3){0,-1,0}));
    CHECK(pe_world_hash(&g->world)==hash&&pe_game_equipped_tool(g)->durability==wear&&g->stamina==100);
}
static void instance_tool_tests(PeGame *g){
    flat(g);settle(g);CHECK(pe_inventory_add(&g->inventory,19,2));
    CHECK(g->inventory.slots[0].instance_id&&g->inventory.slots[1].instance_id&&g->inventory.slots[0].instance_id!=g->inventory.slots[1].instance_id);
    CHECK(g->inventory.slots[0].durability==3553);CHECK(pe_game_equip_slot(g,0));uint32_t id=g->equipped_instance_id;
    PeSlot swap=g->inventory.slots[0];g->inventory.slots[0]=g->inventory.slots[1];g->inventory.slots[1]=swap;
    CHECK(pe_game_equipped_tool(g)->instance_id==id);CHECK(pe_game_equipped_tool(g)==&g->inventory.slots[1]);
    PeToolStats stats;CHECK(pe_game_tool_stats(g,&stats));CHECK(stats.mining_damage==375);CHECK(fabsf(stats.mining_radius-0.49f)<0.001f);
    CHECK(stats.stamina_cost==4&&stats.durability_cost==5);
    CHECK(pe_game_mine(g,eye(g),(PeVec3){1,-1,0}));CHECK(pe_game_equipped_tool(g)->durability==3548);CHECK(g->stamina==96);
    uint64_t hash=pe_world_hash(&g->world);float wear=pe_game_equipped_tool(g)->durability;
    CHECK(!pe_game_mine(g,eye(g),(PeVec3){1,-1,0}));CHECK(pe_world_hash(&g->world)==hash&&pe_game_equipped_tool(g)->durability==wear);
    advance(g,0.25f);CHECK(g->stamina==96);advance(g,0.5f);CHECK(g->stamina==100);
    g->stamina=0;g->tool_cooldown=0;CHECK(pe_game_mine(g,eye(g),(PeVec3){1,-1,0}));CHECK(g->stamina==0&&g->tool_cooldown>0.61f);
    g->inventory.slots[1].durability=0;g->tool_cooldown=0;hash=pe_world_hash(&g->world);
    CHECK(!pe_game_mine(g,eye(g),(PeVec3){1,-1,0}));CHECK(pe_world_hash(&g->world)==hash);CHECK(!pe_game_equip_slot(g,1));
    CHECK(pe_game_equip_slot(g,0));CHECK(pe_game_equipped_tool(g)->durability==3553);
    pe_game_unequip(g);CHECK(!pe_game_equipped_tool(g));
    CHECK(pe_inventory_add(&g->inventory,308,1));CHECK(pe_inventory_add(&g->inventory,1,1));
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==308||g->inventory.slots[i].item_id==1)CHECK(!pe_game_equip_slot(g,i));
    flat(g);settle(g);add_tool(g,26);g->tree_count=1;g->trees[0]=(PeTree){{33,g->player.y,32},8,255};
    for(int i=0;i<6;i++){g->tool_cooldown=0;CHECK(pe_game_gather(g));}
    CHECK(g->trees[0].remaining==0&&g->trees[0].health==0);CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==8);
    CHECK(pe_game_equipped_tool(g)->durability==pe_item_def(26)->durability_max-30);
}
static void pending_craft_tests(PeGame *g){
    flat(g);CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,50));PeInventory before=g->inventory;
    CHECK(!pe_game_start_craft(g,1,6));CHECK(memcmp(&before,&g->inventory,sizeof before)==0);
    CHECK(pe_game_start_craft(g,1,5));advance(g,3.25f);
    CHECK(g->craft.batches_pending==1&&g->craft.batches_left==4);CHECK(pe_inventory_count(&g->inventory,12)==0);CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==40);
    advance(g,1.75f);CHECK(pe_inventory_count(&g->inventory,12)==0&&g->craft.batches_pending==1);
    advance(g,1);CHECK(pe_inventory_count(&g->inventory,12)==2&&g->craft.batches_pending==0);
    CHECK(pe_game_cancel_craft(g));CHECK(g->craft.recipe_id==0);CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==30);
    CHECK(pe_game_start_craft(g,1,3));advance(g,3.25f);CHECK(g->craft.batches_pending==1);before=g->inventory;
    const PeItemDef *wood=pe_item_def(PE_ITEM_WOOD);
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)g->inventory.slots[i]=(PeSlot){.item_id=PE_ITEM_WOOD,.count=wood->stack_limit};
    PeInventory full=g->inventory;PeCraftJob job=g->craft;
    CHECK(!pe_game_cancel_craft(g));CHECK(memcmp(&full,&g->inventory,sizeof full)==0&&memcmp(&job,&g->craft,sizeof job)==0);
    g->inventory=before;CHECK(pe_game_cancel_craft(g));CHECK(pe_inventory_count(&g->inventory,12)==3);CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==20);
    /* Partial work before first completion costs no ingredients on cancellation. */
    before=g->inventory;CHECK(pe_game_start_craft(g,1,1));advance(g,1);CHECK(pe_game_cancel_craft(g));CHECK(memcmp(&before,&g->inventory,sizeof before)==0);
}
static void imported_world_tests(PeGame *g){
    flat(g);settle(g);add_tool(g,19);CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,10));CHECK(pe_game_start_craft(g,1,1));
    PeWorld *world=malloc(sizeof *world);PeGame *before=malloc(sizeof *before);CHECK(world&&before);if(!world||!before){free(world);free(before);return;}
    *before=*g;memset(world,0,sizeof *world);world->source=1;
    CHECK(!pe_game_set_world(g,world,(PeVec3){32,10,32}));CHECK(memcmp(g,before,sizeof *g)==0);
    *world=g->world;world->source=1;world->origin_x=-32;world->origin_y=128;world->origin_z=1024;
    for(int i=0;i<PE_VOXEL_COUNT;i++)if(world->voxels[i].volume)world->voxels[i].material=17;
    CHECK(pe_game_set_world(g,world,(PeVec3){32,10,32}));CHECK(g->world.source==1&&g->tree_count==0&&g->block_count==0);
    CHECK(g->equipped_instance_id==before->equipped_instance_id&&memcmp(&g->inventory,&before->inventory,sizeof g->inventory)==0&&memcmp(&g->craft,&before->craft,sizeof g->craft)==0);
    CHECK(mine_until_drop(g,(PeVec3){1,-1,0},PE_ITEM_STONE));CHECK(pe_inventory_count(&g->inventory,PE_ITEM_STONE)==2);
    for(int i=0;i<PE_VOXEL_COUNT;i++)if(world->voxels[i].volume)world->voxels[i].material=55;
    CHECK(pe_game_set_world(g,world,(PeVec3){32,10,32}));
    g->tool_cooldown=0;PeInventory empty_before=g->inventory;uint32_t rng_before=g->rng_state;uint64_t hash_before=pe_world_hash(&g->world);
    CHECK(pe_game_mine(g,eye(g),(PeVec3){1,-1,0}));CHECK(pe_world_hash(&g->world)!=hash_before&&g->rng_state==rng_before);
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)CHECK(g->inventory.slots[i].count==empty_before.slots[i].count);
    for(int i=0;i<PE_VOXEL_COUNT;i++)if(g->world.voxels[i].volume)g->world.voxels[i].material=255;
    g->tool_cooldown=0;*before=*g;CHECK(!pe_game_mine(g,eye(g),(PeVec3){1,-1,0}));
    CHECK(pe_world_hash(&g->world)==pe_world_hash(&before->world)&&memcmp(&g->inventory,&before->inventory,sizeof g->inventory)==0);
    free(before);free(world);
}
static void block_game_tests(PeGame *g){
    flat(g);settle(g);CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,3));CHECK(pe_inventory_add(&g->inventory,PE_ITEM_STONE,1));
    PeB45Cell cells[4]={{68,20,64,4,11},{69,20,64,4,11},{68,20,65,4,11},{69,20,65,4,11}};
    CHECK(pe_game_build_cells(g,cells,4));CHECK(g->cell_count==4&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==2);
    PeInventory before=g->inventory;CHECK(!pe_game_build_cells(g,cells,4));CHECK(g->cell_count==4&&memcmp(&before,&g->inventory,sizeof before)==0);
    int duplicated[]={0,0};CHECK(!pe_game_remove_cells(g,duplicated,2));CHECK(g->cell_count==4);
    int all[]={0,1,2,3};CHECK(pe_game_remove_cells(g,all,4));CHECK(g->cell_count==0&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==3);
    CHECK(pe_game_build_cells(g,cells,1));CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==2);CHECK(pe_game_remove_cells(g,all,1));CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==2);
    /* Mixed materials round independently, rather than charging the whole brush once. */
    cells[1].material=12;CHECK(pe_game_build_cells(g,cells,2));CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==1&&pe_inventory_count(&g->inventory,PE_ITEM_STONE)==0);
    CHECK(pe_game_remove_cells(g,all,2));CHECK(pe_inventory_count(&g->inventory,PE_ITEM_STONE)==0&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==1);
    before=g->inventory;CHECK(!pe_game_build_cells(g,cells,2));CHECK(memcmp(&before,&g->inventory,sizeof before)==0&&g->cell_count==0);
    PeB45Cell invalid={64,19,64,4,11};CHECK(!pe_game_build_cells(g,&invalid,1)); /* player */
    invalid=(PeB45Cell){68,17,64,4,11};CHECK(!pe_game_build_cells(g,&invalid,1)); /* terrain */
    invalid=(PeB45Cell){127,20,64,4,11};CHECK(!pe_game_build_cells(g,&invalid,1)); /* bounds */
    invalid=(PeB45Cell){100,20,64,4,11};CHECK(!pe_game_build_cells(g,&invalid,1)); /* reach */
    invalid=(PeB45Cell){68,20,64,255,11};CHECK(!pe_game_build_cells(g,&invalid,1)); /* unsupported shape */
    CHECK(memcmp(&before,&g->inventory,sizeof before)==0);
    for(int rotation=0;rotation<4;rotation++){
        PeB45Cell ramp={68,20,64,(uint8_t)((PE_B45_RAMP<<2)|rotation),11};
        CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,1));CHECK(pe_game_build_cells(g,&ramp,1));
        CHECK(g->cells[0].type==ramp.type);CHECK(pe_game_remove_cells(g,all,1));
    }
    /* Collision with stacked cells cannot be bypassed by the grounded step-up. */
    CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,2));
    PeB45Cell wall[4]={{66,19,63,4,11},{66,20,63,4,11},{66,19,64,4,11},{66,20,64,4,11}};
    CHECK(pe_game_build_cells(g,wall,4));
    for(int i=0;i<90;i++)pe_game_step(g,(PeInput){.move_x=1},PE_FIXED_DT);
    CHECK(g->player.x<=32.701f);
    /* Refund overflow leaves every cell untouched. */
    const PeItemDef *wood=pe_item_def(PE_ITEM_WOOD);
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)g->inventory.slots[i]=(PeSlot){.item_id=PE_ITEM_WOOD,.count=wood->stack_limit};
    before=g->inventory;CHECK(!pe_game_remove_cells(g,all,4));CHECK(g->cell_count==4&&memcmp(&before,&g->inventory,sizeof before)==0);
}
static void placement_surface_tests(PeGame *g){
    flat(g);settle(g);
    CHECK(pe_inventory_add(&g->inventory,PE_ITEM_FENCE,1));
    CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,4));
    CHECK(pe_game_place(g,eye(g),(PeVec3){1,-1,0},PE_ITEM_FENCE));
    CHECK(g->block_count==1);
    if(g->block_count!=1)return;
    PeBlock furniture=g->blocks[0];
    /* A jumping player aims at furniture; the same ray also crosses terrain
     * behind it. Only the nearest selected surface may determine placement. */
    g->player=(PeVec3){furniture.x-0.31f,furniture.y+0.5f,furniture.z+0.25f};
    PeVec3 origin=eye(g);
    PeVec3 target={furniture.x+0.25f,furniture.y+1.0f,furniture.z+0.25f};
    PeVec3 direction={target.x-origin.x,target.y-origin.y,target.z-origin.z};
    CHECK(pe_world_raycast(&g->world,origin,direction,3).hit);
    PeB45Cell preview[4];size_t preview_count=0;
    CHECK(pe_game_preview_shape(g,origin,direction,PE_ITEM_WOOD,PE_B45_CUBE,0,1,preview,&preview_count));
    CHECK(preview_count==1&&pe_game_can_build_cells(g,preview,(int)preview_count));
    CHECK(pe_game_place_shape(g,origin,direction,PE_ITEM_WOOD,PE_B45_CUBE,0,1));
    CHECK(g->cell_count==1);
    if(g->cell_count!=1)return;
    CHECK(equal_cells(preview,g->cells,1));
    CHECK(g->cells[0].x==2*furniture.x&&g->cells[0].z==2*furniture.z);
    CHECK(g->cells[0].y==2*(furniture.y+1));
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==3);
    /* Keep the same target after the new cell occludes it. Placement must now
     * use that cell's top, not either the furniture or background terrain. */
    g->player.y=furniture.y+1.0f;
    origin=eye(g);
    target.y=furniture.y+1.5f;
    direction=(PeVec3){target.x-origin.x,target.y-origin.y,target.z-origin.z};
    CHECK(pe_game_preview_shape(g,origin,direction,PE_ITEM_WOOD,PE_B45_CUBE,0,1,preview,&preview_count));
    CHECK(preview_count==1&&pe_game_can_build_cells(g,preview,(int)preview_count));
    CHECK(pe_game_place_shape(g,origin,direction,PE_ITEM_WOOD,PE_B45_CUBE,0,1));
    CHECK(g->cell_count==2);
    if(g->cell_count==2){
        CHECK(g->cells[1].y==2*(furniture.y+1)+1);
        CHECK(equal_cells(preview,&g->cells[1],1));
    }
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==2);
}
static void placement_preview_tests(PeGame *g){
    PeGame *before=malloc(sizeof *before);CHECK(before!=NULL);if(!before)return;
    const int items[]={PE_ITEM_DIRT,PE_ITEM_STONE,PE_ITEM_WOOD};
    const int shapes[]={PE_B45_CUBE,PE_B45_RAMP};
    PeVec3 direction={1,-1,0};
    /* The whole candidate matrix shares the real placement path, including
     * grouped resource cost and the four-cell footprint's terrain elevation. */
    for(size_t material=0;material<3;material++)for(size_t shape=0;shape<2;shape++)
    for(int rotation=0;rotation<4;rotation++)for(int brush=1;brush<=4;brush+=3){
        flat(g);settle(g);CHECK(pe_inventory_add(&g->inventory,items[material],2));
        *before=*g;PeB45Cell candidate[4];size_t count=77;
        CHECK(pe_game_preview_shape(g,eye(g),direction,items[material],shapes[shape],rotation,brush,candidate,&count));
        CHECK(count==(size_t)brush);CHECK(pe_game_can_build_cells(g,candidate,(int)count));
        CHECK(memcmp(g,before,sizeof *g)==0);
        CHECK(pe_game_place_shape(g,eye(g),direction,items[material],shapes[shape],rotation,brush));
        CHECK(g->cell_count==brush&&equal_cells(g->cells,candidate,(size_t)brush));
        CHECK(pe_inventory_count(&g->inventory,items[material])==1);
        *before=*g;CHECK(!pe_game_can_build_cells(g,candidate,brush));
        CHECK(memcmp(g,before,sizeof *g)==0);
    }
    /* Candidate exists without resources; querying affordability is atomic. */
    flat(g);settle(g);*before=*g;PeB45Cell candidate[4];size_t count=0;
    CHECK(pe_game_preview_shape(g,eye(g),direction,PE_ITEM_WOOD,PE_B45_RAMP,2,4,candidate,&count));
    CHECK(count==4&&!pe_game_can_build_cells(g,candidate,4));
    CHECK(memcmp(g,before,sizeof *g)==0);
    /* Invalid arguments, furniture, zero/nonfinite vectors and a sky miss
     * preserve both output buffers as well as the entire game/message. */
    const struct {PeVec3 direction;int item,shape,rotation,brush;} bad[]={
        {{0,1,0},PE_ITEM_WOOD,PE_B45_CUBE,0,1},
        {{0,0,0},PE_ITEM_WOOD,PE_B45_CUBE,0,1},
        {{NAN,0,0},PE_ITEM_WOOD,PE_B45_CUBE,0,1},
        {{1,-1,0},PE_ITEM_FENCE,PE_B45_CUBE,0,1},
        {{1,-1,0},-1,PE_B45_CUBE,0,1},
        {{1,-1,0},270,PE_B45_CUBE,0,1},
        {{1,-1,0},PE_ITEM_WOOD,99,0,1},
        {{1,-1,0},PE_ITEM_WOOD,PE_B45_CUBE,4,1},
        {{1,-1,0},PE_ITEM_WOOD,PE_B45_CUBE,0,2}
    };
    PeB45Cell sentinel[4];memset(sentinel,0xa5,sizeof sentinel);
    for(size_t i=0;i<sizeof bad/sizeof bad[0];i++){
        memcpy(candidate,sentinel,sizeof candidate);count=77;
        CHECK(!pe_game_preview_shape(g,eye(g),bad[i].direction,bad[i].item,bad[i].shape,bad[i].rotation,bad[i].brush,candidate,&count));
        CHECK(count==77&&memcmp(candidate,sentinel,sizeof candidate)==0);
        CHECK(memcmp(g,before,sizeof *g)==0);
    }
    CHECK(!pe_game_preview_shape(NULL,eye(g),direction,PE_ITEM_WOOD,PE_B45_CUBE,0,1,candidate,&count));
    CHECK(!pe_game_preview_shape(g,eye(g),direction,PE_ITEM_WOOD,PE_B45_CUBE,0,1,NULL,&count));
    CHECK(!pe_game_preview_shape(g,eye(g),direction,PE_ITEM_WOOD,PE_B45_CUBE,0,1,candidate,NULL));
    CHECK(!pe_game_preview_shape(g,(PeVec3){0,30,0},direction,PE_ITEM_WOOD,PE_B45_CUBE,0,1,candidate,&count));
    CHECK(count==77&&memcmp(candidate,sentinel,sizeof candidate)==0&&memcmp(g,before,sizeof *g)==0);
    free(before);
}
static uint32_t float_bits(float f){uint32_t bits;memcpy(&bits,&f,4);return bits;}
static void patch32(unsigned char *data,size_t offset,uint32_t value){
    for(int i=0;i<4;i++)data[offset+(size_t)i]=(unsigned char)(value>>(8*i));
}
/* Update payload AND checksum to exercise semantic validation, not just corruption detection. */
static bool patch_save(const char *path,size_t offset,uint32_t value){
    FILE *f=fopen(path,"rb");if(!f)return false;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return false;}
    long length=ftell(f);if(length<16||length>400000||offset+4>(size_t)length){fclose(f);return false;}
    rewind(f);unsigned char *data=malloc((size_t)length);if(!data){fclose(f);return false;}
    bool ok=fread(data,1,(size_t)length,f)==(size_t)length;fclose(f);
    if(ok){
        patch32(data,offset,value);uint32_t sum=2166136261u;
        for(size_t i=16;i<(size_t)length;i++){sum^=data[i];sum*=16777619u;}patch32(data,12,sum);
        f=fopen(path,"wb");if(!f)ok=false;else{ok=fwrite(data,1,(size_t)length,f)==(size_t)length;if(fclose(f)!=0)ok=false;}
    }
    free(data);return ok;
}
static size_t extension_offset(const PeGame *g){
    return 356u+(size_t)g->block_count*16u+(size_t)g->tree_count*16u+PE_VOXEL_COUNT*2u;
}
static bool downgrade_save(const char *path,const PeGame *g,uint32_t version){
    size_t length=extension_offset(g)+(version>=2?316u+4u*(size_t)g->tree_count:0);
    if(version>=3)length+=4u+14u*(size_t)g->cell_count;
    unsigned char *data=malloc(length);if(!data)return false;
    FILE *f=fopen(path,"rb");if(!f){free(data);return false;}
    bool ok=fread(data,1,length,f)==length;fclose(f);
    if(ok){
        patch32(data,4,version);patch32(data,8,(uint32_t)(length-16));uint32_t sum=2166136261u;
        for(size_t i=16;i<length;i++){sum^=data[i];sum*=16777619u;}patch32(data,12,sum);
        f=fopen(path,"wb");if(!f)ok=false;else{ok=fwrite(data,1,length,f)==length;if(fclose(f)!=0)ok=false;}
    }
    free(data);return ok;
}
static void save_v2_tests(PeGame *g){
    char path[]="/tmp/pe-v2-test-XXXXXX",error[200];int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return;close(fd);
    flat(g);settle(g);CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,30));int slot=add_tool(g,19);
    CHECK(slot>=0);if(slot<0){unlink(path);return;}
    CHECK(pe_inventory_add(&g->inventory,19,1));CHECK(pe_game_start_craft(g,1,3));advance(g,3.25f);
    CHECK(g->craft.batches_pending==1&&pe_inventory_count(&g->inventory,12)==0);
    g->inventory.slots[slot].durability=77.5f;g->stamina=31;g->tool_cooldown=0.6f;
    PeGame *saved=malloc(sizeof *saved);CHECK(saved!=NULL);if(!saved){unlink(path);return;}*saved=*g;
    CHECK(pe_game_save(g,path,error,sizeof error));pe_game_init(g,999);CHECK(pe_game_load(g,path,error,sizeof error));
    CHECK(g->craft.batches_pending==1&&g->craft.batches_left==2&&g->craft.elapsed==saved->craft.elapsed&&g->craft.delivery_elapsed==saved->craft.delivery_elapsed);
    CHECK(g->stamina==31&&g->tool_cooldown==0.6f&&pe_game_equipped_tool(g)->durability==77.5f&&g->rng_state==saved->rng_state);
    CHECK(memcmp(&g->inventory,&saved->inventory,sizeof g->inventory)==0);
    size_t ext=extension_offset(g);*saved=*g;
    const struct {size_t offset;uint32_t value;} invalid[]={
        {ext,2},{ext+16,0},{ext+16,g->inventory.slots[slot].instance_id},
        {ext+20+8u*(size_t)slot,0},{ext+24+8u*(size_t)slot,float_bits(99999)},
        {ext+276,UINT32_MAX},{ext+280,0},{ext+284,float_bits(101)},
        {ext+284,UINT32_C(0x7fc00000)},{ext+288,0},{ext+308,10000},{ext+312,float_bits(7)},
        {ext+20+8u*(size_t)(slot+1),g->inventory.slots[slot].instance_id}
    };
    for(size_t i=0;i<sizeof invalid/sizeof invalid[0];i++){
        CHECK(pe_game_save(g,path,error,sizeof error));CHECK(patch_save(path,invalid[i].offset,invalid[i].value));
        CHECK(!pe_game_load(g,path,error,sizeof error));CHECK(memcmp(g,saved,sizeof *g)==0);
    }
    CHECK(pe_game_cancel_craft(g));CHECK(pe_inventory_count(&g->inventory,12)==1&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==20);
    CHECK(!pe_game_cancel_craft(g));CHECK(pe_inventory_count(&g->inventory,12)==1);
    /* Source1 accepts all byte material IDs and preserves signed crop metadata. */
    g->world.source=1;g->world.origin_x=-320;g->world.origin_y=800;g->world.origin_z=1024;
    pe_world_set(&g->world,1,1,1,(PeVoxel){255,200});
    CHECK(pe_game_save(g,path,error,sizeof error));pe_game_init(g,12);CHECK(pe_game_load(g,path,error,sizeof error));
    CHECK(g->world.source==1&&g->world.origin_x==-320&&g->world.origin_y==800&&g->world.origin_z==1024&&pe_world_get(&g->world,1,1,1).material==200);
    /* Build a byte-compatible old-format fixture, rather than treating v2 as v1. */
    flat(g);settle(g);CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,20));slot=add_tool(g,19);
    g->inventory.slots[slot].durability=50;CHECK(pe_game_start_craft(g,1,2));advance(g,1);*saved=*g;
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(downgrade_save(path,g,1));
    pe_game_init(g,71);CHECK(pe_game_load(g,path,error,sizeof error));
    CHECK(g->world.source==0&&g->world.origin_x==0&&g->equipped_instance_id==0);
    CHECK(g->stamina==100&&g->stamina_regen==40&&g->inventory.slots[slot].durability==3553&&g->inventory.slots[slot].instance_id>0);
    CHECK(g->craft.recipe_id==1&&g->craft.batches_left==2&&g->craft.elapsed==saved->craft.elapsed&&g->craft.batches_pending==0);
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==20);CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(g,path,error,sizeof error));
    free(saved);unlink(path);
}
static void save_v3_tests(PeGame *g){
    char path[]="/tmp/pe-v3-test-XXXXXX",error[200];int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return;close(fd);
    flat(g);settle(g);CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,5));
    PeB45Cell cells[4]={{68,20,64,4,11},{69,20,64,9,11},{68,20,65,10,11},{69,20,65,11,11}};
    CHECK(pe_game_build_cells(g,cells,4));g->blocks[0]=(PeBlock){40,15,40,PE_ITEM_FENCE};g->block_count=1;
    PeGame *saved=malloc(sizeof *saved);CHECK(saved!=NULL);if(!saved){unlink(path);return;}*saved=*g;
    CHECK(pe_game_save(g,path,error,sizeof error));pe_game_init(g,555);CHECK(pe_game_load(g,path,error,sizeof error));
    CHECK(g->cell_count==4&&g->block_count==1);
    for(int i=0;i<4;i++)CHECK(g->cells[i].x==saved->cells[i].x&&g->cells[i].y==saved->cells[i].y&&g->cells[i].z==saved->cells[i].z&&g->cells[i].type==saved->cells[i].type&&g->cells[i].material==saved->cells[i].material);
    size_t offset=extension_offset(g)+316u+4u*(size_t)g->tree_count;*saved=*g;
    const struct {size_t offset;uint32_t value;} cases[]={
        {offset,PE_MAX_B45_CELLS+1},{offset+4,10000},{offset+8,10000},{offset+12,10000},
        {offset+16,UINT32_C(0x0904)},{offset+16,UINT32_C(0x0bfc)}
    };
    for(size_t i=0;i<sizeof cases/sizeof cases[0];i++){
        CHECK(pe_game_save(g,path,error,sizeof error));CHECK(patch_save(path,cases[i].offset,cases[i].value));
        CHECK(!pe_game_load(g,path,error,sizeof error));CHECK(memcmp(g,saved,sizeof *g)==0);
    }
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(patch_save(path,offset+18,68));
    CHECK(!pe_game_load(g,path,error,sizeof error));CHECK(memcmp(g,saved,sizeof *g)==0); /* duplicate coords */
    g->player=(PeVec3){34.25f,10.1f,32.25f};CHECK(!pe_game_save(g,path,error,sizeof error));*g=*saved;
    g->cell_count=0;memset(g->cells,0,sizeof g->cells);CHECK(pe_game_save(g,path,error,sizeof error));
    CHECK(downgrade_save(path,g,2));pe_game_init(g,556);CHECK(pe_game_load(g,path,error,sizeof error));
    CHECK(g->cell_count==0&&g->block_count==1&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==4);
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(g,path,error,sizeof error));
    free(saved);unlink(path);
}
static void semantic_save_tests(PeGame *g,const char *path){
    char error[200];PeGame *before=malloc(sizeof *before);CHECK(before!=NULL);if(!before)return;*before=*g;
    /* PECS v1 offsets: player 24/28/32, first slot 68/72, recipe324, block count336. */
    const struct {size_t offset;uint32_t value;} cases[]={
        {24,float_bits(64)},{32,float_bits(64)},{28,float_bits(32)},
        {24,float_bits(0.1f)},{32,float_bits(0.1f)},{28,float_bits(-1)},
        {28,float_bits(0)},{24,UINT32_C(0x7fc00000)},{28,UINT32_C(0x7f800000)},
        {68,1234567},{72,UINT32_C(0x7fffffff)},{72,UINT32_C(0xffffffff)},
        {324,1234567},{336,PE_MAX_BLOCKS+1},{4,999},{0,0}
    };
    for(size_t i=0;i<sizeof cases/sizeof cases[0];i++){
        CHECK(pe_game_save(g,path,error,sizeof error));CHECK(patch_save(path,cases[i].offset,cases[i].value));
        CHECK(!pe_game_load(g,path,error,sizeof error));CHECK(memcmp(g,before,sizeof *g)==0);
    }
    /* Embedded block validation independent of terrain and bounds. */
    g->player=(PeVec3){40.5f,15,40.5f};CHECK(!pe_game_save(g,path,error,sizeof error));*g=*before;
    /* Free-falling saves remain valid: grounded is not a prerequisite. */
    g->player.y=20;g->velocity.y=-3;g->grounded=false;g->jumping=true;g->sprinting=true;
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(g,path,error,sizeof error));CHECK(g->player.y==20&&g->velocity.y==-3);
    CHECK(!g->jumping&&!g->sprinting); /* Held action state never survives reload. */
    *g=*before;free(before);
}
static void save_tests(PeGame *g){
    char path[]="/tmp/pe-core-test-XXXXXX",error[200];int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return;close(fd);
    flat(g);settle(g);CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,12));
    g->blocks[0]=(PeBlock){40,15,40,PE_ITEM_WOOD};g->block_count=1;g->yaw=1.25f;g->pitch=-0.5f;
    pe_world_dig(&g->world,(PeVec3){20,10,20},2);
    const PeRecipeDef *r=&pe_recipes[0];
    for(int i=0;i<r->ingredient_count;i++)CHECK(pe_inventory_add(&g->inventory,r->ingredients[i].item_id,r->ingredients[i].count));
    CHECK(pe_game_start_craft(g,r->id,1));pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    PeGame *copy=malloc(sizeof *copy);CHECK(copy!=NULL);if(!copy){unlink(path);return;}*copy=*g;
    CHECK(pe_game_save(g,path,error,sizeof error));pe_game_init(g,99);CHECK(pe_game_load(g,path,error,sizeof error));
    CHECK(pe_world_hash(&g->world)==pe_world_hash(&copy->world));CHECK(memcmp(&g->inventory,&copy->inventory,sizeof g->inventory)==0);
    CHECK(memcmp(&g->player,&copy->player,sizeof g->player)==0);CHECK(memcmp(&g->craft,&copy->craft,sizeof g->craft)==0);
    CHECK(g->block_count==copy->block_count&&g->blocks[0].item_id==PE_ITEM_WOOD);CHECK(g->game_seconds==copy->game_seconds&&g->yaw==copy->yaw);
    CHECK(memcmp(g->trees,copy->trees,sizeof g->trees)==0);
    *copy=*g;FILE *f=fopen(path,"r+b");CHECK(f!=NULL);if(f){fseek(f,100,SEEK_SET);int c=fgetc(f);fseek(f,100,SEEK_SET);fputc(c^128,f);fclose(f);}
    CHECK(!pe_game_load(g,path,error,sizeof error));CHECK(memcmp(g,copy,sizeof *g)==0);
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(truncate(path,20)==0);
    CHECK(!pe_game_load(g,path,error,sizeof error));CHECK(memcmp(g,copy,sizeof *g)==0);
    CHECK(pe_game_save(g,path,error,sizeof error));f=fopen(path,"ab");if(f){fputc(0,f);fclose(f);}
    CHECK(!pe_game_load(g,path,error,sizeof error));CHECK(memcmp(g,copy,sizeof *g)==0);
    g->inventory.slots[0].count=-1;CHECK(!pe_game_save(g,path,error,sizeof error));*g=*copy;
    semantic_save_tests(g,path);
    free(copy);unlink(path);
}
static int find_slot(const PeGame *g,int item){
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==item&&g->inventory.slots[i].count>0)return i;
    return -1;
}
static void adventure_tests(PeGame *g){
    CHECK(pe_game_init_mode(g,107,PE_GAME_ADVENTURE));
    CHECK(g->mode==PE_GAME_ADVENTURE&&g->adventure_mission==0&&g->welcome_page==0&&g->money==500);
    CHECK(g->hp==500&&g->max_hp==500&&g->known_recipe_count==0);
    for(size_t i=0;i<pe_adventure_starter_count;i++)CHECK(pe_inventory_count(&g->inventory,pe_adventure_starter[i].item_id)==pe_adventure_starter[i].count);
    CHECK(pe_inventory_count(&g->inventory,279)==200&&pe_inventory_count(&g->inventory,229)==400);
    CHECK(!pe_game_start_craft(g,1,1));CHECK(!pe_game_accept_supplies(g));
    int scan=find_slot(g,973);CHECK(scan>=0&&pe_game_use_slot(g,scan));
    CHECK(g->scanner_mask==12&&g->known_recipe_count==0&&!pe_game_recipe_known(g,1));
    CHECK(pe_inventory_add(&g->inventory,973,1));scan=find_slot(g,973);
    CHECK(!pe_game_use_slot(g,scan)&&pe_inventory_count(&g->inventory,973)==1);
    CHECK(pe_test_ack_welcome(g));CHECK(pe_game_arrival_complete(g));CHECK(!pe_game_arrival_complete(g));
    int added=0;while(pe_inventory_first_empty(&g->inventory,308)>=0){CHECK(pe_inventory_add(&g->inventory,308,1));added++;} /* Fill Item category before atomic supplies. */
    PeInventory before=g->inventory;CHECK(!pe_game_accept_supplies(g));
    CHECK(!g->supplies_claimed&&memcmp(&before,&g->inventory,sizeof before)==0);
    CHECK(pe_inventory_remove(&g->inventory,308,added));CHECK(pe_game_accept_supplies(g));
    before=g->inventory;CHECK(!pe_game_accept_supplies(g));CHECK(memcmp(&before,&g->inventory,sizeof before)==0);
    CHECK(g->known_recipe_count==0&&pe_inventory_count(&g->inventory,12)==1);
    int book=find_slot(g,406);CHECK(book>=0);CHECK(pe_game_quickslot_assign(g,2,book));
    CHECK(pe_game_quickslot_use(g,2));CHECK(!g->quickslots[2].item_id);
    CHECK(pe_game_recipe_known(g,1)&&g->known_recipe_count==1&&pe_inventory_count(&g->inventory,406)==0);
    CHECK(pe_inventory_add(&g->inventory,406,1));book=find_slot(g,406);uint32_t book_id=g->inventory.slots[book].instance_id;
    CHECK(!pe_game_use_slot(g,book)&&g->inventory.slots[book].instance_id==book_id);
    book=find_slot(g,519);CHECK(pe_game_use_slot(g,book));CHECK(pe_game_recipe_known(g,114)&&pe_game_recipe_known(g,42));
    CHECK(pe_game_start_craft(g,1,1));advance(g,3.01f);
    CHECK(pe_inventory_count(&g->inventory,12)==2&&pe_inventory_count(&g->inventory,279)==190);
    int knife=find_slot(g,33);CHECK(pe_game_use_slot(g,knife));CHECK(pe_game_equipped_tool(g)->item_id==33);
    int shovel=find_slot(g,12);uint32_t shovel_id=g->inventory.slots[shovel].instance_id;
    g->inventory.slots[shovel].durability=123;CHECK(pe_game_quickslot_assign(g,3,shovel));CHECK(pe_game_quickslot_use(g,3));
    CHECK(g->equipped_instance_id==shovel_id&&pe_game_equipped_tool(g)->durability==123);
    CHECK(pe_game_quickslot_assign(g,3,-1)&&!g->quickslots[3].item_id);
    CHECK(!pe_game_quickslot_assign(g,9,shovel));CHECK(!pe_game_use_slot(g,-1));
    CHECK(!pe_game_use_slot(g,find_slot(g,73))); /* Headlamp behavior explicitly unsupported. */
    g->hp=0;CHECK(!pe_game_use_slot(g,find_slot(g,916)));CHECK(!pe_game_use_slot(g,knife));
    g->hp=500;
}
static void furniture_shortcut_tests(PeGame *g){
    char path[]="/tmp/pe-furniture-shortcut-XXXXXX",error[200];int fd=mkstemp(path);
    CHECK(fd>=0);if(fd<0)return;close(fd);
    PeGame *loaded=calloc(1,sizeof *loaded);CHECK(loaded!=NULL);if(!loaded){unlink(path);return;}
    flat(g);settle(g);g->mode=PE_GAME_ADVENTURE;CHECK(pe_inventory_enable_categories(&g->inventory));CHECK(pe_outfit_init(g));CHECK(pe_test_ack_welcome(g));
    CHECK(pe_inventory_add(&g->inventory,PE_ITEM_FENCE,2));CHECK(pe_inventory_add(&g->inventory,PE_ITEM_LADDER,1));
    CHECK(pe_game_quickslot_assign(g,0,0));CHECK(pe_game_quickslot_assign(g,1,1));CHECK(pe_game_quickslot_assign(g,2,2));
    uint32_t other_fence=g->quickslots[1].instance_id;
    PeInventory before=g->inventory;PeQuickslot shortcuts[PE_QUICKSLOTS];memcpy(shortcuts,g->quickslots,sizeof shortcuts);
    CHECK(!pe_game_place(g,eye(g),(PeVec3){0,1,0},PE_ITEM_FENCE));
    CHECK(memcmp(&before,&g->inventory,sizeof before)==0&&memcmp(shortcuts,g->quickslots,sizeof shortcuts)==0&&g->block_count==0);
    CHECK(pe_game_place(g,eye(g),(PeVec3){1,-1,0},PE_ITEM_FENCE));
    CHECK(g->block_count==1&&pe_inventory_count(&g->inventory,PE_ITEM_FENCE)==1);
    CHECK(!g->quickslots[0].item_id&&!g->quickslots[0].instance_id);
    CHECK(g->quickslots[1].item_id==PE_ITEM_FENCE&&g->quickslots[1].instance_id==other_fence);
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->block_count==1&&memcmp(g->quickslots,loaded->quickslots,sizeof g->quickslots)==0);
    CHECK(pe_game_place(g,eye(g),(PeVec3){-1,-1,0},PE_ITEM_LADDER));
    CHECK(g->block_count==2&&pe_inventory_count(&g->inventory,PE_ITEM_LADDER)==0&&!g->quickslots[2].item_id);
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->block_count==2&&memcmp(g->quickslots,loaded->quickslots,sizeof g->quickslots)==0);
    free(loaded);unlink(path);
}
static void adventure_health_tests(PeGame *g){
    CHECK(pe_game_init_mode(g,108,PE_GAME_ADVENTURE));g->hp=100;
    CHECK(pe_game_quickslot_assign(g,0,find_slot(g,916)));float stamina=g->stamina;
    CHECK(pe_game_quickslot_use(g,0));CHECK(g->hp==130&&g->juice_cooldown==10&&pe_inventory_count(&g->inventory,916)==9);
    CHECK(!pe_game_quickslot_use(g,0));
    for(int i=0;i<60;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    CHECK(g->hp==130);pe_game_step(g,(PeInput){0},PE_FIXED_DT);CHECK(g->hp==160);
    for(int i=61;i<600;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    CHECK(g->hp==400&&g->juice_buff_active&&g->juice_cooldown==0&&g->stamina==stamina);
    pe_game_step(g,(PeInput){0},PE_FIXED_DT);CHECK(g->hp==430&&!g->juice_buff_active);
    CHECK(pe_game_quickslot_use(g,0));CHECK(g->hp==460);g->hp=0;advance(g,2);
    CHECK(g->hp==0&&!pe_game_quickslot_use(g,0));
    g->hp=490;advance(g,9);CHECK(g->hp==500);
    CHECK(pe_inventory_remove(&g->inventory,916,pe_inventory_count(&g->inventory,916)));
    pe_game_step(g,(PeInput){0},PE_FIXED_DT);CHECK(g->quickslots[0].item_id==916&&!pe_game_quickslot_use(g,0));
    CHECK(pe_inventory_add(&g->inventory,916,1));CHECK(pe_game_quickslot_use(g,0));
}
static void adventure_save_tests(PeGame *g){
    char path[]="/tmp/pe-v4-test-XXXXXX",error[200];int fd=mkstemp(path);CHECK(fd>=0);if(fd<0)return;close(fd);
    CHECK(pe_game_init_mode(g,109,PE_GAME_ADVENTURE));CHECK(pe_test_ack_welcome(g));CHECK(pe_game_arrival_complete(g));CHECK(pe_game_accept_supplies(g));
    CHECK(pe_game_use_slot(g,find_slot(g,406)));CHECK(pe_game_start_craft(g,1,2));
    CHECK(pe_game_quickslot_assign(g,0,find_slot(g,916)));g->hp=100;CHECK(pe_game_quickslot_use(g,0));advance(g,3.25f);
    CHECK(g->craft.batches_pending==1);int tool=find_slot(g,12);CHECK(pe_game_equip_slot(g,tool));g->inventory.slots[tool].durability=321;
    CHECK(pe_game_quickslot_assign(g,1,tool));PeGame *saved=malloc(sizeof *saved);CHECK(saved!=NULL);if(!saved){unlink(path);return;}*saved=*g;
    CHECK(pe_game_save(g,path,error,sizeof error));pe_game_init(g,1);CHECK(pe_game_load(g,path,error,sizeof error));
    CHECK(g->mode==PE_GAME_ADVENTURE&&g->supplies_claimed&&g->arrival_complete&&g->money==500);
    CHECK(pe_game_recipe_known(g,1)&&g->known_recipe_count==1&&!pe_game_accept_supplies(g));
    CHECK(memcmp(&g->inventory,&saved->inventory,sizeof g->inventory)==0&&memcmp(g->quickslots,saved->quickslots,sizeof g->quickslots)==0);
    CHECK(g->hp==saved->hp&&g->juice_cooldown==saved->juice_cooldown&&g->juice_buff_elapsed==saved->juice_buff_elapsed&&g->juice_buff_tick==saved->juice_buff_tick);
    CHECK(g->craft.batches_pending==saved->craft.batches_pending);*saved=*g;
    size_t offset=extension_offset(g)+316u+4u*(size_t)g->tree_count+4u+14u*(size_t)g->cell_count;
    const struct {size_t offset;uint32_t value;} bad[]={
        {0,2},{4,9999},{8,2},{8,0},{20,1},{24,PE_KNOWN_RECIPES+1},{28,999999},
        {32,999999},{36,1234},{44,UINT32_MAX}, /* unknown shortcut, stack instance, missing tool instance */
        {32+PE_QUICKSLOTS*8,float_bits(-1)},{32+PE_QUICKSLOTS*8+4,float_bits(0)},
        {32+PE_QUICKSLOTS*8+12,UINT32_C(0x7ff80000)}, /* NaN double cooldown high word */
        {32+PE_QUICKSLOTS*8+24,0}, /* active buff cannot have no initial tick */
        {32+PE_QUICKSLOTS*8+28,2}
    };
    for(size_t i=0;i<sizeof bad/sizeof bad[0];i++){
        CHECK(pe_game_save(g,path,error,sizeof error));CHECK(patch_save(path,offset+bad[i].offset,bad[i].value));
        CHECK(!pe_game_load(g,path,error,sizeof error));CHECK(memcmp(g,saved,sizeof *g)==0);
    }
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(patch_save(path,28,float_bits(2)));
    CHECK(!pe_game_load(g,path,error,sizeof error));CHECK(memcmp(g,saved,sizeof *g)==0); /* MC-solid player position */
    flat(g);CHECK(pe_inventory_add(&g->inventory,279,20));CHECK(pe_game_start_craft(g,1,2));advance(g,1);
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(downgrade_save(path,g,3));
    CHECK(pe_game_load(g,path,error,sizeof error));CHECK(g->mode==PE_GAME_LEGACY&&g->hp==500&&!g->supplies_claimed&&pe_game_recipe_known(g,1));
    CHECK(g->craft.recipe_id==1&&g->craft.batches_left==2);free(saved);unlink(path);
}
static void tree_save_migration_tests(PeGame *g){
    char path[]="/tmp/pe-tree-migration-XXXXXX",error[200];int fd=mkstemp(path);
    CHECK(fd>=0);if(fd<0)return;close(fd);
    PeGame *before=malloc(sizeof *before),*loaded=calloc(1,sizeof *loaded);
    CHECK(before&&loaded);if(!before||!loaded){free(before);free(loaded);unlink(path);return;}
    flat(g);g->world.depth=PE_WORLD_Z;g->mode=PE_GAME_ADVENTURE;CHECK(pe_inventory_enable_categories(&g->inventory));CHECK(pe_outfit_init(g));CHECK(pe_test_ack_welcome(g));
    g->tree_count=1;g->trees[0]=(PeTree){{40.6f,9.5f,40.7f},8,255};
    g->player=(PeVec3){44,10.05f,40.5f};g->velocity=(PeVec3){0,-1,0};g->grounded=false;
    *before=*g;
    /* Strip depth and welcome footers before exercising the v4 collision migration. */
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_test_save_pre_welcome(path,4));
    CHECK(patch_save(path,24,float_bits(40.5f)));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(loaded->player.x!=40.5f||loaded->player.z!=40.5f);
    CHECK(strstr(loaded->message,"to clear a legacy tree overlap")!=NULL);
    CHECK(loaded->player.y==g->player.y&&loaded->velocity.y==-1);
    CHECK(hypotf(loaded->player.x-40.5f,loaded->player.z-40.5f)<=3.001f);
    CHECK(memcmp(&loaded->world,&g->world,sizeof g->world)==0&&loaded->tree_count==1&&
        memcmp(loaded->trees,g->trees,sizeof g->trees[0])==0);
    CHECK(memcmp(loaded->inventory.slots,g->inventory.slots,sizeof g->inventory.slots)==0);
    CHECK(loaded->inventory.next_instance_id==g->inventory.next_instance_id+PE_OUTFIT_SLOTS);
    CHECK(pe_game_save(loaded,path,error,sizeof error));CHECK(pe_game_load(loaded,path,error,sizeof error));
    PeSandboxCollision *collision=pe_sandbox_collision_create();CHECK(collision!=NULL);
    if(collision){CHECK(pe_game_collision_update(loaded,collision,error,sizeof error));
        CHECK(!pe_sandbox_collision_overlap(collision,loaded->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));}
    /* Newly written saves cannot introduce overlapping positions. */
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(patch_save(path,24,float_bits(40.5f)));
    *loaded=*before;CHECK(!pe_game_load(loaded,path,error,sizeof error));CHECK(memcmp(loaded,before,sizeof *loaded)==0);
    g->player.x=40.5f;CHECK(!pe_game_save(g,path,error,sizeof error));*g=*before;
    /* Clear airborne old saves retain exact position/velocity, not ground snap. */
    g->player.y=22;g->velocity=(PeVec3){2,-3,1};*before=*g;
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_test_save_pre_welcome(path,4));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(memcmp(&loaded->player,&g->player,sizeof g->player)==0&&memcmp(&loaded->velocity,&g->velocity,sizeof g->velocity)==0);
    CHECK(strcmp(loaded->message,"Game loaded.")==0);
    /* Depleted trees have no collider and need no relocation. */
    g->player=(PeVec3){40.5f,10.05f,40.5f};g->trees[0].remaining=0;g->trees[0].health=0;
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_test_save_pre_welcome(path,4));CHECK(pe_game_load(loaded,path,error,sizeof error));
    CHECK(memcmp(&loaded->player,&g->player,sizeof g->player)==0);
    if(collision){CHECK(pe_game_collision_update(g,collision,error,sizeof error));
        CHECK(!pe_sandbox_collision_overlap(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));}
    /* A v4 tree penetration surrounded by solid walls cannot escape through them. */
    g->trees[0].remaining=8;g->trees[0].health=255;g->player.x=44;
    for(int y=10;y<=11;y++){
        g->blocks[g->block_count++]=(PeBlock){39,y,40,PE_ITEM_FENCE};
        g->blocks[g->block_count++]=(PeBlock){41,y,40,PE_ITEM_FENCE};
        g->blocks[g->block_count++]=(PeBlock){40,y,39,PE_ITEM_FENCE};
        g->blocks[g->block_count++]=(PeBlock){40,y,41,PE_ITEM_FENCE};
    }
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_test_save_pre_welcome(path,4));CHECK(patch_save(path,24,float_bits(40.5f)));
    *before=*loaded;CHECK(!pe_game_load(loaded,path,error,sizeof error));CHECK(strstr(error,"no safe bounded escape")!=NULL);
    CHECK(memcmp(loaded,before,sizeof *loaded)==0);
    /* Existing LEGACY mode remains unaffected by live-tree overlap. */
    flat(g);g->tree_count=1;g->trees[0]=(PeTree){{32.1f,9.5f,32.2f},8,255};
    CHECK(pe_game_save(g,path,error,sizeof error));CHECK(pe_game_load(loaded,path,error,sizeof error));CHECK(loaded->mode==PE_GAME_LEGACY);
    pe_sandbox_collision_free(collision);free(before);free(loaded);unlink(path);
}
static void medpack_tests(PeGame *g){
    CHECK(pe_game_init_mode(g,81,PE_GAME_ADVENTURE));
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id,.categorized=g->mode==PE_GAME_ADVENTURE};g->equipped_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    g->hp=100;CHECK(pe_inventory_add(&g->inventory,925,3)&&pe_inventory_add(&g->inventory,916,2));
    CHECK(pe_game_quickslot_assign(g,0,find_slot(g,925))&&pe_game_quickslot_assign(g,1,find_slot(g,925)));
    CHECK(pe_game_quickslot_assign(g,2,find_slot(g,916)));
    CHECK(pe_game_quickslot_use(g,0)&&g->hp==450&&g->medpack_cooldown==10&&pe_inventory_count(&g->inventory,925)==2);
    PeInventory before=g->inventory;
    CHECK(!pe_game_quickslot_use(g,1)&&!memcmp(&before,&g->inventory,sizeof before)); /* Alias shares timer. */
    CHECK(pe_game_item_cooldown(g,925)==10&&pe_game_item_cooldown(g,916)==0&&pe_game_item_cooldown(g,917)==0&&pe_game_item_cooldown(NULL,925)==0);
    for(int tick=0;tick<30;tick++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    float med=g->medpack_cooldown;CHECK(med>9.49f&&med<9.51f);
    CHECK(pe_game_quickslot_use(g,2)&&g->hp==480&&g->juice_buff_active&&g->juice_cooldown==10&&g->medpack_cooldown==med);
    for(int tick=30;tick<599;tick++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    CHECK(g->medpack_cooldown>0&&!pe_game_quickslot_use(g,1));
    pe_game_step(g,(PeInput){0},PE_FIXED_DT);CHECK(g->medpack_cooldown==0);
    double juice_cd=g->juice_cooldown,juice_elapsed=g->juice_buff_elapsed;unsigned juice_tick=g->juice_buff_tick;
    CHECK(juice_cd>.49&&juice_cd<.51&&g->juice_buff_active);
    g->hp=490;CHECK(pe_game_quickslot_use(g,1)&&g->hp==500&&pe_inventory_count(&g->inventory,925)==1);
    CHECK(g->juice_cooldown==juice_cd&&g->juice_buff_elapsed==juice_elapsed&&g->juice_buff_tick==juice_tick&&g->juice_buff_active);
    med=g->medpack_cooldown;pe_game_step(g,(PeInput){0},0);pe_game_step(g,(PeInput){0},-.1f);pe_game_step(g,(PeInput){0},1);
    CHECK(g->medpack_cooldown==med); /* Paused/not-stepped and invalid clock deltas do not advance. */
    for(int tick=0;tick<40;tick++)pe_game_step(g,(PeInput){0},.25f);
    CHECK(g->medpack_cooldown==0&&g->hp==500&&pe_game_use_slot(g,find_slot(g,925))); /* Source permits full-HP use. */
    CHECK(pe_inventory_count(&g->inventory,925)==0&&g->quickslots[0].item_id==925&&!pe_game_quickslot_use(g,0));
    CHECK(pe_inventory_add(&g->inventory,925,1));before=g->inventory;g->medpack_cooldown=0;g->hp=0;
    CHECK(!pe_game_quickslot_use(g,0)&&g->hp==0&&!memcmp(&before,&g->inventory,sizeof before));
    g->hp=100;g->max_hp=NAN;CHECK(!pe_game_quickslot_use(g,0)&&!memcmp(&before,&g->inventory,sizeof before));
    g->max_hp=500;g->medpack_cooldown=NAN;CHECK(!pe_game_quickslot_use(g,0)&&!memcmp(&before,&g->inventory,sizeof before));
    g->medpack_cooldown=-1;CHECK(!pe_game_quickslot_use(g,0)&&!memcmp(&before,&g->inventory,sizeof before));g->medpack_cooldown=0;
}
static void broken_sword_tests(void){
    PeGame *g=calloc(1,sizeof *g);CHECK(g!=NULL);if(!g)return;
    CHECK(pe_game_init_mode(g,73,PE_GAME_ADVENTURE));
    CHECK(pe_inventory_add(&g->inventory,1,1));int slot=find_slot(g,1);CHECK(slot>=0);
    if(slot>=0){
        g->inventory.slots[slot].durability=0;
        uint32_t id=g->inventory.slots[slot].instance_id;PeInventory before=g->inventory;
        CHECK(pe_game_equip_slot(g,slot)&&g->equipped_instance_id==id);
        CHECK(!memcmp(&before,&g->inventory,sizeof before));
        PeToolStats stats;CHECK(pe_game_tool_stats(g,&stats)&&stats.attack==pe_player_defaults.attack);
        pe_game_unequip(g);CHECK(pe_game_quickslot_assign(g,0,slot)&&pe_game_quickslot_use(g,0));
        CHECK(g->equipped_instance_id==id&&g->inventory.slots[slot].durability==0);
        pe_game_unequip(g);g->inventory.slots[slot].durability=NAN;
        CHECK(!pe_game_equip_slot(g,slot)&&!g->equipped_instance_id);
    }
    pe_game_destroy(g);free(g);
}
int pe_test_game(void){
    failures=0;PeGame *g=malloc(sizeof *g);if(!g)return 1;
    inventory_tests();broken_sword_tests();movement_tests(g);movement_stamina_tests(g);action_motion_tests(g);collision_game_tests(g);craft_tests(g);pending_craft_tests(g);interaction_tests(g);ceiling_mining_test(g);dig_indicator_game_tests(g);instance_tool_tests(g);block_game_tests(g);placement_surface_tests(g);placement_preview_tests(g);imported_world_tests(g);save_tests(g);save_v2_tests(g);save_v3_tests(g);adventure_tests(g);furniture_shortcut_tests(g);adventure_health_tests(g);adventure_save_tests(g);tree_save_migration_tests(g);medpack_tests(g);free(g);return failures;
}
