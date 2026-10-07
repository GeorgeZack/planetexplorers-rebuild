#include "pe_ui_settings.h"
#include "pe_ui_draw.h"
#include <math.h>
#include <stdio.h>

typedef struct {float scale,ox,oy;Rectangle panel,toggle,back,debug_toggle,debug_actions[3];} SettingsLayout;
static Rectangle settings_rect(SettingsLayout l,float x,float y,float w,float h){
    return (Rectangle){l.ox+x*l.scale,l.oy+y*l.scale,w*l.scale,h*l.scale};
}
static SettingsLayout settings_layout(bool debug){
    SettingsLayout l={0};
    l.scale=fminf(GetScreenWidth()/1280.f,GetScreenHeight()/720.f);
    l.ox=(GetScreenWidth()-1280*l.scale)/2;l.oy=(GetScreenHeight()-720*l.scale)/2;
    l.panel=settings_rect(l,240,170,800,380);
    l.toggle=settings_rect(l,340,250,600,46);
    l.back=settings_rect(l,540,465,200,45);
    if(debug){
        l.panel=settings_rect(l,240,45,800,635);
        l.toggle=settings_rect(l,340,112,600,42);
        l.debug_toggle=settings_rect(l,340,288,600,42);
        for(unsigned i=0;i<3;i++)l.debug_actions[i]=settings_rect(l,340,340+48*i,600,40);
        l.back=settings_rect(l,540,600,200,45);
    }
    return l;
}
static void settings_center(Font font,const char *text,Rectangle r,float size,Color color){
    Vector2 m=MeasureTextEx(font,text,size,0);
    DrawTextEx(font,text,(Vector2){r.x+(r.width-m.x)/2,r.y+(r.height-m.y)/2},size,0,color);
}
static void settings_button(const PeUiAssets *ui,const char *text,Rectangle r,float scale,bool enabled,bool selected){
    if(selected||CheckCollisionPointRec(GetMousePosition(),r)){
        DrawRectangleGradientH((int)r.x,(int)r.y,(int)r.width,(int)r.height,
            (Color){37,76,112,30},(Color){76,145,211,105});
        DrawRectangle((int)(r.x+r.width-2*scale),(int)r.y,(int)(2*scale),(int)r.height,(Color){117,221,255,220});
    }
    settings_center(pe_ui_font(ui,false),text,r,20*scale,enabled?(Color){172,217,242,255}:(Color){81,107,123,255});
}
void pe_ui_settings_init(PeUiSettings *settings){
    if(!settings)return;
    *settings=(PeUiSettings){0};
    settings->ready=pe_preferences_default_path(settings->path,sizeof settings->path,settings->error,sizeof settings->error);
    if(!settings->ready||!pe_preferences_load(&settings->preferences,settings->path,settings->error,sizeof settings->error))
        fprintf(stderr,"Settings: %s\n",settings->error);
}
void pe_ui_settings_open(PeUiSettings *settings){if(settings){settings->open=true;settings->selected=0;settings->debug_request=PE_DEBUG_NONE;}}
bool pe_ui_settings_update(PeUiSettings *settings,bool input_allowed){
    if(!settings)return false;
    settings->debug_request=PE_DEBUG_NONE;
    if(!settings->debug_available)settings->debug_enabled=false;
    if(!settings->open)return false;
    if(!input_allowed||!IsWindowFocused())return true;
    if(IsKeyPressed(KEY_ESCAPE)){settings->open=false;return true;}
    int back=settings->debug_available?5:1;
    if(settings->selected<0||settings->selected>back||
       (settings->debug_available&&!settings->debug_enabled&&settings->selected>=2&&settings->selected<=4))settings->selected=0;
    if(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_UP)){
        int step=IsKeyPressed(KEY_DOWN)?1:-1;
        do{settings->selected=(settings->selected+step+back+1)%(back+1);}
        while(settings->debug_available&&!settings->debug_enabled&&settings->selected>=2&&settings->selected<=4);
    }
    SettingsLayout l=settings_layout(settings->debug_available);Vector2 mouse=GetMousePosition();
    bool click=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    int chosen=-1;
    if(click){
        if(CheckCollisionPointRec(mouse,l.back))chosen=back;
        else if(CheckCollisionPointRec(mouse,l.toggle))chosen=0;
        else if(settings->debug_available){
            if(CheckCollisionPointRec(mouse,l.debug_toggle))chosen=1;
            for(int i=0;i<3;i++)if(CheckCollisionPointRec(mouse,l.debug_actions[i]))chosen=i+2;
        }
    }
    bool space_toggle=IsKeyPressed(KEY_SPACE)&&(settings->selected==0||
        (settings->debug_available&&settings->selected==1));
    if(chosen<0&&(IsKeyPressed(KEY_ENTER)||space_toggle))chosen=settings->selected;
    if(chosen==back){
        settings->open=false;return true;
    }
    if(chosen==0&&settings->ready){
        PePreferences next=settings->preferences;next.camera_facing_attacks=!next.camera_facing_attacks;
        if(pe_preferences_save(&next,settings->path,settings->error,sizeof settings->error))settings->preferences=next;
        else fprintf(stderr,"Settings: %s\n",settings->error);
    }
    else if(settings->debug_available&&chosen==1){
        settings->debug_enabled=!settings->debug_enabled;
        settings->selected=1;
    }
    else if(settings->debug_available&&settings->debug_enabled&&chosen>=2&&chosen<=4){
        const PeDebugAction actions[]={PE_DEBUG_MEDICINE,PE_DEBUG_COMBAT_SUPPLIES,PE_DEBUG_START_HUNT};
        settings->debug_request=actions[chosen-2];settings->selected=chosen;
    }
    return true;
}
void pe_ui_settings_draw(const PeUiSettings *settings,const PeUiAssets *ui){
    if(!settings||!settings->open)return;
    bool debug=settings->debug_available;
    SettingsLayout l=settings_layout(debug);float scale=l.scale;
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,.8f));
    if(!pe_ui_sprite(ui,"midbg",l.panel,WHITE,scale))DrawRectangleRec(l.panel,(Color){24,45,79,255});
    settings_center(pe_ui_font_heading(ui),"Settings",settings_rect(l,260,debug?60:190,760,40),26*scale,WHITE);
    char label[128];snprintf(label,sizeof label,"Camera-facing attacks: %s",settings->preferences.camera_facing_attacks?"ON":"OFF");
    settings_button(ui,label,l.toggle,scale,settings->ready,settings->selected==0);
    settings_center(pe_ui_font(ui,false),"Free-mouse third person only. Applies to tutorial and sandbox.",settings_rect(l,260,debug?163:310,760,28),17*scale,(Color){190,215,235,255});
    settings_center(pe_ui_font(ui,false),"ON: aim new attacks along the camera. OFF: movement / character facing.",settings_rect(l,260,debug?194:341,760,28),17*scale,(Color){190,215,235,255});
    settings_center(pe_ui_font(ui,false),"Sandbox target assistance applies. Camera preference saves automatically.",settings_rect(l,260,debug?225:372,760,28),17*scale,(Color){190,215,235,255});
    if(settings->error[0])settings_center(pe_ui_font_small(ui,false),"Settings could not be read or saved; see terminal for details.",settings_rect(l,260,debug?254:410,760,28),16*scale,(Color){238,201,118,255});
    if(debug){
        snprintf(label,sizeof label,"Debug tools: %s",settings->debug_enabled?"ON":"OFF");
        settings_button(ui,label,l.debug_toggle,scale,true,settings->selected==1);
        if(settings->debug_enabled){
            const char *names[]={"Give 5 Herbal Drinks","Combat supplies + heal","Skip to hare hunt"};
            for(unsigned i=0;i<3;i++)settings_button(ui,names[i],l.debug_actions[i],scale,true,settings->selected==(int)i+2);
        }else settings_center(pe_ui_font(ui,false),"Enable debug tools for this session to show actions.",settings_rect(l,260,370,760,28),17*scale,(Color){190,215,235,255});
        settings_center(pe_ui_font(ui,false),"Debug actions change this game. Resume, then F9 to save.",settings_rect(l,260,493,760,28),17*scale,(Color){238,201,118,255});
        if(settings->debug_notice[0]){
            Rectangle area=settings_rect(l,270,536,740,54);
            PeTrainingUILayout layout={.scale=scale};
            PeUiCanvas canvas={.assets=ui,.layout=&layout};
            pe_ui_draw_wrap(&canvas,settings->debug_notice,
                (PeTrainingUIRect){area.x,area.y,area.width,area.height},
                16,(Color){190,215,235,255},false,2);
        }
    }
    settings_button(ui,"Back",l.back,scale,true,settings->selected==(debug?5:1));
    if(ui&&IsWindowFocused())pe_ui_sprite(ui,"mouse",(Rectangle){GetMouseX(),GetMouseY(),40*scale,40*scale},WHITE,scale);
}
