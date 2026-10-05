#include "pe_training_ui.h"
#include "pe_training_view.h"
#include "pe_training_text.h"
#include "pe_training_npc.h"
#include "../generated/pe_training_ui_text.h"
#include "pe_training_ui_replicator.h"
#include "pe_training_ui_mission.h"
#include <raymath.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const Color ink={236,246,255,255},blue={112,220,255,255},gold={220,226,112,255};
static Rectangle rect(PeTrainingUIRect r){return (Rectangle){r.x,r.y,r.w,r.h};}
static PeTrainingUIRect map(PeTrainingUi *u,float x,float y,float w,float h,PeTrainingUIAnchor a){return pe_training_ui_map(&u->layout,(PeTrainingUIRect){x,y,w,h},a);}
static void sprite(PeTrainingUi *u,const char *name,PeTrainingUIRect r,Color tint){
    if(!pe_ui_sprite(u->assets,name,rect(r),tint,u->layout.scale))DrawRectangleRec(rect(r),(Color){36,62,105,210});
}
static void effect(PeTrainingUi *u,PeUiEffect role,PeTrainingUIRect r){pe_training_ui_shader_draw(u->effects,role,rect(r),(float)u->seconds,1,false);}
static void text(PeTrainingUi *u,const char *s,float x,float y,float size,Color color,bool bold){
    DrawTextEx(size<=15?pe_ui_font_small(u->assets,bold):pe_ui_font(u->assets,bold),s,(Vector2){x,y},size*u->layout.scale,0,color);
}
static void center(PeTrainingUi *u,const char *s,PeTrainingUIRect r,float size,Color color,bool bold){
    Vector2 measure=MeasureTextEx(size<=15?pe_ui_font_small(u->assets,bold):pe_ui_font(u->assets,bold),s,size*u->layout.scale,0);
    text(u,s,r.x+(r.w-measure.x)*.5f,r.y+(r.h-measure.y)*.5f,size,color,bold);
}
static float wrap(PeTrainingUi *u,const char *s,PeTrainingUIRect r,float size,Color color,bool bold){
    if(!s)return 0;
    Font font=size<=15?pe_ui_font_small(u->assets,bold):pe_ui_font(u->assets,bold);float px=size*u->layout.scale,step=(size+2)*u->layout.scale,y=r.y;
    char line[1024]={0};size_t used=0;
    while(*s&&y+px<=r.y+r.h+.5f){
        const char *end=s;while(*end&&*end!=' '&&*end!='\n')end++;
        size_t n=(size_t)(end-s);if(n>sizeof line-2)n=sizeof line-2;
        char measured[2048];snprintf(measured,sizeof measured,"%s%.*s",line,(int)n,s);
        if(used&&(used+n+2>=sizeof line||MeasureTextEx(font,measured,px,0).x>r.w)){
            DrawTextEx(font,line,(Vector2){r.x,y},px,0,color);y+=step;used=0;
        }
        if(y+px>r.y+r.h+.5f)break;
        memcpy(line+used,s,n);used+=n;line[used]=0;s=end;
        if(*s=='\n'){DrawTextEx(font,line,(Vector2){r.x,y},px,0,color);y+=step;used=0;s++;}
        else if(*s==' '){if(used<sizeof line-2)line[used++]=' ';s++;}
        line[used]=0;
    }
    if(used&&y+px<=r.y+r.h+.5f){DrawTextEx(font,line,(Vector2){r.x,y},px,0,color);y+=step;}
    return y-r.y;
}
static const char *title(const PeTraining *t){const PeTrainingUiMissionText *m=pe_training_ui_mission_text(t->tutorial.mission_id);return m?m->title:"";}
bool pe_training_ui_load(PeTrainingUi *u,const char *dir,char *error,size_t cap){
    if(!u||!dir)return false;
    memset(u,0,sizeof *u);char path[4096],manifest[4096];
    int n=snprintf(path,sizeof path,"%s/ui",dir),m=snprintf(manifest,sizeof manifest,"%s/ui/atlas.peui",dir);
    if(n<0||n>=(int)sizeof path||m<0||m>=(int)sizeof manifest)return false;
    if(FileExists(manifest)&&!(u->assets=pe_ui_assets_load(path,error,cap)))return false;
    if(u->assets&&!(u->effects=pe_training_ui_shader_load(path,error,cap))){pe_training_ui_unload(u);return false;}
    u->preview=LoadRenderTexture(620,960);
    if(!u->preview.id){pe_training_ui_unload(u);return false;}
    SetTextureFilter(u->preview.texture,TEXTURE_FILTER_BILINEAR);
    n=snprintf(path,sizeof path,"%s/ui/training-minimap.png",dir);
    if(n>0&&n<(int)sizeof path&&FileExists(path)){
        u->minimap=LoadTexture(path);if(!u->minimap.id){pe_training_ui_unload(u);return false;}
        SetTextureFilter(u->minimap,TEXTURE_FILTER_BILINEAR);
        if(!pe_training_ui_minimap_load(&u->map,error,cap)){pe_training_ui_unload(u);return false;}
    }
    pe_training_ui_resize(u);return true;
}
void pe_training_ui_unload(PeTrainingUi *u){if(!u)return;if(u->preview.id)UnloadRenderTexture(u->preview);pe_training_ui_shader_free(u->effects);pe_training_ui_minimap_unload(&u->map);pe_ui_assets_free(u->assets);if(u->minimap.id)UnloadTexture(u->minimap);memset(u,0,sizeof *u);}
void pe_training_ui_resize(PeTrainingUi *u){if(u)pe_training_ui_windows_layout((float)GetScreenWidth(),(float)GetScreenHeight(),&u->windows,&u->layout);}
bool pe_training_ui_pointer_over(const PeTrainingUi *u,Vector2 p){
    return u&&(pe_training_ui_contains(u->layout.hud,p.x,p.y)||pe_training_ui_contains(u->layout.minimap,p.x,p.y)||pe_training_ui_contains(u->layout.mission_tracker,p.x,p.y)||pe_training_ui_contains(u->layout.menu_button,p.x,p.y));
}
static void panel(PeTrainingUi *u,PeTrainingUIRect r,const char *label){
    sprite(u,"midbg",r,WHITE);sprite(u,"midbg",r,(Color){0,33,255,77});
    float z=u->layout.scale;
    sprite(u,"titilebg",(PeTrainingUIRect){r.x+15*z,r.y+32*z,r.w-65*z,14*z},WHITE);
    /* UISwitchFont.MakePixelPerfect resets the authored 37px title to32. */
    DrawTextEx(pe_ui_font_heading(u->assets),label,(Vector2){r.x+32*z,r.y+20*z},32*z,0,(Color){159,197,234,255});
}
static void slot(PeTrainingUi *u,PeTrainingUIRect r,const char *icon,unsigned count,bool equipped){
    sprite(u,"itembox",r,WHITE);
    if(icon){sprite(u,icon,r,WHITE);if(count>1)text(u,TextFormat("%u",count),r.x+r.w-20*u->layout.scale,r.y+r.h-15*u->layout.scale,14,gold,false);}
    if(equipped)sprite(u,"itemhand_get",r,blue);
    Vector2 mouse=GetMousePosition();if(pe_training_ui_contains(r,mouse.x,mouse.y))DrawRectangleLinesEx(rect(r),u->layout.scale,blue);
}
static void pagination(PeTrainingUi *u,PeTrainingUIRect r,const char *pages){
    float z=u->layout.scale;
    const char *icons[]={"leftendbutton","leftbutton","rightbutton","rightendbutton"};float x[]={0,25,105,130};
    for(unsigned i=0;i<4;i++)sprite(u,icons[i],(PeTrainingUIRect){r.x+x[i]*z,r.y+3*z,16*z,16*z},(Color){200,220,240,180});
    center(u,pages,(PeTrainingUIRect){r.x+48*z,r.y,50*z,r.h},18,ink,true);
}
static void inventory(PeTrainingUi *u,const PeTraining *t,int tab,bool storage){
    PeTrainingUILayout *l=&u->layout;float z=l->scale;PeTrainingUIRect p=storage?l->storage:l->inventory;
    PeTrainingUIRect outer={p.x-4*z,p.y-(storage?4:3)*z,384*z,(storage?460:576)*z};
    sprite(u,"midbg",outer,WHITE);sprite(u,"midbg",outer,(Color){0,33,255,77});
    effect(u,storage?PE_UI_EFFECT_STORAGE:PE_UI_EFFECT_INVENTORY,(PeTrainingUIRect){outer.x,outer.y,385*z,(storage?460:578)*z});
    sprite(u,"titilebg",(PeTrainingUIRect){p.x+12*z,p.y+(storage?24:34)*z,314*z,14*z},WHITE);
    DrawTextEx(pe_ui_font_heading(u->assets),storage?"STORAGE":"INVENTORY",(Vector2){p.x+28*z,p.y+(storage?15:17)*z},32*z,0,(Color){159,197,234,255});
    sprite(u,"close1",storage?l->storage_close:l->inventory_close,(Color){165,185,255,255});
    PeTrainingUIRect *tabs=storage?l->storage_tabs:l->inventory_tabs;
    sprite(u,"insidebox2",(PeTrainingUIRect){p.x+13*z,tabs[0].y,p.w-26*z,35*z},WHITE);
    static const char *icons[]={"item","equipment","resource","armor","mission"};
    for(unsigned i=0;i<(storage?4u:5u);i++) {
        char selected[64];snprintf(selected,sizeof selected,"%s%s",icons[i],i==(storage?0u:(unsigned)tab)?"_1":"");
        sprite(u,selected,(PeTrainingUIRect){tabs[i].x+(tabs[i].w-40*z)*.5f,tabs[i].y-2*z,40*z,40*z},WHITE);
    }
    PeTrainingUIRect *slots=storage?l->storage_slots:l->inventory_slots;
    unsigned total=storage?PE_UI_STORAGE_SLOTS:PE_UI_INVENTORY_SLOTS;
    PeTrainingUIRect content={p.x+13*z,slots[0].y-24*z,p.w-26*z,storage?326*z:418*z};sprite(u,storage?"insidebox1":"insidebox2",content,WHITE);
    for(unsigned i=0;i<total;i++)slot(u,slots[i],NULL,0,false);
    if(storage)effect(u,PE_UI_EFFECT_STORAGE_SCAN,(PeTrainingUIRect){p.x-1.5f*z,p.y+98*z,378*z,326*z});
    if(storage){if(!t->kit_transferred)slot(u,slots[0],"drug_hot01",5,false);}
    else if(tab==0){
        if(t->juice_count)slot(u,slots[0],"drug_hot01",(unsigned)t->juice_count,false);
        if(t->gathering.script){slot(u,slots[1],"shovel_wood",1,false);sprite(u,"leftdown_script",slots[1],WHITE);}
    }else if(tab==1){
        if(t->knife_owned&&!t->knife_equipped)slot(u,slots[0],"ax_knife",1,false);
        if(t->gathering.axe&&!t->axe_equipped)slot(u,slots[1],"safety_axe",1,false);
        for(unsigned i=0;i<2;i++)if(t->shovels[i].instance_id&&t->equipped_shovel!=(int)i)slot(u,slots[2+i],"shovel_wood",1,false);
    }else if(tab==2){
        if(t->gathering.wood)slot(u,slots[0],"build_wood",t->gathering.wood,false);
        if(t->gathering.dirt)slot(u,slots[1],"build_soil",t->gathering.dirt,false);
    }
    pagination(u,storage?l->storage_page:l->inventory_page,storage?"1/1":"1/10");
    if(!storage){
        const char *tools[]={"icohe","icocai","icodelete"};for(unsigned i=0;i<3;i++)sprite(u,tools[i],(PeTrainingUIRect){p.x+(267+34*i)*z,p.y+p.h-46*z,25*z,27*z},(Color){210,232,255,150});
    }
}
static void hud(PeTrainingUi *u,const PeTraining *t,PeTrainingUiState state){
    PeTrainingUILayout *l=&u->layout;float z=l->scale;
    sprite(u,"shortbar1bg",map(u,217.5f,644,845,54,PE_UI_BOTTOM_CENTER),WHITE);
    sprite(u,"shortbar1",map(u,217.5f,611,845,110,PE_UI_BOTTOM_CENTER),WHITE);
    sprite(u,"shortbar1",map(u,217.5f,611,845,110,PE_UI_BOTTOM_CENTER),(Color){0,33,255,77});
    effect(u,PE_UI_EFFECT_FRAME,map(u,218,611,844,110,PE_UI_BOTTOM_CENTER));
    effect(u,PE_UI_EFFECT_HUD_SCAN,map(u,225,646,830,50,PE_UI_BOTTOM_CENTER));
    sprite(u,"bloodlan_6bg",map(u,558,648,164,30,PE_UI_BOTTOM_CENTER),WHITE);
    PeTrainingUIRect hp=map(u,558,648,164,30,PE_UI_BOTTOM_CENTER);
    BeginScissorMode((int)hp.x,(int)hp.y,(int)ceilf(hp.w*Clamp(t->hp/t->max_hp,0,1)),(int)ceilf(hp.h));sprite(u,"bloodlan_6",hp,WHITE);effect(u,PE_UI_EFFECT_HEALTH,hp);EndScissorMode();
    PeTrainingUIRect stamina=map(u,571,678.5f,138,9,PE_UI_BOTTOM_CENTER);sprite(u,"bloodlan_7bg",stamina,WHITE);
    BeginScissorMode((int)stamina.x,(int)stamina.y,(int)ceilf(stamina.w*Clamp(t->stamina/100,0,1)),(int)ceilf(stamina.h));sprite(u,"bloodlan_7_1",stamina,(Color){65,131,185,255});EndScissorMode();
    sprite(u,"face3",map(u,535,620,12,13,PE_UI_BOTTOM_CENTER),WHITE);
    sprite(u,"tili",map(u,734,620,12,13,PE_UI_BOTTOM_CENTER),WHITE);
    sprite(u,"bloodlan_11bg",map(u,551,626,64,5,PE_UI_BOTTOM_CENTER),WHITE);
    sprite(u,"bloodlan_12bg",map(u,666,625,64,5,PE_UI_BOTTOM_CENTER),WHITE);
    sprite(u,"bloodlan_8bg",map(u,426,704,87,4,PE_UI_BOTTOM_CENTER),WHITE);
    sprite(u,"bloodlan_9bg",map(u,767,704,87,4,PE_UI_BOTTOM_CENTER),WHITE);
    sprite(u,"arr3",map(u,535,695,20,20,PE_UI_BOTTOM_CENTER),WHITE);
    sprite(u,"arr5",map(u,723,695,20,20,PE_UI_BOTTOM_CENTER),WHITE);
    for(unsigned i=0;i<PE_UI_QUICKSLOTS;i++){
        PeTrainingUIRect r=l->quickslots[i];
        text(u,TextFormat("%u",i+1),r.x+r.w*.4f,r.y+18*z,15,(Color){205,211,214,75},true);
        if(i==0&&t->quickslot_item_id==916){
            sprite(u,"drug_hot01",r,t->juice_count?WHITE:GRAY);
            text(u,TextFormat("%d",t->juice_count),r.x+r.w-18*z,r.y+r.h-14*z,13,gold,false);
            if(t->juice_cooldown>0){DrawRectangleRec(rect(r),(Color){32,55,110,130});center(u,TextFormat("%.0f",ceilf(t->juice_cooldown)),r,18,ink,true);}
        }
    }
    sprite(u,"icons:itemhand",l->hand_slot,WHITE);
    const char *weapon=t->knife_equipped?"ax_knife":(t->axe_equipped?"safety_axe":(t->equipped_shovel>=0?"shovel_wood":NULL));
    if(weapon)sprite(u,weapon,l->hand_slot,WHITE);
    sprite(u,"icons:itemhand_get",l->hand_slot,WHITE);
    sprite(u,"buttonb",l->build_button,t->build_mode?blue:WHITE);
    text(u,"1",map(u,250,678,0,0,PE_UI_BOTTOM_CENTER).x,map(u,250,678,0,0,PE_UI_BOTTOM_CENTER).y,14,gold,false);
    sprite(u,"Triangle_btn_Up_on",map(u,233,676,16,16,PE_UI_BOTTOM_CENTER),(Color){200,220,240,180});
    sprite(u,"Triangle_btn_bottom_on",map(u,261,676,16,16,PE_UI_BOTTOM_CENTER),(Color){200,220,240,180});
    (void)state;
}
static void tracker(PeTrainingUi *u,const PeTraining *t,PeTrainingUiState state){
    const PeTrainingUiMissionText *mission=pe_training_ui_mission_text(t->tutorial.mission_id);
    if(!mission||!*mission->title||(t->tutorial.mission_id==739&&t->dialogue_index<2))return;
    PeTrainingUIRect r=u->layout.mission_tracker;float z=u->layout.scale;
    text(u,mission->title,r.x+17*z,r.y-7*z,18,mission->mission_type==1?(Color){155,61,244,255}:(Color){55,203,244,255},true);
    float y=r.y+25*z;
    for(size_t i=0;i<mission->objective_count;i++){
        const PeTrainingUiObjectiveText *o=pe_training_ui_mission_objective(t->tutorial.mission_id,i);
        bool done=pe_tutorial_objective_done(&t->tutorial,o->target_id);
        unsigned count=done?1u:0u;
        if(o->kind==1)count=t->combat_kill_entity_id?1u:0u;
        else if(o->kind==2)count=o->item_id==916?(unsigned)(t->juice_count>0?t->juice_count:0):pe_training_gathering_count(&t->gathering,o->item_id);
        char label[1024];
        if(o->max_count>0)snprintf(label,sizeof label,"%s %u/%u",o->text,count,o->max_count);
        else snprintf(label,sizeof label,"%s",o->text);
        /* Runtime MissionTrackNode uses UIFont18_talk, width233, and
           variable-height objective text, including three-line goals. */
        y+=wrap(u,label,(PeTrainingUIRect){r.x+18*z,y,233*z,fmaxf(0,r.y+r.h-y)},18,done?(Color){180,180,180,255}:WHITE,true)+5*z;
    }
    if(t->tutorial.mission_id==PE_TUTORIAL_RECEIVERS)text(u,TextFormat("%u / 50",state.receivers_blocked),r.x+12*z,y,15,ink,false);
}
static void dialogue(PeTrainingUi *u,const PeTraining *t){
    int id=pe_training_dialogue(t);if(!id)return;
    PeTrainingUILayout *l=&u->layout;const PeTrainingDialogue *entry=pe_training_dialogue_entry(id);
    bool tips=entry&&entry->speaker_npc_id==-9995,player=entry&&entry->speaker_npc_id==-9999;
    PeTrainingUIRect bg=map(u,215,465,850,150,PE_UI_BOTTOM_CENTER);
    sprite(u,"talkbg",bg,WHITE);sprite(u,"talkbg",bg,(Color){0,33,255,77});effect(u,PE_UI_EFFECT_DIALOGUE,bg);
    sprite(u,tips||player?"npc_big_Unknown":"npc_big_Andy",map(u,230,453,130,130,PE_UI_BOTTOM_CENTER),WHITE);
    sprite(u,"namebg",l->dialogue_name,WHITE);center(u,tips?"Tips":(player?"You":"Andy"),l->dialogue_name,18,ink,true);
    wrap(u,pe_training_dialogue_text(id),l->dialogue_text,18,ink,true);
    sprite(u,"btn_down",l->dialogue_next,CheckCollisionPointRec(GetMousePosition(),rect(l->dialogue_next))?blue:(Color){220,220,220,255});
}
static void character(PeTrainingUi *u,const PeTraining *t){
    PeTrainingUILayout *l=&u->layout;float z=l->scale;PeTrainingUIRect r=l->equipment;
    /* PlayerInfoWnd's active 620x602 panel, in source root-local coordinates.
       MakePixelPerfect restores the original UIFont sizes: title32, rows18. */
    const float cx=r.x+310*z,cy=r.y+301*z;
    sprite(u,"midbg",r,WHITE);sprite(u,"midbg",r,(Color){0,33,255,77});
    effect(u,PE_UI_EFFECT_CHARACTER,r);
    sprite(u,"titilebg",(PeTrainingUIRect){cx-295*z,cy-261*z,266*z,14*z},WHITE);
    DrawTextEx(pe_ui_font_heading(u->assets),"CHARACTER",(Vector2){cx-289*z,cy-280.5f*z},32*z,0,(Color){159,197,234,255});
    sprite(u,"close1",l->equipment_close,(Color){146,156,255,255});
    static const float section_y[]={240,196,-60,-136};
    static const float section_h[]={40,252,72,143};
    for(unsigned i=0;i<4;i++)sprite(u,"insidebox2",(PeTrainingUIRect){cx-295.5f*z,cy-section_y[i]*z,278*z,section_h[i]*z},WHITE);
    PeTrainingUIRect preview=l->equipment_preview;
    sprite(u,"insidebox2",(PeTrainingUIRect){cx-13.7f*z,cy-240*z,310*z,520*z},WHITE);
    sprite(u,"bloodlan_2",(PeTrainingUIRect){cx-13.7f*z,cy-202*z,310*z,2*z},(Color){255,255,255,48});
    if(u->preview.texture.id)DrawTexturePro(u->preview.texture,(Rectangle){0,0,(float)u->preview.texture.width,-(float)u->preview.texture.height},rect(preview),(Vector2){0},0,WHITE);
    static const char *masks[]={"Icons_helmet","Icons_coat","Icons_gloves","Icons_battery","Icons_arms","Icons_back","Icons_pants","Icons_shoes","Icons_glass","Icons_dun"};
    for(unsigned i=0;i<10;i++){
        slot(u,l->equipment_slots[i],NULL,0,false);
        sprite(u,masks[i],l->equipment_slots[i],(Color){175,200,220,110});
    }
    const char *weapon=t->knife_equipped?"ax_knife":(t->axe_equipped?"safety_axe":(t->equipped_shovel>=0?"shovel_wood":NULL));
    if(weapon)slot(u,l->equipment_weapon,weapon,1,false);
    sprite(u,"choose",(PeTrainingUIRect){cx-7*z,cy-240*z,104*z,32*z},WHITE);
    center(u,"Equipment",(PeTrainingUIRect){cx-6.5f*z,cy-240*z,104*z,32*z},18,WHITE,true);
    center(u,"Armor",(PeTrainingUIRect){cx+105.5f*z,cy-238*z,84*z,28*z},18,(Color){255,255,255,110},true);
    text(u,"Tutorial",cx-285*z,cy-229*z,18,WHITE,true);
    sprite(u,"female_icon",(PeTrainingUIRect){cx-123*z,cy-234*z,28*z,28*z},(Color){255,99,154,255});
    static const char *labels[]={"Health","Stamina","Hunger","Comfort","Oxygen","E-Shield","Energy","Attack","Defense"};
    static const float row_y[]={176,142,108,74,40,6,-28,-78,-112};
    for(unsigned i=0;i<9;i++){
        float top=cy-(row_y[i]+9)*z;
        text(u,labels[i],cx-285*z,top,18,WHITE,true);
        char amount[64];
        if(i==0)snprintf(amount,sizeof amount,"%.0f/%.0f",t->hp,t->max_hp);
        else if(i==1)snprintf(amount,sizeof amount,"%.0f/100",t->stamina);
        else snprintf(amount,sizeof amount,"--");
        float width=MeasureTextEx(pe_ui_font(u->assets,true),amount,18*z,0).x;
        text(u,amount,cx-95*z-width,top,18,i<2?(Color){255,224,0,255}:(Color){159,180,204,150},true);
        if(i<7){
            PeTrainingUIRect bar={cx-285*z,cy-(row_y[i]-13)*z,188*z,2*z};
            sprite(u,"bloodlan_2",bar,(Color){0,0,0,128});
            /* Unsupported attributes have no invented values or filled bars. */
            float fraction=i==0?(t->max_hp>0?t->hp/t->max_hp:0):(i==1?t->stamina/100:0);
            bar.w*=fminf(1,fmaxf(0,fraction));
            if(bar.w>0)sprite(u,"bloodlan_2",bar,(Color){64,192,255,255});
        }
    }
    center(u,"Skill",(PeTrainingUIRect){cx-267.5f*z,cy+143*z,200*z,18*z},18,(Color){255,255,255,110},true);
    Vector2 mouse=GetMousePosition();
    if(weapon&&pe_training_ui_contains(l->equipment_weapon,mouse.x,mouse.y)){
        PeTrainingUIRect tip={mouse.x+15*z,mouse.y+20*z,260*z,34*z};DrawRectangleRec(rect(tip),(Color){18,38,64,245});center(u,"Right mouse click to unequip",tip,14,ink,false);
    }
}
static void tooltip(PeTrainingUi *u,const PeTraining *t,int tab){
    if(pe_training_dialogue(t)||u->windows.dragging||t->replicator_open)return;
    Vector2 m=GetMousePosition();
    unsigned visible=(t->kit_open?(1u<<PE_UI_WINDOW_STORAGE)|(1u<<PE_UI_WINDOW_INVENTORY):0)|
        (t->inventory_open?1u<<PE_UI_WINDOW_INVENTORY:0)|(t->character_open?1u<<PE_UI_WINDOW_CHARACTER:0);
    int top=pe_training_ui_window_at(&u->layout,visible,m.x,m.y);
    if(top!=PE_UI_WINDOW_STORAGE&&top!=PE_UI_WINDOW_INVENTORY)return;
    int index=pe_training_ui_hit(top==PE_UI_WINDOW_STORAGE?u->layout.storage_slots:u->layout.inventory_slots,
        top==PE_UI_WINDOW_STORAGE?PE_UI_STORAGE_SLOTS:PE_UI_INVENTORY_SLOTS,m.x,m.y);
    const char *line=NULL;
    if(top==PE_UI_WINDOW_STORAGE&&index==0&&!t->kit_transferred)line="Herbal Juice\nRight mouse click to pick up";
    else if(top==PE_UI_WINDOW_INVENTORY&&tab==0){if(index==0&&t->juice_count)line="Herbal Juice\nDrag to quick bar 1";if(index==1&&t->gathering.script)line="Wood Shovel Script\nRight mouse click to learn";}
    else if(top==PE_UI_WINDOW_INVENTORY&&tab==1){if(index==0&&t->knife_owned&&!t->knife_equipped)line="Knife\nRight mouse click to equip";if(index==1&&t->gathering.axe&&!t->axe_equipped)line="Safety Axe\nRight mouse click to equip";if(index>=2&&index<=3&&t->shovels[index-2].instance_id&&t->equipped_shovel!=index-2)line=TextFormat("Wood Shovel\nDurability %.0f / 3553\nRight mouse click to equip",t->shovels[index-2].durability);}
    if(line){float z=u->layout.scale;PeTrainingUIRect box={fminf(m.x+18*z,u->layout.width-300*z),fminf(m.y+20*z,u->layout.height-90*z),290*z,78*z};DrawRectangleRec(rect(box),(Color){18,38,64,235});DrawRectangleLinesEx(rect(box),z,blue);box.x+=10*z;box.y+=8*z;box.w-=20*z;wrap(u,line,box,14,ink,false);}
}
void pe_training_ui_draw(PeTrainingUi *u,const PeTraining *t,PeTrainingUiState state){
    if(!u||!t)return;
    pe_training_ui_resize(u);
    hud(u,t,state);tracker(u,t,state);
    if(u->minimap.id&&u->assets)pe_training_ui_minimap_draw(&u->map,u->assets,u->minimap,&u->layout,t->position,state.heading,u->seconds);
    sprite(u,"map_02",map(u,1089,0,158,200,PE_UI_TOP_RIGHT),(Color){205,245,255,120});
    sprite(u,"map_01",map(u,1069,0,20,200,PE_UI_TOP_RIGHT),WHITE);
    sprite(u,"map_01",map(u,1069,0,20,200,PE_UI_TOP_RIGHT),(Color){0,33,255,120});
    sprite(u,"map_03",map(u,1246,0,33,200,PE_UI_TOP_RIGHT),WHITE);
    sprite(u,"map_03",map(u,1246,0,33,200,PE_UI_TOP_RIGHT),(Color){0,33,255,120});
    effect(u,PE_UI_EFFECT_MAP_LEFT,map(u,1069,0,20,208,PE_UI_TOP_RIGHT));
    effect(u,PE_UI_EFFECT_MAP_RIGHT,map(u,1246,0,34,208,PE_UI_TOP_RIGHT));
    effect(u,PE_UI_EFFECT_MAP_SCAN,map(u,1094,0,148,208,PE_UI_TOP_RIGHT));
    sprite(u,"mapeye",map(u,1253,4,22,22,PE_UI_TOP_RIGHT),WHITE);
    sprite(u,"maplight1",map(u,1253,29,22,22,PE_UI_TOP_RIGHT),WHITE);
    sprite(u,"NpcTalkHistoryBtn",map(u,1253,148,22,22,PE_UI_TOP_RIGHT),WHITE);
    sprite(u,"Icon_mission",map(u,1254,173,20,26,PE_UI_TOP_RIGHT),WHITE);
    if(t->kit_open)inventory(u,t,0,true);
    if(t->character_open)character(u,t);
    if(t->inventory_open||t->kit_open)inventory(u,t,state.inventory_tab,false);
    if(t->replicator_open)pe_training_ui_replicator_draw(u->assets,&u->layout,t,u->effects,(float)u->seconds);
    if(state.log)pe_training_ui_mission_draw(u->assets,&u->layout,t,u->effects,(float)u->seconds);
    sprite(u,"menu1",u->layout.menu_button,WHITE);
    if(state.menu){
        static const char *items[]={"Inventory  [I]","Character  [C]","Replicator  [Y]","Mission  [Q]","Pause"};
        for(unsigned i=0;i<PE_UI_MENU_ITEMS;i++){PeTrainingUIRect r=u->layout.menu_items[i];sprite(u,"menulist",r,WHITE);center(u,items[i],r,16,ink,false);}
    }
    if(u->previous_mission!=(int)t->tutorial.mission_id){u->completed_mission=u->previous_mission;u->previous_mission=t->tutorial.mission_id;u->notice_until=u->completed_mission?u->seconds+5:0;}
    if(u->seconds<u->notice_until){
        const PeTrainingUiMissionText *old=pe_training_ui_mission_text(u->completed_mission);float z=u->layout.scale;
        if(old&&*old->title){text(u,"Mission Completed:",42*z,7*z,18,blue,false);float x=42*z+MeasureTextEx(pe_ui_font(u->assets,false),"Mission Completed: ",18*z,0).x;text(u,old->title,x,7*z,18,gold,false);}
        if(*title(t)){text(u,"New Mission:",42*z,31*z,18,blue,false);float x=42*z+MeasureTextEx(pe_ui_font(u->assets,false),"New Mission: ",18*z,0).x;text(u,title(t),x,31*z,18,gold,false);}
    }
    dialogue(u,t);tooltip(u,t,state.inventory_tab);
    if(state.dragging){Vector2 m=GetMousePosition();sprite(u,"drug_hot01",(PeTrainingUIRect){m.x-24*u->layout.scale,m.y-24*u->layout.scale,48*u->layout.scale,48*u->layout.scale},WHITE);}
    if(state.paused){
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,.48f));
        PeTrainingUIRect r=map(u,425,220,430,280,PE_UI_CENTER);panel(u,r,"PAUSED");
        effect(u,PE_UI_EFFECT_INVENTORY,r);
        static const char *labels[]={"Resume  [Esc]","Main Menu  [M]","Quit  [Q]"};
        Vector2 pointer=GetMousePosition();
        for(int i=0;i<3;i++) {
            PeTrainingUIRect button=pe_training_pause_button(&u->layout,i);
            bool hover=pe_training_ui_contains(button,pointer.x,pointer.y);
            sprite(u,"menulist",button,hover?blue:WHITE);
            center(u,labels[i],button,18,ink,false);
        }
    }
    if(u->assets&&state.cursor_visible&&!state.dragging){Vector2 m=GetMousePosition();sprite(u,"mouse",(PeTrainingUIRect){m.x,m.y,40*u->layout.scale,40*u->layout.scale},WHITE);}
}
