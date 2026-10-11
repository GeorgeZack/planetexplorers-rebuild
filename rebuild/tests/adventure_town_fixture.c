/* TEST-ONLY graphical fixtures: actual source terrain/building collision and
 * archived people. Public welcome/Arrival/supply transactions precede explicit
 * test funding. No graphics, NPC relocation, fake geometry or quest flag edits. */
#include "pe_test_paths.h"
#include "adventure_test_helpers.h"
#include "pe_adventure_npc.h"
#include "pe_actor_scene.h"
#include "pe_settlement.h"
#include "pe_town_interaction.h"
#include "../generated/pe_adventure_population.h"
#include "../generated/pe_adventure_settlement.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool present(const char *path){FILE *file=fopen(path,"rb");if(!file)return false;fclose(file);return true;}
static bool start_world(PeGame *game,const char *assets,char *error,size_t cap){
    char terrain[4096],collision[4096];
    int a=snprintf(terrain,sizeof terrain,"%s/adventure-route/terrain.pevx",assets);
    int b=snprintf(collision,sizeof collision,"%s/adventure-route/route-collision.obj",assets);
    if(a<0||b<0||(size_t)a>=sizeof terrain||(size_t)b>=sizeof collision){snprintf(error,cap,"Asset path too long");return false;}
    if(!present(terrain)||!present(collision)){
        a=snprintf(terrain,sizeof terrain,"%s/adventure-settlement/terrain.pevx",assets);
        if(a<0||(size_t)a>=sizeof terrain){snprintf(error,cap,"Settlement path too long");return false;}
    }
    PeWorld *world=calloc(1,sizeof *world);if(!world){snprintf(error,cap,"World allocation failed");return false;}
    bool ok=pe_settlement_load_world(terrain,world,error,cap);
    if(ok){
        const double *p=pe_adventure_settlement_initial_spawn;
        PeVec3 spawn={(float)(p[0]-world->origin_x),(float)(p[1]-world->origin_y),(float)(p[2]-world->origin_z)};
        world->seed=game->world.seed;ok=pe_game_set_world(game,world,spawn);
        if(!ok)snprintf(error,cap,"Actual recovered world has no safe initial spawn/collision");
    }
    pe_world_destroy(world);free(world);return ok;
}
static PeVec3 add(PeVec3 a,PeVec3 b){return(PeVec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static PeVec3 scale(PeVec3 a,float s){return(PeVec3){a.x*s,a.y*s,a.z*s};}
static void camera_ray(const PeGame *g,const PeSandboxCollision *collision,PeVec3 *origin,PeVec3 *direction){
    PeVec3 d={sinf(g->yaw)*cosf(g->pitch),sinf(g->pitch),-cosf(g->yaw)*cosf(g->pitch)};
    PeVec3 eye=add(g->player,(PeVec3){0,1.58f,0}),target=add(eye,scale(d,2));
    PeVec3 delta=add(scale(d,-6),(PeVec3){0,.5f,0});
    PeSceneHit hit=pe_sandbox_collision_sweep_geometry(collision,add(eye,(PeVec3){0,-.1f,0}),.1f,.2f,delta);
    *origin=add(eye,scale(delta,hit.hit?fmaxf(.05f,hit.fraction-.02f):1));
    *direction=add(target,scale(*origin,-1));
    float length=sqrtf(direction->x*direction->x+direction->y*direction->y+direction->z*direction->z);
    *direction=scale(*direction,1/length);
}
static bool selectable(const PeGame *g,const PeAdventureNpc *npc,bool male,const PeSandboxCollision *collision,float *distance){
    PeVec3 origin,direction;camera_ray(g,collision,&origin,&direction);
    PeSceneHit obstacle=pe_sandbox_collision_ray_geometry(collision,origin,direction,100);
    return pe_adventure_npc_pick_sex(npc,male,g->player,origin,direction,obstacle.hit?obstacle.distance:INFINITY,distance);
}
static bool position(PeGame *g,const PeAdventureNpc *npc,bool male,const PeSandboxCollision *collision,bool annot){
    PeVec3 best={0};float best_yaw=0,best_pitch=0,best_score=INFINITY;
    float front=npc->yaw*0.0174532925199433f;unsigned supported=0,picked=0;
    /* Start directly in front; try the other supported directions only when
     * authored walls/steps make that candidate unavailable. */
    for(int ring=0;ring<4;ring++)for(int angle=0;angle<48;angle++){
        float a=front+angle*6.28318530718f/48,radius=2.75f+ring*.25f;
        PeVec3 wanted={npc->position.x+sinf(a)*radius,npc->position.y+.2f,npc->position.z+cosf(a)*radius},settled;
        if(!pe_sandbox_collision_settle(collision,wanted,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,4,5,&settled))continue;
        if(annot&&hypotf(settled.x+g->world.origin_x+284,settled.z+g->world.origin_z-134)>=15)continue;
        supported++;g->player=settled;g->yaw=atan2f(npc->position.x-settled.x,settled.z-npc->position.z);
        for(int slope=0;slope<=90;slope++){
            g->pitch=-.65f+slope*.01f;float distance;
            if(!selectable(g,npc,male,collision,&distance))continue;
            picked++;PeVec3 origin,direction;camera_ray(g,collision,&origin,&direction);
            PeVec3 hit=add(origin,scale(direction,distance));
            /* Prefer torso framing from the NPC front, and source eye height. */
            float score=fabsf(hit.y-(npc->position.y+.95f))+(1-cosf(a-front))*.25f+ring*.03f+fabsf(settled.y-npc->position.y)*.2f;
            if(score<best_score){best_score=score;best=settled;best_yaw=g->yaw;best_pitch=g->pitch;}
        }
    }
    if(!isfinite(best_score)){fprintf(stderr,"Fixture search: %u supported candidates, %u selectable samples, NPC local=(%.3f,%.3f,%.3f)\n",supported,picked,npc->position.x,npc->position.y,npc->position.z);return false;}
    g->player=best;g->yaw=best_yaw;g->pitch=best_pitch;g->velocity=(PeVec3){0};g->grounded=true;
    return !pe_sandbox_collision_overlap(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT);
}
static void report(const char *path,const PeGame *g){
    printf("TEST ONLY %s mode=%d world=%dx%dx%d origin=(%d,%d,%d) welcome=%u arrival=%d supplies=%d annot=%d mission=%d money=%u position=(%.6f,%.6f,%.6f) source=(%.6f,%.6f,%.6f) yaw=%.8f pitch=%.8f\n",
        path,g->mode,pe_world_width(&g->world),pe_world_size_y(&g->world),pe_world_depth(&g->world),g->world.origin_x,g->world.origin_y,g->world.origin_z,
        g->welcome_page,g->arrival_complete,g->supplies_claimed,g->annot_complete,g->adventure_mission,g->money,g->player.x,g->player.y,g->player.z,
        g->player.x+g->world.origin_x,g->player.y+g->world.origin_y,g->player.z+g->world.origin_z,g->yaw,g->pitch);
    printf("  inventory:");for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].count)printf(" %dx%d",g->inventory.slots[i].item_id,g->inventory.slots[i].count);
    printf("\n  changed stock:");for(int i=0;i<PE_TOWN_STOCK_CAPACITY;i++)if(g->town_stock[i].initialized)printf(" %d:%u",i,g->town_stock[i].remaining);putchar('\n');
}
int main(int argc,char **argv){
    if((argc!=3&&argc!=5)||(strcmp(argv[1],"--shop")&&strcmp(argv[1],"--annot")&&strcmp(argv[1],"--inspect"))||(argc==5&&strcmp(argv[3],"--assets"))){
        fprintf(stderr,"Usage: %s --shop|--annot|--inspect FILE.pesave [--assets ASSET_ROOT]\n",argv[0]);return 2;
    }
    bool annot=!strcmp(argv[1],"--annot"),inspect=!strcmp(argv[1],"--inspect"),ok=false;
    const char *path=argv[2],*assets=argc==5?argv[4]:pe_test_path("rebuild/build/reference-assets");
    char error[256]="Fixture allocation failed";float pick_distance=0;
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);
    PeSandboxCollision *collision=pe_sandbox_collision_create();PeAdventureNpc npc={0};
    if(!g||!loaded||!collision)goto done;
    if(!pe_settlement_configure(assets,error,sizeof error))goto done;
    if(inspect){ok=pe_game_load(g,path,error,sizeof error);if(ok)report(path,g);goto done;}
    if(!pe_game_init_mode(g,666,PE_GAME_ADVENTURE)||!start_world(g,assets,error,sizeof error))goto done;
    if(annot&&!pe_settlement_is_route(&g->world)){snprintf(error,sizeof error,"Annot fixture requires the actual recovered route terrain and buildings");goto done;}
    if(!pe_test_ack_welcome(g)||!pe_game_arrival_complete(g)||!pe_game_accept_supplies(g)){snprintf(error,sizeof error,"Public Adventure transaction failed: %s",g->message);goto done;}
    /* Deliberate fixture-only money grant, never a runtime/progression rule. */
    g->money+=10000;
    if(!pe_game_collision_update(g,collision,error,sizeof error))goto done;
    const PeAdventurePopulationSource *source=NULL;
    for(int i=0;i<PE_ADVENTURE_POPULATION_COUNT;i++)if(pe_adventure_population_sources[i].entity_id==(annot?20018:20002))source=&pe_adventure_population_sources[i];
    if(!source){snprintf(error,sizeof error,"Required archived fixture NPC metadata unavailable");goto done;}
    char scene_path[4096];int scene_length=snprintf(scene_path,sizeof scene_path,"%s/%s",assets,source->scene);
    if(scene_length<0||(size_t)scene_length>=sizeof scene_path){snprintf(error,sizeof error,"Actor asset path too long");goto done;}
    PeActorSceneManifest *scene=pe_actor_scene_manifest_load(scene_path,error,sizeof error);
    if(!scene)goto done;
    pe_actor_scene_manifest_free(scene);
    PeVec3 anchor={source->position[0]-g->world.origin_x,source->position[1]-g->world.origin_y,source->position[2]-g->world.origin_z};
    if(!pe_adventure_npc_refresh_source(&npc,&g->world,collision,anchor,source->yaw_degrees,error,sizeof error))goto done;
    if(!position(g,&npc,source->male!=0,collision,annot)||!selectable(g,&npc,source->male!=0,collision,&pick_distance)){
        snprintf(error,sizeof error,"No supported selectable fixture pose by %s",source->name);goto done;
    }
    if(g->annot_complete||!pe_game_save(g,path,error,sizeof error)||!pe_game_load(loaded,path,error,sizeof error))goto done;
    if(loaded->annot_complete||!loaded->arrival_complete||!loaded->supplies_claimed||loaded->welcome_page!=9||loaded->money!=g->money||
       memcmp(&loaded->inventory,&g->inventory,sizeof g->inventory)||memcmp(&loaded->player,&g->player,sizeof g->player)||
       loaded->yaw!=g->yaw||loaded->pitch!=g->pitch||pe_world_hash(&loaded->world)!=pe_world_hash(&g->world)||
       !selectable(loaded,&npc,source->male!=0,collision,&pick_distance)){
        snprintf(error,sizeof error,"Native fixture save roundtrip or selection failed");goto done;
    }
    report(path,loaded);
    printf("  NPC entity%d %s source=(%.6f,%.6f,%.6f) camera_pick=%.6f; explicit test grant=10000\n",source->entity_id,source->name,source->position[0],source->position[1],source->position[2],pick_distance);
    /* Prove the pending transition using only the reloaded disposable copy. */
    if(pe_adventure_annot_step(loaded)!=annot||loaded->annot_complete!=annot){snprintf(error,sizeof error,"Fixture Annot eligibility differs from requested case");goto done;}
    printf("PASS: native v8 %s fixture saved; Annot remains incomplete in the file.\n",annot?"Annot arrival":"Cherish shop");ok=true;
done:
    if(!ok)fprintf(stderr,"Town fixture: %s\n",error);
    pe_sandbox_collision_free(collision);pe_game_destroy(loaded);pe_game_destroy(g);free(loaded);free(g);return ok?0:1;
}
