/* Controlled encounter fixture: public Adventure start and actual NPC placement;
 * supplier mode ACKs welcome through the public transaction; --welcome keeps
 * initial progression. No Arrival/supply transaction is committed. */
#include "adventure_test_helpers.h"
#include "pe_adventure_dialogue.h"
#include "pe_adventure_npc.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PeVec3 add(PeVec3 a,PeVec3 b){return(PeVec3){a.x+b.x,a.y+b.y,a.z+b.z};}
static PeVec3 scale(PeVec3 a,float s){return(PeVec3){a.x*s,a.y*s,a.z*s};}
static void camera_ray(const PeGame *g,const PeSandboxCollision *c,PeVec3 *origin,PeVec3 *direction){
    PeVec3 d={sinf(g->yaw)*cosf(g->pitch),sinf(g->pitch),-cosf(g->yaw)*cosf(g->pitch)};
    PeVec3 eye=add(g->player,(PeVec3){0,1.58f,0}),target=add(eye,scale(d,2));
    PeVec3 delta=add(scale(d,-6),(PeVec3){0,.5f,0});
    PeSceneHit h=pe_sandbox_collision_sweep_geometry(c,add(eye,(PeVec3){0,-.1f,0}),.1f,.2f,delta);
    *origin=add(eye,scale(delta,h.hit?fmaxf(.05f,h.fraction-.02f):1));
    *direction=add(target,scale(*origin,-1));
    *direction=scale(*direction,1/sqrtf(direction->x*direction->x+direction->y*direction->y+direction->z*direction->z));
}
/* Conservative original tree-mesh bounds for fixture framing, not collision. */
static bool foliage(PeVec3 tree,PeVec3 a,PeVec3 b){
    float lo[]={tree.x-2.65711046f,tree.y-.24681623f,tree.z-3.46337006f};
    float hi[]={tree.x+2.15026356f,tree.y+7.78633261f,tree.z+2.08653378f};
    float p[]={a.x,a.y,a.z},d[]={b.x-a.x,b.y-a.y,b.z-a.z};float enter=0,leave=1;
    for(int i=0;i<3;i++){
        if(fabsf(d[i])<1e-7f){if(p[i]<lo[i]||p[i]>hi[i])return false;continue;}
        float first=(lo[i]-p[i])/d[i],last=(hi[i]-p[i])/d[i];
        if(first>last){float t=first;first=last;last=t;}
        enter=fmaxf(enter,first);leave=fminf(leave,last);if(enter>leave)return false;
    }
    return true;
}
static bool pick(const PeGame *g,const PeAdventureNpc *npc,const PeSandboxCollision *c,float *distance){
    PeVec3 o,d;camera_ray(g,c,&o,&d);
    PeSceneHit obstacle=pe_sandbox_collision_ray_geometry(c,o,d,100);
    return pe_adventure_npc_pick(npc,g->player,o,d,obstacle.hit?obstacle.distance:INFINITY,distance);
}
static bool position(PeGame *g,const PeAdventureNpc *npc,const PeSandboxCollision *c,int *occluders){
    PeVec3 best={0};float yaw=0,pitch=0,best_score=INFINITY;int best_foliage=0;
    for(int ring=0;ring<4;ring++)for(int angle=0;angle<64;angle++){
        float a=angle*6.28318530718f/64,radius=3+ring*.25f;
        PeVec3 preferred={npc->position.x+cosf(a)*radius,0,npc->position.z+sinf(a)*radius},settled;
        preferred.y=pe_world_height(&g->world,preferred.x,preferred.z)+.15f;
        if(!pe_sandbox_collision_settle(c,preferred,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,4,5,&settled))continue;
        g->player=settled;g->yaw=atan2f(npc->position.x-settled.x,settled.z-npc->position.z);
        for(int slope=0;slope<=100;slope++){
            g->pitch=-.7f+slope*.01f;float distance;
            if(!pick(g,npc,c,&distance))continue;
            PeVec3 o,d;camera_ray(g,c,&o,&d);
            PeVec3 hit=add(o,scale(d,distance)),torso=add(npc->position,(PeVec3){0,.9f,0});
            int blocked=0;for(int t=0;t<g->tree_count;t++)if(g->trees[t].remaining>0&&
                (foliage(g->trees[t].position,o,torso)||foliage(g->trees[t].position,o,add(settled,(PeVec3){0,1.2f,0}))))blocked++;
            float score=blocked*100+fabsf(hit.y-torso.y)+ring*.1f;
            if(score<best_score){best_score=score;best=settled;yaw=g->yaw;pitch=g->pitch;best_foliage=blocked;}
        }
    }
    if(!isfinite(best_score))return false;
    g->player=best;g->yaw=yaw;g->pitch=pitch;g->velocity=(PeVec3){0};g->grounded=true;
    *occluders=best_foliage;return true;
}
static void report(const char *path,const PeGame *g){
    printf("%s mode=%d mission=%d welcome_page=%u arrival=%d supplies=%d known=%d wood=%d meat=%d juice=%d "
        "shovels=%d pickaxes=%d axes=%d script406=%d position=(%.6f,%.6f,%.6f) yaw=%.8f pitch=%.8f grounded=%d\n",
        path,g->mode,g->adventure_mission,g->welcome_page,g->arrival_complete,g->supplies_claimed,g->known_recipe_count,
        pe_inventory_count(&g->inventory,279),pe_inventory_count(&g->inventory,229),pe_inventory_count(&g->inventory,916),
        pe_inventory_count(&g->inventory,12),pe_inventory_count(&g->inventory,19),pe_inventory_count(&g->inventory,26),
        pe_inventory_count(&g->inventory,406),g->player.x,g->player.y,g->player.z,g->yaw,g->pitch,g->grounded);
    printf("  inventory:");for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].count)
        printf(" %dx%d",g->inventory.slots[i].item_id,g->inventory.slots[i].count);
    putchar('\n');
}
int main(int argc,char **argv){
    bool welcome=argc==3&&!strcmp(argv[1],"--welcome");
    bool inspect=argc==3&&!strcmp(argv[1],"--inspect");
    if(!welcome&&!inspect&&!(argc==2&&argv[1][0]!='-')){
        fprintf(stderr,"Usage: %s [--welcome] OUTPUT.pesave\n       %s --inspect EXISTING.pesave\n",argv[0],argv[0]);return 2;
    }
    const char *path=argv[argc-1];PeVec3 anchor=welcome?(PeVec3){28,0,35}:(PeVec3){32,0,27};
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);
    PeSandboxCollision *c=pe_sandbox_collision_create();PeAdventureNpc npc={0},reloaded_npc={0};
    bool ok=false;char error[200]="Fixture allocation failed";int occluders=0;float distance=0;
    if(!g||!loaded||!c)goto done;
    if(inspect){ok=pe_game_load(g,path,error,sizeof error);if(ok)report(path,g);goto done;}
    if(!pe_game_init_mode(g,666,PE_GAME_ADVENTURE)||(!welcome&&!pe_test_ack_welcome(g))||
       !pe_game_collision_update(g,c,error,sizeof error)||
       !pe_adventure_npc_refresh_at(&npc,&g->world,c,anchor,error,sizeof error))goto done;
    strcpy(error,"No supported selectable player pose near the actual NPC");
    if(!position(g,&npc,c,&occluders)||!pick(g,&npc,c,&distance))goto done;
    strcpy(error,"Fixture unexpectedly changed progression");
    int expected_mission=welcome?0:PE_ADVENTURE_ARRIVAL;unsigned expected_page=welcome?0:PE_ADVENTURE_WELCOME_PAGES;
    if(g->arrival_complete||g->supplies_claimed||g->adventure_mission!=expected_mission||g->welcome_page!=expected_page||g->known_recipe_count)goto done;
    *loaded=*g;
    if(welcome){
        PeAdventureDialogue dialogue;pe_adventure_dialogue_init(&dialogue);
        if(pe_adventure_arrival_step(loaded,npc.spawn_position)||
           !pe_adventure_dialogue_open(&dialogue,loaded,1,20008,true))goto done;
        const PeAdventureDialogueLine *line=pe_adventure_dialogue_current(&dialogue,loaded);
        if(!line||line->talk_id!=1949)goto done;
    }else if(!pe_adventure_arrival_step(loaded,npc.spawn_position)||!loaded->arrival_complete||loaded->supplies_claimed)goto done;
    if(!pe_game_save(g,path,error,sizeof error)||!pe_game_load(loaded,path,error,sizeof error))goto done;
    strcpy(error,"Saved fixture failed native state/encounter roundtrip");
    if(loaded->arrival_complete||loaded->supplies_claimed||loaded->adventure_mission!=expected_mission||loaded->welcome_page!=expected_page||
       memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory)||memcmp(&g->player,&loaded->player,sizeof g->player)||
       g->yaw!=loaded->yaw||g->pitch!=loaded->pitch||pe_world_hash(&g->world)!=pe_world_hash(&loaded->world)||
       !pe_adventure_npc_refresh_at(&reloaded_npc,&loaded->world,c,anchor,error,sizeof error)||
       memcmp(&npc.position,&reloaded_npc.position,sizeof npc.position)||!pick(loaded,&reloaded_npc,c,&distance))goto done;
    report(path,loaded);
    printf("  %s=(%.6f,%.6f,%.6f) player_XZ_distance=%.6f camera_pick=%.6f foliage_bounds=%d\n",
        welcome?"welcome":"supplier",npc.position.x,npc.position.y,npc.position.z,
        hypotf(g->player.x-npc.position.x,g->player.z-npc.position.z),distance,occluders);
    printf("PASS: %s fixture saved with Arrival and supplies unclaimed.\n",welcome?"initial welcome":"post-welcome supplier");ok=true;
done:
    if(!ok)fprintf(stderr,"%s\n",error);
    pe_sandbox_collision_free(c);pe_game_destroy(loaded);pe_game_destroy(g);free(loaded);free(g);return ok?0:1;
}
