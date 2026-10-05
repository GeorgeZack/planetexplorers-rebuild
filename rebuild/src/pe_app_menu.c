#include "pe_app_menu.h"
#include "pe_ui_assets.h"
#include <raylib.h>
#include <rlgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static Rectangle scaled(float x,float y,float w,float h,float scale,float ox,float oy) {
    return (Rectangle){ox+x*scale,oy+y*scale,w*scale,h*scale};
}
static void centered(Font font,const char *text,Rectangle r,float size,Color color) {
    Vector2 m=MeasureTextEx(font,text,size,0);
    DrawTextEx(font,text,(Vector2){r.x+(r.width-m.x)/2,r.y+(r.height-m.y)/2},size,0,color);
}
static bool menu_button(PeUiAssets *ui,const char *text,Rectangle r,float scale,bool enabled,bool selected) {
    bool over=CheckCollisionPointRec(GetMousePosition(),r);
    if(selected||over) {
        DrawRectangleGradientH((int)r.x,(int)r.y,(int)r.width,(int)r.height,
                               (Color){37,76,112,30},(Color){76,145,211,105});
        DrawRectangle((int)(r.x+r.width-2*scale),(int)r.y,(int)(2*scale),(int)r.height,(Color){117,221,255,220});
    }
    centered(pe_ui_font(ui,false),text,r,20*scale,enabled?(Color){172,217,242,255}:(Color){81,107,123,255});
    return enabled&&over&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
PeAppChoice pe_app_menu(const char *dir,bool can_continue,bool active,const char *notice,int frames,const char *shot) {
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280,720,"Planet Explorers | Main Menu");
    if(!IsWindowReady())return PE_APP_MENU_ERROR;
    SetWindowMinSize(800,450);SetTargetFPS(60);SetExitKey(KEY_NULL);EnableCursor();
    char path[4096],error[256]={0};PeUiAssets *ui=NULL;
    int n=snprintf(path,sizeof path,"%s/ui",dir);
    if(n>0&&n<(int)sizeof path)ui=pe_ui_assets_load(path,error,sizeof error);
    if(!ui)fprintf(stderr,"Main menu UI fallback: %s\n",error);
    Texture2D logo={0};
    n=snprintf(path,sizeof path,"%s/ui/main-menu-logo.png",dir);
    if(n>0&&n<(int)sizeof path&&FileExists(path))logo=LoadTexture(path);
    else if(FileExists(PE_SOURCE_ROOT "/Assets/Resources/Texture2d/GameCredits/Logo.png"))
        logo=LoadTexture(PE_SOURCE_ROOT "/Assets/Resources/Texture2d/GameCredits/Logo.png");
    if(logo.id)SetTextureFilter(logo,TEXTURE_FILTER_BILINEAR);
    n=snprintf(path,sizeof path,"%s/room-collision.obj",dir);
    bool tutorial=n>0&&n<(int)sizeof path&&FileExists(path);
    const char *names[]={"Continue Sandbox","New Sandbox","Tutorial","Quit"};
    const PeAppChoice choices[]={PE_APP_CONTINUE_SANDBOX,PE_APP_NEW_SANDBOX,PE_APP_TUTORIAL,PE_APP_QUIT};
    bool enabled[]={can_continue,true,tutorial,true};
    int selected=can_continue?0:1,frame=0;bool confirm=false,done=false;PeAppChoice result=PE_APP_QUIT;
    while(!WindowShouldClose()&&!done) {
        if(IsWindowFocused()&&ui)HideCursor();else ShowCursor();
        float scale=fminf(GetScreenWidth()/1280.f,GetScreenHeight()/720.f);
        float ox=(GetScreenWidth()-1280*scale)/2,oy=(GetScreenHeight()-720*scale)/2;
        if(!frames&&IsKeyPressed(KEY_ESCAPE)){if(confirm)confirm=false;else done=true;}
        if(!frames&&!confirm&&(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_UP))) {
            int step=IsKeyPressed(KEY_DOWN)?1:-1;
            do {selected=(selected+step+4)%4;}while(!enabled[selected]);
        }
        BeginDrawing();ClearBackground((Color){2,8,13,255});
        DrawRectangleGradientV(0,0,GetScreenWidth(),GetScreenHeight(),(Color){0,5,9,255},(Color){5,35,51,255});
        /* Deterministic stars and horizon; original logo/fonts are recovered,
           the original planet shader and animated menu scene are not ported. */
        for(unsigned i=0;i<160;i++) {
            unsigned a=i*1103515245u+12345u,b=a*1664525u+1013904223u;
            float x=(a%1280)*scale+ox,y=(b%510)*scale+oy;
            float pulse=.45f+.2f*sinf((float)GetTime()*.25f+i);
            DrawCircleV((Vector2){x,y},i%9?scale*.6f:scale,Fade((Color){165,206,230,255},pulse));
        }
        DrawCircleGradient((Vector2){ox+360*scale,oy+1180*scale},650*scale,(Color){8,76,116,255},(Color){8,36,58,0});
        DrawEllipse((int)(ox+415*scale),(int)(oy+854*scale),1010*scale,230*scale,(Color){12,63,94,255});
        DrawEllipseLines((int)(ox+415*scale),(int)(oy+854*scale),1010*scale,230*scale,(Color){94,189,236,145});
        if(logo.id)DrawTexturePro(logo,(Rectangle){0,0,(float)logo.width,(float)logo.height},scaled(143,239,728,92,scale,ox,oy),(Vector2){0},0,WHITE);
        else centered(pe_ui_font_heading(ui),"PLANET EXPLORERS",scaled(100,230,780,120,scale,ox,oy),46*scale,WHITE);
        DrawRectangle((int)(ox+1008*scale),(int)(oy+127*scale),(int)(2*scale),(int)(326*scale),(Color){88,154,195,100});
        DrawRectangle((int)(ox+1192*scale),(int)(oy+127*scale),(int)(2*scale),(int)(326*scale),(Color){119,116,218,150});
        for(int i=0;i<4;i++) {
            Rectangle r=scaled(1016,146+63*i,169,46,scale,ox,oy);
            bool hit=menu_button(ui,names[i],r,scale,enabled[i],selected==i);
            if(!frames&&!confirm&&(hit||(selected==i&&IsKeyPressed(KEY_ENTER)))) {
                selected=i;
                if(i==1&&active)confirm=true;
                else {result=choices[i];done=true;}
            }
        }
        if(notice&&*notice)centered(pe_ui_font(ui,false),notice,scaled(130,510,1020,50,scale,ox,oy),18*scale,(Color){238,201,118,255});
        if(!tutorial)centered(pe_ui_font_small(ui,false),"Tutorial content is unavailable",scaled(940,425,320,30,scale,ox,oy),14*scale,(Color){165,184,198,255});
        if(confirm) {
            DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,.6f));
            Rectangle r=scaled(350,255,580,210,scale,ox,oy);
            if(!pe_ui_sprite(ui,"midbg",r,WHITE,scale))DrawRectangleRec(r,(Color){24,45,79,255});
            centered(pe_ui_font(ui,false),"Start a new sandbox?",scaled(370,275,540,38,scale,ox,oy),22*scale,WHITE);
            centered(pe_ui_font(ui,false),"Unsaved progress in the current world will be lost.",scaled(370,318,540,40,scale,ox,oy),16*scale,(Color){190,215,235,255});
            bool yes=menu_button(ui,"New World",scaled(400,384,210,45,scale,ox,oy),scale,true,false);
            bool no=menu_button(ui,"Cancel",scaled(670,384,210,45,scale,ox,oy),scale,true,false);
            if(!frames&&yes){result=PE_APP_NEW_SANDBOX;done=true;}
            if(!frames&&no)confirm=false;
        }
        if(ui)pe_ui_sprite(ui,"mouse",(Rectangle){GetMouseX(),GetMouseY(),40*scale,40*scale},WHITE,scale);
        if(frames&&frame+1>=frames) {
            if(shot){rlDrawRenderBatchActive();Image capture=LoadImageFromScreen();bool ok=capture.data&&ExportImage(capture,shot);UnloadImage(capture);if(!ok)result=PE_APP_MENU_ERROR;}
            done=true;
        }
        EndDrawing();frame++;
    }
    if(frames&&frame<frames)result=PE_APP_MENU_ERROR;
    if(logo.id)UnloadTexture(logo);
    pe_ui_assets_free(ui);ShowCursor();CloseWindow();
    printf("Main menu: %d frames, choice %d\n",frame,result);
    return result;
}
