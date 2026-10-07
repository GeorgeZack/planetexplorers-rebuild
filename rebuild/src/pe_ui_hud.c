#include "pe_ui_hud.h"
#include <math.h>
#include <stdio.h>

static bool valid(const PeUiCanvas *c){return c&&c->layout&&isfinite(c->layout->scale)&&c->layout->scale>0;}
static float fraction(float n,float d){return isfinite(n)&&isfinite(d)&&d>0?fminf(1,fmaxf(0,n/d)):0;}
static Rectangle rectangle(PeTrainingUIRect r){return (Rectangle){r.x,r.y,r.w,r.h};}
static void bar(const PeUiCanvas *c,const char *background,const char *fill,PeTrainingUIRect r,float value,Color tint,bool health){
    pe_ui_draw_sprite(c,background,r,WHITE);
    if(value<=0)return;
    BeginScissorMode((int)r.x,(int)r.y,(int)ceilf(r.w*value),(int)ceilf(r.h));
    pe_ui_draw_sprite(c,fill,r,tint);
    if(health)pe_ui_draw_effect(c,PE_UI_EFFECT_HEALTH,r);
    EndScissorMode();
}
void pe_ui_hud_draw(const PeUiCanvas *c,const PeUiHudView *v){
    if(!valid(c)||!v)return;
    const PeTrainingUILayout *l=c->layout;float z=l->scale;
    PeTrainingUIRect frame=pe_ui_draw_map(c,217.5f,611,845,110,PE_UI_BOTTOM_CENTER);
    pe_ui_draw_sprite(c,"shortbar1bg",pe_ui_draw_map(c,217.5f,644,845,54,PE_UI_BOTTOM_CENTER),WHITE);
    pe_ui_draw_sprite(c,"shortbar1",frame,WHITE);
    pe_ui_draw_sprite(c,"shortbar1",frame,(Color){0,33,255,77});
    pe_ui_draw_effect(c,PE_UI_EFFECT_FRAME,frame);
    pe_ui_draw_effect(c,PE_UI_EFFECT_HUD_SCAN,pe_ui_draw_map(c,225,646,830,50,PE_UI_BOTTOM_CENTER));
    bar(c,"bloodlan_6bg","bloodlan_6",l->health,fraction(v->health,v->health_max),WHITE,true);
    bar(c,"bloodlan_7bg","bloodlan_7_1",l->stamina,fraction(v->stamina,v->stamina_max),(Color){65,131,185,255},false);
    static const struct {const char *name;float x,y,w,h;} decorations[]={
        {"face3",535,620,12,13},{"tili",734,620,12,13},
        {"bloodlan_11bg",551,626,64,5},{"bloodlan_12bg",666,625,64,5},
        {"bloodlan_8bg",426,704,87,4},{"bloodlan_9bg",767,704,87,4},
        {"arr3",535,695,20,20},{"arr5",723,695,20,20}
    };
    for(size_t i=0;i<sizeof decorations/sizeof *decorations;i++){
        pe_ui_draw_sprite(c,decorations[i].name,pe_ui_draw_map(c,decorations[i].x,decorations[i].y,decorations[i].w,decorations[i].h,PE_UI_BOTTOM_CENTER),WHITE);
    }
    for(int k=0;k<PE_UI_QUICKSLOTS;k++){
        PeTrainingUIRect r=l->quickslots[k];const PeUiQuickslotView *slot=&v->slots[k];char label[32];
        if(slot->item_id)pe_ui_draw_item(c,slot->item_id,slot->count,r,false,slot->count>0?WHITE:PE_UI_DIM);
        if(isfinite(slot->cooldown)&&slot->cooldown>0){
            DrawRectangleRec(rectangle(r),(Color){25,40,90,160});
            snprintf(label,sizeof label,"%.0f",ceilf(slot->cooldown));
            pe_ui_draw_center(c,label,r,18,PE_UI_INK,true);
        }
        snprintf(label,sizeof label,"%d",k+1);
        pe_ui_draw_text(c,label,r.x+4*z,r.y+2*z,14,(Color){220,225,235,180},true);
    }
    pe_ui_draw_sprite(c,"icons:itemhand",l->hand_slot,WHITE);
    if(v->hand_item_id)pe_ui_draw_item(c,v->hand_item_id,1,l->hand_slot,false,WHITE);
    pe_ui_draw_sprite(c,"icons:itemhand_get",l->hand_slot,WHITE);
    pe_ui_draw_sprite(c,"buttonb",l->build_button,v->building?PE_UI_BLUE:WHITE);
    PeTrainingUIRect page=pe_ui_draw_map(c,250,678,0,0,PE_UI_BOTTOM_CENTER);
    pe_ui_draw_text(c,"1",page.x,page.y,14,PE_UI_GOLD,false);
    pe_ui_draw_sprite(c,"Triangle_btn_Up_on",pe_ui_draw_map(c,233,676,16,16,PE_UI_BOTTOM_CENTER),(Color){200,220,240,180});
    pe_ui_draw_sprite(c,"Triangle_btn_bottom_on",pe_ui_draw_map(c,261,676,16,16,PE_UI_BOTTOM_CENTER),(Color){200,220,240,180});
    char number[96];
    if(pe_training_ui_contains(l->health,c->mouse_x,c->mouse_y)){
        snprintf(number,sizeof number,"%.0f / %.0f",v->health,v->health_max);
        pe_ui_draw_center(c,number,l->health,14,WHITE,true);
    }
    if(pe_training_ui_contains(l->stamina,c->mouse_x,c->mouse_y)){
        snprintf(number,sizeof number,"Stamina %.0f / %.0f",v->stamina,v->stamina_max);
        pe_ui_draw_text(c,number,l->stamina.x,l->stamina.y-20*z,14,PE_UI_INK,false);
    }
}
void pe_ui_minimap_chrome_draw(const PeUiCanvas *c,const PeUiMinimapView *v){
    if(!valid(c)||!v)return;
    pe_ui_draw_sprite(c,"map_02",pe_ui_draw_map(c,1089,0,158,200,PE_UI_TOP_RIGHT),(Color){205,245,255,120});
    static const struct {const char *name;float x,w;} edges[]={{"map_01",1069,20},{"map_03",1246,33}};
    for(size_t i=0;i<2;i++){
        PeTrainingUIRect r=pe_ui_draw_map(c,edges[i].x,0,edges[i].w,200,PE_UI_TOP_RIGHT);
        pe_ui_draw_sprite(c,edges[i].name,r,WHITE);pe_ui_draw_sprite(c,edges[i].name,r,(Color){0,33,255,120});
    }
    pe_ui_draw_effect(c,PE_UI_EFFECT_MAP_LEFT,pe_ui_draw_map(c,1069,0,20,208,PE_UI_TOP_RIGHT));
    pe_ui_draw_effect(c,PE_UI_EFFECT_MAP_RIGHT,pe_ui_draw_map(c,1246,0,34,208,PE_UI_TOP_RIGHT));
    pe_ui_draw_effect(c,PE_UI_EFFECT_MAP_SCAN,pe_ui_draw_map(c,1094,0,148,208,PE_UI_TOP_RIGHT));
    pe_ui_draw_sprite(c,"mapeye",pe_ui_draw_map(c,1253,4,22,22,PE_UI_TOP_RIGHT),WHITE);
    pe_ui_draw_sprite(c,"maplight1",pe_ui_draw_map(c,1253,29,22,22,PE_UI_TOP_RIGHT),WHITE);
    pe_ui_draw_sprite(c,"NpcTalkHistoryBtn",pe_ui_draw_map(c,1253,148,22,22,PE_UI_TOP_RIGHT),WHITE);
    pe_ui_draw_sprite(c,"Icon_mission",c->layout->mission_button,WHITE);
    char text[128];
    if(v->clock_available&&v->hour>=0&&v->hour<=99&&v->minute>=0&&v->minute<60){
        snprintf(text,sizeof text,"%02d:%02d",v->hour,v->minute);
        pe_ui_draw_center(c,text,pe_ui_draw_map(c,1090,179,156,18,PE_UI_TOP_RIGHT),14,PE_UI_INK,true);
    }
    if(v->coordinates_available&&isfinite(v->x)&&isfinite(v->z)){
        snprintf(text,sizeof text,"P: %.0f, %.0f",v->x,v->z);
        PeTrainingUIRect r=pe_ui_draw_map(c,1090,7,156,18,PE_UI_TOP_RIGHT);
        pe_ui_draw_text(c,text,r.x,r.y,14,PE_UI_INK,false);
    }
}
float pe_ui_tracker_draw(const PeUiCanvas *c,const PeUiTrackerView *v){
    if(!valid(c)||!v||!v->title||!*v->title||(!v->objectives&&v->objective_count))return 0;
    float z=c->layout->scale;PeTrainingUIRect r=c->layout->mission_tracker;
    float start=r.y-7*z,bottom=r.y+r.h,y=start;
    PeTrainingUIRect area={r.x+18*z,y,fmaxf(0,r.w-34*z),fmaxf(0,bottom-y)};
    y+=pe_ui_draw_wrap(c,v->title,area,18,v->main_story?(Color){155,61,244,255}:(Color){55,203,244,255},true,2);
    y+=12*z;
    for(size_t i=0;i<v->objective_count&&y+18*z<=bottom;i++){
        if(!v->objectives[i].text||!*v->objectives[i].text)continue;
        area=(PeTrainingUIRect){r.x+18*z,y,fminf(233*z,fmaxf(0,r.w-34*z)),bottom-y};
        y+=pe_ui_draw_wrap(c,v->objectives[i].text,area,18,v->objectives[i].completed?(Color){180,180,180,255}:WHITE,true,2);
        y+=5*z;
    }
    return fminf(bottom-start,fmaxf(0,y-start));
}
