/* Follow observed original world-coordinate waypoints through normal C movement.
 * Fixture setup acknowledges the welcome/supplies APIs; it never relocates the
 * player after the archived initial spawn or edits terrain to open a path. */
#include "pe_game.h"
#include "pe_settlement.h"
#include "pe_town_interaction.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float x,y,z; } Waypoint;
int main(int argc,char **argv){
    if(argc<4||(argc-4)%2){fprintf(stderr,"Usage: %s WAYPOINTS.xyz CHECKPOINT.pesave RESULT.json [--arrival-save PATH] [--pre-annot-save PATH --pre-annot-waypoint INDEX]\n",argv[0]);return 2;}
    const char *arrival_path=NULL,*pre_path=NULL;size_t pre_index=SIZE_MAX;
    for(int i=4;i<argc;i+=2){
        if(!strcmp(argv[i],"--arrival-save"))arrival_path=argv[i+1];
        else if(!strcmp(argv[i],"--pre-annot-save"))pre_path=argv[i+1];
        else if(!strcmp(argv[i],"--pre-annot-waypoint")){
            char *end=NULL;unsigned long index=strtoul(argv[i+1],&end,10);
            if(!*argv[i+1]||*end||index>=4096)return 2;
            pre_index=(size_t)index;
        }else return 2;
    }
    if((pre_path!=NULL)!=(pre_index!=SIZE_MAX))return 2;
    Waypoint targets[4096];size_t count=0;
    FILE *input=fopen(argv[1],"r");if(!input){perror(argv[1]);return 1;}
    while(count<sizeof targets/sizeof targets[0]&&fscanf(input,"%f %f %f",&targets[count].x,&targets[count].y,&targets[count].z)==3){
        if(!isfinite(targets[count].x)||!isfinite(targets[count].y)||!isfinite(targets[count].z)){fclose(input);return 2;}
        count++;
    }
    bool input_ok=feof(input)&&count>0&&(!pre_path||pre_index<count);fclose(input);if(!input_ok){fprintf(stderr,"Invalid/oversized waypoint file\n");return 2;}
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);
    PeSandboxCollision *collision=pe_sandbox_collision_create();
    char error[256]={0};bool ok=false,saved=false,pre_saved=false,arrival_saved=false;size_t reached=0;unsigned ticks=0,stalled=0;
    double travelled=0;PeVec3 start={0},pre_position={0};
    if(!g||!loaded||!collision){snprintf(error,sizeof error,"Allocation failed");goto done;}
    if(!pe_game_init_mode(g,666,PE_GAME_ADVENTURE)||!pe_settlement_start(g,error,sizeof error)||!pe_settlement_is_route(&g->world)){
        if(!error[0])snprintf(error,sizeof error,"Complete route assets required");
        goto done;
    }
    while(g->welcome_page<PE_ADVENTURE_WELCOME_PAGES)if(!pe_game_welcome_advance(g,g->welcome_page))goto done;
    if(!pe_game_arrival_complete(g)||!pe_game_accept_supplies(g)||!pe_game_collision_update(g,collision,error,sizeof error))goto done;
    start=g->player;
    for(reached=0;reached<count;reached++){
        PeVec3 goal={targets[reached].x-g->world.origin_x,targets[reached].y-g->world.origin_y,targets[reached].z-g->world.origin_z};
        if(goal.x<PE_PLAYER_RADIUS||goal.x>=pe_world_width(&g->world)-PE_PLAYER_RADIUS-1||goal.z<PE_PLAYER_RADIUS||goal.z>=pe_world_depth(&g->world)-PE_PLAYER_RADIUS-1){snprintf(error,sizeof error,"Waypoint outside playable crop");goto done;}
        unsigned leg_ticks=0;stalled=0;
        while(hypotf(goal.x-g->player.x,goal.z-g->player.z)>1.25f){
            if(++ticks>60000||++leg_ticks>9000){snprintf(error,sizeof error,"Movement budget exhausted");goto done;}
            PeVec3 prior=g->player;float dx=goal.x-prior.x,dz=goal.z-prior.z,length=hypotf(dx,dz);
            /* Normal run input. A short obstruction triggers an ordinary jump,
             * charged by PeGame; all contact remains in the movement controller. */
            PeInput controls={.move_x=dx/length,.move_z=dz/length,.jump=stalled==60};
            if(!pe_game_step_collision(g,controls,PE_FIXED_DT,collision)){snprintf(error,sizeof error,"Collision movement rejected state");goto done;}
            double step=hypot(g->player.x-prior.x,g->player.z-prior.z);travelled+=step;
            if(step<.002)stalled++;else stalled=0;
            if(stalled>240){snprintf(error,sizeof error,"Observed waypoint blocked by native geometry");goto done;}
            if(pe_sandbox_collision_overlap(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT)){snprintf(error,sizeof error,"Player penetrated geometry");goto done;}
            pe_adventure_annot_step(g);
        }
        if(!saved&&reached>=count/2){
            if(!pe_game_save(g,argv[2],error,sizeof error)||!pe_game_load(loaded,argv[2],error,sizeof error))goto done;
            if(memcmp(&g->player,&loaded->player,sizeof g->player)||memcmp(&g->velocity,&loaded->velocity,sizeof g->velocity)||
               g->game_seconds!=loaded->game_seconds||g->stamina!=loaded->stamina||memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory)||
               pe_world_hash(&g->world)!=pe_world_hash(&loaded->world)){
                snprintf(error,sizeof error,"Midroute save did not retain movement/game state");goto done;
            }
            pe_game_move(g,loaded);saved=true;
            if(!pe_game_collision_update(g,collision,error,sizeof error))goto done;
        }
        if(pre_path&&reached==pre_index){
            if(g->annot_complete||g->money!=500||!g->supplies_claimed){snprintf(error,sizeof error,"Pre-Annot checkpoint must retain normal uncompleted progression");goto done;}
            /* Face the next observed waypoint for physical continuation. This
             * changes only the saved camera heading, never position or money. */
            if(!pe_game_clone(loaded,g)){snprintf(error,sizeof error,"Checkpoint clone failed");goto done;}
            if(reached+1<count)loaded->yaw=atan2f(targets[reached+1].x-(loaded->player.x+loaded->world.origin_x),
                                                loaded->player.z+loaded->world.origin_z-targets[reached+1].z);
            if(!pe_game_save(loaded,pre_path,error,sizeof error))goto done;
            pre_position=loaded->player;pre_saved=true;pe_game_destroy(loaded);
        }
    }
    pe_adventure_annot_step(g);
    ok=saved&&g->annot_complete&&g->money==500&&(!pre_path||pre_saved);
    if(!ok)snprintf(error,sizeof error,"Route did not enter the Annot completion radius");
    if(ok&&arrival_path){
        ok=pe_game_save(g,arrival_path,error,sizeof error)&&pe_game_load(loaded,arrival_path,error,sizeof error)&&
           loaded->annot_complete&&loaded->money==500&&loaded->supplies_claimed&&
           !memcmp(&g->player,&loaded->player,sizeof g->player)&&!memcmp(&g->velocity,&loaded->velocity,sizeof g->velocity)&&
           !memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory)&&g->game_seconds==loaded->game_seconds;
        arrival_saved=ok;
        if(!ok&&!error[0])snprintf(error,sizeof error,"Final route save did not retain completed progression");
    }
 done:;
    FILE *output=fopen(argv[3],"w");
    if(output){
        fprintf(output,"{\n  \"schema\":1,\n  \"kind\":\"implemented-C-headless-route-movement\",\n  \"success\":%s,\n  \"waypointsReached\":%zu,\n  \"waypoints\":%zu,\n  \"ticks\":%u,\n  \"fixedDt\":%.9g,\n  \"sampledTravelMeters\":%.9g,\n  \"midrouteSaveReload\":%s,\n  \"annotComplete\":%s,\n  \"startLocal\":[%.9g,%.9g,%.9g],\n  \"finalSource\":[%.9g,%.9g,%.9g],\n  \"error\":\"%s\",\n",
            ok?"true":"false",reached,count,ticks,(double)PE_FIXED_DT,travelled,saved?"true":"false",g&&g->annot_complete?"true":"false",
            (double)start.x,(double)start.y,(double)start.z,g?(double)g->player.x+g->world.origin_x:0,g?(double)g->player.y+g->world.origin_y:0,g?(double)g->player.z+g->world.origin_z:0,error);
        fprintf(output,"  \"money\":%u,\n  \"arrivalSaveReload\":%s,\n  \"preAnnotSave\":%s,\n  \"preAnnotSource\":",g?g->money:0,arrival_saved?"true":"false",pre_saved?"true":"false");
        if(pre_saved)fprintf(output,"[%.9g,%.9g,%.9g]",(double)pre_position.x+g->world.origin_x,(double)pre_position.y+g->world.origin_y,(double)pre_position.z+g->world.origin_z);
        else fprintf(output,"null");
        fprintf(output,"\n}\n");
        if(fclose(output))ok=false;
    }else{perror(argv[3]);ok=false;}
    fprintf(stderr,"Headless route: %s; %zu/%zu waypoints; %u ticks; %s\n",ok?"PASS":"FAIL",reached,count,ticks,error);
    pe_sandbox_collision_free(collision);pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);return ok?0:1;
}
