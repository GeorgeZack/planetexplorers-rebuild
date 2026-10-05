#include "pe_training_ui_replicator.h"
#include <math.h>
#include <stdio.h>

typedef struct { const PeUiAssets *assets; float x,y,s; } Canvas;
static const Color ink={255,255,255,255}, dim={145,155,175,105}, title_color={159,197,234,255};
/* Original NGUI widget coordinates relative to CompoundWnd root. Pivot0
   top-left,3 middle-left,4 center,5 middle-right,7 bottom-center. */
static Rectangle bounds(Canvas c,float x,float y,float w,float h,int pivot) {
    float ax=0,ay=0;
    if(pivot==4||pivot==7)ax=w*.5f;
    if(pivot==5)ax=w;
    if(pivot==3||pivot==4||pivot==5)ay=h*.5f;
    if(pivot==7)ay=h;
    return (Rectangle){c.x+(x-ax)*c.s,c.y+(-y-ay)*c.s,w*c.s,h*c.s};
}
static void sprite(Canvas c,const char *name,float x,float y,float w,float h,int pivot,Color color) {
    pe_ui_sprite(c.assets,name,bounds(c,x,y,w,h,pivot),color,c.s);
}
static void text(Canvas c,const char *value,float x,float y,float size,int pivot,Color color,bool bold) {
    Font f=size>=30?pe_ui_font_heading(c.assets):(size<=15?pe_ui_font_small(c.assets,bold):pe_ui_font(c.assets,bold));float pixels=size*c.s;
    Vector2 extent=MeasureTextEx(f,value,pixels,0);
    float px=c.x+x*c.s,py=c.y-y*c.s;
    if(pivot==4)px-=extent.x*.5f;
    if(pivot==5)px-=extent.x;
    if(pivot==3||pivot==4||pivot==5)py-=extent.y*.5f;
    DrawTextEx(f,value,(Vector2){px,py},pixels,0,color);
}
static void item(Canvas c,const char *icon,float x,float y) {
    sprite(c,"itembox",x,y,48,48,4,ink);sprite(c,icon,x,y,48,48,4,ink);
}
static void count(Canvas c,unsigned n,float x,float y,Color color) {
    char value[32];snprintf(value,sizeof value,"%u",n);text(c,value,x,y,14,5,color,false);
}
void pe_training_ui_replicator_draw(const PeUiAssets *assets,const PeTrainingUILayout *l,const PeTraining *t,PeTrainingUiShader *effects,float seconds) {
    if(!assets||!l||!t||!isfinite(l->scale)||l->scale<=0||
       !isfinite(l->replicator.x)||!isfinite(l->replicator.y))return;
    Canvas c={assets,l->replicator.x+440*l->scale,l->replicator.y+240*l->scale,l->scale};
    /* Exact positions, dimensions, sprites and color tints from
       docs/audit/TRAINING_UI_LAYOUT.json / source CompoundWnd.prefab. */
    sprite(c,"bigbg",0,0,880,480,4,ink);
    sprite(c,"bigbg",0,0,880,480,4,(Color){0,33,255,77});
    pe_training_ui_shader_draw(effects,PE_UI_EFFECT_REPLICATOR,bounds(c,0,0,903,492,4),seconds,1,false);
    sprite(c,"close1",412.5f,213.5f,25,25,4,(Color){146,156,255,255});
    text(c,"REPLICATOR",-356,210,32,3,title_color,false);
    sprite(c,"insidebox2",-275,-44,200,354,4,ink);
    sprite(c,"insidebox2",-165,85,570,210,0,ink);
    sprite(c,"inputbox3",-375,150,200,26,3,dim);
    text(c,"ALL",-364,150,18,3,(Color){200,200,200,180},false);
    sprite(c,"inputbox1",-62,186,400,30,3,dim);
    sprite(c,"search",360,187,18,20,4,dim);
    sprite(c,"clear",385,188,18,20,4,dim);
    sprite(c,"inputbox1",122,138,576,50,4,dim);
    sprite(c,"leftbutton",-150,138,16,16,4,dim);
    sprite(c,"rightbutton",396,138,16,16,4,dim);
    static const char *categories[]={"Icons_all_1","Icons_Arms1","Icons_Coat1","Icons_tool","Icons_tower",
        "Icons_consumables","Icons_block","Icons_creat","Icons_furnishings","Icons_iso"};
    static const float category_y[]={135,102,69,36,7,-25,-57,-90,-123,-156};
    for(unsigned i=0;i<10;i++)sprite(c,categories[i],-395,category_y[i],32,32,4,i?dim:ink);
    /* Inert source scroll/history/track controls are visibly dimmed; there
       is only one supported formula and no fake filtering/quantity/cancel. */
    sprite(c,"pullOut_bg",-393,138,4,250,0,dim);
    text(c,"REPLICATION TREE:",-71.5f,98,18,4,ink,false);
    sprite(c,"func_redo_3",345,64.5f,28,19,4,dim);
    sprite(c,"func_undo_2",375,64.5f,28,19,4,dim);
    sprite(c,"checkbox_1",380,-110,18,18,4,dim);
    text(c,"Track",368,-110,18,5,dim,false);

    bool learned=t->gathering.formula1;
    if(learned) {
        /* First dynamic list row: source UIGrid(-276,106), ListItem icon
           offset(-70,+1), text(-35,+2), and row stride58. */
        sprite(c,"multplayer_hall_select",-320,107,142,50,3,(Color){160,205,255,210});
        item(c,"shovel_wood",-346,107);
        text(c,"Wood Shovel",-311,108,18,3,ink,false);
        sprite(c,"DividLine2",-274,78,194,4,4,ink);
        /* Actual one-child UITreeGrid result: width470 gives contentx251;
           scroll parent(-155,88.1922), contenty10, rowheight75. Root and
           child end at(96,23.1922) and(96,-51.8078), respectively. */
        const float gx=96,gy=23.192207f;
        sprite(c,"bloodlan_2",gx,gy-24,2,27,0,(Color){148,203,240,220});
        sprite(c,"bloodlan_2",gx,gy,54,54,4,(Color){103,185,255,130});
        item(c,"shovel_wood",gx,gy);item(c,"build_wood",gx,gy-75);
        text(c,"+ 1",gx+30,gy,14,3,ink,false);
        text(c,"- 10",gx+30,gy-75,14,3,t->gathering.wood>=10?ink:(Color){255,70,70,255},false);
        count(c,t->gathering.shovel,gx+20,gy-15,(Color){239,217,141,255});
        count(c,t->gathering.wood,gx+20,gy-90,(Color){239,217,141,255});
        item(c,"shovel_wood",-140,-165);
    }
    /* Source quantity widgets stay visible with fixed1; no +/- or min/max
       action is provided by this renderer or advertised as implemented. */
    text(c,"Number:",-43.5f,-149,18,4,learned?ink:dim,false);
    sprite(c,"inputbox1",-89,-176,80,26,3,dim);
    text(c,learned?"1":"0",-79,-176,18,3,learned?ink:dim,false);
    sprite(c,"jia_1",-101,-176,16,16,4,dim);
    sprite(c,"jia",5,-176,16,16,4,dim);
    text(c,"MIN",65.5f,-174,18,4,dim,false);
    text(c,"Max",150.3f,-174,18,4,dim,false);
    sprite(c,"jindulan_1",-162.74f,-204,460,12,3,ink);
    if(t->gathering.crafting) {
        float fraction=fminf(t->craft_elapsed_ticks/180.f,1.f);
        if(fraction>0)sprite(c,"jindulan_2",-162.74f,-204,460*fraction,12,3,ink);
        char remaining[48];snprintf(remaining,sizeof remaining,"%.1f s",3.f*(1-fraction));
        text(c,remaining,240,-174,18,4,ink,false);
    }
    /* Ask the authoritative action on a value-copy, so availability cannot
       disagree with its lesson, dialogue, health, formula or capacity gates. */
    PeTraining probe=*t;bool enabled=pe_training_start_shovel_craft(&probe);
    sprite(c,"Craft1",355.5f,-187.5f,71,71,4,enabled?(Color){255,255,255,230}:dim);
}
