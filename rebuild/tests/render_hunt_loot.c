/* Staged real marker rendering; no game simulation, saves or desktop input. */
#include "../src/pe_sandbox_view.c"
#include <assert.h>
int main(int argc,char **argv){
    const char *display=getenv("DISPLAY"),*owned=getenv("PE_UI_PRIVATE_DISPLAY");
    if(argc!=2||!display||!owned||strcmp(display,owned)||getenv("WAYLAND_DISPLAY")||
       display[0]!=':'||atoi(display+1)<100||atoi(display+1)>999)return 2;
    PeGame *g=calloc(1,sizeof *g);assert(g);
    InitWindow(1280,720,"Hunt loot staged renderer");
    char error[256];PeUiAssets *assets=pe_ui_assets_load(PE_SOURCE_ROOT "/rebuild/build/reference-assets/ui",error,sizeof error);
    if(!assets){fprintf(stderr,"%s\n",error);return 1;}
    PeSandboxUI ui={.assets=assets};
    g->player=(PeVec3){0,0,6};g->hunt_loot.count=7;
    for(unsigned i=0;i<7;i++){
        g->hunt_loot.drops[i]=(PeHuntLootDrop){i+1,pe_hunt_loot_items[i].item_id,i?2:30,
            i<4?(PeVec3){-1,0,0}:(PeVec3){1,0,0},0};
        assert(pe_ui_sprite_exists(assets,pe_item_def(g->hunt_loot.drops[i].item_id)->icon));
    }
    Camera3D camera={.position={0,3,9},.target={0,0,0},.up={0,1,0},.fovy=50,.projection=CAMERA_PERSPECTIVE};
    const int sizes[][2]={{1280,720},{640,480}};
    for(unsigned i=0;i<2;i++){
        SetWindowSize(sizes[i][0],sizes[i][1]);
        for(int frame=0;frame<4;frame++){
            assert(pe_training_ui_layout(GetScreenWidth(),GetScreenHeight(),&ui.layout));
            BeginDrawing();ClearBackground((Color){26,44,53,255});
            source_begin_3d(camera);DrawGrid(20,1);source_end_3d();
            sandbox_loot_markers(g,NULL,camera,&ui);
            DrawText("Staged source item icons: two saved drop clusters",16,16,18,RAYWHITE);
            EndDrawing();
        }
        Image shot=LoadImageFromScreen();assert(shot.data);char path[4096];
        snprintf(path,sizeof path,"%s/loot-%dx%d.png",argv[1],GetScreenWidth(),GetScreenHeight());
        assert(ExportImage(shot,path));UnloadImage(shot);
    }
    pe_ui_assets_free(assets);CloseWindow();free(g);puts("Staged hunt loot renderer: PASS");return 0;
}
