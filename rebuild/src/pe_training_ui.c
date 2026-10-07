#include "pe_training_ui.h"
#include "pe_ui_draw.h"
#include "pe_ui_hud.h"
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

#define ink PE_UI_INK
#define blue PE_UI_BLUE
#define gold PE_UI_GOLD
static PeUiCanvas canvas(const PeTrainingUi *u){Vector2 m=GetMousePosition();return(PeUiCanvas){u->assets,u->effects,&u->layout,(float)u->seconds,m.x,m.y};}
static Rectangle rect(PeTrainingUIRect r){return (Rectangle){r.x,r.y,r.w,r.h};}
static PeTrainingUIRect map(PeTrainingUi *u,float x,float y,float w,float h,PeTrainingUIAnchor a){return pe_training_ui_map(&u->layout,(PeTrainingUIRect){x,y,w,h},a);}
static void sprite(PeTrainingUi *u,const char *n,PeTrainingUIRect r,Color tint){PeUiCanvas c=canvas(u);pe_ui_draw_sprite(&c,n,r,tint);}
static void effect(PeTrainingUi *u,PeUiEffect fx,PeTrainingUIRect r){PeUiCanvas c=canvas(u);pe_ui_draw_effect(&c,fx,r);}
static void text(PeTrainingUi *u,const char *v,float x,float y,float size,Color tint,bool bold){PeUiCanvas c=canvas(u);pe_ui_draw_text(&c,v,x,y,size,tint,bold);}
static void center(PeTrainingUi *u,const char *v,PeTrainingUIRect r,float size,Color tint,bool bold){PeUiCanvas c=canvas(u);pe_ui_draw_center(&c,v,r,size,tint,bold);}
static float wrap(PeTrainingUi *u,const char *v,PeTrainingUIRect r,float size,Color tint,bool bold){PeUiCanvas c=canvas(u);return pe_ui_draw_wrap(&c,v,r,size,tint,bold,2);}
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
static void panel(PeTrainingUi *u,PeTrainingUIRect r,const char *label){PeUiCanvas c=canvas(u);pe_ui_draw_panel(&c,r,label,PE_UI_EFFECT_INVENTORY);}
static void slot(PeTrainingUi *u,PeTrainingUIRect r,const char *icon,unsigned count,bool equipped){
    sprite(u,"itembox",r,WHITE);
    if(icon){sprite(u,icon,r,WHITE);if(count>1)text(u,TextFormat("%u",count),r.x+r.w-20*u->layout.scale,r.y+r.h-15*u->layout.scale,14,gold,false);}
    if(equipped)sprite(u,"itemhand_get",r,blue);
    Vector2 mouse=GetMousePosition();if(pe_training_ui_contains(r,mouse.x,mouse.y))DrawRectangleLinesEx(rect(r),u->layout.scale,blue);
}
static void inventory(PeTrainingUi *u,const PeTraining *t,int tab,bool storage){
    PeTrainingUILayout *l=&u->layout;float z=l->scale;PeTrainingUIRect p=storage?l->storage:l->inventory;
    PeUiCanvas c=canvas(u);pe_ui_draw_inventory_frame(&c,storage,storage?0:tab);
    PeTrainingUIRect *slots=storage?l->storage_slots:l->inventory_slots;
    unsigned total=storage?PE_UI_STORAGE_SLOTS:PE_UI_INVENTORY_SLOTS;
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
    pe_ui_draw_pagination(&c,storage?l->storage_page:l->inventory_page,1,1);
    if(!storage){
        const char *tools[]={"icohe","icocai","icodelete"};for(unsigned i=0;i<3;i++)sprite(u,tools[i],(PeTrainingUIRect){p.x+(267+34*i)*z,p.y+p.h-46*z,25*z,27*z},(Color){210,232,255,150});
    }
}
static void hud(PeTrainingUi *u,const PeTraining *t,PeTrainingUiState state){
    PeUiCanvas c=canvas(u);PeUiHudView v={.health=t->hp,.health_max=t->max_hp,.stamina=t->stamina,.stamina_max=100,.building=t->build_mode};
    if(t->quickslot_item_id==916)v.slots[0]=(PeUiQuickslotView){916,t->juice_count,t->juice_cooldown};
    v.hand_item_id=t->knife_equipped?33:t->axe_equipped?1527:t->equipped_shovel>=0?12:0;
    pe_ui_hud_draw(&c,&v);(void)state;
}
static void tracker(PeTrainingUi *u,const PeTraining *t,PeTrainingUiState state){
    const PeTrainingUiMissionText *mission=pe_training_ui_mission_text(t->tutorial.mission_id);
    if(!mission||!*mission->title||(t->tutorial.mission_id==739&&t->dialogue_index<2))return;
    size_t count=mission->objective_count+(t->tutorial.mission_id==PE_TUTORIAL_RECEIVERS?1:0);
    PeUiTrackerObjective goals[count?count:1];char labels[count?count:1][1024];
    for(size_t i=0;i<mission->objective_count;i++){
        const PeTrainingUiObjectiveText *o=pe_training_ui_mission_objective(t->tutorial.mission_id,i);
        bool done=pe_tutorial_objective_done(&t->tutorial,o->target_id);unsigned n=done?1u:0u;
        if(o->kind==1)n=t->combat_kill_entity_id?1u:0u;
        else if(o->kind==2)n=o->item_id==916?(unsigned)(t->juice_count>0?t->juice_count:0):pe_training_gathering_count(&t->gathering,o->item_id);
        if(o->max_count>0)snprintf(labels[i],sizeof labels[i],"%s %u/%u",o->text,n,o->max_count);else snprintf(labels[i],sizeof labels[i],"%s",o->text);
        goals[i]=(PeUiTrackerObjective){labels[i],done};
    }
    if(count>mission->objective_count){snprintf(labels[count-1],sizeof labels[0],"%u / 50",state.receivers_blocked);goals[count-1]=(PeUiTrackerObjective){labels[count-1],false};}
    PeUiCanvas c=canvas(u);PeUiTrackerView v={mission->title,mission->mission_type==1,goals,count};pe_ui_tracker_draw(&c,&v);
}
void pe_training_ui_record_dialogue(PeTrainingUi *u,const PeTraining *t){
    if(!u||!t)return;
    int id=pe_training_dialogue(t);const PeTrainingDialogue *entry=pe_training_dialogue_entry(id);
    if(!entry){u->recorded_talk=0;return;}
    if(id==u->recorded_talk)return;
    u->recorded_talk=id;
    const char *speaker=entry->speaker_npc_id==-9995?"Tips":entry->speaker_npc_id==-9999?"You":"Andy";
    bool added=false;
    if(pe_conversation_history_add(&u->conversation_history,speaker,pe_training_dialogue_text(id),&added)&&added)u->history.scroll=-1;
}
static void dialogue(PeTrainingUi *u,const PeTraining *t){
    int id=pe_training_dialogue(t);if(!id)return;
    PeTrainingUILayout *l=&u->layout;const PeTrainingDialogue *entry=pe_training_dialogue_entry(id);
    bool tips=entry&&entry->speaker_npc_id==-9995,player=entry&&entry->speaker_npc_id==-9999;
    PeUiCanvas c=canvas(u);pe_ui_draw_dialogue_frame(&c,tips?"Tips":player?"You":"Andy");
    sprite(u,tips||player?"npc_big_Unknown":"npc_big_Andy",l->dialogue_portrait,WHITE);
    pe_ui_draw_wrap(&c,pe_training_dialogue_text(id),l->dialogue_text,18,ink,true,2);
    pe_ui_draw_continue(&c);
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
    if(pe_training_dialogue(t)||u->history.open||u->windows.dragging||t->replicator_open)return;
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
    PeUiCanvas c=canvas(u);PeUiMinimapView map_view={0};pe_ui_minimap_chrome_draw(&c,&map_view);
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
    if(u->history.open){PeUiCanvas c=canvas(u);pe_ui_history_draw(&c,&u->conversation_history,&u->history);}
    if(state.dragging){Vector2 m=GetMousePosition();sprite(u,"drug_hot01",(PeTrainingUIRect){m.x-24*u->layout.scale,m.y-24*u->layout.scale,48*u->layout.scale,48*u->layout.scale},WHITE);}
    if(state.paused){
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,.48f));
        PeTrainingUIRect r=map(u,425,220,430,280,PE_UI_CENTER);panel(u,r,"PAUSED");
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
