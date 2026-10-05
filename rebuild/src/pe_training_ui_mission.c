#include "pe_training_ui_mission.h"
#include "../generated/pe_training_ui_text.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {const PeUiAssets *assets;float x,y,s;} Canvas;
static const Color ink={255,255,255,255}, dim={160,174,195,100}, heading={159,197,234,255};
static Rectangle box(Canvas c,float x,float y,float w,float h,int pivot) {
    float ax=(pivot==4?w*.5f:pivot==5?w:0),ay=(pivot==3||pivot==4||pivot==5?h*.5f:0);
    return(Rectangle){c.x+(x-ax)*c.s,c.y+(-y-ay)*c.s,w*c.s,h*c.s};
}
static void sprite(Canvas c,const char *name,float x,float y,float w,float h,int pivot,Color tint) {
    pe_ui_sprite(c.assets,name,box(c,x,y,w,h,pivot),tint,c.s);
}
static void text(Canvas c,const char *value,float x,float y,float size,int pivot,Color tint) {
    if(size>=30)size=32;else if(size==20)size=18;
    Font f=size>=30?pe_ui_font_heading(c.assets):pe_ui_font(c.assets,false);Vector2 extent=MeasureTextEx(f,value,size*c.s,0);
    float px=c.x+x*c.s,py=c.y-y*c.s;
    if(pivot==4)px-=extent.x*.5f;
    if(pivot==5)px-=extent.x;
    if(pivot==3||pivot==4||pivot==5)py-=extent.y*.5f;
    DrawTextEx(f,value,(Vector2){px,py},size*c.s,0,tint);
}
/* Word-based bounded UTF-8 wrapping: never splits a multibyte word. Source
   lesson text is short; geometry clips a too-wide individual token. */
static float wrapped(Canvas c,const char *value,float x,float y,float width,float height,float size,Color tint) {
    if(size==20)size=18;
    Rectangle area=box(c,x,y,width,height,0);Font f=pe_ui_font(c.assets,false);
    float line_height=(size+4)*c.s,dy=0;char line[1024]={0};size_t used=0;
    const char *p=value;
    while(*p&&dy+size*c.s<=area.height) {
        if(*p=='\n') {DrawTextEx(f,line,(Vector2){area.x,area.y+dy},size*c.s,0,tint);used=0;line[0]=0;dy+=line_height;p++;continue;}
        while(*p==' ')p++;
        if(!*p)break;
        size_t n=strcspn(p," \n");
        if(!n)continue;
        if(n>=sizeof line)break;
        char candidate[1024];int bytes=snprintf(candidate,sizeof candidate,"%s%s%.*s",line,used?" ":"",(int)n,p);
        if(bytes<0||(size_t)bytes>=sizeof candidate)break;
        if(used&&MeasureTextEx(f,candidate,size*c.s,0).x>area.width) {
            DrawTextEx(f,line,(Vector2){area.x,area.y+dy},size*c.s,0,tint);
            used=0;line[0]=0;dy+=line_height;continue;
        }
        memcpy(line,candidate,(size_t)bytes+1);used=(size_t)bytes;p+=n;
    }
    if(used&&dy+size*c.s<=area.height){DrawTextEx(f,line,(Vector2){area.x,area.y+dy},size*c.s,0,tint);dy+=line_height;}
    return dy/c.s;
}
static unsigned current_count(const PeTraining *t,const PeTrainingUiObjectiveText *objective) {
    if(objective->kind==1)return t->combat_kill_entity_id?1u:0u;
    if(objective->kind==2) {
        if(objective->item_id==916)return t->juice_count>0?(unsigned)t->juice_count:0;
        return pe_training_gathering_count(&t->gathering,objective->item_id);
    }
    return pe_tutorial_objective_done(&t->tutorial,objective->target_id)?1u:0u;
}
static void andy(Canvas c,float x,float y,float name_x,float name_y) {
    sprite(c,"npc_big_Andy",x,y,130,130,4,ink);
    sprite(c,"namebg",name_x,name_y,160,24,4,ink);
    text(c,"Andy",name_x,name_y+1,20,4,ink);
}
void pe_training_ui_mission_draw(const PeUiAssets *assets,const PeTrainingUILayout *l,const PeTraining *t,PeTrainingUiShader *effects,float seconds) {
    if(!assets||!l||!t||!isfinite(l->scale)||l->scale<=0||!isfinite(l->mission.x)||!isfinite(l->mission.y))return;
    Canvas c={assets,l->mission.x+520*l->scale,l->mission.y+270*l->scale,l->scale};
    /* MissionWnd.prefab root initially(-100,0), reset by GameUI.AddUIPrefab:
       these positions exclude that authored root translation. */
    sprite(c,"bigbg",0,0,1040,540,4,ink);
    sprite(c,"bigbg",0,0,1040,540,4,(Color){0,33,255,77});
    pe_training_ui_shader_draw(effects,PE_UI_EFFECT_MISSION,box(c,0,0,1062,552,4),seconds,1,false);
    text(c,"MISSION",-433,237.5f,37,3,heading);
    sprite(c,"close1",489.5f,240.5f,25,25,4,(Color){146,156,255,255});
    sprite(c,"insidebox2",-330,-50,310,400,4,ink);
    sprite(c,"insidebox2",-330,175,310,35,4,ink);
    sprite(c,"insidebox2",163,98,646,190,4,ink);
    sprite(c,"insidebox2",0,-141.018f,320.04f,218.027f,4,ink);
    sprite(c,"insidebox2",330,-141,316,218,4,ink);
    text(c,"About",486,208,20,5,ink);
    text(c,"Goal",160,-17,20,5,ink);
    text(c,"Reward",488,-17,20,5,ink);
    const PeTrainingUiMissionText *mission=pe_training_ui_mission_text(t->tutorial.mission_id);
    bool main_story=mission&&mission->mission_type==1;
    sprite(c,"choose",main_story?-418:-294,175,main_story?124:115,32,4,(Color){190,216,255,180});
    text(c,"Main Story",-418,175,20,4,main_story?ink:dim);
    text(c,"Side Quest",-293.5f,175,20,4,main_story?dim:ink);
    if(!mission||!*mission->title)return;
    /* One actual active mission, no fabricated completed-quest history. The
       source first node is Table(-472,138) with a286x30 selection sprite. */
    sprite(c,"multplayer_hall_select",-472,138,286,30,0,(Color){180,210,255,200});
    sprite(c,"mission_closed",-472,131,12,12,0,dim);
    wrapped(c,mission->title,-457,134,220,52,20,ink);
    sprite(c,"checkbox_1",-222,123,18,18,4,dim);
    sprite(c,"checkbox_3",-222,123,16,16,4,dim);
    sprite(c,"checkbox_2",-197,123,18,18,4,dim);
    /* Source Quest_List.Npc is Andy only for these four lessons. The other
       source portrait/name controls are disabled, not supplied fake NPCs. */
    int id=(int)t->tutorial.mission_id;
    if(id==718||id==721||id==739||id==742)andy(c,-83,104,-79,27);
    wrapped(c,mission->description,15,183,430,170,20,ink);
    float y=-37;
    for(size_t i=0;i<mission->objective_count&&y>-230;i++) {
        const PeTrainingUiObjectiveText *objective=pe_training_ui_mission_objective(id,i);
        if(!objective)continue;
        bool done=pe_tutorial_objective_done(&t->tutorial,objective->target_id);
        char value[1024];
        if(objective->max_count>0)snprintf(value,sizeof value,"%s %u/%d",objective->text,current_count(t,objective),objective->max_count);
        else snprintf(value,sizeof value,"%s",objective->text);
        float used=wrapped(c,value,-155,y,294,230+y,18,done?(Color){150,220,170,255}:ink);
        y-=used+9;
    }
    /* All19 implemented Quest_List rows have zero completion item rewards.
       Acceptance grants are not shown as completion rewards. ReplyNpc is
       Andy only for742, matching source SetSubmit portrait/name controls. */
    if(id==742)andy(c,247,-151,252,-228);
}
