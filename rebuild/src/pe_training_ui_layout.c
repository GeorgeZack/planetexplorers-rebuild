#include "pe_training_ui_layout.h"
#include <limits.h>
#include <math.h>
#include <string.h>

static bool valid(const PeTrainingUILayout *l) {
    return l && isfinite(l->width) && isfinite(l->height) && isfinite(l->scale)
        && l->width>0 && l->height>0 && l->scale>0;
}
static bool offset(const PeTrainingUILayout *l, PeTrainingUIAnchor anchor, float *x, float *y) {
    if (!valid(l)) return false;
    float dx=l->width-1280*l->scale, dy=l->height-720*l->scale;
    switch(anchor) {
    case PE_UI_TOP_LEFT: *x=0; *y=0; break;
    case PE_UI_TOP_CENTER: *x=dx*.5f; *y=0; break;
    case PE_UI_TOP_RIGHT: *x=dx; *y=0; break;
    case PE_UI_CENTER: *x=dx*.5f; *y=dy*.5f; break;
    case PE_UI_BOTTOM_CENTER: *x=dx*.5f; *y=dy; break;
    case PE_UI_BOTTOM_RIGHT: *x=dx; *y=dy; break;
    default: return false;
    }
    return isfinite(*x) && isfinite(*y);
}
PeTrainingUIRect pe_training_ui_map(const PeTrainingUILayout *l, PeTrainingUIRect r, PeTrainingUIAnchor a) {
    float x,y;
    if (!offset(l,a,&x,&y) || !isfinite(r.x) || !isfinite(r.y) ||
        !isfinite(r.w) || !isfinite(r.h) || r.w<0 || r.h<0) return (PeTrainingUIRect){0};
    PeTrainingUIRect result={x+r.x*l->scale,y+r.y*l->scale,r.w*l->scale,r.h*l->scale};
    if (!isfinite(result.x) || !isfinite(result.y) || !isfinite(result.w) || !isfinite(result.h)) return (PeTrainingUIRect){0};
    return result;
}
bool pe_training_ui_unmap(const PeTrainingUILayout *l, PeTrainingUIAnchor a,
                          float sx,float sy,float *rx,float *ry) {
    float x,y;
    if (!rx || !ry || !isfinite(sx) || !isfinite(sy) || !offset(l,a,&x,&y)) return false;
    x=(sx-x)/l->scale; y=(sy-y)/l->scale;
    if (!isfinite(x) || !isfinite(y)) return false;
    *rx=x; *ry=y; return true;
}
bool pe_training_ui_minimap_uv(float px,float pz,float cx,float cz,PeTrainingUIRect *uv) {
    if (!uv || !isfinite(px) || !isfinite(pz) || !isfinite(cx) || !isfinite(cz)) return false;
    double dx=fmax(-62.,fmin(62.,(double)px-cx));
    double dz=fmax(-62.,fmin(62.,(double)pz-cz));
    *uv=(PeTrainingUIRect){(float)(.25+dx/248.),(float)(.25+dz/248.),.5f,.5f};
    return true;
}
bool pe_training_ui_contains(PeTrainingUIRect r,float x,float y) {
    return isfinite(x) && isfinite(y) && isfinite(r.x) && isfinite(r.y) &&
        isfinite(r.w) && isfinite(r.h) && r.w>0 && r.h>0 &&
        x>=r.x && y>=r.y && (double)x<(double)r.x+r.w && (double)y<(double)r.y+r.h;
}
int pe_training_ui_hit(const PeTrainingUIRect *rects,size_t count,float x,float y) {
    if (!rects || count>INT_MAX) return -1;
    for (size_t i=0;i<count;i++) if (pe_training_ui_contains(rects[i],x,y)) return (int)i;
    return -1;
}
static PeTrainingUIRect rect(const PeTrainingUILayout *l,PeTrainingUIAnchor a,float x,float y,float w,float h) {
    return pe_training_ui_map(l,(PeTrainingUIRect){x,y,w,h},a);
}
bool pe_training_ui_layout(float width,float height,PeTrainingUILayout *out) {
    if (!out || !isfinite(width) || !isfinite(height) || width<=0 || height<=0) return false;
    PeTrainingUILayout l; memset(&l,0,sizeof l);
    l.width=width; l.height=height; l.scale=fminf(width/1280,height/720);
    if (!valid(&l)) return false;
    const PeTrainingUIAnchor bottom=PE_UI_BOTTOM_CENTER, right=PE_UI_TOP_RIGHT,
        left=PE_UI_TOP_LEFT, center=PE_UI_CENTER;
#define R(field,anchor,x,y,w,h) l.field=rect(&l,anchor,x,y,w,h)
    /* Measured original screenshots: native-tutorial, storage-open,
       inventory-extra-grant and knife-inventory, all 1280x720. */
    R(hud,bottom,222,615,835,104);
    R(hand_slot,bottom,278,647,48,48); R(build_button,bottom,1001,648,48,49);
    for (int i=0;i<PE_UI_QUICKSLOTS;i++)
        l.quickslots[i]=rect(&l,bottom,i<4?329+51*i:748+51*(i-4),647,48,48);
    R(health,bottom,558,648,164,30); R(stamina,bottom,571,678.5f,138,9);
    R(mission_button,right,1254,173,20,26);
    R(minimap,right,1069,0,210,208); R(mission_tracker,right,994,224,282,309);
    R(notification,left,42,0,520,64);
    /* UIMinMapCtrl.UpdateMapPos runtime 190x200, centered at(-96,-101)
       from top-right. MapTex local y+2; MapSubInfo local(.5,-.5),
       clipRange(-15,0,165,200). */
    R(minimap_texture,right,1089,-1,190,200); R(minimap_clip,right,1087,1.5f,165,200);
    R(minimap_player,right,1172,86,25,31);
    R(dialogue,bottom,216,470,847,141); R(dialogue_portrait,bottom,230,460,130,123);
    R(dialogue_name,bottom,216,582,163,29); R(dialogue_text,bottom,395,485,645,112);
    R(dialogue_next,bottom,1034,573,24,24);
    /* MissionWnd authoredrootx=-100 is reset to0 by GameUI.AddUIPrefab. */
    R(mission,center,120,90,1040,540); R(mission_close,center,1117,107,25,25);
    R(menu_button,PE_UI_BOTTOM_RIGHT,1191,659.5f,48,49);
    /* Compact supported menu subset; original full menu contents are not
       reproduced. Keep 15px interior margins and five distinct hit rows. */
    R(menu_panel,PE_UI_BOTTOM_RIGHT,1070,474,170,180);
    for(int i=0;i<PE_UI_MENU_ITEMS;i++)l.menu_items[i]=rect(&l,PE_UI_BOTTOM_RIGHT,1080,489+30*i,150,30);
    R(inventory,right,752,110,375,568); R(inventory_title,right,777,131,304,28);
    R(inventory_close,right,1101,126,19,19);
    R(inventory_page,right,862,597,156,23);
    for (int i=0;i<PE_UI_INVENTORY_TABS;i++) l.inventory_tabs[i]=rect(&l,right,778+66*i,166,60,35);
    for (int i=0;i<PE_UI_INVENTORY_SLOTS;i++) l.inventory_slots[i]=rect(&l,right,778+55*(i%6),231+52*(i/6),48,48);
    R(storage,left,152,134,375,450); R(storage_title,left,178,154,298,26);
    R(storage_close,left,501,146,19,19); R(storage_page,left,262,522,156,23);
    for (int i=0;i<PE_UI_STORAGE_TABS;i++) l.storage_tabs[i]=rect(&l,left,195+75*i,189,60,34);
    for (int i=0;i<PE_UI_STORAGE_SLOTS;i++) l.storage_slots[i]=rect(&l,left,178+55*(i%6),261+52*(i/6),48,48);
    /* TRAINING_UI_LAYOUT.json active PlayerInfoWnd / CompoundWnd widgets,
       plus GameUISystem UIDefaultPostion: player(-350,-25), compound(0,0).
       NGUI center-pivot coordinates converted from y-up to screen y-down. */
    R(equipment,center,-20,84,620,602); R(equipment_close,center,565.9f,103,25,25);
    R(equipment_preview,center,276,185,310,480);
    /* UIPlayerInfoCtrl.InitGrid: two columns, five rows; source mask1<<4
       is the weapon slot. GridContent root-relative center=(24,164). */
    for (int i=0;i<10;i++) l.equipment_slots[i]=rect(&l,center,290+235*(i/5),197+58*(i%5),48,48);
    l.equipment_weapon=l.equipment_slots[4];
    R(replicator,center,200,120,880,480); R(replicator_close,center,1040,134,25,25);
    R(replicator_recipes,center,265,227,200,354); R(replicator_recipe,center,267,228,200,50);
    R(replicator_output,center,476,501,48,48); R(replicator_progress,center,477.26f,558,460,12);
    R(replicator_craft,center,967.5f,519.5f,55,55);
    R(replicator_craft_icon,center,960,512,71,71);
    /* Recipe graph placement is dynamic; preserve a bounded interim slot
       until the graph itself is reproduced. This is not a source node position. */
    R(replicator_material,center,470,330,48,48);
#undef R
    *out=l; return true;
}


PeTrainingUIRect pe_training_ui_window_rect(const PeTrainingUILayout *l,int w) {
    if(!l)return (PeTrainingUIRect){0};
    switch(w){
    case PE_UI_WINDOW_STORAGE:return l->storage;
    case PE_UI_WINDOW_CHARACTER:return l->equipment;
    case PE_UI_WINDOW_INVENTORY:return l->inventory;
    case PE_UI_WINDOW_REPLICATOR:return l->replicator;
    case PE_UI_WINDOW_MISSION:return l->mission;
    default:return (PeTrainingUIRect){0};
    }
}
static void move_rect(PeTrainingUIRect *r,float x,float y){r->x+=x;r->y+=y;}
static void move_window(PeTrainingUILayout *l,int w,float x,float y) {
#define M(field) move_rect(&l->field,x,y)
#define A(field) for(size_t i=0;i<sizeof l->field/sizeof l->field[0];i++)move_rect(&l->field[i],x,y)
    switch(w){
    case PE_UI_WINDOW_STORAGE:M(storage);M(storage_close);M(storage_title);M(storage_page);A(storage_tabs);A(storage_slots);break;
    case PE_UI_WINDOW_INVENTORY:M(inventory);M(inventory_close);M(inventory_title);M(inventory_page);A(inventory_tabs);A(inventory_slots);break;
    case PE_UI_WINDOW_CHARACTER:M(equipment);M(equipment_close);M(equipment_preview);M(equipment_weapon);A(equipment_slots);break;
    case PE_UI_WINDOW_MISSION:M(mission);M(mission_close);break;
    case PE_UI_WINDOW_REPLICATOR:M(replicator);M(replicator_close);M(replicator_recipes);M(replicator_recipe);M(replicator_material);M(replicator_craft);M(replicator_output);M(replicator_progress);M(replicator_craft_icon);break;
    }
#undef A
#undef M
}
bool pe_training_ui_windows_layout(float width,float height,PeUIWindows *windows,PeTrainingUILayout *out){
    PeTrainingUILayout l;
    if(!windows||!out||!pe_training_ui_layout(width,height,&l))return false;
    for(int w=0;w<PE_UI_WINDOW_COUNT;w++)if(!isfinite(windows->x[w])||!isfinite(windows->y[w]))return false;
    for(int w=0;w<PE_UI_WINDOW_COUNT;w++){
        PeTrainingUIRect r=pe_training_ui_window_rect(&l,w);float z=l.scale;
        /* Include inventory/storage's decorative overhang in the clamp. */
        float margin=5*z,extra=(w==PE_UI_WINDOW_INVENTORY||w==PE_UI_WINDOW_STORAGE)?10*z:0;
        float x=fmaxf(margin,fminf(width-r.w-margin-extra,r.x+windows->x[w]*z));
        float y=fmaxf(margin,fminf(height-r.h-margin-extra,r.y+windows->y[w]*z));
        windows->x[w]=(x-r.x)/z;windows->y[w]=(y-r.y)/z;
        move_window(&l,w,x-r.x,y-r.y);
    }
    *out=l;return true;
}
int pe_training_ui_window_at(const PeTrainingUILayout *l,unsigned visible,float x,float y){
    if(!valid(l))return -1;
    for(int w=PE_UI_WINDOW_COUNT-1;w>=0;w--)if((visible&(1u<<w))&&pe_training_ui_contains(pe_training_ui_window_rect(l,w),x,y))return w;
    return -1;
}
bool pe_training_ui_window_drag(PeUIWindows *s,PeTrainingUILayout *l,int w,float x,float y,bool pressed,bool released,bool cancel){
    if(!s||!valid(l))return false;
    bool active=s->dragging!=0;
    if(cancel||!isfinite(x)||!isfinite(y)){s->dragging=0;return active;}
    if(s->dragging<0||s->dragging>PE_UI_WINDOW_COUNT){s->dragging=0;return false;}
    if(!active&&pressed&&w>=0&&w<PE_UI_WINDOW_COUNT){
        PeTrainingUIRect r=pe_training_ui_window_rect(l,w),close={0};
        switch(w){case PE_UI_WINDOW_STORAGE:close=l->storage_close;break;case PE_UI_WINDOW_CHARACTER:close=l->equipment_close;break;
        case PE_UI_WINDOW_INVENTORY:close=l->inventory_close;break;case PE_UI_WINDOW_REPLICATOR:close=l->replicator_close;break;case PE_UI_WINDOW_MISSION:close=l->mission_close;break;}
        r.h=50*l->scale;
        if(pe_training_ui_contains(r,x,y)&&!pe_training_ui_contains(close,x,y)){
            s->dragging=w+1;s->grab_x=(x-r.x)/l->scale;s->grab_y=(y-r.y)/l->scale;active=true;
        }
    }
    if(active){
        w=s->dragging-1;PeTrainingUIRect r=pe_training_ui_window_rect(l,w);
        s->x[w]+=(x-r.x)/l->scale-s->grab_x;s->y[w]+=(y-r.y)/l->scale-s->grab_y;
        pe_training_ui_windows_layout(l->width,l->height,s,l);
        if(released)s->dragging=0;
    }
    return active;
}
