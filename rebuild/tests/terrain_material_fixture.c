/* Controlled graphics fixture: a cut across all four render regions exposes
 * synthetic grass/soil/stone. Tree removal here is fixture setup only. */
#include "pe_game.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    PeGame *g=malloc(sizeof *g);PeSandboxCollision *c=pe_sandbox_collision_create();
    char error[256]={0};bool ok=false;
    if(!g||!c||!pe_game_init_mode(g,73,PE_GAME_ADVENTURE))goto done;
    g->tree_count=0;
    PeVec3 center={32,pe_world_height(&g->world,32,32)-1,32};
    if(!pe_world_dig(&g->world,center,5.5f)||!pe_game_collision_update(g,c,error,sizeof error))goto done;
    PeVec3 preferred={36,pe_world_height(&g->world,36,40)+1,40},settled;
    if(!pe_sandbox_collision_settle(c,preferred,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,4,5,&settled))goto done;
    g->player=settled;g->velocity=(PeVec3){0};g->grounded=true;
    g->yaw=atan2f(32-g->player.x,g->player.z-32);g->pitch=-.42f;
    ok=pe_game_save(g,argv[1],error,sizeof error);
done:
    if(!ok)fprintf(stderr,"Terrain material fixture failed: %s\n",error);
    free(g);pe_sandbox_collision_free(c);return ok?0:1;
}
