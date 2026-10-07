#include "../generated/pe_stamina_recovery_source.h"
#include "pe_sandbox_ui.h"
#include "../generated/pe_revival_source.h"
#include "pe_shield.h"
#include "pe_town_wallet.h"
#include "pe_build_history.h"
#include "pe_adventure_medicine.h"
#include "pe_adventure_hunt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../generated/pe_adventure_settlement.h"
#include <string.h>

static const Color ink={236,246,255,255},blue={112,220,255,255},gold={220,226,112,255},dim={150,170,194,150};
/* Resolved names from the preserved Adventure reference fixture. Generated
 * population/name selection is not yet ported; never relabel tutorial NPCs. */
static const PeAdventureDialogueBindings supplier_names={.player_name="You",.welcome_name="Westina",
    .supplier_name="Eitai",.town_name="Annot",.player_full_name="You",
    .welcome_full_name="Westina Nangle",.supplier_full_name="Eitai Takayama",.annot_name="Naoko",.annot_full_name="Naoko Ueda"};
static Rectangle rect(PeTrainingUIRect r) {return (Rectangle){r.x,r.y,r.w,r.h};}
static PeTrainingUIRect map(const PeSandboxUI *u,float x,float y,float w,float h,PeTrainingUIAnchor a) {
    return pe_training_ui_map(&u->layout,(PeTrainingUIRect){x,y,w,h},a);
}
static PeTrainingUIRect rel(const PeSandboxUI *u,PeTrainingUIRect p,float x,float y,float w,float h) {
    float z=u->layout.scale;return (PeTrainingUIRect){p.x+x*z,p.y+y*z,w*z,h*z};
}
static void sprite(const PeSandboxUI *u,const char *name,PeTrainingUIRect r,Color tint) {
    if(name&&*name)pe_ui_sprite(u->assets,name,rect(r),tint,u->layout.scale);
}
static void effect(const PeSandboxUI *u,PeUiEffect role,PeTrainingUIRect r) {
    pe_training_ui_shader_draw(u->effects,role,rect(r),(float)u->seconds,1,false);
}
static Font font(const PeSandboxUI *u,float size,bool bold) {
    return size>=30?pe_ui_font_heading(u->assets):(size<=15?pe_ui_font_small(u->assets,bold):pe_ui_font(u->assets,bold));
}
static void text(const PeSandboxUI *u,const char *value,float x,float y,float size,Color tint,bool bold) {
    DrawTextEx(font(u,size,bold),value,(Vector2){x,y},size*u->layout.scale,0,tint);
}
static void center(const PeSandboxUI *u,const char *value,PeTrainingUIRect r,float size,Color tint,bool bold) {
    Vector2 m=MeasureTextEx(font(u,size,bold),value,size*u->layout.scale,0);
    text(u,value,r.x+(r.w-m.x)*.5f,r.y+(r.h-m.y)*.5f,size,tint,bold);
}
static void clipped(const PeSandboxUI *u,const char *value,PeTrainingUIRect r,float size,Color tint,bool bold) {
    BeginScissorMode((int)r.x,(int)r.y,(int)ceilf(r.w),(int)ceilf(r.h));
    text(u,value,r.x,r.y,size,tint,bold);EndScissorMode();
}
static void wrapped_spaced(const PeSandboxUI *u,const char *value,PeTrainingUIRect r,float size,Color tint,float gap) {
    char line[512]={0};size_t used=0;const char *p=value;float y=r.y,step=(size+gap)*u->layout.scale;
    BeginScissorMode((int)r.x,(int)r.y,(int)ceilf(r.w),(int)ceilf(r.h));
    while(*p&&y+size*u->layout.scale<=r.y+r.h) {
        if(*p=='\n') {text(u,line,r.x,y,size,tint,false);used=0;line[0]=0;y+=step;p++;continue;}
        while(*p==' ')p++;
        size_t n=strcspn(p," \n");if(!n)continue;if(n>=sizeof line)break;
        char next[512];int count=snprintf(next,sizeof next,"%s%s%.*s",line,used?" ":"",(int)n,p);
        if(count<0||(size_t)count>=sizeof next)break;
        if(used&&MeasureTextEx(font(u,size,false),next,size*u->layout.scale,0).x>r.w) {
            text(u,line,r.x,y,size,tint,false);used=0;line[0]=0;y+=step;continue;
        }
        memcpy(line,next,(size_t)count+1);used=(size_t)count;p+=n;
    }
    if(used&&y+size*u->layout.scale<=r.y+r.h)text(u,line,r.x,y,size,tint,false);
    EndScissorMode();
}
static void wrapped(const PeSandboxUI *u,const char *value,PeTrainingUIRect r,float size,Color tint){wrapped_spaced(u,value,r,size,tint,5);}
static float fraction(float n,float d) {return d>0?fminf(1,fmaxf(0,n/d)):0;}
static void bar(const PeSandboxUI *u,const char *bg,const char *fill,PeTrainingUIRect r,float f,Color tint) {
    sprite(u,bg,r,WHITE);
    if(f<=0)return;
    BeginScissorMode((int)r.x,(int)r.y,(int)ceilf(r.w*fminf(1,f)),(int)ceilf(r.h));
    sprite(u,fill,r,tint);EndScissorMode();
}
static void item(const PeSandboxUI *u,int id,int count,PeTrainingUIRect r,bool box,Color tint) {
    if(box)sprite(u,"itembox",r,WHITE);
    const PeItemDef *d=pe_item_def(id);if(!d)return;
    sprite(u,d->icon,r,tint);sprite(u,d->icon_overlay1,r,tint);sprite(u,d->icon_overlay2,r,tint);
    if(count>1) {
        char value[24];snprintf(value,sizeof value,"%d",count);
        Vector2 extent=MeasureTextEx(font(u,14,false),value,14*u->layout.scale,0);
        text(u,value,r.x+r.w-extent.x-2*u->layout.scale,r.y+r.h-15*u->layout.scale,14,gold,false);
    }
}
static void button(const PeSandboxUI *u,PeTrainingUIRect r,const char *label,bool enabled) {
    bool hover=pe_training_ui_contains(r,u->mouse_x,u->mouse_y);
    sprite(u,"menulist",r,enabled?(hover?blue:WHITE):dim);
    center(u,label,r,18,enabled?ink:dim,false);
}
static void panel(const PeSandboxUI *u,PeTrainingUIRect r,const char *title,PeUiEffect fx) {
    sprite(u,fx==PE_UI_EFFECT_REPLICATOR||fx==PE_UI_EFFECT_MISSION?"bigbg":"midbg",r,WHITE);
    sprite(u,fx==PE_UI_EFFECT_REPLICATOR||fx==PE_UI_EFFECT_MISSION?"bigbg":"midbg",r,(Color){0,33,255,77});
    effect(u,fx,r);
    sprite(u,"titilebg",rel(u,r,15,32,r.w/u->layout.scale-65,14),WHITE);
    float title_x=fx==PE_UI_EFFECT_REPLICATOR?84:fx==PE_UI_EFFECT_MISSION?87:32;
    float title_y=fx==PE_UI_EFFECT_REPLICATOR?14:fx==PE_UI_EFFECT_MISSION?16.5f:20;
    text(u,title,r.x+title_x*u->layout.scale,r.y+title_y*u->layout.scale,32,(Color){159,197,234,255},false);
}
bool pe_sandbox_ui_load(PeSandboxUI *u,const char *root,char *error,size_t cap) {
    if(error&&cap)*error=0;
    if(!u||!root||!IsWindowReady()){if(error&&cap)snprintf(error,cap,"Sandbox UI requires an active window and asset directory");return false;}
    if(u->loaded||u->assets){if(error&&cap)snprintf(error,cap,"Sandbox UI must be empty before loading");return false;}
    pe_sandbox_ui_init(u,(float)GetScreenWidth(),(float)GetScreenHeight());
    char path[4096];int n=snprintf(path,sizeof path,"%s/ui",root);
    if(n<0||n>=(int)sizeof path){if(error&&cap)snprintf(error,cap,"UI asset directory is too long");return false;}
    u->assets=pe_ui_assets_load(path,error,cap);if(!u->assets)return false;
    u->effects=pe_training_ui_shader_load(path,error,cap);if(!u->effects){pe_sandbox_ui_unload(u);return false;}
    u->preview=LoadRenderTexture(620,960);
    if(!u->preview.id){if(error&&cap)snprintf(error,cap,"Cannot allocate character preview");pe_sandbox_ui_unload(u);return false;}
    SetTextureFilter(u->preview.texture,TEXTURE_FILTER_BILINEAR);u->loaded=true;return true;
}
void pe_sandbox_ui_unload(PeSandboxUI *u) {
    if(!u)return;
    if(u->preview.id)UnloadRenderTexture(u->preview);
    if(u->minimap.id)UnloadTexture(u->minimap);
    pe_training_ui_shader_free(u->effects);pe_ui_assets_free(u->assets);memset(u,0,sizeof *u);
}
void pe_sandbox_ui_set_preview(PeSandboxUI *u,PeSandboxUIPreview draw,void *context) {
    if(u){u->draw_preview=draw;u->preview_context=context;}
}
PeSandboxUIResult pe_sandbox_ui_update(PeSandboxUI *u,PeGame *g,float dt) {
    if(!u||!g)return (PeSandboxUIResult){.consume_world=true};
    pe_sandbox_ui_resize(u,(float)GetScreenWidth(),(float)GetScreenHeight());
    if(isfinite(dt)&&dt>=0)u->seconds+=fminf(dt,.25f);
    if(strcmp(u->last_message,g->message)) {
        snprintf(u->last_message,sizeof u->last_message,"%s",g->message);u->message_until=u->seconds+5;
    }
    Vector2 mouse=GetMousePosition();char typed[64];size_t n=0;int code;
    while((code=GetCharPressed())!=0)if(code>=32&&code<127&&n+1<sizeof typed)typed[n++]=(char)code;
    typed[n]=0;
    bool keys=!u->search_focus;
    bool control=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL);
    bool history_keys=IsKeyDown(KEY_Z)||IsKeyDown(KEY_X);
    PeSandboxUIInput i={.frame_dt=dt,.mouse_x=mouse.x,.mouse_y=mouse.y,.wheel=GetMouseWheelMove(),.focused=IsWindowFocused(),
        .left_pressed=IsMouseButtonPressed(MOUSE_BUTTON_LEFT),.left_released=IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
        .right_pressed=IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),.inventory=keys&&IsKeyPressed(KEY_I),
        .character=keys&&IsKeyPressed(KEY_C),.missions=keys&&IsKeyPressed(KEY_Q),.replicator=keys&&IsKeyPressed(KEY_Y),
        .escape=IsKeyPressed(KEY_ESCAPE),.tab=keys&&IsKeyPressed(KEY_TAB),.save=keys&&IsKeyPressed(KEY_F9),
        .load=keys&&IsKeyPressed(KEY_F10),.backspace=IsKeyPressed(KEY_BACKSPACE),
        .undo_build=keys&&control&&IsKeyPressed(KEY_Z),.redo_build=keys&&control&&IsKeyPressed(KEY_X),
        .build_history_chord=control&&history_keys,.build_history_keys_down=history_keys,
        .advance=IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE),.quickslot=-1,.typed=typed};
    if(keys)for(int k=0;k<9;k++)if(IsKeyPressed(KEY_ONE+k)||IsKeyPressed(KEY_KP_1+k)){i.quickslot=k;i.response=(unsigned)k+1;break;}
    return pe_sandbox_ui_route(u,g,i);
}
static void prepare_map(PeSandboxUI *u,const PeGame *g) {
    const PeWorld *w=&g->world;
    int depth=pe_world_depth(w),width=pe_world_width(w),height=pe_world_size_y(w);
    if(u->map_valid&&u->minimap.width==width&&u->minimap.height==depth&&u->map_world_height==height&&u->map_revision==w->revision&&u->map_seed==w->seed&&u->map_source==w->source&&
       u->map_origin_x==w->origin_x&&u->map_origin_y==w->origin_y&&u->map_origin_z==w->origin_z)return;
    if(width<=0||depth<=0||height<=0)return;
    Color *pixels=malloc((size_t)width*(size_t)depth*sizeof *pixels);if(!pixels)return;
    /* Actual bounded-world top samples. No training-room map or fictitious NPC
       markers. This is a terrain overview, not the original top-down renderer. */
    for(int z=0;z<depth;z++)for(int x=0;x<width;x++) {
        int y=height-1;PeVoxel v={0};
        for(;y>=0;y--){v=pe_world_get(w,x,y,z);if(v.volume>=PE_ISO_LEVEL)break;}
        int id=pe_world_material_item(w,v.material);Color c=y<0?(Color){12,24,32,255}:
            id==PE_ITEM_STONE?(Color){114,127,135,255}:id==PE_ITEM_DIRT?(Color){102,119,68,255}:(Color){91,113,78,255};
        float light=.6f+.4f*(y+1)/(float)height;c.r=(unsigned char)(c.r*light);c.g=(unsigned char)(c.g*light);c.b=(unsigned char)(c.b*light);
        pixels[(depth-1-z)*width+x]=c;
    }
    if(u->minimap.id&&(u->minimap.width!=width||u->minimap.height!=depth)){UnloadTexture(u->minimap);u->minimap=(Texture2D){0};}
    if(u->minimap.id)UpdateTexture(u->minimap,pixels);
    else {Image image={.data=pixels,.width=width,.height=depth,.mipmaps=1,.format=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};u->minimap=LoadTextureFromImage(image);}
    free(pixels);
    if(u->minimap.id){u->map_valid=true;u->map_world_height=height;u->map_revision=w->revision;u->map_seed=w->seed;u->map_source=w->source;
        u->map_origin_x=w->origin_x;u->map_origin_y=w->origin_y;u->map_origin_z=w->origin_z;}
}
void pe_sandbox_ui_prepare(PeSandboxUI *u,const PeGame *g) {
    if(!u||!u->loaded||!g)return;
    prepare_map(u,g);
    if(u->character_open&&u->draw_preview&&u->preview.id) {
        BeginTextureMode(u->preview);ClearBackground(BLANK);u->draw_preview(u->preview_context,g);EndTextureMode();
    }
}
static void hud(const PeSandboxUI *u,const PeGame *g,bool building) {
    const PeTrainingUILayout *l=&u->layout;float z=l->scale;
    sprite(u,"shortbar1bg",map(u,217.5f,644,845,54,PE_UI_BOTTOM_CENTER),WHITE);
    PeTrainingUIRect frame=map(u,217.5f,611,845,110,PE_UI_BOTTOM_CENTER);
    sprite(u,"shortbar1",frame,WHITE);sprite(u,"shortbar1",frame,(Color){0,33,255,77});effect(u,PE_UI_EFFECT_FRAME,frame);
    effect(u,PE_UI_EFFECT_HUD_SCAN,map(u,225,646,830,50,PE_UI_BOTTOM_CENTER));
    bar(u,"bloodlan_6bg","bloodlan_6",l->health,fraction(g->hp,g->max_hp),WHITE);
    float hp=fraction(g->hp,g->max_hp);
    if(hp>0){BeginScissorMode((int)l->health.x,(int)l->health.y,(int)ceilf(l->health.w*hp),(int)ceilf(l->health.h));effect(u,PE_UI_EFFECT_HEALTH,l->health);EndScissorMode();}
    bar(u,"bloodlan_7bg","bloodlan_7_1",l->stamina,fraction(g->stamina,g->stamina_max),(Color){65,131,185,255});
    for(int k=0;k<9;k++) {
        PeTrainingUIRect r=l->quickslots[k];
        const int id=g->quickslots[k].item_id;int count=0;
        for(int slot=0;slot<PE_INVENTORY_SLOTS;slot++) {
            const PeSlot *owned=&g->inventory.slots[slot];
            if(owned->count>0&&owned->item_id==id&&owned->instance_id==g->quickslots[k].instance_id)count+=owned->count;
        }
        char number[4];snprintf(number,sizeof number,"%d",k+1);
        text(u,number,r.x+4*z,r.y+2*z,14,(Color){220,225,235,130},true);
        if(id)item(u,id,count,r,false,count?WHITE:dim);
        float cooldown=pe_game_item_cooldown(g,id);
        if(cooldown>0) {
            DrawRectangleRec(rect(r),(Color){25,40,90,160});center(u,TextFormat("%.0f",ceilf(cooldown)),r,18,ink,true);
        }
    }
    sprite(u,"icons:itemhand",l->hand_slot,WHITE);
    const PeSlot *tool=pe_game_equipped_tool(g);if(tool)item(u,tool->item_id,1,l->hand_slot,false,WHITE);
    sprite(u,"icons:itemhand_get",l->hand_slot,WHITE);sprite(u,"buttonb",l->build_button,building?blue:WHITE);
    if(pe_training_ui_contains(l->health,u->mouse_x,u->mouse_y))center(u,TextFormat("%.0f / %.0f",g->hp,g->max_hp),l->health,14,WHITE,true);
    if(pe_training_ui_contains(l->stamina,u->mouse_x,u->mouse_y))text(u,TextFormat("Stamina %.0f / %.0f",g->stamina,g->stamina_max),l->stamina.x,l->stamina.y-20*z,14,ink,false);
}
static void minimap(const PeSandboxUI *u,const PeGame *g) {
    int depth=pe_world_depth(&g->world),width=pe_world_width(&g->world);
    float z=u->layout.scale;PeTrainingUIRect area=map(u,1090,3,156,174,PE_UI_TOP_RIGHT);
    if(u->minimap.id)DrawTexturePro(u->minimap,(Rectangle){0,0,(float)width,(float)depth},rect(area),(Vector2){0},0,WHITE);
    BeginScissorMode((int)area.x,(int)area.y,(int)area.w,(int)area.h);
    for(int i=0;i<g->cell_count;i++) {
        const PeB45Cell *c=&g->cells[i];
        DrawRectangleRec((Rectangle){area.x+c->x*.5f/(width-1)*area.w,area.y+(1-c->z*.5f/(depth-1))*area.h,2*z,2*z},gold);
    }
    if(g->mode==PE_GAME_ADVENTURE&&u->supplier_present) {
        float bx=area.x+u->supplier_position.x/(width-1)*area.w;
        float bz=area.y+(1-u->supplier_position.z/(depth-1))*area.h;
        sprite(u,"sign_npc",(PeTrainingUIRect){bx-6*z,bz-6*z,12*z,12*z},g->supplies_claimed?dim:gold);
    }
    if(g->mode==PE_GAME_ADVENTURE&&u->welcome_present) {
        float bx=area.x+u->welcome_position.x/(width-1)*area.w;
        float bz=area.y+(1-u->welcome_position.z/(depth-1))*area.h;
        sprite(u,"sign_npc",(PeTrainingUIRect){bx-6*z,bz-6*z,12*z,12*z},g->welcome_page<PE_ADVENTURE_WELCOME_PAGES?gold:dim);
    }
    if(g->mode==PE_GAME_ADVENTURE&&u->resident_markers)for(size_t i=0;i<u->resident_marker_count;i++) {
        const PeSandboxMapNpc *npc=&u->resident_markers[i];
        if(npc->position.x<0||npc->position.x>=width||npc->position.z<0||npc->position.z>=depth)continue;
        float x=area.x+npc->position.x/(width-1)*area.w,y=area.y+(1-npc->position.z/(depth-1))*area.h;
        sprite(u,"sign_npc",(PeTrainingUIRect){x-5*z,y-5*z,10*z,10*z},pe_town_shop_count(npc->entity_id)?gold:blue);
    }
    if(g->mode==PE_GAME_ADVENTURE&&g->supplies_claimed&&!g->annot_complete&&g->world.source) {
        double tx=pe_adventure_settlement_town4_destination[0]-g->world.origin_x;
        double tz=pe_adventure_settlement_town4_destination[2]-g->world.origin_z;
        if(tx>=0&&tx<width&&tz>=0&&tz<depth){
            float x=area.x+(float)tx/(width-1)*area.w,y=area.y+(1-(float)tz/(depth-1))*area.h;
            sprite(u,"Icon_mission",(PeTrainingUIRect){x-7*z,y-9*z,14*z,18*z},gold);
        }
    }
    float px=area.x+g->player.x/(width-1)*area.w,py=area.y+(1-g->player.z/(depth-1))*area.h;
    DrawCircleV((Vector2){px,py},3*z,WHITE);
    DrawLineEx((Vector2){px,py},(Vector2){px+sinf(g->yaw)*9*z,py+cosf(g->yaw)*9*z},2*z,blue);
    EndScissorMode();
    sprite(u,"map_02",map(u,1089,0,158,200,PE_UI_TOP_RIGHT),(Color){205,245,255,120});
    sprite(u,"map_01",map(u,1069,0,20,200,PE_UI_TOP_RIGHT),WHITE);
    sprite(u,"map_03",map(u,1246,0,33,200,PE_UI_TOP_RIGHT),WHITE);
    effect(u,PE_UI_EFFECT_MAP_LEFT,map(u,1069,0,20,208,PE_UI_TOP_RIGHT));
    effect(u,PE_UI_EFFECT_MAP_RIGHT,map(u,1246,0,34,208,PE_UI_TOP_RIGHT));
    sprite(u,"Icon_mission",map(u,1254,173,20,26,PE_UI_TOP_RIGHT),WHITE);
    double tod=fmod(g->game_seconds,PE_DAY_SECONDS);int hour=(int)(tod/3600),minute=(int)(tod/60)%60;
    center(u,TextFormat("%02d:%02d",hour,minute),map(u,1090,179,156,18,PE_UI_TOP_RIGHT),14,ink,true);
    int ox=g->world.source?g->world.origin_x:0,oz=g->world.source?g->world.origin_z:0;
    text(u,TextFormat("P: %.0f, %.0f",g->player.x+ox,g->player.z+oz),area.x,area.y+4*z,14,ink,false);
}
static void inventory(const PeSandboxUI *u,const PeGame *g) {
    const PeTrainingUILayout *l=&u->layout;float z=l->scale;PeTrainingUIRect p=l->inventory;
    PeTrainingUIRect outer={p.x-4*z,p.y-3*z,384*z,576*z};panel(u,outer,"INVENTORY",PE_UI_EFFECT_INVENTORY);
    sprite(u,"close1",l->inventory_close,(Color){165,185,255,255});
    static const char *icons[]={"item","equipment","resource","armor","mission"};
    sprite(u,"insidebox2",rel(u,p,13,56,349,35),WHITE);
    for(int t=0;t<5;t++) {
        char name[32];snprintf(name,sizeof name,"%s%s",icons[t],t==u->inventory_tab?"_1":"");
        PeTrainingUIRect r=l->inventory_tabs[t];r.x+=(r.w-40*z)*.5f;r.y-=2*z;r.w=r.h=40*z;sprite(u,name,r,WHITE);
    }
    sprite(u,"insidebox2",rel(u,p,13,97,349,418),WHITE);
    for(int k=0;k<PE_UI_INVENTORY_SLOTS;k++) {
        PeTrainingUIRect r=l->inventory_slots[k];int index=pe_sandbox_ui_inventory_slot(u,g,k);
        const PeSlot *s=index>=0?&g->inventory.slots[index]:NULL;item(u,s?s->item_id:0,s?s->count:0,r,true,WHITE);
        if(index>=0&&index==u->selected_slot)DrawRectangleLinesEx(rect(r),z,blue);
    }
    int n=pe_sandbox_ui_inventory_count(u,g),pages=(n+41)/42;if(pages<1)pages=1;
    PeTrainingUIRect page=l->inventory_page;
    static const char *arrows[]={"leftendbutton","leftbutton","rightbutton","rightendbutton"};
    const float offset[]={0,25,105,130};
    for(int i=0;i<4;i++)sprite(u,arrows[i],rel(u,page,offset[i],3,16,16),WHITE);
    center(u,TextFormat("%d/%d",u->inventory_page+1,pages),rel(u,page,48,0,50,23),18,ink,true);
    text(u,TextFormat("Money: %u",g->money),p.x+24*z,p.y+531*z,14,gold,false);
    text(u,"Right-click: use / equip   Drag: quickbar",p.x+17*z,p.y+507*z,14,dim,false);
}
static const int outfit_ui_slots[PE_OUTFIT_SLOTS]={1,6,7}; /* coat, pants, shoes */
static void character(const PeSandboxUI *u,const PeGame *g) {
    const PeTrainingUILayout *l=&u->layout;float z=l->scale;PeTrainingUIRect p=l->equipment;
    panel(u,p,"CHARACTER",PE_UI_EFFECT_CHARACTER);sprite(u,"close1",l->equipment_close,WHITE);
    sprite(u,"insidebox2",rel(u,p,15,61,278,520),WHITE);sprite(u,"insidebox2",rel(u,p,296,61,310,520),WHITE);
    if(u->draw_preview&&u->preview.texture.id)DrawTexturePro(u->preview.texture,(Rectangle){0,0,620,-960},rect(l->equipment_preview),(Vector2){0},0,WHITE);
    else center(u,"Character",l->equipment_preview,18,dim,false);
    const PeSlot *equipped=pe_game_equipped_tool(g);const PeItemDef *tool=equipped?pe_item_def(equipped->item_id):NULL;
    static const char *masks[]={"Icons_helmet","Icons_coat","Icons_gloves","Icons_battery","Icons_arms","Icons_back","Icons_pants","Icons_shoes","Icons_glass","Icons_dun"};
    for(int k=0;k<10;k++){item(u,0,0,l->equipment_slots[k],true,WHITE);sprite(u,masks[k],l->equipment_slots[k],(Color){175,200,220,110});}
    for(unsigned k=0;k<PE_OUTFIT_SLOTS;k++){
        const PeWornItem *w=&g->outfit.items[k];
        if(w->item_id)item(u,w->item_id,1,l->equipment_slots[outfit_ui_slots[k]],true,w->durability>0?WHITE:GRAY);
    }
    if(equipped)item(u,equipped->item_id,1,l->equipment_weapon,true,equipped->durability>0?WHITE:GRAY);
    const PeSlot *shield=pe_shield_equipped(g);
    if(shield)item(u,shield->item_id,1,l->equipment_slots[9],true,u->starter_pair_available&&shield->durability>0?WHITE:GRAY);
    text(u,"Explorer",p.x+25*z,p.y+70*z,18,ink,true);
    static const char *labels[]={"Health","Stamina","Hunger","Comfort","Oxygen","E-Shield","Energy","Attack","Defense"};
    for(int k=0;k<9;k++) {
        float y=p.y+(116+k*34)*z;text(u,labels[k],p.x+25*z,y,18,ink,true);
        char number[64];
        if(k==0)snprintf(number,sizeof number,"%.0f / %.0f",g->hp,g->max_hp);
        else if(k==1)snprintf(number,sizeof number,"%.0f / %.0f",g->stamina,g->stamina_max);
        else if(k==3&&g->mode==PE_GAME_ADVENTURE)snprintf(number,sizeof number,"%.1f / %.0f",g->comfort,pe_stamina_recovery_source.comfort_max);
        else if(k==7)snprintf(number,sizeof number,"%.0f",pe_player_defaults.attack+(tool&&!(tool->id==1&&equipped->durability<=0)?tool->attack:0));
        else if(k==8)snprintf(number,sizeof number,"%.0f",pe_player_defaults.defense+pe_outfit_defense(&g->outfit)+(u->starter_pair_available?pe_shield_defense(shield):0));
        else snprintf(number,sizeof number,"--");
        Vector2 extent=MeasureTextEx(font(u,18,true),number,18*z,0);
        text(u,number,p.x+265*z-extent.x,y,18,k==0||k==1||(k==3&&g->mode==PE_GAME_ADVENTURE)||k==7||k==8?gold:dim,true);
    }
    if(tool)wrapped(u,TextFormat("%s\nDurability %.0f / %d\nRight-click hand to unequip",tool->name,equipped->durability,tool->durability_max),rel(u,p,25,446,250,110),14,ink);
}
static void replicator(const PeSandboxUI *u,const PeGame *g) {
    const PeTrainingUILayout *l=&u->layout;PeTrainingUIRect p=l->replicator;float z=l->scale;
    panel(u,p,"REPLICATOR",PE_UI_EFFECT_REPLICATOR);sprite(u,"close1",l->replicator_close,WHITE);
    sprite(u,"insidebox2",l->replicator_recipes,WHITE);
    PeTrainingUIRect search=pe_sandbox_ui_search(u);sprite(u,"inputbox1",search,WHITE);
    clipped(u,*u->search?u->search:"Search learned recipes",rel(u,search,9,5,380,22),18,*u->search?ink:dim,false);
    if(u->search_focus)DrawRectangleLinesEx(rect(search),z,blue);
    text(u,"ALL",p.x+76*z,p.y+75*z,18,ink,false);
    for(int row=0;row<6;row++) {
        int id=pe_sandbox_ui_recipe_at(u,g,row);const PeRecipeDef *r=pe_recipe_def(id);if(!r)continue;
        const PeItemDef *d=pe_item_def(r->output_id);PeTrainingUIRect box=pe_sandbox_ui_recipe_row(u,row);
        if(id==u->recipe_id)sprite(u,"multplayer_hall_select",box,WHITE);
        item(u,r->output_id,0,rel(u,box,2,2,48,48),true,WHITE);
        if(d)clipped(u,d->name,rel(u,box,56,15,142,24),14,ink,false);
        sprite(u,"DividLine2",rel(u,box,0,53,194,4),WHITE);
    }
    if(!pe_sandbox_ui_recipe_count(u,g))wrapped(u,*u->search?"No matching recipes.":"No learned recipes. Use a recipe script from your inventory.",rel(u,p,76,125,174,190),18,dim);
    const PeRecipeDef *r=pe_recipe_def(u->recipe_id);bool known=r&&pe_game_recipe_known(g,r->id);
    sprite(u,"insidebox2",rel(u,p,275,100,570,210),WHITE);
    if(known) {
        const PeItemDef *d=pe_item_def(r->output_id);
        if(d)clipped(u,d->name,rel(u,p,295,106,520,24),18,ink,true);
        item(u,r->output_id,r->output_count,rel(u,p,530,144,48,48),true,WHITE);
        for(int k=0;k<r->ingredient_count;k++) {
            PeIngredient ingredient=r->ingredients[k];int owned=pe_inventory_count(&g->inventory,ingredient.item_id);
            PeTrainingUIRect box=rel(u,p,302+87*(float)k,228,48,48);
            item(u,ingredient.item_id,owned,box,true,WHITE);
            long long needed=(long long)ingredient.count*u->batches;
            text(u,TextFormat("- %lld",needed),box.x,box.y+50*z,14,owned>=needed?ink:(Color){255,95,95,255},false);
        }
        item(u,r->output_id,0,l->replicator_output,true,WHITE);
        text(u,TextFormat("%.1f s each",r->seconds),p.x+310*z,p.y+355*z,14,dim,false);
    }
    text(u,"Number:",p.x+355*z,p.y+377*z,18,ink,false);
    sprite(u,"inputbox1",rel(u,p,351,400,80,26),WHITE);
    center(u,TextFormat("%d",u->batches),rel(u,p,351,400,80,26),18,ink,false);
    button(u,pe_sandbox_ui_quantity(u,0),"-",known);button(u,pe_sandbox_ui_quantity(u,1),"+",known);
    button(u,pe_sandbox_ui_quantity(u,2),"MIN",known);button(u,pe_sandbox_ui_quantity(u,3),"MAX",known);
    sprite(u,"Craft1",l->replicator_craft_icon,known&&!g->craft.recipe_id?WHITE:dim);
    if(g->craft.recipe_id) {
        const PeRecipeDef *active=pe_recipe_def(g->craft.recipe_id);
        float f=active?fraction(g->craft.elapsed,active->seconds):0;
        bar(u,"jindulan_1","jindulan_2",l->replicator_progress,f,WHITE);
        button(u,pe_sandbox_ui_cancel_button(u),"Cancel",true);
        text(u,TextFormat("%d left, %d ready",g->craft.batches_left,g->craft.batches_pending),p.x+285*z,p.y+456*z,14,ink,false);
    } else sprite(u,"jindulan_1",l->replicator_progress,WHITE);
}
static const char *mission_goal(const PeSandboxUI *u,const PeGame *g) {
    if(g->mode!=PE_GAME_ADVENTURE||g->welcome_page<PE_ADVENTURE_WELCOME_PAGES)return "";
    static char value[2048];
    if(g->hunt_state){
        return pe_sandbox_ui_hunt_goal(u,g,value,sizeof value)?value:"";
    }
    if(g->medicine_state==PE_MEDICINE_ACTIVE){
        const PeItemDef *item=pe_item_def(917);PeAdventureDialogueBindings names=supplier_names;
        names.item_name=item?item->name:"Medicine";names.item_count="5";
        char objective[1024];int held=pe_inventory_count(&g->inventory,917);
        if(!pe_adventure_dialogue_format(pe_adventure_mission_objective(9072),&names,objective,sizeof objective))return "";
        snprintf(value,sizeof value,"%s (%d/5)%s",objective,held<5?held:5,pe_adventure_medicine_ready(g)?" - Return to Naoko":"");return value;
    }
    if(g->annot_complete)return "";
    const char *raw=pe_adventure_mission_objective(g->supplies_claimed?PE_ADVENTURE_SUPPLIES:PE_ADVENTURE_ARRIVAL);
    return raw&&pe_adventure_dialogue_format(raw,&supplier_names,value,sizeof value)?value:"";
}
static void missions(const PeSandboxUI *u,const PeGame *g) {
    PeTrainingUIRect p=u->layout.mission;float z=u->layout.scale;
    panel(u,p,"MISSION",PE_UI_EFFECT_MISSION);sprite(u,"close1",u->layout.mission_close,WHITE);
    sprite(u,"insidebox2",rel(u,p,35,95,310,400),WHITE);sprite(u,"insidebox2",rel(u,p,365,75,640,390),WHITE);
    if(*pe_sandbox_ui_mission_title(g)) {
        wrapped(u,pe_sandbox_ui_mission_title(g),rel(u,p,50,115,275,70),18,blue);
        text(u,"Goal",p.x+390*z,p.y+110*z,18,ink,true);
        wrapped(u,mission_goal(u,g),rel(u,p,390,150,570,120),18,ink);
        if(g->hunt_state){
            if(g->hunt_state==PE_HUNT_ENCOUNTER_PENDING&&!u->hunt_encounter_available)wrapped(u,"The creature encounter is unavailable in this session.",rel(u,p,390,275,560,100),18,ink);
            else {
                char description[2048];int mission=pe_sandbox_ui_hunt_mission_id(g);
                if(pe_adventure_dialogue_format(pe_adventure_mission_description(mission),&supplier_names,description,sizeof description))wrapped(u,description,rel(u,p,390,275,560,100),18,ink);
            }
        }else if(g->medicine_state==PE_MEDICINE_ACTIVE){
            char description[2048];
            if(pe_adventure_dialogue_format(pe_adventure_mission_description(9072),&supplier_names,description,sizeof description))wrapped(u,description,rel(u,p,390,275,560,60),18,ink);
            wrapped(u,"Learn the Herbal Drink script from your inventory, then use the Replicator to create five units. Return to Naoko in Annot to deliver them.",rel(u,p,390,345,560,100),17,ink);
        }else if(g->supplies_claimed){
            double x=pe_adventure_settlement_town4_destination[0]-g->world.origin_x;
            double zpos=pe_adventure_settlement_town4_destination[2]-g->world.origin_z;
            bool reachable=g->world.source&&x>=0&&x<pe_world_width(&g->world)&&zpos>=0&&zpos<pe_world_depth(&g->world);
            wrapped(u,reachable?"Go to Annot at P: -284, 134. Approach within 15 meters to complete the objective.":"Annot is at P: -284, 134, outside this saved world's terrain.",rel(u,p,390,300,560,125),18,ink);
        }
    } else wrapped(u,g->medicine_state==PE_MEDICINE_COMPLETE?"Herb Collecting: completed. Ask Naoko about the next request.":g->annot_complete?"Supplies: completed. Speak with Naoko in Annot about the delayed supplies.":"No active missions.",rel(u,p,60,125,270,180),18,dim);
}
static void supplier_portrait(const PeSandboxUI *u,PeTrainingUIRect r) {
    Texture2D t=u->dialogue.role==1?u->welcome_portrait:u->dialogue.role==3?u->annot_portrait:u->supplier_portrait;
    if(t.id)DrawTexturePro(t,(Rectangle){0,0,(float)t.width,-(float)t.height},rect(r),(Vector2){0},0,WHITE);
}
static void supplier_dialogue(const PeSandboxUI *u,const PeGame *g) {
    if(u->dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE)return;
    const PeAdventureDialogueLine *line=pe_adventure_dialogue_current(&u->dialogue,g);
    char value[2048]={0},seed[32];snprintf(seed,sizeof seed,"%u",g->world.seed);
    PeAdventureDialogueBindings names=supplier_names;names.seed_name=seed;names.start_town_name=u->start_town_name[0]?u->start_town_name:NULL;
    if(line&&!pe_adventure_dialogue_format(line->text,&names,value,sizeof value))return;
    if(u->dialogue.stage==PE_ADVENTURE_DIALOGUE_CHOICE){
        const PeTrainingUILayout *l=&u->layout;
        sprite(u,"talkbg",l->dialogue,WHITE);sprite(u,"talkbg",l->dialogue,(Color){0,33,255,77});effect(u,PE_UI_EFFECT_DIALOGUE,l->dialogue);
        supplier_portrait(u,l->dialogue_portrait);sprite(u,"namebg",l->dialogue_name,WHITE);
        center(u,supplier_names.annot_full_name,l->dialogue_name,18,ink,true);
        for(int row=0;row<9;row++){
            int index=pe_sandbox_ui_dialogue_choice_index(u,g,row);if(index<0)break;
            const PeAdventureDialogueLine *option=pe_adventure_dialogue_choice(&u->dialogue,g,(unsigned)index);
            PeTrainingUIRect choice=pe_sandbox_ui_supplier_choice(u,row);
            char number[8];snprintf(number,sizeof number,"%d.",row+1);
            text(u,number,choice.x+5*l->scale,choice.y+2*l->scale,16,ink,false);
            text(u,option->text,choice.x+25*l->scale,choice.y+2*l->scale,16,pe_training_ui_contains(choice,u->mouse_x,u->mouse_y)?blue:ink,false);
        }
        if(u->dialogue_result==PE_ADVENTURE_DIALOGUE_NO_SPACE||u->dialogue_result==PE_ADVENTURE_DIALOGUE_UNAVAILABLE)
            wrapped(u,g->message,rel(u,l->dialogue,180,82,640,42),16,gold);
    }else if(u->dialogue.stage==PE_ADVENTURE_DIALOGUE_MENU) {
        PeTrainingUIRect p=pe_sandbox_ui_supplier_menu(u);
        sprite(u,"midbg",p,WHITE);sprite(u,"midbg",p,(Color){0,33,255,77});
        effect(u,PE_UI_EFFECT_INVENTORY,p);
        sprite(u,"insidebox1",rel(u,p,13,63,400,400),WHITE);
        sprite(u,"titilebg",rel(u,p,18,47,210,14),WHITE);
        supplier_portrait(u,rel(u,p,24,21,48,48));
        text(u,u->dialogue.role==1?"Westina Nangle":u->dialogue.role==3?"Naoko Ueda":"Eitai Takayama",p.x+78*u->layout.scale,p.y+34*u->layout.scale,20,ink,true);
        sprite(u,"close1",pe_sandbox_ui_supplier_close(u),WHITE);
            if(line) {
                PeTrainingUIRect choice=pe_sandbox_ui_supplier_choice(u,0);
                bool ready=u->dialogue.role!=3||g->medicine_state!=PE_MEDICINE_ACTIVE||pe_adventure_medicine_ready(g);
                sprite(u,u->dialogue.role==3&&g->medicine_state==PE_MEDICINE_ACTIVE?"MainMissionEnd":"MainMissionGet",rel(u,p,22,88,14,14),ready?WHITE:dim);
                text(u,"1.",choice.x+10*u->layout.scale,choice.y,18,ready?ink:dim,false);
                choice.x+=34*u->layout.scale;choice.w-=34*u->layout.scale;
                wrapped(u,value,choice,18,!ready?dim:pe_training_ui_contains(pe_sandbox_ui_supplier_choice(u,0),u->mouse_x,u->mouse_y)?blue:ink);
            }
            PeTrainingUIRect close=pe_sandbox_ui_supplier_choice(u,line?1:0);
            text(u,line?"2.  That's all.":"1.  That's all.",close.x,close.y,18,pe_training_ui_contains(close,u->mouse_x,u->mouse_y)?blue:ink,false);
    } else if(line) {
        const PeTrainingUILayout *l=&u->layout;
        sprite(u,"talkbg",l->dialogue,WHITE);sprite(u,"talkbg",l->dialogue,(Color){0,33,255,77});
        effect(u,PE_UI_EFFECT_DIALOGUE,l->dialogue);
        if(line->speaker!=PE_ADVENTURE_SPEAKER_PLAYER)supplier_portrait(u,l->dialogue_portrait);
        sprite(u,"namebg",l->dialogue_name,WHITE);
        const char *speaker=pe_adventure_dialogue_speaker(line,&supplier_names);
        center(u,speaker?speaker:"",l->dialogue_name,18,ink,true);
        wrapped_spaced(u,value,l->dialogue_text,17,ink,1);
        text(u,"1. Continue",l->dialogue_next.x-105*l->scale,l->dialogue_next.y+26*l->scale,16,ink,false);
        sprite(u,"btn_down",l->dialogue_next,pe_training_ui_contains(l->dialogue_next,u->mouse_x,u->mouse_y)?blue:WHITE);
    }
}
static void town_dialogue(const PeSandboxUI *u,const PeGame *g) {
    if(!u->town_open)return;
    if(!u->shop_open){
        PeTrainingUIRect p=pe_sandbox_ui_supplier_menu(u);
        sprite(u,"midbg",p,WHITE);sprite(u,"midbg",p,(Color){0,33,255,77});effect(u,PE_UI_EFFECT_INVENTORY,p);
        sprite(u,"insidebox1",rel(u,p,13,63,400,400),WHITE);sprite(u,"titilebg",rel(u,p,18,47,210,14),WHITE);
        if(u->town_portrait.id)DrawTexturePro(u->town_portrait,(Rectangle){0,0,(float)u->town_portrait.width,-(float)u->town_portrait.height},rect(rel(u,p,24,21,48,48)),(Vector2){0},0,WHITE);
        text(u,u->town_name,p.x+78*u->layout.scale,p.y+34*u->layout.scale,20,ink,true);
        sprite(u,"close1",pe_sandbox_ui_supplier_close(u),WHITE);
        if(u->deploy_entity){
            if(u->deploy_get_pending){
                text(u,"Picking up...",p.x+29*u->layout.scale,p.y+85*u->layout.scale,18,ink,false);
                PeTrainingUIRect progress=rel(u,p,29,119,365,22);
                DrawRectangleRec(rect(progress),(Color){16,29,38,230});
                progress.w*=(float)fminf((float)u->deploy_get_ticks/200000.0f,1.0f);
                DrawRectangleRec(rect(progress),blue);
                text(u,"Esc: Cancel",p.x+29*u->layout.scale,p.y+157*u->layout.scale,16,dim,false);
                return;
            }
            const char *labels[]={"1.  Get (returns Charcoal)","2.  Turn","3.  That's all."};
            for(int k=0;k<3;k++){PeTrainingUIRect r=pe_sandbox_ui_supplier_choice(u,k);text(u,labels[k],r.x,r.y,18,pe_training_ui_contains(r,u->mouse_x,u->mouse_y)?blue:ink,false);}
            wrapped(u,g->message,rel(u,p,29,220,365,150),16,dim);return;
        }
        bool vendor=pe_town_shop_count(u->town_entity)>0;
        if(vendor){PeTrainingUIRect r=pe_sandbox_ui_supplier_choice(u,0);text(u,"1.  Shop",r.x,r.y,18,pe_training_ui_contains(r,u->mouse_x,u->mouse_y)?blue:ink,false);}
        PeTrainingUIRect r=pe_sandbox_ui_supplier_choice(u,vendor?1:0);text(u,vendor?"2.  That's all.":"1.  That's all.",r.x,r.y,18,pe_training_ui_contains(r,u->mouse_x,u->mouse_y)?blue:ink,false);
        return;
    }
    PeTrainingUIRect p=pe_sandbox_ui_shop_panel(u);float z=u->layout.scale;
    panel(u,p,"SHOP",PE_UI_EFFECT_INVENTORY);sprite(u,"close1",pe_sandbox_ui_shop_button(u,4),WHITE);
    text(u,u->town_name,p.x+180*z,p.y+29*z,20,ink,true);
    static const char *tabs[]={"Buy","Sell","Buyback"};
    for(int tab=0;tab<3;tab++){
        PeTrainingUIRect r=pe_sandbox_ui_shop_tab(u,(PeSandboxShopMode)tab);button(u,r,tabs[tab],true);
        if(u->shop_mode==(PeSandboxShopMode)tab)DrawRectangleRec(rect(r),(Color){64,139,205,65});
    }
    uint32_t wallet=0;bool have_wallet=pe_town_wallet_balance(g,u->town_entity,&wallet);
    text(u,TextFormat("Your currency: %u",g->money),p.x+24*z,p.y+94*z,16,gold,false);
    text(u,have_wallet?TextFormat("Vendor currency: %u",wallet):"Vendor currency unavailable",p.x+300*z,p.y+94*z,16,gold,false);
    int count=pe_sandbox_ui_shop_count(u,g);
    for(int row=0;row<8&&row+u->shop_scroll<count;row++){
        PeSandboxShopRow entry;if(!pe_sandbox_ui_shop_item(u,g,row+u->shop_scroll,&entry))continue;
        const PeItemDef *d=pe_item_def(entry.item.item_id);
        PeTrainingUIRect r=pe_sandbox_ui_shop_row(u,row);unsigned stock=(unsigned)entry.item.count;
        sprite(u,"insidebox2",r,WHITE);if(row+u->shop_scroll==u->shop_selection)DrawRectangleRec(rect(r),(Color){64,139,205,65});
        item(u,entry.item.item_id,0,(PeTrainingUIRect){r.x+2*z,r.y+2*z,28*z,28*z},false,entry.eligible?WHITE:dim);
        clipped(u,d?d->name:"",(PeTrainingUIRect){r.x+38*z,r.y+6*z,360*z,22*z},16,entry.eligible?ink:dim,false);
        text(u,entry.eligible?TextFormat("%u",entry.unit_price):"--",r.x+410*z,r.y+6*z,16,entry.eligible?gold:dim,false);
        text(u,TextFormat("x%u",stock),r.x+510*z,r.y+6*z,16,entry.eligible?ink:dim,false);
    }
    if(!count)center(u,u->shop_mode==PE_SANDBOX_SHOP_BUYBACK?"No items to buy back.":"No items to trade.",rel(u,p,24,190,580,40),18,dim,false);
    PeSandboxShopRow selected={0};bool have=pe_sandbox_ui_shop_item(u,g,u->shop_selection,&selected);
    unsigned stock=have?(unsigned)selected.item.count:0;
    bool can=pe_sandbox_ui_shop_can_trade(u,g);
    const char *action=u->shop_mode==PE_SANDBOX_SHOP_SELL?"Sell":u->shop_mode==PE_SANDBOX_SHOP_BUYBACK?"Buy back":"Buy";
    button(u,pe_sandbox_ui_shop_button(u,0),"-",u->shop_quantity>1);
    center(u,TextFormat("%u",u->shop_quantity),rel(u,p,60,408,58,30),18,ink,true);
    button(u,pe_sandbox_ui_shop_button(u,1),"+",u->shop_quantity<stock);
    button(u,pe_sandbox_ui_shop_button(u,2),have&&selected.eligible?TextFormat("%s - %llu",action,(unsigned long long)selected.unit_price*u->shop_quantity):action,can);
    button(u,pe_sandbox_ui_shop_button(u,3),"Back",true);
    text(u,u->shop_mode==PE_SANDBOX_SHOP_BUYBACK?"Buyback clears when you load or start a new game.":"Unit price shown beside each item.",p.x+24*z,p.y+391*z,14,dim,false);
    wrapped(u,have&&!selected.eligible?selected.reason:g->message,rel(u,p,24,450,580,45),16,ink);
    text(u,TextFormat("%d-%d of %d - Scroll to browse",count?u->shop_scroll+1:0,(int)fminf(u->shop_scroll+8,(float)count),count),p.x+24*z,p.y+496*z,15,ink,false);
}
static void tooltip(const PeSandboxUI *u,const PeGame *g) {
    if(u->town_open||u->dialogue.stage!=PE_ADVENTURE_DIALOGUE_IDLE||u->replicator_open||u->missions_open||u->menu_open||u->windows.dragging)return;
    unsigned visible=(u->inventory_open?1u<<PE_UI_WINDOW_INVENTORY:0)|(u->character_open?1u<<PE_UI_WINDOW_CHARACTER:0);
    int top=pe_training_ui_window_at(&u->layout,visible,u->mouse_x,u->mouse_y),index=-1;
    if(top==PE_UI_WINDOW_INVENTORY)index=pe_sandbox_ui_inventory_slot(u,g,pe_training_ui_hit(u->layout.inventory_slots,PE_UI_INVENTORY_SLOTS,u->mouse_x,u->mouse_y));
    if(top==PE_UI_WINDOW_CHARACTER&&pe_training_ui_contains(u->layout.equipment_weapon,u->mouse_x,u->mouse_y)) {
        for(int k=0;k<PE_INVENTORY_SLOTS;k++)if(g->equipped_instance_id&&g->inventory.slots[k].instance_id==g->equipped_instance_id)index=k;
    }
    if(top==PE_UI_WINDOW_CHARACTER&&pe_training_ui_contains(u->layout.equipment_slots[9],u->mouse_x,u->mouse_y)){
        const PeSlot *shield=pe_shield_equipped(g);
        if(shield)index=(int)(shield-g->inventory.slots);
    }
    PeSlot worn={0};
    if(top==PE_UI_WINDOW_CHARACTER)for(unsigned k=0;k<PE_OUTFIT_SLOTS;k++)
        if(pe_training_ui_contains(u->layout.equipment_slots[outfit_ui_slots[k]],u->mouse_x,u->mouse_y)){
            const PeWornItem *w=&g->outfit.items[k];
            if(w->item_id)worn=(PeSlot){w->item_id,1,w->instance_id,w->durability};
        }
    if(index<0&&!worn.item_id)return;
    const PeSlot *s=worn.item_id?&worn:&g->inventory.slots[index];const PeItemDef *d=pe_item_def(s->item_id);if(!d)return;
    float z=u->layout.scale;PeTrainingUIRect box={fminf(u->mouse_x+18*z,u->layout.width-312*z),fminf(u->mouse_y+20*z,u->layout.height-134*z),300*z,122*z};
    DrawRectangleRec(rect(box),(Color){14,29,51,245});DrawRectangleLinesEx(rect(box),z,blue);
    char value[512];
    const char *action=d->learn_count?"Right-click to learn":(d->equip_position&(16|512))?"Right-click to equip":d->id==916?"Right-click to use":"Drag to quickbar";
    if(s->instance_id&&s->instance_id==g->shield_instance_id)action=u->starter_pair_available?"Right-click shield slot to unequip":"Unavailable; right-click slot to unequip";
    if(worn.item_id)action=worn.durability>0?"Equipped clothing":"Broken clothing";
    if(s->instance_id&&d->durability_max>0)snprintf(value,sizeof value,"%s\nDurability %.0f / %d\n%s",d->name,s->durability,d->durability_max,action);
    else snprintf(value,sizeof value,"%s\n%d owned\n%s",d->name,pe_inventory_count(&g->inventory,s->item_id),action);
    wrapped(u,value,rel(u,box,10,8,280,108),14,ink);
}
static void build_shape_icon(const PeSandboxUI *u,PeTrainingUIRect r,int shape) {
    PeB45Cell cell={0,0,0,(uint8_t)(shape<<2),0};
    PeVertex vertices[PE_B45_MAX_VERTICES];size_t count=pe_b45_mesh(&cell,vertices,PE_B45_MAX_VERTICES);
    float z=u->layout.scale;
    for(size_t k=0;k<count;k+=3) {
        PeVec3 n=vertices[k].normal;
        if(n.x+n.y+n.z<=0)continue;
        Vector2 p[3];
        for(int j=0;j<3;j++) {
            PeVec3 v=vertices[k+j].position;
            p[j]=(Vector2){r.x+24*z+(v.x-v.z)*42*z,r.y+47*z-v.y*47*z-(v.x+v.z)*19*z};
        }
        Color c=n.y>.5f?(Color){178,205,219,255}:n.x>.5f?(Color){97,141,163,255}:(Color){55,86,102,255};
        DrawTriangle(p[0],p[2],p[1],c);DrawTriangle(p[0],p[1],p[2],c);
    }
}
static void build_hud(const PeSandboxUI *u,const PeGame *g) {
    /* UIBuild.prefab11400564/11400508 use shortbar_1,614x48/614x59.
     * Runtime bottom placement follows the observed1280x720 build screen.
     * Supported subset: source frame/icons, native cube/ramp shape geometry,
     * real inventory counts. No creation/selection/save-ISO commands implied. */
    float z=u->layout.scale;
    sprite(u,"shortbar_1",map(u,333,600,614,48,PE_UI_BOTTOM_CENTER),WHITE);
    sprite(u,"shortbar_1",map(u,333,659,614,59,PE_UI_BOTTOM_CENTER),WHITE);
    PeTrainingUIRect title=map(u,20,136,420,32,PE_UI_TOP_LEFT);
    text(u,"BUILD MODE",title.x,title.y,26,ink,false);
    PeTrainingUIRect help=map(u,25,174,320,240,PE_UI_TOP_LEFT);
    wrapped(u,"B: Enter / Exit\nT: Rotate    V: Cube / Ramp\nG: One / Four cells\nCtrl+Z: Undo   Ctrl+X: Redo\n\nLeft mouse: Place\nAlt + Left mouse: Remove\nRight mouse + move: Look\n\n1-7: Shapes / Materials\nSelect a tool on the toolbar",help,18,blue);
    static const int items[]={0,PE_ITEM_DIRT,PE_ITEM_STONE,PE_ITEM_WOOD,0,PE_ITEM_FENCE,PE_ITEM_LADDER,0,0};
    for(int k=0;k<9;k++) {
        PeTrainingUIRect r=pe_sandbox_ui_build_slot(u,k);
        sprite(u,"itembox",r,WHITE);
        if(k==0||k==4)build_shape_icon(u,r,k==0?PE_B45_CUBE:PE_B45_RAMP);
        else if(items[k])item(u,items[k],pe_inventory_count(&g->inventory,items[k]),r,false,pe_inventory_count(&g->inventory,items[k])?WHITE:dim);
        if((items[k]&&items[k]==u->build_item)||(k==0&&u->build_shape==PE_B45_CUBE)||(k==4&&u->build_shape==PE_B45_RAMP))sprite(u,"select_box",r,blue);
        center(u,TextFormat("%d",k+1),rel(u,r,0,0,48,28),18,(Color){235,245,255,180},false);
    }
    for(int k=0;k<8;k++) {
        PeTrainingUIRect r=pe_sandbox_ui_build_tool(u,k);
        if(k==0)item(u,u->build_item,pe_inventory_count(&g->inventory,u->build_item),r,true,WHITE);
        else if(k==1){sprite(u,"itembox",r,WHITE);build_shape_icon(u,r,u->build_shape);}
        else if(k==7)center(u,"Exit [B]",r,16,ink,false);
        else if(k==5||k==6)button(u,r,k==5?"Undo":"Redo",k==5?pe_build_history_can_undo(g):pe_build_history_can_redo(g));
        else {
            static const char *icons[]={"","","build_point","build_area","build_gjxie","build_save","build_delete"};
            sprite(u,icons[k],rel(u,r,11.5f,8.5f,25,29),k>=5?dim:WHITE);
            if((k==2&&u->build_brush==1)||(k==3&&u->build_brush==4))sprite(u,"select_box",r,blue);
        }
    }
    PeTrainingUIRect detail=map(u,333,567,614,27,PE_UI_BOTTOM_CENTER);
    const PeItemDef *material=pe_item_def(u->build_item);
    if(pe_training_ui_contains(pe_sandbox_ui_build_tool(u,5),u->mouse_x,u->mouse_y)||
       pe_training_ui_contains(pe_sandbox_ui_build_tool(u,6),u->mouse_x,u->mouse_y))
        center(u,"Undo Ctrl+Z / Redo Ctrl+X - last five cube/ramp edits",detail,14,dim,false);
    else center(u,TextFormat("%s   %s   %d cells   %d deg",material?material->name:"",u->build_shape==PE_B45_CUBE?"Cube":"Ramp",u->build_brush,u->build_rotation*90),detail,16,ink,false);
    (void)z;
}
void pe_sandbox_ui_draw(PeSandboxUI *u,const PeGame *g,bool building) {
    if(!u||!u->loaded||!g)return;
    if(building)build_hud(u,g);else hud(u,g,false);
    minimap(u,g);
    if(*pe_sandbox_ui_mission_title(g)) {
        PeTrainingUIRect r=u->layout.mission_tracker;
        wrapped(u,pe_sandbox_ui_mission_title(g),rel(u,r,18,-7,250,65),18,(Color){155,61,244,255});
        wrapped(u,mission_goal(u,g),rel(u,r,18,57,233,120),18,ink);
    }
    if(u->character_open)character(u,g);
    if(u->inventory_open)inventory(u,g);
    if(u->replicator_open)replicator(u,g);
    if(u->missions_open)missions(u,g);
    supplier_dialogue(u,g);town_dialogue(u,g);
    sprite(u,"menu1",u->layout.menu_button,WHITE);
    if(u->menu_open) {
        static const char *labels[]={"Inventory [I]","Character [C]","Replicator [Y]","Mission [Q]","Pause"};
        for(int k=0;k<5;k++)button(u,u->layout.menu_items[k],labels[k],true);
    }
    if(u->seconds<u->message_until&&*u->last_message)wrapped(u,u->last_message,map(u,36,22,600,70,PE_UI_TOP_LEFT),18,ink);
    if(u->dragging)item(u,u->drag_item,0,(PeTrainingUIRect){u->mouse_x-24*u->layout.scale,u->mouse_y-24*u->layout.scale,48*u->layout.scale,48*u->layout.scale},false,WHITE);
    else if(!u->paused)tooltip(u,g);
    if(u->revival_open&&!u->paused) {
        DrawRectangle(0,0,(int)u->layout.width,(int)u->layout.height,(Color){0,0,0,110});
        panel(u,map(u,395,220,490,300,PE_UI_CENTER),g->hp>0?"REVIVING":"REVIVE",PE_UI_EFFECT_INVENTORY);
        int shots=pe_inventory_count(&g->inventory,pe_revival_source.item_id);
        item(u,pe_revival_source.item_id,shots,map(u,455,285,64,64,PE_UI_CENTER),true,WHITE);
        wrapped(u,g->hp>0?"Getting up...":TextFormat("Revival Shot x%d\nRevive here with 33.3%% HP.",shots),
            map(u,535,285,290,85,PE_UI_CENTER),18,ink);
        button(u,pe_sandbox_ui_revival_button(u,0),"1. Revive (1 shot)",g->hp<=0&&shots>0);
        button(u,pe_sandbox_ui_revival_button(u,1),"2. Load saved game",true);
    }
    if(u->paused) {
        DrawRectangle(0,0,(int)u->layout.width,(int)u->layout.height,(Color){0,0,0,145});
        panel(u,map(u,425,200,430,320,PE_UI_CENTER),"PAUSED",PE_UI_EFFECT_INVENTORY);
        static const char *labels[]={"Resume","Save game","Load game","Return to main menu"};
        for(int k=0;k<4;k++)button(u,pe_sandbox_ui_pause_button(u,k),labels[k],true);
    }
    if(pe_sandbox_ui_cursor(u)&&!u->dragging)sprite(u,"mouse",(PeTrainingUIRect){u->mouse_x,u->mouse_y,40*u->layout.scale,40*u->layout.scale},WHITE);
}
