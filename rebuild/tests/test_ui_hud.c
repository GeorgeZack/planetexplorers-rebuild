/* Headless shared presentation contract: real renderer/wrapping, recorded GPU
 * calls and synthetic font metrics. No display, assets or game mutation. */
#include "pe_ui_hud.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#c);exit(1);}}while(0)
typedef struct {int kind;char text[2048];Rectangle box;Color tint;int font;} Draw;
static Draw calls[2048];static size_t count;static int scissors;
static void record(int kind,const char *text,Rectangle r,Color color,int font){CHECK(count<2048);Draw *d=&calls[count++];memset(d,0,sizeof *d);d->kind=kind;d->box=r;d->tint=color;d->font=font;if(text)snprintf(d->text,sizeof d->text,"%s",text);}
bool pe_ui_sprite(const PeUiAssets *a,const char *s,Rectangle r,Color c,float z){(void)a;(void)z;record(1,s,r,c,0);return true;}
/* Kind 8: a mirrored sprite; font carries the flags (1 horizontal, 2 vertical). */
bool pe_ui_sprite_flip(const PeUiAssets *a,const char *s,Rectangle r,Color c,bool h,bool v){(void)a;record(8,s,r,c,(h?1:0)|(v?2:0));return true;}
Font pe_ui_font(const PeUiAssets *a,bool bold){(void)a;return (Font){.baseSize=bold?19:18};}
Font pe_ui_font_small(const PeUiAssets *a,bool bold){(void)a;return (Font){.baseSize=bold?15:14};}
Font pe_ui_font_heading(const PeUiAssets *a){(void)a;return (Font){.baseSize=32};}
Vector2 MeasureTextEx(Font f,const char *s,float size,float spacing){(void)f;(void)spacing;return (Vector2){strlen(s)*size*.55f,size};}
void DrawTextEx(Font f,const char *s,Vector2 p,float size,float spacing,Color c){(void)spacing;record(2,s,(Rectangle){p.x,p.y,MeasureTextEx(f,s,size,0).x,size},c,f.baseSize);}
void DrawRectangleRec(Rectangle r,Color c){record(3,NULL,r,c,0);}
void DrawRectangleLinesEx(Rectangle r,float width,Color c){(void)width;record(4,NULL,r,c,0);}
void BeginScissorMode(int x,int y,int w,int h){CHECK(w>=0&&h>=0);scissors++;record(5,NULL,(Rectangle){x,y,w,h},WHITE,0);}
void EndScissorMode(void){CHECK(scissors>0);scissors--;record(6,NULL,(Rectangle){0},WHITE,0);}
void pe_training_ui_shader_draw(PeTrainingUiShader *fx,PeUiEffect role,Rectangle r,float seconds,float alpha,bool refresh){(void)fx;(void)seconds;(void)alpha;(void)refresh;record(7,NULL,r,WHITE,role);}
static const Draw *find(int kind,const char *text){for(size_t i=0;i<count;i++)if(calls[i].kind==kind&&!strcmp(calls[i].text,text))return &calls[i];return NULL;}
static void reset(void){CHECK(scissors==0);count=0;memset(calls,0,sizeof calls);}
static bool near(float a,float b){return fabsf(a-b)<.01f;}
int main(void){
    const int sizes[][2]={{1280,720},{640,480},{320,240},{1920,1080}};
    for(size_t size=0;size<sizeof sizes/sizeof *sizes;size++){
        PeTrainingUILayout l;CHECK(pe_training_ui_layout(sizes[size][0],sizes[size][1],&l));
        PeUiCanvas canvas={.layout=&l,.mouse_x=-1,.mouse_y=-1};
        PeUiHudView view={.health=50,.health_max=100,.stamina=40,.stamina_max=100,.hand_item_id=33};
        view.slots[0]=(PeUiQuickslotView){916,5,3.2f};view.slots[8]=(PeUiQuickslotView){925,0,0};
        PeUiHudView before=view;reset();pe_ui_hud_draw(&canvas,&view);
        CHECK(!memcmp(&view,&before,sizeof view));CHECK(scissors==0);
        CHECK(find(1,"face3")&&find(1,"tili")&&find(1,"bloodlan_8bg")&&find(1,"arr5"));
        CHECK(find(1,"Triangle_btn_Up_on")&&find(1,"Triangle_btn_bottom_on"));
        CHECK(find(1,"drug_hot01")&&find(1,"ax_knife"));
        const Draw *label=find(2,"9");CHECK(label&&near(label->box.x,l.quickslots[8].x+4*l.scale)&&near(label->box.y,l.quickslots[8].y+2*l.scale));
        CHECK(find(2,"4")); /* cooldown rounds up, no clock advancement */
        PeUiMinimapView map={.coordinates_available=true,.clock_available=true,.x=-284,.z=134,.hour=25,.minute=59};
        reset();pe_ui_minimap_chrome_draw(&canvas,&map);
        CHECK(find(1,"mapeye")&&find(1,"maplight1")&&find(1,"NpcTalkHistoryBtn"));
        CHECK(find(2,"25:59")&&find(2,"P: -284, 134"));
        bool scan=false;for(size_t k=0;k<count;k++)if(calls[k].kind==7&&calls[k].font==PE_UI_EFFECT_MAP_SCAN)scan=true;CHECK(scan);
        map.coordinates_available=map.clock_available=false;reset();pe_ui_minimap_chrome_draw(&canvas,&map);CHECK(!find(2,"25:59")&&!find(2,"P: -284, 134"));
        PeUiTrackerObjective objectives[]={{"First goal",false},{"Completed goal",true}};
        PeUiTrackerView mission={"A long camera mission title that must wrap above its objectives",true,objectives,2};
        reset();float used=pe_ui_tracker_draw(&canvas,&mission);CHECK(used>0&&used<=l.mission_tracker.h+7*l.scale+.01f);CHECK(scissors==0);
        const Draw *goal=find(2,"First goal"),*done=find(2,"Completed goal");CHECK(goal&&done);CHECK(goal->font==19&&done->tint.r==180);
        float title_bottom=0;for(size_t k=0;k<count;k++)if(calls[k].kind==2){if(&calls[k]==goal)break;title_bottom=fmaxf(title_bottom,calls[k].box.y+calls[k].box.height);}
        CHECK(goal->box.y>=title_bottom+12*l.scale);
        CHECK(goal->box.y>=l.minimap.y+l.minimap.h);
        CHECK(done->box.y+done->box.height<=l.mission_tracker.y+l.mission_tracker.h+.01f);
        mission.title="Short";reset();pe_ui_tracker_draw(&canvas,&mission);goal=find(2,"First goal");CHECK(goal&&near(goal->box.y,l.mission_tracker.y+25*l.scale));
        mission.title="";reset();CHECK(pe_ui_tracker_draw(&canvas,&mission)==0&&count==0);
        /* Hunger: 5_Hunger's rotated track always; its fill only with host data, from the right end. */
        if(size==0)CHECK(near(l.hunger.x,666)&&near(l.hunger.y,625)&&near(l.hunger.w,64)&&near(l.hunger.h,5));
        reset();pe_ui_hud_draw(&canvas,&view);
        const Draw *track=find(8,"bloodlan_12bg");CHECK(track&&track->font==3&&near(track->box.x,l.hunger.x)&&near(track->box.width,l.hunger.w));
        CHECK(!find(1,"bloodlan_12bg")&&!find(8,"bloodlan_12")&&!find(1,"bloodlan_12"));
        PeUiHudView fed=view;fed.hunger_available=true;fed.hunger=250;fed.hunger_max=500;before=fed;
        reset();pe_ui_hud_draw(&canvas,&fed);CHECK(!memcmp(&fed,&before,sizeof fed));CHECK(scissors==0);
        const Draw *fill=find(8,"bloodlan_12");CHECK(fill&&fill->font==3&&near(fill->box.x,l.hunger.x)&&near(fill->box.width,l.hunger.w));
        const Draw *clip=fill-1;CHECK(clip->kind==5);
        float end=l.hunger.x+l.hunger.w;CHECK(clip->box.x==floorf(end-l.hunger.w*.5f)&&clip->box.x+clip->box.width==ceilf(end));
        fed.hunger=900;reset();pe_ui_hud_draw(&canvas,&fed);fill=find(8,"bloodlan_12");CHECK(fill&&(fill-1)->box.x==floorf(l.hunger.x));
        fed.hunger=0;reset();pe_ui_hud_draw(&canvas,&fed);CHECK(!find(8,"bloodlan_12")&&find(8,"bloodlan_12bg"));
        fed.hunger=NAN;reset();pe_ui_hud_draw(&canvas,&fed);CHECK(!find(8,"bloodlan_12"));
        fed.hunger=250;fed.hunger_max=0;reset();pe_ui_hud_draw(&canvas,&fed);CHECK(!find(8,"bloodlan_12"));
        reset();pe_ui_hud_draw(NULL,&view);pe_ui_minimap_chrome_draw(NULL,&map);CHECK(count==0);
    }
    puts("shared HUD, minimap chrome and bounded tracker presentation pass");return 0;
}
