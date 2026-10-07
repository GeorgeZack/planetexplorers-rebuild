/* Creates an isolated manual-test checkpoint without opening a display. */
#include "pe_vegetation.h"
#include "pe_settlement.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
    if(argc!=2){fprintf(stderr,"Usage: %s OUTPUT.pesave\n",argv[0]);return 2;}
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);char error[256]={0};bool ok=false;
    if(!g||!loaded||!pe_game_init_mode(g,666,PE_GAME_ADVENTURE)||!pe_settlement_start(g,error,sizeof error))goto done;
    if(!pe_vegetation_install_fixture(g,g->player)){snprintf(error,sizeof error,"Patch did not fit at initial spawn");goto done;}
    if(!pe_game_save(g,argv[1],error,sizeof error)||!pe_game_load(loaded,argv[1],error,sizeof error))goto done;
    if(memcmp(&g->vegetation,&loaded->vegetation,sizeof g->vegetation)||g->tree_count!=loaded->tree_count){snprintf(error,sizeof error,"Vegetation save mismatch");goto done;}
    printf("Player local %.3f %.3f %.3f; world origin %d %d %d\n",g->player.x,g->player.y,g->player.z,g->world.origin_x,g->world.origin_y,g->world.origin_z);
    for(unsigned i=0;i<g->vegetation.count;i++){const PePlant *p=&g->vegetation.plants[i];printf("Plant %u prototype %u local %.3f %.3f %.3f\n",i,p->prototype_id,p->position.x,p->position.y,p->position.z);}
    for(int i=0;i<g->tree_count;i++)printf("Tree %d local %.3f %.3f %.3f\n",i,g->trees[i].position.x,g->trees[i].position.y,g->trees[i].position.z);
    ok=true;
done:
    fprintf(stderr,"Vegetation checkpoint: %s %s\n",ok?"PASS":"FAIL",error);
    pe_game_destroy(g);free(g);pe_game_destroy(loaded);free(loaded);return ok?0:1;
}
