/* Script the real menu against input/draw mocks and real settings persistence.
 * No graphics context is created. Geometry checks use a fixed-width test font;
 * they do not replace inspection of recovered-font screenshots. */
#define _POSIX_C_SOURCE 200809L
#include "pe_app_menu.h"
#include "pe_preferences.h"
#include "pe_ui_assets.h"
#include "pe_ui_settings.h"
#include <raylib.h>
#include <rlgl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"app-menu-settings:%d %s\n",__LINE__,#c);failures++;}}while(0)
typedef struct {int key,key2;bool click;float x,y;void (*prepare)(void);} Input;
typedef struct {bool settings,on,off,error,debug_on,debug_off,debug_warning,notice_end;unsigned menu_labels,debug_actions,notice_lines;} Drawn;
static Input inputs[32];static Drawn drawn[32];
static int frame,frame_count,width=1280,height=720,closed;
static bool tutorial_available=true,prepared,focused=true;
static bool scissored;
static Rectangle scissor;
static char settings_path[512];

static float scale(void){return fminf(width/1280.f,height/720.f);}
static Vector2 screen_point(float x,float y){float s=scale();return (Vector2){(width-1280*s)/2+x*s,(height-720*s)/2+y*s};}
static void reset(int count,int w,int h){memset(inputs,0,sizeof inputs);memset(drawn,0,sizeof drawn);frame=0;frame_count=count;width=w;height=h;closed=0;prepared=false;tutorial_available=true;focused=true;}
static void click_at(int f,float x,float y){inputs[f].click=true;inputs[f].x=x;inputs[f].y=y;}
static bool saved_value(void){PePreferences p={0};char error[128];CHECK(pe_preferences_load(&p,settings_path,error,sizeof error));return p.camera_facing_attacks;}
static void remove_blocking_directory(void){CHECK(rmdir(settings_path)==0);}

void SetTraceLogLevel(int v){(void)v;}
void SetConfigFlags(unsigned int v){(void)v;}
void InitWindow(int w,int h,const char *title){(void)w;(void)h;(void)title;}
bool IsWindowReady(void){return true;}
void SetWindowMinSize(int w,int h){CHECK(w<=width&&h<=height);}
void SetTargetFPS(int v){(void)v;}
void SetExitKey(int v){CHECK(v==KEY_NULL);}
void EnableCursor(void){}
void HideCursor(void){}
void ShowCursor(void){}
void CloseWindow(void){closed++;}
bool IsWindowFocused(void){return focused;}
bool WindowShouldClose(void){if(frame>=frame_count)return true;if(!prepared){prepared=true;if(inputs[frame].prepare)inputs[frame].prepare();}return false;}
int GetScreenWidth(void){return width;}
int GetScreenHeight(void){return height;}
bool IsKeyPressed(int key){return key!=0&&(inputs[frame].key==key||inputs[frame].key2==key);}
Vector2 GetMousePosition(void){return screen_point(inputs[frame].x,inputs[frame].y);}
int GetMouseX(void){return (int)GetMousePosition().x;}
int GetMouseY(void){return (int)GetMousePosition().y;}
bool IsMouseButtonPressed(int b){return b==MOUSE_BUTTON_LEFT&&inputs[frame].click;}
bool CheckCollisionPointRec(Vector2 p,Rectangle r){return p.x>=r.x&&p.x<=r.x+r.width&&p.y>=r.y&&p.y<=r.y+r.height;}
bool FileExists(const char *p){return tutorial_available&&strstr(p,"room-collision.obj")!=NULL;}
Texture2D LoadTexture(const char *p){(void)p;return (Texture2D){0};}
void UnloadTexture(Texture2D t){(void)t;}
void SetTextureFilter(Texture2D t,int f){(void)t;(void)f;}
double GetTime(void){return frame/60.;}
void BeginDrawing(void){}
void EndDrawing(void){CHECK(drawn[frame].menu_labels==5);frame++;prepared=false;}
void ClearBackground(Color c){(void)c;}
void DrawRectangleGradientH(int x,int y,int w,int h,Color a,Color b){(void)x;(void)y;(void)w;(void)h;(void)a;(void)b;}
void DrawRectangleGradientV(int x,int y,int w,int h,Color a,Color b){(void)x;(void)y;(void)w;(void)h;(void)a;(void)b;}
void DrawRectangle(int x,int y,int w,int h,Color c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void DrawRectangleRec(Rectangle r,Color c){(void)c;CHECK(r.x>=0&&r.y>=0&&r.x+r.width<=width+.01f&&r.y+r.height<=height+.01f);}
void BeginScissorMode(int x,int y,int w,int h){CHECK(!scissored);scissored=true;scissor=(Rectangle){x,y,w,h};CHECK(x>=0&&y>=0&&x+w<=width&&y+h<=height);}
void EndScissorMode(void){CHECK(scissored);scissored=false;}
void DrawCircleV(Vector2 v,float r,Color c){(void)v;(void)r;(void)c;}
void DrawCircleGradient(Vector2 v,float r,Color a,Color b){(void)v;(void)r;(void)a;(void)b;}
void DrawEllipse(int x,int y,float w,float h,Color c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void DrawEllipseLines(int x,int y,float w,float h,Color c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void DrawTexturePro(Texture2D t,Rectangle s,Rectangle d,Vector2 o,float r,Color c){(void)t;(void)s;(void)d;(void)o;(void)r;(void)c;}
Color Fade(Color c,float a){c.a=(unsigned char)(255*a);return c;}
Vector2 MeasureTextEx(Font f,const char *t,float size,float spacing){(void)f;(void)spacing;return (Vector2){strlen(t)*size*.5f,size};}
void DrawTextEx(Font f,const char *text,Vector2 p,float size,float spacing,Color color){
    (void)color;Vector2 measured=MeasureTextEx(f,text,size,spacing);
    float right=p.x+measured.x;
    if(scissored){
        CHECK(fabsf(size-16*scale())<.01f); /* Notice stays readable instead of shrinking to one line. */
        CHECK(p.y>=scissor.y&&p.y+measured.y<=scissor.y+scissor.height+.01f);
        right=fminf(right,scissor.x+scissor.width);drawn[frame].notice_lines++;
        if(strstr(text,"preserved."))drawn[frame].notice_end=true;
    }
    CHECK(p.x>=0&&p.y>=0&&right<=width+.01f&&p.y+measured.y<=height+.01f);
    if(!strcmp(text,"Settings")){if(drawn[frame].menu_labels<5)drawn[frame].menu_labels++;else drawn[frame].settings=true;}
    else if(!strcmp(text,"Continue Sandbox")||!strcmp(text,"New Sandbox")||!strcmp(text,"Tutorial")||!strcmp(text,"Quit"))drawn[frame].menu_labels++;
    if(!strcmp(text,"Camera-facing attacks: ON")){drawn[frame].on=true;drawn[frame].settings=true;}
    if(!strcmp(text,"Camera-facing attacks: OFF")){drawn[frame].off=true;drawn[frame].settings=true;}
    if(strstr(text,"Settings could not be read or saved"))drawn[frame].error=true;
    if(!strcmp(text,"Debug tools: ON"))drawn[frame].debug_on=true;
    if(!strcmp(text,"Debug tools: OFF"))drawn[frame].debug_off=true;
    if(!strcmp(text,"Give 5 Herbal Drinks")||!strcmp(text,"Combat supplies + heal")||!strcmp(text,"Skip to hare hunt"))drawn[frame].debug_actions++;
    if(!strcmp(text,"Debug actions change this game. Resume, then F9 to save."))drawn[frame].debug_warning=true;
}
void rlDrawRenderBatchActive(void){}
Image LoadImageFromScreen(void){return (Image){0};}
bool ExportImage(Image i,const char *p){(void)i;(void)p;return false;}
void UnloadImage(Image i){(void)i;}
PeUiAssets *pe_ui_assets_load(const char *d,char *e,size_t c){(void)d;(void)e;(void)c;return (PeUiAssets *)(uintptr_t)1;}
void pe_ui_assets_free(PeUiAssets *p){CHECK(p==(PeUiAssets *)(uintptr_t)1);}
bool pe_ui_sprite(const PeUiAssets *u,const char *n,Rectangle r,Color c,float s){(void)u;(void)n;(void)r;(void)c;(void)s;return false;}
Font pe_ui_font(const PeUiAssets *u,bool b){(void)u;(void)b;return (Font){0};}
Font pe_ui_font_heading(const PeUiAssets *u){return pe_ui_font(u,false);}
Font pe_ui_font_small(const PeUiAssets *u,bool b){return pe_ui_font(u,b);}

int main(void){
    char directory[]="/tmp/pe-app-menu-settings-XXXXXX";
    CHECK(mkdtemp(directory)!=NULL);if(failures)return 1;
    snprintf(settings_path,sizeof settings_path,"%s/settings.conf",directory);
    CHECK(setenv("PE_SETTINGS_PATH",settings_path,1)==0);

    reset(11,1280,720);
    inputs[0].key=inputs[1].key=inputs[2].key=KEY_DOWN;
    inputs[3].key=KEY_ENTER;inputs[5].key=KEY_ENTER;
    inputs[7].key=KEY_ESCAPE;inputs[7].key2=KEY_ENTER;
    inputs[9].key=KEY_UP;inputs[10].key=KEY_ENTER;
    CHECK(pe_app_menu("unused",true,false,NULL,0,NULL)==PE_APP_TUTORIAL);
    CHECK(frame==11&&closed==1);
    CHECK(drawn[3].off&&drawn[4].off&&drawn[5].on&&drawn[6].on);
    CHECK(!drawn[7].settings&&!drawn[8].settings);
    CHECK(saved_value());

    /* Pointer-held/release frames have no fresh press; close cannot click the
     * background Quit row even with a simultaneous fresh pointer event. */
    reset(7,800,450);click_at(0,1100,355);
    inputs[1].x=640;inputs[1].y=273; /* Held, not newly pressed. */
    click_at(2,640,273);
    inputs[3].x=640;inputs[3].y=273; /* Release, not newly pressed. */
    inputs[4].key=KEY_ESCAPE;click_at(4,1100,420);
    click_at(6,1100,165);
    CHECK(pe_app_menu("unused",true,false,NULL,0,NULL)==PE_APP_CONTINUE_SANDBOX);
    CHECK(frame==7&&closed==1);
    CHECK(drawn[0].on&&drawn[1].on&&drawn[2].off&&drawn[3].off);
    CHECK(!drawn[4].settings&&!drawn[5].settings&&!saved_value());

    /* Back's click is not admitted by the underlying menu; a fresh press is
     * needed to open Settings again. Keyboard Back also only closes its modal. */
    reset(8,1000,800);click_at(0,1100,355);click_at(1,640,485);
    click_at(3,1100,355);inputs[4].key=KEY_DOWN;inputs[5].key=KEY_ENTER;
    inputs[7].key=KEY_ESCAPE;
    CHECK(pe_app_menu("unused",true,false,NULL,0,NULL)==PE_APP_QUIT);
    CHECK(frame==8&&closed==1&&drawn[0].off&&!drawn[1].settings&&!drawn[2].settings);
    CHECK(drawn[3].off&&!drawn[5].settings&&!drawn[6].settings&&!saved_value());

    /* Existing directory: load and replacement fail. OFF remains displayed;
     * removing the obstruction permits a fresh retry to persist ON. */
    CHECK(unlink(settings_path)==0);CHECK(mkdir(settings_path,0700)==0);
    reset(6,800,450);click_at(0,1100,355);click_at(1,640,273);
    inputs[3].prepare=remove_blocking_directory;click_at(3,640,273);
    inputs[5].key=KEY_ESCAPE;
    CHECK(pe_app_menu("unused",true,false,NULL,0,NULL)==PE_APP_QUIT);
    CHECK(drawn[0].off&&drawn[1].off&&drawn[2].off&&drawn[2].error);
    CHECK(drawn[4].on&&!drawn[4].error&&saved_value());

    /* Disabled Continue/Tutorial are skipped both directions and Quit is the
     * fifth row. New Sandbox still opens from its initial enabled selection. */
    reset(2,800,450);tutorial_available=false;inputs[0].key=KEY_UP;inputs[1].key=KEY_ENTER;
    CHECK(pe_app_menu("unused",false,false,NULL,0,NULL)==PE_APP_QUIT&&frame==2);
    reset(3,800,450);tutorial_available=false;inputs[0].key=KEY_DOWN;inputs[1].key=KEY_ENTER;
    CHECK(pe_app_menu("unused",false,false,NULL,0,NULL)==PE_APP_QUIT&&drawn[1].on);
    reset(1,1280,720);inputs[0].key=KEY_ENTER;
    CHECK(pe_app_menu("unused",false,false,NULL,0,NULL)==PE_APP_NEW_SANDBOX);

    /* Shared submodal owner can be called by either paused host. Suppressed
     * opening input and focus loss retain ownership without changing anything;
     * drawing is read-only, and closing returns ownership for that frame. */
    reset(1,800,450);PeUiSettings panel;pe_ui_settings_init(&panel);
    CHECK(!panel.open&&panel.ready&&panel.preferences.camera_facing_attacks);
    CHECK(!pe_ui_settings_update(&panel,true));pe_ui_settings_open(&panel);
    inputs[0].key=KEY_ENTER;PeUiSettings before=panel;
    CHECK(pe_ui_settings_update(&panel,false)&&!memcmp(&panel,&before,sizeof panel));
    focused=false;
    CHECK(pe_ui_settings_update(&panel,true)&&!memcmp(&panel,&before,sizeof panel));
    focused=true;pe_ui_settings_draw(&panel,(PeUiAssets *)(uintptr_t)1);
    CHECK(!memcmp(&panel,&before,sizeof panel));
    CHECK(pe_ui_settings_update(&panel,true)&&!panel.preferences.camera_facing_attacks&&!saved_value());
    inputs[0].key=KEY_ESCAPE;
    CHECK(pe_ui_settings_update(&panel,true)&&!panel.open);
    CHECK(!pe_ui_settings_update(&panel,true));

    /* No gameplay context: old geometry/navigation has no debug entry, even
     * if a caller accidentally leaves an enabled flag/request behind. */
    reset(1,800,450);pe_ui_settings_init(&panel);pe_ui_settings_open(&panel);
    CHECK(!panel.debug_available&&!panel.debug_enabled&&panel.debug_request==PE_DEBUG_NONE);
    panel.debug_enabled=true;panel.debug_request=PE_DEBUG_MEDICINE;
    CHECK(pe_ui_settings_update(&panel,true)&&!panel.debug_enabled&&panel.debug_request==PE_DEBUG_NONE);
    pe_ui_settings_draw(&panel,(PeUiAssets *)(uintptr_t)1);
    CHECK(!drawn[0].debug_on&&!drawn[0].debug_off&&!drawn[0].debug_actions&&!drawn[0].debug_warning);
    inputs[0].key=KEY_DOWN;
    CHECK(pe_ui_settings_update(&panel,true)&&panel.selected==1);
    inputs[0].key=KEY_ENTER;CHECK(pe_ui_settings_update(&panel,true)&&!panel.open);

    /* Adventure capability exposes an OFF session toggle. Camera file I/O
     * readiness is irrelevant to debug commands; the host owns execution. */
    reset(1,640,360);pe_ui_settings_init(&panel);panel.debug_available=true;panel.ready=false;
    pe_ui_settings_open(&panel);before=panel;pe_ui_settings_draw(&panel,(PeUiAssets *)(uintptr_t)1);
    CHECK(!memcmp(&panel,&before,sizeof panel)&&drawn[0].debug_off&&!drawn[0].debug_actions&&drawn[0].debug_warning);
    inputs[0].key=KEY_DOWN;CHECK(pe_ui_settings_update(&panel,true)&&panel.selected==1);
    CHECK(pe_ui_settings_update(&panel,true)&&panel.selected==5); /* OFF skips all command rows. */
    inputs[0].key=KEY_UP;CHECK(pe_ui_settings_update(&panel,true)&&panel.selected==1);
    inputs[0].key=0;click_at(0,640,360); /* Hidden medicine row cannot grant. */
    CHECK(pe_ui_settings_update(&panel,true)&&panel.debug_request==PE_DEBUG_NONE&&!panel.debug_enabled);
    inputs[0]=(Input){.key=KEY_ENTER};
    CHECK(pe_ui_settings_update(&panel,true)&&panel.debug_enabled&&panel.debug_request==PE_DEBUG_NONE);
    CHECK(!saved_value()); /* Session toggle did not overwrite camera preference. */

    const PeDebugAction commands[]={PE_DEBUG_MEDICINE,PE_DEBUG_COMBAT_SUPPLIES,PE_DEBUG_START_HUNT};
    for(unsigned i=0;i<3;i++){
        inputs[0]=(Input){.key=KEY_DOWN};CHECK(pe_ui_settings_update(&panel,true)&&panel.selected==(int)i+2);
        inputs[0]=(Input){.key=KEY_ENTER};CHECK(pe_ui_settings_update(&panel,true)&&panel.debug_request==commands[i]&&panel.open);
        inputs[0]=(Input){0};CHECK(pe_ui_settings_update(&panel,true)&&panel.debug_request==PE_DEBUG_NONE); /* Held/released means no fresh press. */
        inputs[0]=(Input){.key=KEY_SPACE};CHECK(pe_ui_settings_update(&panel,true)&&panel.debug_request==PE_DEBUG_NONE);
    }
    /* Fresh pointer commands share the same rows as drawing. Suppressed input
     * and focus loss clear stale requests instead of replaying them later. */
    for(unsigned i=0;i<3;i++){
        inputs[0]=(Input){0};click_at(0,640,360+48*i);
        CHECK(pe_ui_settings_update(&panel,true)&&panel.debug_request==commands[i]);
        CHECK(pe_ui_settings_update(&panel,false)&&panel.debug_request==PE_DEBUG_NONE);
        panel.debug_request=commands[i];focused=false;
        CHECK(pe_ui_settings_update(&panel,true)&&panel.debug_request==PE_DEBUG_NONE);focused=true;
    }
    inputs[0]=(Input){0};click_at(0,640,309);
    panel.debug_request=PE_DEBUG_START_HUNT;
    CHECK(pe_ui_settings_update(&panel,true)&&!panel.debug_enabled&&panel.debug_request==PE_DEBUG_NONE&&panel.selected==1);
    panel.selected=4;inputs[0]=(Input){.key=KEY_ENTER};
    CHECK(pe_ui_settings_update(&panel,true)&&panel.debug_request==PE_DEBUG_NONE&&!panel.debug_enabled); /* Invalid disabled selection cannot dispatch. */
    panel.debug_enabled=true;panel.debug_request=PE_DEBUG_MEDICINE;inputs[0]=(Input){.key=KEY_ESCAPE};
    CHECK(pe_ui_settings_update(&panel,true)&&!panel.open&&panel.debug_enabled&&panel.debug_request==PE_DEBUG_NONE);
    CHECK(!pe_ui_settings_update(&panel,true));
    panel.debug_request=PE_DEBUG_MEDICINE;pe_ui_settings_open(&panel);CHECK(panel.debug_request==PE_DEBUG_NONE);
    inputs[0]=(Input){0};click_at(0,640,622);
    CHECK(pe_ui_settings_update(&panel,true)&&!panel.open&&panel.debug_request==PE_DEBUG_NONE);
    panel.debug_available=false;panel.debug_request=PE_DEBUG_START_HUNT;
    CHECK(!pe_ui_settings_update(&panel,true)&&!panel.debug_enabled&&panel.debug_request==PE_DEBUG_NONE);

    /* Both states and a maximal bounded notice fit normal and small windows;
     * this checks actual draw coordinates, without replacing font inspection. */
    const int debug_sizes[][2]={{1280,720},{800,450},{640,360}};
    for(unsigned i=0;i<3;i++)for(unsigned enabled=0;enabled<2;enabled++){
        reset(1,debug_sizes[i][0],debug_sizes[i][1]);pe_ui_settings_init(&panel);
        panel.debug_available=true;panel.debug_enabled=enabled;pe_ui_settings_open(&panel);
        memset(panel.debug_notice,'W',sizeof panel.debug_notice-1);panel.debug_notice[sizeof panel.debug_notice-1]=0;
        before=panel;pe_ui_settings_draw(&panel,(PeUiAssets *)(uintptr_t)1);
        CHECK(!memcmp(&panel,&before,sizeof panel));
        CHECK(drawn[0].debug_on==(bool)enabled&&drawn[0].debug_off==!enabled&&drawn[0].debug_actions==(enabled?3u:0u)&&drawn[0].debug_warning);
        CHECK(drawn[0].notice_lines==1&&!scissored); /* Oversized tokens are clipped by the shared renderer. */
        drawn[0]=(Drawn){0};
        snprintf(panel.debug_notice,sizeof panel.debug_notice,"This hunt is already completed. Start a new Adventure to use this shortcut; your existing hunt progress and rewards have been preserved.");
        before=panel;pe_ui_settings_draw(&panel,(PeUiAssets *)(uintptr_t)1);
        CHECK(!memcmp(&panel,&before,sizeof panel)&&!scissored);
        CHECK(drawn[0].notice_lines>=2&&drawn[0].notice_lines<=3&&drawn[0].notice_end);
    }
    pe_ui_settings_init(&panel);CHECK(!panel.debug_enabled&&!panel.debug_available&&panel.debug_request==PE_DEBUG_NONE&&!saved_value());

    CHECK(unlink(settings_path)==0);CHECK(rmdir(directory)==0);
    printf("app menu settings checks: %s\n",failures?"FAIL":"PASS");
    return failures?1:0;
}
