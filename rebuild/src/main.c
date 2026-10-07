/* New native C frontend. Original content provenance: generated/content_manifest.json. */
#include "pe_game.h"
#include "pe_legacy.h"
#include "pe_reference.h"
#include "pe_training_view.h"
#include "pe_actor_view.h"
#include "pe_actor_scene.h"
#include "pe_sandbox_view.h"
#include "pe_app_menu.h"
#include "pe_settlement.h"
#include "pe_vegetation.h"
#include <raylib.h>
#include <raymath.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

static void destroy_game(PeGame *game){if(game){pe_game_destroy(game);free(game);}}
static const Color ink = {219, 234, 228, 255};
static const Color muted = {137, 166, 162, 255};
static bool parse_origin(const char *text,int out[3]) {
    const char *p=text;
    for(int i=0;i<3;i++) {
        char *end=NULL;errno=0;long value=strtol(p,&end,10);
        if(errno||p==end||value<0||value>INT_MAX||(*end!=(i==2?'\0':',')))return false;
        out[i]=(int)value;p=end+1;
    }
    return true;
}
static bool import_world(PeGame *game,const char *path,const int *explicit_origin,char *error,size_t cap) {
    PeLegacyFile *file=pe_legacy_open(path,0,error,cap);
    if(!file)return false;
    PeLegacyInfo info=pe_legacy_info(file);
    int x=0,y=0,z=0,origin[3];
    if(explicit_origin) {
        if(explicit_origin[0]>info.extent_x-PE_WORLD_X || explicit_origin[1]>info.extent_y-PE_WORLD_Y || explicit_origin[2]>info.extent_z-PE_WORLD_Z) {
            pe_legacy_close(file);snprintf(error,cap,"Crop origin is outside this terrain file.");return false;
        }
        memcpy(origin,explicit_origin,sizeof(origin));
        x=origin[0]+32;y=origin[1]+16;z=origin[2]+32;
    } else if(pe_legacy_find_surface(file,&x,&y,&z,error,cap)) {
        origin[0]=(int)Clamp((float)x-32,0,(float)info.extent_x-PE_WORLD_X);
        origin[1]=(int)Clamp((float)y-10,0,(float)info.extent_y-PE_WORLD_Y);
        origin[2]=(int)Clamp((float)z-32,0,(float)info.extent_z-PE_WORLD_Z);
    } else {pe_legacy_close(file);return false;}
    PeWorld *world=calloc(1,sizeof(*world));
    if(!world){pe_legacy_close(file);snprintf(error,cap,"Out of memory importing terrain.");return false;}
    bool ok=pe_legacy_crop(file,origin[0],origin[1],origin[2],world,error,cap);
    pe_legacy_close(file);
    if(ok) {
        PeVec3 preferred={(float)(x-origin[0]),(float)(y-origin[1])+1,(float)(z-origin[2])};
        ok=pe_game_set_world(game,world,preferred);
        if(!ok)snprintf(error,cap,"No safe player spawn in this terrain crop.");
    }
    pe_world_destroy(world);free(world);return ok;
}
static void usage(const char *name) {
    printf("Usage: %s [--seed UINT32] [--save PATH] [--smoke FRAMES] [--screenshot PATH]\n"
           "       [--load PATH] [--terrain FILE.voxelform] [--origin X,Y,Z]\n"
           "       [--smoke-panel inventory|crafting|character|missions|building] [--reference-room FILE.obj]\n"
           "       [--training-room ASSET_DIRECTORY] [--reference-actor FILE.peac]\n"
           "       [--reference-actor-scene FILE.peas] [--sandbox | --main-menu] [--assets DIR]\n"
           "       [--vegetation-examples] Add a fixed sample patch near the current player once.\n"
           "WASD move, mouse look, E gather, LMB dig, RMB build, X recover,\n"
           "1-9 quickslots, B build mode, V shape, T rotate, G brush, Y craft, I inventory,\n"
           "C character, Q missions, Tab cursor, F5 camera, F9 save, F10 load, Esc pause.\n",name);
}
/* Reference inspection uses a free camera and never loads or saves a world. */
static int reference_preview(const char *path,int smoke_frames,const char *screenshot) {
    if(!FileExists(path)){fprintf(stderr,"Reference OBJ missing: %s\n",path);return 1;}
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280,800,"Planet Explorers | recovered asset preview");
    if(!IsWindowReady())return 1;
    SetTargetFPS(60);
    PeReferenceModel reference={0};char error[180];
    if(!pe_reference_load(&reference,path,error,sizeof error)) {
        fprintf(stderr,"Reference load failed: %s\n",error);CloseWindow();return 1;
    }
    Camera3D camera=pe_reference_camera(&reference);
    bool room=reference.bounds.max.x-reference.bounds.min.x>60;
    Camera3D interior={{32,5.6f,12},{7,3,12},{0,1,0},60,CAMERA_PERSPECTIVE};
    if(room)camera=interior;
    int frame=0;bool failed=false;
    while(!WindowShouldClose()) {
        if(!smoke_frames) {
            UpdateCamera(&camera,CAMERA_FREE);
            if(IsKeyPressed(KEY_HOME))camera=pe_reference_camera(&reference);
            if(room&&IsKeyPressed(KEY_ONE))camera=interior;
        }
        BeginDrawing();ClearBackground((Color){20,27,34,255});
        BeginMode3D(camera);pe_reference_draw(&reference);EndMode3D();
        DrawRectangle(0,0,GetScreenWidth(),68,Fade(BLACK,.8f));
        DrawText("RECOVERED ASSET PREVIEW",20,14,20,ink);
        DrawText("WASD / mouse camera   Q/E vertical   1 deck view   Home overview   Esc exit",20,42,16,muted);
        EndDrawing();frame++;
        if(smoke_frames&&frame>=smoke_frames) {
            if(screenshot) {
                Image capture=LoadImageFromScreen();
                failed=!capture.data||!ExportImage(capture,screenshot);UnloadImage(capture);
                if(failed)fprintf(stderr,"Screenshot export failed: %s\n",screenshot);
            }
            break;
        }
    }
    if(smoke_frames&&frame<smoke_frames){fprintf(stderr,"Reference smoke closed early\n");failed=true;}
    printf("Reference preview: %d frames, %zu triangles\n",frame,reference.triangles);
    pe_reference_unload(&reference);CloseWindow();return failed?1:0;
}
int main(int argc,char **argv) {
    uint32_t seed=666;
    int smoke_frames=0;
    const char *save_path="planet_explorers.pesave",*screenshot=NULL,*terrain_path=NULL,*load_path=NULL,*smoke_panel=NULL,*reference_path=NULL,*training_path=NULL,*actor_path=NULL,*actor_scene_path=NULL;
    int origin[3]={0};bool origin_given=false,save_given=false;
    bool direct=false,main_menu=false,vegetation_examples=false;
    const char *assets=PE_SOURCE_ROOT "/rebuild/build/reference-assets";
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--help")) {usage(argv[0]);return 0;}
        if(!strcmp(argv[i],"--sandbox")){direct=true;continue;}
        if(!strcmp(argv[i],"--main-menu")){main_menu=true;continue;}
        if(!strcmp(argv[i],"--vegetation-examples")){vegetation_examples=true;continue;}
        if(i+1>=argc) {usage(argv[0]);return 2;}
        const char *option=argv[i++],*value=argv[i];
        if(!strcmp(option,"--seed") || !strcmp(option,"--smoke")) {
            errno=0;char *end=NULL;unsigned long v=strtoul(value,&end,10);
            if(errno||!*value||*end||value[0]=='-'||v>UINT32_MAX) {fprintf(stderr,"Invalid number: %s\n",value);return 2;}
            if(!strcmp(option,"--seed")) seed=(uint32_t)v;
            else {if(v<1||v>100000){fprintf(stderr,"Smoke frames must be 1..100000\n");return 2;}smoke_frames=(int)v;}
        } else if(!strcmp(option,"--save")) {save_path=value;save_given=true;}
        else if(!strcmp(option,"--screenshot")) screenshot=value;
        else if(!strcmp(option,"--terrain"))terrain_path=value;
        else if(!strcmp(option,"--reference-room"))reference_path=value;
        else if(!strcmp(option,"--training-room"))training_path=value;
        else if(!strcmp(option,"--reference-actor"))actor_path=value;
        else if(!strcmp(option,"--reference-actor-scene"))actor_scene_path=value;
        else if(!strcmp(option,"--assets"))assets=value;
        else if(!strcmp(option,"--smoke-panel")) {
            if(strcmp(value,"inventory")&&strcmp(value,"crafting")&&strcmp(value,"character")&&strcmp(value,"missions")&&strcmp(value,"building")){fprintf(stderr,"Smoke panel must be inventory, crafting, character, missions or building\n");return 2;}
            smoke_panel=value;
        }
        else if(!strcmp(option,"--load"))load_path=value;
        else if(!strcmp(option,"--origin")) {
            if(!parse_origin(value,origin)){fprintf(stderr,"Origin must be three nonnegative integers: X,Y,Z\n");return 2;}
            origin_given=true;
        }
        else {usage(argv[0]);return 2;}
    }
    if(smoke_panel&&!smoke_frames){fprintf(stderr,"--smoke-panel requires --smoke FRAMES\n");return 2;}
    if(load_path&&!save_given)save_path=load_path;
    if(screenshot && !smoke_frames) {fprintf(stderr,"--screenshot requires --smoke FRAMES\n");return 2;}
    if(origin_given&&!terrain_path){fprintf(stderr,"--origin requires --terrain FILE\n");return 2;}
    if(vegetation_examples&&(main_menu||training_path||reference_path||actor_path||actor_scene_path)){
        fprintf(stderr,"--vegetation-examples requires the sandbox route\n");return 2;
    }
    if((main_menu&&direct)||((main_menu||direct)&&(training_path||reference_path||actor_path||actor_scene_path))||
       (main_menu&&(terrain_path||load_path||smoke_panel))){fprintf(stderr,"Conflicting application routes\n");return 2;}
    if(actor_scene_path) {
        if(actor_path||reference_path||training_path||terrain_path||load_path||save_given||smoke_panel) {
            fprintf(stderr,"--reference-actor-scene cannot combine with other previews, training, terrain, load, save, or panels\n");return 2;
        }
        if(smoke_frames>36000){fprintf(stderr,"Actor scene smoke frames must be 1..36000\n");return 2;}
        return pe_actor_scene_preview(actor_scene_path,smoke_frames,screenshot);
    }
    if(actor_path) {
        if(reference_path||training_path||terrain_path||load_path||save_given||smoke_panel) {
            fprintf(stderr,"--reference-actor cannot combine with other previews, training, terrain, load, save, or panels\n");return 2;
        }
        return pe_actor_preview(actor_path,smoke_frames,screenshot);
    }
    if(training_path) {
        if(reference_path||terrain_path||load_path||save_given||smoke_panel) {
            fprintf(stderr,"--training-room cannot combine with reference preview, terrain, load, save, or panels\n");return 2;
        }
        int result=pe_training_view(training_path,smoke_frames,screenshot);
        if(result!=2)return result;
        assets=training_path;main_menu=true;
    }
    if(reference_path) {
        if(terrain_path||load_path||save_given||smoke_panel) {
            fprintf(stderr,"--reference-room cannot combine with terrain, load, save, or panels\n");return 2;
        }
        return reference_preview(reference_path,smoke_frames,screenshot);
    }
    PeGame *game=NULL;int result=0;char error[256]={0};
    if(!pe_settlement_configure(assets,error,sizeof error)){fprintf(stderr,"%s\n",error);return 1;}
    bool open_menu=main_menu||(!direct&&!vegetation_examples&&!smoke_frames&&!load_path&&!terrain_path);
    if(!open_menu) {
        game=calloc(1,sizeof *game);
        if(!game||!pe_game_init_mode(game,seed,PE_GAME_ADVENTURE)){destroy_game(game);return 1;}
        if(load_path&&!pe_game_load(game,load_path,error,sizeof error)) {
            fprintf(stderr,"Load failed: %s\n",error);destroy_game(game);return 1;
        }
        if(terrain_path&&!import_world(game,terrain_path,origin_given?origin:NULL,error,sizeof error)) {
            fprintf(stderr,"Import failed: %s\n",error);destroy_game(game);return 1;
        }
        if(!load_path&&!terrain_path&&!pe_settlement_start(game,error,sizeof error)){
            fprintf(stderr,"Settlement failed: %s\n",error);destroy_game(game);return 1;
        }
        if((vegetation_examples||(!load_path&&!terrain_path))&&!pe_vegetation_install_fixture(game,game->player)){
            fprintf(stderr,"Vegetation examples need an Adventure world with enough supported space near the player.\n");
            destroy_game(game);return 1;
        }
        result=pe_sandbox_view(game,assets,save_path,smoke_frames,smoke_panel,screenshot);
        if(result!=2){destroy_game(game);return result;}
    }
    char notice[256]={0};
    for(;;) {
        PeAppChoice choice=pe_app_menu(assets,game!=NULL||FileExists(save_path),game!=NULL,
                                        notice,main_menu?smoke_frames:0,main_menu?screenshot:NULL);
        notice[0]=0;
        if(choice==PE_APP_QUIT||choice==PE_APP_MENU_ERROR){result=choice==PE_APP_MENU_ERROR?1:0;break;}
        if(choice==PE_APP_TUTORIAL) {
            result=pe_training_view(assets,0,NULL);
            if(result==0)break;
            if(result==1)snprintf(notice,sizeof notice,"Tutorial could not start; see terminal for details.");
            continue;
        }
        if(choice==PE_APP_NEW_SANDBOX||!game) {
            PeGame *next=calloc(1,sizeof *next);
            if(!next||!pe_game_init_mode(next,seed,PE_GAME_ADVENTURE)) {
                destroy_game(next);snprintf(notice,sizeof notice,"Could not create the sandbox.");continue;
            }
            if(choice==PE_APP_CONTINUE_SANDBOX&&!pe_game_load(next,save_path,error,sizeof error)) {
                destroy_game(next);snprintf(notice,sizeof notice,"Load failed: %.220s",error);continue;
            }
            if(choice==PE_APP_NEW_SANDBOX&&!pe_settlement_start(next,error,sizeof error)){
                destroy_game(next);snprintf(notice,sizeof notice,"Settlement failed: %.210s",error);continue;
            }
            if(choice==PE_APP_NEW_SANDBOX&&!pe_vegetation_install_fixture(next,next->player)){
                destroy_game(next);snprintf(notice,sizeof notice,"Could not place the vegetation examples safely.");continue;
            }
            destroy_game(game);game=next;
        }
        result=pe_sandbox_view(game,assets,save_path,0,NULL,NULL);
        if(result==0)break;
        if(result==1)snprintf(notice,sizeof notice,"Sandbox could not continue; see terminal for details.");
    }
    destroy_game(game);return result;
}
