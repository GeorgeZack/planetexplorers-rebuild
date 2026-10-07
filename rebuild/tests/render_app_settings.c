/* Actual menu rendering with scripted keys, only on an owned private display.
 * This is a staged UI check, not physical gameplay acceptance. */
#define _POSIX_C_SOURCE 200809L
#include <raylib.h>
#include <rlgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pe_preferences.h"
static int frame_index,width,height,failed;
static const char *output;
static bool scripted_key(int key){
    if(frame_index==0||frame_index==1)return key==KEY_DOWN;
    if(frame_index==2||frame_index==3)return key==KEY_ENTER;
    if(frame_index==5||frame_index==6)return key==KEY_ESCAPE;
    return false;
}
static void scripted_window(int w,int h,const char *title){(void)w;(void)h;InitWindow(width,height,title);}
static void scripted_minimum(int w,int h){(void)w;(void)h;}
static void scripted_end(void){
    if(frame_index==2||frame_index==4){
        char path[4096];snprintf(path,sizeof path,"%s/settings-%dx%d-%s.png",output,width,height,frame_index==2?"off":"on");
        rlDrawRenderBatchActive();Image shot=LoadImageFromScreen();
        if(!shot.data||!ExportImage(shot,path))failed=1;
        UnloadImage(shot);
    }
    EndDrawing();frame_index++;
}
#define IsKeyPressed scripted_key
#define InitWindow scripted_window
#define SetWindowMinSize scripted_minimum
#define EndDrawing scripted_end
#include "../src/pe_ui_settings.c"
#include "../src/pe_app_menu.c"
#undef EndDrawing
#undef SetWindowMinSize
#undef InitWindow
#undef IsKeyPressed
int main(int argc,char **argv){
    if(argc!=2||!getenv("PE_UI_PRIVATE_DISPLAY")||!getenv("DISPLAY")||
       strcmp(getenv("PE_UI_PRIVATE_DISPLAY"),getenv("DISPLAY")))return 2;
    output=argv[1];char path[4096],error[256];
    snprintf(path,sizeof path,"%s/render-settings.conf",output);
    if(setenv("PE_SETTINGS_PATH",path,1))return 1;
    const int sizes[][2]={{1280,720},{800,450}};
    for(unsigned i=0;i<2;i++){
        width=sizes[i][0];height=sizes[i][1];frame_index=0;
        PePreferences preferences={0};
        if(!pe_preferences_save(&preferences,path,error,sizeof error))return 1;
        if(pe_app_menu(PE_SOURCE_ROOT "/rebuild/build/reference-assets",false,false,NULL,0,NULL)!=PE_APP_QUIT)return 1;
        if(frame_index!=7||!pe_preferences_load(&preferences,path,error,sizeof error)||!preferences.camera_facing_attacks)return 1;
    }
    printf("Settings actual renderer: %s (normal/small, OFF/ON, persisted)\n",failed?"FAIL":"PASS");
    return failed;
}
