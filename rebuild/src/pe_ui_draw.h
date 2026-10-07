#ifndef PE_UI_DRAW_H
#define PE_UI_DRAW_H
#include "pe_ui_assets.h"
#include "pe_training_ui_layout.h"
#include "pe_training_ui_shader.h"
/* Borrowed frame-local presentation only: no gameplay, input or ownership. */
typedef struct {
    const PeUiAssets *assets;
    PeTrainingUiShader *effects;
    const PeTrainingUILayout *layout;
    float seconds,mouse_x,mouse_y;
} PeUiCanvas;
extern const Color PE_UI_INK,PE_UI_BLUE,PE_UI_GOLD,PE_UI_DIM;
Rectangle pe_ui_draw_rect(PeTrainingUIRect r);
PeTrainingUIRect pe_ui_draw_map(const PeUiCanvas *c,float x,float y,float w,float h,PeTrainingUIAnchor anchor);
Font pe_ui_draw_font(const PeUiCanvas *c,float size,bool bold);
void pe_ui_draw_sprite(const PeUiCanvas *c,const char *name,PeTrainingUIRect r,Color tint);
void pe_ui_draw_effect(const PeUiCanvas *c,PeUiEffect effect,PeTrainingUIRect r);
void pe_ui_draw_text(const PeUiCanvas *c,const char *text,float x,float y,float size,Color tint,bool bold);
void pe_ui_draw_center(const PeUiCanvas *c,const char *text,PeTrainingUIRect r,float size,Color tint,bool bold);
/* Word wrapping with pixel clipping and explicit row gap; returns used pixels. */
float pe_ui_draw_wrap(const PeUiCanvas *c,const char *text,PeTrainingUIRect r,float size,Color tint,bool bold,float gap);
void pe_ui_draw_item(const PeUiCanvas *c,int id,int count,PeTrainingUIRect r,bool box,Color tint);
void pe_ui_draw_button(const PeUiCanvas *c,PeTrainingUIRect r,const char *label,bool enabled);
void pe_ui_draw_panel(const PeUiCanvas *c,PeTrainingUIRect r,const char *title,PeUiEffect effect);
void pe_ui_draw_inventory_frame(const PeUiCanvas *c,bool storage,int tab);
void pe_ui_draw_pagination(const PeUiCanvas *c,PeTrainingUIRect r,int page,int pages);
void pe_ui_draw_dialogue_frame(const PeUiCanvas *c,const char *speaker);
void pe_ui_draw_continue(const PeUiCanvas *c);
#endif
