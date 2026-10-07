#include "pe_ui_settings.h"
#include <math.h>
#include <stdio.h>

typedef struct {float scale,ox,oy;Rectangle panel,toggle,back;} SettingsLayout;
static Rectangle settings_rect(SettingsLayout l,float x,float y,float w,float h){
    return (Rectangle){l.ox+x*l.scale,l.oy+y*l.scale,w*l.scale,h*l.scale};
}
static SettingsLayout settings_layout(void){
    SettingsLayout l={0};
    l.scale=fminf(GetScreenWidth()/1280.f,GetScreenHeight()/720.f);
    l.ox=(GetScreenWidth()-1280*l.scale)/2;l.oy=(GetScreenHeight()-720*l.scale)/2;
    l.panel=settings_rect(l,240,170,800,380);
    l.toggle=settings_rect(l,340,250,600,46);
    l.back=settings_rect(l,540,465,200,45);
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
void pe_ui_settings_open(PeUiSettings *settings){if(settings){settings->open=true;settings->selected=0;}}
bool pe_ui_settings_update(PeUiSettings *settings,bool input_allowed){
    if(!settings||!settings->open)return false;
    if(!input_allowed||!IsWindowFocused())return true;
    if(IsKeyPressed(KEY_ESCAPE)){settings->open=false;return true;}
    if(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_UP))settings->selected=1-settings->selected;
    SettingsLayout l=settings_layout();Vector2 mouse=GetMousePosition();
    bool click=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if((click&&CheckCollisionPointRec(mouse,l.back))||(settings->selected==1&&IsKeyPressed(KEY_ENTER))){
        settings->open=false;return true;
    }
    if(settings->ready&&((click&&CheckCollisionPointRec(mouse,l.toggle))||
       (settings->selected==0&&(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE))))){
        PePreferences next=settings->preferences;next.camera_facing_attacks=!next.camera_facing_attacks;
        if(pe_preferences_save(&next,settings->path,settings->error,sizeof settings->error))settings->preferences=next;
        else fprintf(stderr,"Settings: %s\n",settings->error);
    }
    return true;
}
void pe_ui_settings_draw(const PeUiSettings *settings,const PeUiAssets *ui){
    if(!settings||!settings->open)return;
    SettingsLayout l=settings_layout();float scale=l.scale;
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,.8f));
    if(!pe_ui_sprite(ui,"midbg",l.panel,WHITE,scale))DrawRectangleRec(l.panel,(Color){24,45,79,255});
    settings_center(pe_ui_font_heading(ui),"Settings",settings_rect(l,260,190,760,40),26*scale,WHITE);
    char label[128];snprintf(label,sizeof label,"Camera-facing attacks: %s",settings->preferences.camera_facing_attacks?"ON":"OFF");
    settings_button(ui,label,l.toggle,scale,settings->ready,settings->selected==0);
    settings_center(pe_ui_font(ui,false),"Free-mouse third person only. Applies to tutorial and sandbox.",settings_rect(l,260,310,760,28),17*scale,(Color){190,215,235,255});
    settings_center(pe_ui_font(ui,false),"ON: aim new attacks along the camera. OFF: movement / character facing.",settings_rect(l,260,341,760,28),17*scale,(Color){190,215,235,255});
    settings_center(pe_ui_font(ui,false),"Sandbox nearby-target assistance still applies. Changes save automatically.",settings_rect(l,260,372,760,28),17*scale,(Color){190,215,235,255});
    if(settings->error[0])settings_center(pe_ui_font_small(ui,false),"Settings could not be read or saved; see terminal for details.",settings_rect(l,260,410,760,28),16*scale,(Color){238,201,118,255});
    settings_button(ui,"Back",l.back,scale,true,settings->selected==1);
    if(ui&&IsWindowFocused())pe_ui_sprite(ui,"mouse",(Rectangle){GetMouseX(),GetMouseY(),40*scale,40*scale},WHITE,scale);
}
