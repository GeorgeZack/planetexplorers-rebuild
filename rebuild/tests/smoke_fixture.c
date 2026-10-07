/* Produce an exercised game state for repeatable graphics and reference captures. */
#include "pe_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static bool place_brush(PeGame *g,int shape) {
    PeVec3 eye={g->player.x,g->player.y+1.58f,g->player.z};
    for(int slope=1;slope<=3;slope++)for(int angle=0;angle<32;angle++) {
        float a=angle*6.28318530718f/32;
        if(pe_game_place_shape(g,eye,(PeVec3){cosf(a),-.5f*slope,sinf(a)},
                               PE_ITEM_WOOD,shape,angle%4,4))return true;
    }
    fprintf(stderr,"Construction fixture failed: %s\n",g->message);return false;
}
int main(int argc,char **argv) {
    if(argc!=2){fprintf(stderr,"Usage: %s OUTPUT.pesave\n",argv[0]);return 2;}
    PeGame *g=malloc(sizeof(*g));if(!g)return 1;
    pe_game_init(g,666);
    /* Bootstrap through the actual gather, craft, step and equip interfaces. */
    for(int i=0;i<24;i++) {
        if(!pe_game_gather(g)){fprintf(stderr,"Gather failed: %s\n",g->message);free(g);return 1;}
        for(int step=0;step<40;step++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    }
    if(!pe_game_start_craft(g,7,1)){free(g);return 1;}
    for(int i=0;i<190;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    bool equipped=false;
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==19)equipped=pe_game_equip_slot(g,i);
    PeVec3 eye={g->player.x,g->player.y+1.58f,g->player.z};
    uint32_t before=g->world.revision;
    if(!equipped||!pe_game_mine(g,eye,(PeVec3){1,-1,0})||g->world.revision==before){fprintf(stderr,"Tool action failed: %s\n",g->message);free(g);return 1;}
    if(!place_brush(g,PE_B45_CUBE)||!place_brush(g,PE_B45_RAMP)||
       !pe_game_start_craft(g,152,5)){free(g);return 1;}
    for(int i=0;i<190;i++)pe_game_step(g,(PeInput){0},PE_FIXED_DT);
    char error[200];bool saved=pe_game_save(g,argv[1],error,sizeof(error));
    if(!saved)fprintf(stderr,"%s\n",error);
    else printf("Saved exercised fixture: tool equipped, terrain mined, %d construction cells, crafting active.\n",g->cell_count);
    free(g);return saved?0:1;
}
