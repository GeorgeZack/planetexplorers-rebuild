/* Paired actual UI renderers, staged presentation only. No gameplay host,
 * controller, input routing, saves or source-game session is instantiated.
 * Run only through the explicitly owned high-display supervisor. */
#include "pe_test_paths.h"
#include "pe_training_ui.h"
#include "pe_sandbox_ui.h"
#include "pe_ui_hud.h"
#include "pe_ui_settings.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"shared UI render:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void private_display_required(void){
    const char *display=getenv("DISPLAY"),*owned=getenv("PE_UI_PRIVATE_DISPLAY");
    REQUIRE(display&&owned&&!strcmp(display,owned)&&!getenv("WAYLAND_DISPLAY"));
    REQUIRE(display[0]==':'&&display[1]>='0'&&display[1]<='9');
    errno=0;char *end=NULL;long number=strtol(display+1,&end,10);
    REQUIRE(!errno&&end&&!*end&&number>=100&&number<=999);
}
static Image export_screen(const char *directory,const char *name,int width,int height){
    char path[2048];int n=snprintf(path,sizeof path,"%s/%s-%dx%d.png",directory,name,width,height);
    REQUIRE(n>0&&(size_t)n<sizeof path);
    Image image=LoadImageFromScreen();REQUIRE(image.data&&image.width==width&&image.height==height);
    REQUIRE(ExportImage(image,path));return image;
}
static const PeUiSettings settings_fixture={.open=true,.ready=true};
static Image training_capture(PeTrainingUi *ui,const PeTraining *training,const char *directory,const char *name,int width,int height){
    for(int frame=0;frame<3;frame++){
        BeginDrawing();ClearBackground((Color){28,45,54,255});
        pe_training_ui_draw(ui,training,(PeTrainingUiState){.paused=strstr(name,"pause")||strstr(name,"settings")});
        if(strstr(name,"settings"))pe_ui_settings_draw(&settings_fixture,ui->assets);
        EndDrawing();
    }
    return export_screen(directory,name,width,height);
}
static Image sandbox_capture(PeSandboxUI *ui,const PeGame *game,const char *directory,const char *name,int width,int height){
    for(int frame=0;frame<3;frame++){
        Vector2 mouse=GetMousePosition();ui->mouse_x=mouse.x;ui->mouse_y=mouse.y;
        BeginDrawing();ClearBackground((Color){28,45,54,255});
        ui->paused=strstr(name,"pause")||strstr(name,"settings");pe_sandbox_ui_draw(ui,game,false);ui->paused=false;
        if(strstr(name,"settings"))pe_ui_settings_draw(&settings_fixture,ui->assets);
        EndDrawing();
    }
    return export_screen(directory,name,width,height);
}
static void compare_region(Image a,Image b,PeTrainingUIRect r,const char *label){
    REQUIRE(a.width==b.width&&a.height==b.height);
    int x0=(int)fmaxf(0,floorf(r.x)),y0=(int)fmaxf(0,floorf(r.y));
    int x1=(int)fminf(a.width,ceilf(r.x+r.w)),y1=(int)fminf(a.height,ceilf(r.y+r.h));
    REQUIRE(x1>x0&&y1>y0);
    Color *left=LoadImageColors(a),*right=LoadImageColors(b);REQUIRE(left&&right);
    size_t different=0,checked=0;
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){
        size_t k=(size_t)y*(size_t)a.width+(size_t)x;checked++;
        if(memcmp(&left[k],&right[k],sizeof(Color))){
            if(!different)fprintf(stderr,"first %s mismatch (%d,%d): tutorial=%u,%u,%u,%u sandbox=%u,%u,%u,%u\n",label,x,y,left[k].r,left[k].g,left[k].b,left[k].a,right[k].r,right[k].g,right[k].b,right[k].a);
            different++;
        }
    }
    UnloadImageColors(left);UnloadImageColors(right);
    printf("%s_PIXELS size=%dx%d checked=%zu different=%zu\n",label,a.width,a.height,checked,different);fflush(stdout);
    REQUIRE(different==0);
}
/* The same comparison outside one excluded rectangle; returns that rectangle's
 * fill columns (the hunger sprite's teal) counted from its right end. */
static void compare_hud_except(Image a,Image b,PeTrainingUIRect r,PeTrainingUIRect skip,const char *label){
    REQUIRE(a.width==b.width&&a.height==b.height);
    int x0=(int)fmaxf(0,floorf(r.x)),y0=(int)fmaxf(0,floorf(r.y)),x1=(int)fminf(a.width,ceilf(r.x+r.w)),y1=(int)fminf(a.height,ceilf(r.y+r.h));
    int sx0=(int)floorf(skip.x),sy0=(int)floorf(skip.y),sx1=(int)ceilf(skip.x+skip.w),sy1=(int)ceilf(skip.y+skip.h);
    Color *left=LoadImageColors(a),*right=LoadImageColors(b);REQUIRE(left&&right);
    size_t different=0,checked=0;
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){
        if(x>=sx0&&x<sx1&&y>=sy0&&y<sy1)continue;
        size_t k=(size_t)y*(size_t)a.width+(size_t)x;checked++;
        if(memcmp(&left[k],&right[k],sizeof(Color)))different++;
    }
    UnloadImageColors(left);UnloadImageColors(right);
    printf("%s_PIXELS size=%dx%d checked=%zu different=%zu\n",label,a.width,a.height,checked,different);fflush(stdout);
    REQUIRE(different==0);
}
static void hunger_fill(Image b,PeTrainingUIRect r,float fraction,const char *label){
    Color *px=LoadImageColors(b);REQUIRE(px);
    int x0=(int)floorf(r.x),x1=(int)ceilf(r.x+r.w),y0=(int)floorf(r.y),y1=(int)ceilf(r.y+r.h);
    int first=-1,last=-1;
    for(int x=x0;x<x1;x++)for(int y=y0;y<y1;y++){
        Color c=px[(size_t)y*(size_t)b.width+(size_t)x];
        if(c.g>120&&c.b>120&&c.r<120&&abs((int)c.g-(int)c.b)<40){if(first<0||x<first)first=x;if(x>last)last=x;}
    }
    UnloadImageColors(px);
    float filled=first<0?0:(float)(last-first+1);
    printf("%s_FILL size=%dx%d track=%.1f..%.1f teal_columns=%d..%d expected_width=%.1f\n",label,b.width,b.height,r.x,r.x+r.w,first,last,r.w*fraction);fflush(stdout);
    REQUIRE(first>=0&&last>=x1-2&&filled<=r.w*fraction+2&&filled>=r.w*fraction-3);
}
static int bag(const PeGame *g,int item){
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==item&&g->inventory.slots[i].count>0)return i;
    return -1;
}
static void tracker_capture(PeTrainingUi *ui,const char *directory,int width,int height){
    PeUiCanvas canvas={ui->assets,ui->effects,&ui->layout,0,-100,-100};
    const PeUiTrackerObjective goals[]={
        {"The first objective begins below the complete wrapped heading.",false},
        {"Completed objectives keep the same spacing and use the completed tint.",true}
    };
    const PeUiTrackerView tracker={"Long camera heading: third-person orbit and free pointer movement",true,goals,2};
    for(int frame=0;frame<3;frame++){
        BeginDrawing();ClearBackground((Color){28,45,54,255});
        pe_ui_minimap_chrome_draw(&canvas,&(PeUiMinimapView){0});
        float used=pe_ui_tracker_draw(&canvas,&tracker);
        REQUIRE(used>0&&used<=ui->layout.mission_tracker.h+7*ui->layout.scale+.01f);
        REQUIRE(ui->layout.mission_tracker.y-7*ui->layout.scale>=ui->layout.minimap.y+ui->layout.minimap.h);
        EndDrawing();
    }
    Image result=export_screen(directory,"shared-long-tracker",width,height);UnloadImage(result);
}
int main(int argc,char **argv){
    private_display_required();REQUIRE(argc==2&&DirectoryExists(argv[1]));
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(1280,720,"Private paired shared UI render fixture");REQUIRE(IsWindowReady());
    SetMousePosition(-100,-100);HideCursor();
    /* These are explicit draw fixtures, not accepted mission/save state. */
    PeTraining *training=calloc(1,sizeof *training);PeGame *game=calloc(1,sizeof *game);
    REQUIRE(training&&game&&pe_game_init_mode(game,911,PE_GAME_LEGACY));
    *training=(PeTraining){.hp=200,.max_hp=500,.stamina=67,.equipped_shovel=-1,
        .knife_owned=true,.knife_equipped=true,.quickslot_item_id=916,.juice_count=5,.juice_cooldown=3.2f};
    game->hp=200;game->max_hp=500;game->stamina=67;game->stamina_max=100;game->juice_cooldown=3.2;
    game->inventory=(PeInventory){.next_instance_id=1};game->equipped_instance_id=0;game->shield_instance_id=0;
    memset(game->quickslots,0,sizeof game->quickslots);
    REQUIRE(pe_inventory_add(&game->inventory,916,5)&&pe_inventory_add(&game->inventory,33,1));
    int juice=bag(game,916),knife=bag(game,33);REQUIRE(juice>=0&&knife>=0);
    game->quickslots[0]=(PeQuickslot){.item_id=916,.instance_id=game->inventory.slots[juice].instance_id};
    game->equipped_instance_id=game->inventory.slots[knife].instance_id;
    REQUIRE(pe_game_equipped_tool(game)&&pe_game_equipped_tool(game)->item_id==33);
    PeTrainingUi tutorial={0};PeSandboxUI sandbox={0};char error[256];
    REQUIRE(pe_training_ui_load(&tutorial,pe_test_path("rebuild/build/reference-assets"),error,sizeof error));
    REQUIRE(pe_sandbox_ui_load(&sandbox,pe_test_path("rebuild/build/reference-assets"),error,sizeof error));
    REQUIRE(tutorial.assets&&sandbox.assets);tutorial.seconds=sandbox.seconds=0;
    /* Twenty retained, resolved draw-fixture records, independently owned by
       the tutorial UI and sandbox game; no active dialogue/capture hook. */
    for(unsigned i=0;i<PE_CONVERSATION_HISTORY_CAPACITY;i++){
        char speaker[64],content[1024];
        snprintf(speaker,sizeof speaker,"Staged speaker %u",i+1);
        snprintf(content,sizeof content,"Record %u: This long conversation demonstrates the original cyan speaker and white body with wrapped lines. \nScrolling keeps earlier remarks available and reopening returns to the newest retained entry. These are staged presentation records, not observed gameplay.",i+1);
        REQUIRE(pe_conversation_history_add(&tutorial.conversation_history,speaker,content,NULL));
        REQUIRE(pe_conversation_history_add(&game->conversation_history,speaker,content,NULL));
    }
    REQUIRE(tutorial.conversation_history.count==20&&game->conversation_history.count==20);
    const int sizes[][2]={{1280,720},{640,480},{320,240}};
    for(size_t k=0;k<sizeof sizes/sizeof *sizes;k++){
        int w=sizes[k][0],h=sizes[k][1];SetWindowSize(w,h);SetMousePosition(-100,-100);
        pe_training_ui_resize(&tutorial);pe_sandbox_ui_resize(&sandbox,(float)w,(float)h);
        pe_sandbox_ui_prepare(&sandbox,game);
        Image a=training_capture(&tutorial,training,argv[1],"tutorial-hud",w,h);
        Image b=sandbox_capture(&sandbox,game,argv[1],"sandbox-hud",w,h);
        compare_region(a,b,pe_training_ui_map(&tutorial.layout,(PeTrainingUIRect){217,610,847,111},PE_UI_BOTTOM_CENTER),"HUD");UnloadImage(b);
        /* Adventure only: the same staged HUD with hunger 150 of 500. Everything else
         * matches the tutorial; the track fills its right 30% (5_Hunger is rotated). */
        game->mode=PE_GAME_ADVENTURE;game->hunger.hunger=game->hunger.raw=150;
        b=sandbox_capture(&sandbox,game,argv[1],"sandbox-hud-hunger",w,h);
        game->mode=PE_GAME_LEGACY;
        compare_hud_except(a,b,pe_training_ui_map(&tutorial.layout,(PeTrainingUIRect){217,610,847,111},PE_UI_BOTTOM_CENTER),sandbox.layout.hunger,"HUD_OUTSIDE_HUNGER");
        hunger_fill(b,sandbox.layout.hunger,.3f,"HUNGER");UnloadImage(a);UnloadImage(b);
        training->inventory_open=sandbox.inventory_open=true;
        a=training_capture(&tutorial,training,argv[1],"tutorial-inventory",w,h);
        b=sandbox_capture(&sandbox,game,argv[1],"sandbox-inventory",w,h);
        UnloadImage(a);UnloadImage(b);training->inventory_open=sandbox.inventory_open=false;
        tracker_capture(&tutorial,argv[1],w,h);
        tutorial.history.open=sandbox.history.open=true;
        tutorial.history.scroll=sandbox.history.scroll=-1;
        a=training_capture(&tutorial,training,argv[1],"tutorial-history-bottom",w,h);
        b=sandbox_capture(&sandbox,game,argv[1],"sandbox-history-bottom",w,h);
        REQUIRE(tutorial.history.content_height>tutorial.history.height&&tutorial.history.scroll>0);
        REQUIRE(fabsf(tutorial.history.scroll-sandbox.history.scroll)<.01f);
        compare_region(a,b,tutorial.layout.history,"HISTORY_BOTTOM");UnloadImage(a);UnloadImage(b);
        tutorial.history.scroll=sandbox.history.scroll=0;
        a=training_capture(&tutorial,training,argv[1],"tutorial-history-top",w,h);
        b=sandbox_capture(&sandbox,game,argv[1],"sandbox-history-top",w,h);
        REQUIRE(tutorial.history.scroll==0&&sandbox.history.scroll==0);
        compare_region(a,b,tutorial.layout.history,"HISTORY_TOP");UnloadImage(a);UnloadImage(b);
        tutorial.history.open=sandbox.history.open=false;
        a=training_capture(&tutorial,training,argv[1],"tutorial-pause",w,h);
        b=sandbox_capture(&sandbox,game,argv[1],"sandbox-pause",w,h);UnloadImage(a);UnloadImage(b);
        a=training_capture(&tutorial,training,argv[1],"tutorial-settings",w,h);
        b=sandbox_capture(&sandbox,game,argv[1],"sandbox-settings",w,h);UnloadImage(a);UnloadImage(b);
    }
    pe_training_ui_unload(&tutorial);pe_sandbox_ui_unload(&sandbox);
    pe_game_destroy(game);free(game);free(training);CloseWindow();
    puts("paired staged shared UI captures complete; actual-host HUD and history pixels match");return 0;
}
