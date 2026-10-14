#include "pe_ui_draw.h"
#include "pe_content.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
const Color PE_UI_INK={236,246,255,255},PE_UI_BLUE={112,220,255,255},
    PE_UI_GOLD={220,226,112,255},PE_UI_DIM={150,170,194,150};
Rectangle pe_ui_draw_rect(PeTrainingUIRect r){return(Rectangle){r.x,r.y,r.w,r.h};}
PeTrainingUIRect pe_ui_draw_map(const PeUiCanvas *c,float x,float y,float w,float h,PeTrainingUIAnchor a){return pe_training_ui_map(c->layout,(PeTrainingUIRect){x,y,w,h},a);}
Font pe_ui_draw_font(const PeUiCanvas *c,float size,bool bold){return size>=30?pe_ui_font_heading(c->assets):size<=15?pe_ui_font_small(c->assets,bold):pe_ui_font(c->assets,bold);}
void pe_ui_draw_sprite(const PeUiCanvas *c,const char *name,PeTrainingUIRect r,Color tint){
    if(!name||!*name)return;
    if(!pe_ui_sprite(c->assets,name,pe_ui_draw_rect(r),tint,c->layout->scale))DrawRectangleRec(pe_ui_draw_rect(r),(Color){36,62,105,210});
}
void pe_ui_draw_sprite_flip(const PeUiCanvas *c,const char *name,PeTrainingUIRect r,Color tint,bool horizontal,bool vertical){
    if(!name||!*name)return;
    if(!pe_ui_sprite_flip(c->assets,name,pe_ui_draw_rect(r),tint,horizontal,vertical))DrawRectangleRec(pe_ui_draw_rect(r),(Color){36,62,105,210});
}
void pe_ui_draw_effect(const PeUiCanvas *c,PeUiEffect fx,PeTrainingUIRect r){pe_training_ui_shader_draw(c->effects,fx,pe_ui_draw_rect(r),c->seconds,1,false);}
void pe_ui_draw_text(const PeUiCanvas *c,const char *s,float x,float y,float size,Color tint,bool bold){if(s)DrawTextEx(pe_ui_draw_font(c,size,bold),s,(Vector2){x,y},size*c->layout->scale,0,tint);}
void pe_ui_draw_center(const PeUiCanvas *c,const char *s,PeTrainingUIRect r,float size,Color tint,bool bold){
    if(!s)return;
    Vector2 m=MeasureTextEx(pe_ui_draw_font(c,size,bold),s,size*c->layout->scale,0);
    pe_ui_draw_text(c,s,r.x+(r.w-m.x)*.5f,r.y+(r.h-m.y)*.5f,size,tint,bold);
}
static float text_flow(const PeUiCanvas *c,const char *s,PeTrainingUIRect r,float size,Color tint,bool bold,float gap,bool draw){
    float px=size*c->layout->scale,step=(size+gap)*c->layout->scale,y=r.y;
    if(!s||r.w<=0||r.h<=0||px<=0||step<=0)return 0;
    Font font=pe_ui_draw_font(c,size,bold);char line[2048]={0};size_t used=0;
    while(*s&&y+px<=r.y+r.h+.01f){
        if(*s=='\n'){
            if(draw)pe_ui_draw_text(c,line,r.x,y,size,tint,bold);
            y+=step;used=0;line[0]=0;s++;continue;
        }
        if(*s==' '||*s=='\t'){s++;continue;}
        size_t n=strcspn(s," \t\n");
        size_t take=n<sizeof(line)-1?n:sizeof(line)-1;
        /* Clip oversized tokens without splitting their trailing UTF-8 codepoint. */
        if(take<n)while(take&&((unsigned char)s[take]&0xc0)==0x80)take--;
        char next[2048];bool fits=used+(used?1:0)+take<sizeof(next);
        if(fits){memcpy(next,line,used);size_t at=used;if(used)next[at++]=' ';memcpy(next+at,s,take);next[at+take]=0;}
        if(used&&(!fits||MeasureTextEx(font,next,px,0).x>r.w)){
            if(draw)pe_ui_draw_text(c,line,r.x,y,size,tint,bold);
            y+=step;used=0;line[0]=0;continue;
        }
        if(used)line[used++]=' ';
        memcpy(line+used,s,take);used+=take;line[used]=0;s+=n;
    }
    if(used&&y+px<=r.y+r.h+.01f){if(draw)pe_ui_draw_text(c,line,r.x,y,size,tint,bold);y+=step;}
    return fminf(r.h,fmaxf(0,y-r.y));
}
float pe_ui_draw_flow(const PeUiCanvas *c,const char *s,PeTrainingUIRect r,float size,Color tint,bool bold,float gap,bool draw){
    r.h=INFINITY;return text_flow(c,s,r,size,tint,bold,gap,draw);
}
float pe_ui_draw_wrap(const PeUiCanvas *c,const char *s,PeTrainingUIRect r,float size,Color tint,bool bold,float gap){
    if(!s||r.w<=0||r.h<=0)return 0;
    BeginScissorMode((int)floorf(r.x),(int)floorf(r.y),(int)ceilf(r.w),(int)ceilf(r.h));
    float used=text_flow(c,s,r,size,tint,bold,gap,true);
    EndScissorMode();return used;
}

void pe_ui_draw_item(const PeUiCanvas *c,int id,int count,PeTrainingUIRect r,bool box,Color tint){
    if(box)pe_ui_draw_sprite(c,"itembox",r,WHITE);
    const PeItemDef *d=pe_item_def(id);if(!d)return;
    pe_ui_draw_sprite(c,d->icon,r,tint);pe_ui_draw_sprite(c,d->icon_overlay1,r,tint);pe_ui_draw_sprite(c,d->icon_overlay2,r,tint);
    if(count>1){char value[24];snprintf(value,sizeof value,"%d",count);float z=c->layout->scale;
        Vector2 m=MeasureTextEx(pe_ui_draw_font(c,14,false),value,14*z,0);
        pe_ui_draw_text(c,value,r.x+r.w-m.x-2*z,r.y+r.h-15*z,14,PE_UI_GOLD,false);}
}
void pe_ui_draw_button(const PeUiCanvas *c,PeTrainingUIRect r,const char *s,bool enabled){
    bool hover=pe_training_ui_contains(r,c->mouse_x,c->mouse_y);
    pe_ui_draw_sprite(c,"menulist",r,enabled?(hover?PE_UI_BLUE:WHITE):PE_UI_DIM);
    pe_ui_draw_center(c,s,r,18,enabled?PE_UI_INK:PE_UI_DIM,false);
}
void pe_ui_draw_panel(const PeUiCanvas *c,PeTrainingUIRect r,const char *title,PeUiEffect fx){
    float z=c->layout->scale;bool large=fx==PE_UI_EFFECT_REPLICATOR||fx==PE_UI_EFFECT_MISSION;
    pe_ui_draw_sprite(c,large?"bigbg":"midbg",r,WHITE);pe_ui_draw_sprite(c,large?"bigbg":"midbg",r,(Color){0,33,255,77});
    pe_ui_draw_effect(c,fx,r);
    pe_ui_draw_sprite(c,"titilebg",(PeTrainingUIRect){r.x+15*z,r.y+32*z,r.w-65*z,14*z},WHITE);
    float x=fx==PE_UI_EFFECT_REPLICATOR?84:fx==PE_UI_EFFECT_MISSION?87:32;
    float y=fx==PE_UI_EFFECT_REPLICATOR?14:fx==PE_UI_EFFECT_MISSION?16.5f:20;
    pe_ui_draw_text(c,title,r.x+x*z,r.y+y*z,32,(Color){159,197,234,255},false);
}
void pe_ui_draw_inventory_frame(const PeUiCanvas *c,bool storage,int tab){
    const PeTrainingUILayout *l=c->layout;float z=l->scale;PeTrainingUIRect p=storage?l->storage:l->inventory;
    PeTrainingUIRect outer={p.x-4*z,p.y-(storage?4:3)*z,384*z,(storage?460:576)*z};
    pe_ui_draw_panel(c,outer,storage?"STORAGE":"INVENTORY",storage?PE_UI_EFFECT_STORAGE:PE_UI_EFFECT_INVENTORY);
    pe_ui_draw_sprite(c,"close1",storage?l->storage_close:l->inventory_close,(Color){165,185,255,255});
    const PeTrainingUIRect *tabs=storage?l->storage_tabs:l->inventory_tabs,*slots=storage?l->storage_slots:l->inventory_slots;
    pe_ui_draw_sprite(c,"insidebox2",(PeTrainingUIRect){p.x+13*z,tabs[0].y,p.w-26*z,35*z},WHITE);
    static const char *icons[]={"item","equipment","resource","armor","mission"};
    for(int i=0;i<(storage?4:5);i++){char name[40];snprintf(name,sizeof name,"%s%s",icons[i],i==tab?"_1":"");pe_ui_draw_sprite(c,name,(PeTrainingUIRect){tabs[i].x+(tabs[i].w-40*z)*.5f,tabs[i].y-2*z,40*z,40*z},WHITE);}
    pe_ui_draw_sprite(c,storage?"insidebox1":"insidebox2",(PeTrainingUIRect){p.x+13*z,slots[0].y-24*z,p.w-26*z,(storage?326:418)*z},WHITE);
}
void pe_ui_draw_pagination(const PeUiCanvas *c,PeTrainingUIRect r,int page,int pages){
    float z=c->layout->scale;const char *icons[]={"leftendbutton","leftbutton","rightbutton","rightendbutton"};float x[]={0,25,105,130};
    for(int i=0;i<4;i++)pe_ui_draw_sprite(c,icons[i],(PeTrainingUIRect){r.x+x[i]*z,r.y+3*z,16*z,16*z},(i<2?page>1:page<pages)?WHITE:PE_UI_DIM);
    char value[40];snprintf(value,sizeof value,"%d/%d",page,pages);pe_ui_draw_center(c,value,(PeTrainingUIRect){r.x+48*z,r.y,50*z,r.h},18,PE_UI_INK,true);
}
void pe_ui_draw_dialogue_frame(const PeUiCanvas *c,const char *speaker){
    const PeTrainingUILayout *l=c->layout;
    pe_ui_draw_sprite(c,"talkbg",l->dialogue,WHITE);pe_ui_draw_sprite(c,"talkbg",l->dialogue,(Color){0,33,255,77});pe_ui_draw_effect(c,PE_UI_EFFECT_DIALOGUE,l->dialogue);
    pe_ui_draw_sprite(c,"namebg",l->dialogue_name,WHITE);pe_ui_draw_center(c,speaker,l->dialogue_name,18,PE_UI_INK,true);
}
void pe_ui_draw_continue(const PeUiCanvas *c){
    const PeTrainingUILayout *l=c->layout;float z=l->scale;
    pe_ui_draw_text(c,"1. Continue",l->dialogue_next.x-105*z,l->dialogue_next.y+26*z,16,PE_UI_INK,false);
    pe_ui_draw_sprite(c,"btn_down",l->dialogue_next,pe_training_ui_contains(l->dialogue_next,c->mouse_x,c->mouse_y)?PE_UI_BLUE:WHITE);
}
