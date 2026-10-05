/* Reproducible Adventure state produced through public gameplay transactions.
 * No graphics, input injection, invented resources, or direct progression edits.
 * The optional airborne copy deliberately changes only position/motion, as a
 * controlled regression fixture for preserving valid midair saves at runtime. */
#include "adventure_test_helpers.h"
#include "pe_game.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int slot(const PeGame *game,int item,bool last){
    int found=-1;
    for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(game->inventory.slots[i].item_id==item&&game->inventory.slots[i].count){
        found=i;if(!last)break;
    }
    return found;
}
static void report(const char *path,const PeGame *g){
    const PeSlot *tool=pe_game_equipped_tool(g);
    printf("%s: mode=%d mission=%d arrival=%d supplies=%d known=%d crafted=%u "
           "wood=%d dirt=%d stone=%d shovels=%d axes=%d juice=%d cells=%d "
           "craft=%d left=%d pending=%d elapsed=%.6f "
           "equipped=%d instance=%u durability=%.3f "
           "position=(%.6f,%.6f,%.6f) velocity=(%.6f,%.6f,%.6f) grounded=%d hp=%.1f/%.1f\n",
           path,g->mode,g->adventure_mission,g->arrival_complete,g->supplies_claimed,
           g->known_recipe_count,g->crafted,pe_inventory_count(&g->inventory,279),
           pe_inventory_count(&g->inventory,268),pe_inventory_count(&g->inventory,280),
           pe_inventory_count(&g->inventory,12),pe_inventory_count(&g->inventory,26),
           pe_inventory_count(&g->inventory,916),g->cell_count,
           g->craft.recipe_id,g->craft.batches_left,g->craft.batches_pending,g->craft.elapsed,
           tool?tool->item_id:0,tool?tool->instance_id:0,tool?tool->durability:0,
           g->player.x,g->player.y,g->player.z,g->velocity.x,g->velocity.y,g->velocity.z,
           g->grounded,g->hp,g->max_hp);
    printf("  recipes:");for(int i=0;i<g->known_recipe_count;i++)printf(" %d",g->known_recipes[i]);
    printf("; quickslots:");for(int i=0;i<PE_QUICKSLOTS;i++)printf(" %d:%u",g->quickslots[i].item_id,g->quickslots[i].instance_id);
    putchar('\n');
}
static bool refresh(PeGame *g,PeSandboxCollision *collision){
    char error[200];
    if(pe_game_collision_update(g,collision,error,sizeof error))return true;
    fprintf(stderr,"Fixture collision update: %s\n",error);return false;
}
static bool frames(PeGame *g,PeSandboxCollision *collision,int count){
    for(int i=0;i<count;i++)if(!pe_game_step_collision(g,(PeInput){0},PE_FIXED_DT,collision))return false;
    return true;
}
static bool place(PeGame *g,int shape){
    PeVec3 eye={g->player.x,g->player.y+1.58f,g->player.z};
    for(int slope=1;slope<=3;slope++)for(int angle=0;angle<32;angle++){
        float a=angle*6.28318530718f/32;
        if(pe_game_place_shape(g,eye,(PeVec3){cosf(a),-.5f*slope,sinf(a)},279,shape,angle%4,4))return true;
    }
    return false;
}
static bool save_verified(PeGame *g,PeGame *loaded,const char *path){
    char error[200];
    if(!pe_game_save(g,path,error,sizeof error)||!pe_game_load(loaded,path,error,sizeof error)){
        fprintf(stderr,"Fixture save/reload: %s\n",error);return false;
    }
    /* Save format intentionally excludes held-input action state and messages. */
    bool same=loaded->mode==PE_GAME_ADVENTURE&&loaded->arrival_complete&&loaded->supplies_claimed&&
        pe_world_hash(&loaded->world)==pe_world_hash(&g->world)&&
        memcmp(&loaded->inventory,&g->inventory,sizeof g->inventory)==0&&
        memcmp(loaded->quickslots,g->quickslots,sizeof g->quickslots)==0&&
        memcmp(&loaded->player,&g->player,sizeof g->player)==0&&
        memcmp(&loaded->velocity,&g->velocity,sizeof g->velocity)==0&&
        loaded->grounded==g->grounded&&loaded->cell_count==g->cell_count&&
        loaded->equipped_instance_id==g->equipped_instance_id&&
        loaded->known_recipe_count==1&&pe_game_recipe_known(loaded,1)&&loaded->crafted==1;
    if(!same)fprintf(stderr,"Fixture reload changed required state.\n");
    if(same)report(path,loaded);
    return same;
}
int main(int argc,char **argv){
    if(argc<2||argc>3||(strcmp(argv[1],"--inspect")==0&&argc!=3)){
        fprintf(stderr,"Usage: %s OUTPUT.pesave [AIRBORNE.pesave]\n       %s --inspect EXISTING.pesave\n",argv[0],argv[0]);return 2;
    }
    PeGame *game=calloc(1,sizeof *game),*loaded=calloc(1,sizeof *loaded);
    PeSandboxCollision *collision=pe_sandbox_collision_create();
    bool ok=false;char error[200];
    if(!game||!loaded||!collision)goto done;
    if(strcmp(argv[1],"--inspect")==0){
        ok=pe_game_load(game,argv[2],error,sizeof error);
        if(ok)report(argv[2],game);else fprintf(stderr,"%s\n",error);
        goto done;
    }
    if(argc==3&&strcmp(argv[1],argv[2])==0){fprintf(stderr,"Fixture output paths must differ.\n");goto done;}
    if(!pe_game_init_mode(game,666,PE_GAME_ADVENTURE)||!refresh(game,collision))goto done;
    if(game->known_recipe_count||pe_inventory_count(&game->inventory,279)!=200||
       !pe_test_ack_welcome(game)||!pe_game_arrival_complete(game)||!pe_game_accept_supplies(game)||
       !pe_game_use_slot(game,slot(game,406,false))||!pe_game_recipe_known(game,1)||
       !pe_game_start_craft(game,1,1)||!frames(game,collision,190))goto action_failed;
    if(game->craft.recipe_id||game->crafted!=1||pe_inventory_count(&game->inventory,279)!=190||
       pe_inventory_count(&game->inventory,12)!=2)goto action_failed;
    /* Choose the newly crafted instance, rather than the earlier supply shovel. */
    if(!pe_game_equip_slot(game,slot(game,12,true))||
       !pe_game_quickslot_assign(game,0,slot(game,279,false))||
       !pe_game_quickslot_assign(game,1,slot(game,916,false))||
       !pe_game_quickslot_assign(game,2,slot(game,12,true)))goto action_failed;
    uint32_t revision=game->world.revision;int swings=0;
    while(!game->gathered&&swings<40){
        PeVec3 eye={game->player.x,game->player.y+1.58f,game->player.z};
        if(!pe_game_mine(game,eye,(PeVec3){1,-1,0}))goto action_failed;
        swings++;
        if(!refresh(game,collision)||!frames(game,collision,45))goto action_failed;
    }
    if(!game->gathered||game->world.revision==revision||
       !place(game,PE_B45_CUBE)||!place(game,PE_B45_RAMP)||
       !refresh(game,collision)||!frames(game,collision,60))goto action_failed;
    if(game->cell_count!=8||pe_inventory_count(&game->inventory,279)!=188)goto action_failed;
    printf("Executed source start/quest/script/craft transactions; %d successful shovel swings, %u removed resource samples, two four-cell brushes.\n",swings,game->gathered);
    if(!save_verified(game,loaded,argv[1]))goto done;
    if(argc==3){
        game->player.y+=6;game->velocity=(PeVec3){0,-1,0};game->grounded=false;
        game->jumping=false;game->sprinting=false;game->jump_action_elapsed=0;
        if(pe_sandbox_collision_overlap(collision,game->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT)){
            fprintf(stderr,"Airborne fixture is obstructed or outside the bounded world.\n");goto done;
        }
        if(!save_verified(game,loaded,argv[2]))goto done;
        float y=loaded->player.y;
        if(!pe_game_step_collision(loaded,(PeInput){0},PE_FIXED_DT,collision)||
           loaded->player.y>=y||loaded->player.y<y-.1f||loaded->grounded){
            fprintf(stderr,"Airborne load did not continue falling from its saved position.\n");goto done;
        }
    }
    ok=true;goto done;
action_failed:
    fprintf(stderr,"Adventure fixture action failed: %s\n",game->message);
done:
    pe_sandbox_collision_free(collision);pe_game_destroy(loaded);pe_game_destroy(game);free(loaded);free(game);return ok?0:1;
}
