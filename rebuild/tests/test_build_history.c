#include "pe_build_history.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"build history:%d %s\n",__LINE__,#c);failures++;}}while(0)
static void fresh(PeGame *g){
    pe_game_destroy(g);pe_game_init(g,761);
    for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++)
        pe_world_set(&g->world,x,y,z,(PeVoxel){y<10?255:0,y<10?PE_MAT_DIRT:PE_MAT_AIR});
    g->player=(PeVec3){32,10.05f,32};g->velocity=(PeVec3){0};g->block_count=0;g->cell_count=0;
    memset(g->blocks,0,sizeof g->blocks);memset(g->cells,0,sizeof g->cells);
    g->inventory=(PeInventory){.next_instance_id=1};g->equipped_instance_id=0;
    memset(g->quickslots,0,sizeof g->quickslots);memset(&g->craft,0,sizeof g->craft);
    CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,100)&&pe_inventory_add(&g->inventory,PE_ITEM_STONE,100));
}
static PeB45Cell cell(int z){return (PeB45Cell){68,20,z,PE_B45_CUBE<<2,11};}
static bool equal(const PeB45Cell *a,const PeB45Cell *b){
    return a->x==b->x&&a->y==b->y&&a->z==b->z&&a->type==b->type&&a->material==b->material;
}
static bool unchanged(const PeGame *g,const PeGame *before){
    size_t off=offsetof(PeGame,message),after=off+sizeof g->message;
    return !memcmp(g,before,off)&&!memcmp((const char *)g+after,(const char *)before+after,sizeof *g-after);
}
static void rejected(PeGame *g,PeGame *before,bool undo){
    memcpy(before,g,sizeof *g);
    CHECK(!(undo?pe_build_history_undo(g):pe_build_history_redo(g))&&unchanged(g,before));
}
static void accounting(PeGame *g,PeGame *before){
    fresh(g);PeB45Cell c=cell(64);uint32_t built=g->built;
    CHECK(pe_build_history_state_valid(g)&&!pe_build_history_can_undo(g)&&!pe_build_history_can_redo(g));
    rejected(g,before,true);rejected(g,before,false);
    CHECK(pe_game_build_cells(g,&c,1)&&g->cell_count==1&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==99);
    CHECK(g->build_history.count==1&&g->build_history.cursor==1&&g->build_history.edits[0].addition);
    CHECK(pe_build_history_can_undo(g)&&!pe_build_history_can_redo(g));
    CHECK(pe_build_history_undo(g)&&!g->cell_count&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==99);
    CHECK(g->build_history.count==1&&g->build_history.cursor==0&&g->built==built+1);
    CHECK(pe_build_history_redo(g)&&g->cell_count==1&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==98);
    CHECK(g->built==built+1&&equal(&g->cells[0],&c));
    int index=0;CHECK(pe_game_remove_cells(g,&index,1)&&!g->cell_count&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==98);
    CHECK(g->build_history.count==2&&!g->build_history.edits[1].addition);
    CHECK(pe_build_history_undo(g)&&g->cell_count==1&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==97);
    CHECK(pe_build_history_redo(g)&&!g->cell_count&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==97);
    /* Group by material, not total cell count; rotations/shapes round equally. */
    fresh(g);PeB45Cell mixed[5];int indices[5];
    for(int i=0;i<5;i++){mixed[i]=cell(62+i);mixed[i].type=(uint8_t)((i%2?PE_B45_RAMP:PE_B45_CUBE)<<2|(i&3));indices[i]=i;}
    mixed[4].material=12;
    CHECK(pe_game_build_cells(g,mixed,5));
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==99&&pe_inventory_count(&g->inventory,PE_ITEM_STONE)==99);
    CHECK(pe_build_history_undo(g)&&!g->cell_count);
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==100&&pe_inventory_count(&g->inventory,PE_ITEM_STONE)==99);
    CHECK(pe_build_history_redo(g)&&g->cell_count==5);
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==99&&pe_inventory_count(&g->inventory,PE_ITEM_STONE)==98);
    CHECK(pe_game_remove_cells(g,indices,5));
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==100&&pe_inventory_count(&g->inventory,PE_ITEM_STONE)==98);
    CHECK(pe_build_history_undo(g)&&g->cell_count==5);
    for(int i=0;i<5;i++)CHECK(equal(&g->cells[i],&mixed[i]));
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==99&&pe_inventory_count(&g->inventory,PE_ITEM_STONE)==97);
}
static void journal(PeGame *g,PeGame *before){
    fresh(g);PeB45Cell cells[6];
    for(int i=0;i<6;i++){cells[i]=cell(62+i);CHECK(pe_game_build_cells(g,&cells[i],1));}
    CHECK(g->build_history.count==5&&g->build_history.cursor==5&&equal(&g->build_history.edits[0].cells[0],&cells[1]));
    for(int i=0;i<5;i++)CHECK(pe_build_history_undo(g));
    CHECK(g->cell_count==1&&equal(&g->cells[0],&cells[0])&&!pe_build_history_can_undo(g));
    rejected(g,before,true);
    for(int i=0;i<5;i++)CHECK(pe_build_history_redo(g));
    CHECK(g->cell_count==6&&!pe_build_history_can_redo(g));
    CHECK(pe_build_history_undo(g)&&pe_build_history_undo(g));
    CHECK(g->build_history.cursor==3&&g->build_history.count==5);
    PeBuildHistory old=g->build_history;PeInventory inventory=g->inventory;
    CHECK(!pe_game_build_cells(g,&cells[0],1)&&!memcmp(&g->build_history,&old,sizeof old)&&!memcmp(&g->inventory,&inventory,sizeof inventory));
    PeB45Cell alternate={69,22,64,PE_B45_RAMP<<2|3,12};CHECK(pe_game_build_cells(g,&alternate,1));
    CHECK(g->build_history.count==4&&g->build_history.cursor==4&&!pe_build_history_can_redo(g));
    CHECK(equal(&g->build_history.edits[3].cells[0],&alternate)&&!g->build_history.edits[4].count);
    CHECK(pe_build_history_state_valid(g));
    /* The largest supported brush is one action, not64 separate actions. */
    fresh(g);PeB45Cell batch[64];int indices[64];
    for(int i=0;i<64;i++){batch[i]=(PeB45Cell){66+(i&3),20+((i>>2)&3),62+(i>>4),PE_B45_CUBE<<2,11};indices[i]=i;}
    CHECK(pe_game_build_cells(g,batch,64)&&g->cell_count==64&&g->build_history.count==1&&g->build_history.edits[0].count==64);
    CHECK(pe_build_history_undo(g)&&!g->cell_count&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==100);
    CHECK(pe_build_history_redo(g)&&g->cell_count==64);
    CHECK(pe_game_remove_cells(g,indices,64)&&g->build_history.count==2&&!g->cell_count);
    CHECK(pe_build_history_undo(g)&&g->cell_count==64&&pe_build_history_state_valid(g));
}
static void resources(PeGame *g,PeGame *before){
    fresh(g);PeB45Cell c=cell(64);CHECK(pe_game_build_cells(g,&c,1)&&pe_build_history_undo(g));
    CHECK(pe_inventory_remove(&g->inventory,PE_ITEM_WOOD,99));rejected(g,before,false);
    CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,1)&&pe_build_history_redo(g));
    CHECK(pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==0);
    int index=0;CHECK(pe_game_remove_cells(g,&index,1));rejected(g,before,true);
    CHECK(pe_inventory_add(&g->inventory,PE_ITEM_WOOD,1)&&pe_build_history_undo(g));
    /* Full inventory blocks a nonzero refund without changing geometry/history. */
    fresh(g);PeB45Cell four[4];for(int i=0;i<4;i++)four[i]=cell(62+i);
    CHECK(pe_game_build_cells(g,four,4));
    g->inventory=(PeInventory){.next_instance_id=1};CHECK(pe_inventory_add(&g->inventory,12,32));
    rejected(g,before,true);
    g->inventory.slots[31]=(PeSlot){0};CHECK(pe_build_history_undo(g)&&pe_inventory_count(&g->inventory,PE_ITEM_WOOD)==1);
    /* A zero refund still succeeds with a full bag; history is not a bag reset. */
    fresh(g);CHECK(pe_game_build_cells(g,&c,1));
    g->inventory=(PeInventory){.next_instance_id=1};CHECK(pe_inventory_add(&g->inventory,12,32));
    PeInventory bag=g->inventory;CHECK(pe_build_history_undo(g)&&!memcmp(&bag,&g->inventory,sizeof bag));
}
static void geometry(PeGame *g,PeGame *before){
    fresh(g);PeB45Cell c=cell(64);CHECK(pe_game_build_cells(g,&c,1));
    g->cells[0].material=12;CHECK(pe_build_history_state_valid(g));rejected(g,before,true);
    g->cells[0]=c;g->cells[0].type=PE_B45_RAMP<<2;rejected(g,before,true);g->cells[0]=c;
    g->cells[1]=c;g->cell_count=2;rejected(g,before,true);g->cells[1]=(PeB45Cell){0};g->cell_count=1;
    PeVec3 player=g->player;g->player.x=10;rejected(g,before,true);g->player=player;
    CHECK(pe_build_history_undo(g));
    /* Redo must not overwrite any occupied coordinate, even the same bytes. */
    g->cells[0]=c;g->cell_count=1;rejected(g,before,false);g->cells[0]=(PeB45Cell){0};g->cell_count=0;
    g->player=(PeVec3){34.25f,10.05f,32.25f};rejected(g,before,false);g->player=player;
    PeVoxel voxel=pe_world_get(&g->world,34,10,32);CHECK(pe_world_set(&g->world,34,10,32,(PeVoxel){255,PE_MAT_DIRT}));
    rejected(g,before,false);CHECK(pe_world_set(&g->world,34,10,32,voxel));
    g->blocks[0]=(PeBlock){34,10,32,PE_ITEM_FENCE};g->block_count=1;rejected(g,before,false);g->block_count=0;g->blocks[0]=(PeBlock){0};
    CHECK(pe_build_history_redo(g));
    g->hp=0;rejected(g,before,true);g->hp=NAN;rejected(g,before,true);g->hp=500;
    g->cell_count=-1;rejected(g,before,true);g->cell_count=PE_MAX_B45_CELLS+1;rejected(g,before,true);g->cell_count=1;
    CHECK(pe_build_history_undo(g));
}
static void mining(PeGame *g,PeGame *before){
    fresh(g);PeB45Cell c=cell(64);CHECK(pe_game_build_cells(g,&c,1));
    PeBuildHistory journal=g->build_history;PeVec3 eye={32,11.55f,32.25f};PeVec3 direction={2.25f,-1.3f,0};
    CHECK(pe_game_mine(g,eye,direction)&&!g->cell_count);
    CHECK(!memcmp(&journal,&g->build_history,sizeof journal)&&pe_build_history_state_valid(g));
    rejected(g,before,true); /* Direct mining did not become a recoverable journal edit. */
    CHECK(strstr(g->message,"changed")!=NULL);
    /* Failed tool use also leaves redo intact. */
    fresh(g);CHECK(pe_game_build_cells(g,&c,1)&&pe_build_history_undo(g));journal=g->build_history;
    CHECK(!pe_game_mine(g,eye,(PeVec3){0,1,0})&&!memcmp(&journal,&g->build_history,sizeof journal));
}
static void malformed(PeGame *g,PeGame *before){
    fresh(g);PeB45Cell c=cell(64);CHECK(pe_game_build_cells(g,&c,1));PeBuildHistory valid=g->build_history;
    g->build_history.count=6;CHECK(!pe_build_history_state_valid(g));rejected(g,before,true);g->build_history=valid;
    g->build_history.cursor=2;CHECK(!pe_build_history_state_valid(g));rejected(g,before,true);g->build_history=valid;
    g->build_history.edits[0].count=65;CHECK(!pe_build_history_state_valid(g));rejected(g,before,true);g->build_history=valid;
    g->build_history.edits[0].cells[0].material=255;CHECK(!pe_build_history_state_valid(g));rejected(g,before,true);g->build_history=valid;
    g->build_history.edits[0].cells[0].x=-1;CHECK(!pe_build_history_state_valid(g));rejected(g,before,true);g->build_history=valid;
    g->build_history.edits[0].cells[1]=c;g->build_history.edits[0].count=2;CHECK(!pe_build_history_state_valid(g));rejected(g,before,true);g->build_history=valid;
    g->build_history.edits[4].cells[0]=c;CHECK(!pe_build_history_state_valid(g));rejected(g,before,true);
    memcpy(before,g,sizeof *g);PeB45Cell another=cell(65);CHECK(!pe_game_build_cells(g,&another,1)&&unchanged(g,before));
    int index=0;memcpy(before,g,sizeof *g);CHECK(!pe_game_remove_cells(g,&index,1)&&unchanged(g,before));
    g->build_history=valid;PeB45Cell twice[2]={c,c};CHECK(!pe_build_history_record(g,twice,2,true));
    pe_build_history_clear(g);CHECK(pe_build_history_state_valid(g)&&!pe_build_history_can_undo(g)&&!pe_build_history_can_redo(g));
    CHECK(!pe_build_history_state_valid(NULL)&&!pe_build_history_can_undo(NULL)&&!pe_build_history_can_redo(NULL));
    CHECK(!pe_build_history_undo(NULL)&&!pe_build_history_redo(NULL));pe_build_history_clear(NULL);
}
int pe_test_build_history(void){
    failures=0;PeGame *g=calloc(1,sizeof *g),*before=malloc(sizeof *before);CHECK(g&&before);
    if(g&&before){accounting(g,before);journal(g,before);resources(g,before);geometry(g,before);mining(g,before);malformed(g,before);}
    pe_game_destroy(g);free(g);free(before);return failures;
}
