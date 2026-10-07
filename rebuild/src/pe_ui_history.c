#include "pe_ui_history.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool valid(const PeUiCanvas *c,const PeConversationHistory *h){
    return c&&c->layout&&isfinite(c->layout->scale)&&c->layout->scale>0&&
        isfinite(c->layout->history_content.w)&&c->layout->history_content.w>0&&
        isfinite(c->layout->history_content.h)&&c->layout->history_content.h>0&&
        pe_conversation_history_valid(h);
}
static float entries(const PeUiCanvas *c,const PeConversationHistory *h,float scroll,bool draw){
    PeTrainingUIRect box=c->layout->history_content;float z=c->layout->scale;
    float y=box.y-scroll,start=y,width=fmaxf(1,box.w-10*z);
    for(unsigned k=0;k<h->count;k++){
        char speaker[PE_CONVERSATION_SPEAKER_CAPACITY+4],body[PE_CONVERSATION_CONTENT_CAPACITY];
        snprintf(speaker,sizeof speaker,"%s :",h->entries[k].speaker);
        /* NpcTalkHistoryWnd.AddHistroy removes LF for presentation only. */
        size_t at=0;for(const char *s=h->entries[k].content;*s;s++)if(*s!='\n')body[at++]=*s;body[at]=0;
        y+=pe_ui_draw_flow(c,speaker,(PeTrainingUIRect){box.x+5*z,y,width,0},18,(Color){69,208,255,255},true,2,draw);
        y+=pe_ui_draw_flow(c,body,(PeTrainingUIRect){box.x+5*z,y,width,0},18,WHITE,true,2,draw);
        if(k+1<h->count)y+=10*z; /* Source UITable.padding.y=10. */
    }
    return y-start;
}
void pe_ui_history_update(const PeUiCanvas *c,const PeConversationHistory *h,PeUiHistoryState *state,float wheel){
    if(!state||!valid(c,h))return;
    PeTrainingUIRect box=c->layout->history_content;float height=entries(c,h,0,false);
    float maximum=fmaxf(0,height-box.h);
    bool reflow=state->width!=box.w||state->height!=box.h||state->content_height!=height;
    if(reflow)state->scroll_dragging=false;
    if(reflow||!isfinite(state->scroll)||state->scroll<0)state->scroll=maximum;
    state->width=box.w;state->height=box.h;state->content_height=height;
    pe_ui_history_scroll_input(state,c->layout,wheel,false,0);
}
void pe_ui_history_seek(const PeUiCanvas *c,const PeConversationHistory *h,PeUiHistoryState *state,float pointer_y){
    if(!state||!valid(c,h)||!isfinite(pointer_y))return;
    pe_ui_history_update(c,h,state,0);
    pe_ui_history_scroll_input(state,c->layout,0,true,pointer_y);
}
void pe_ui_history_draw(const PeUiCanvas *c,const PeConversationHistory *h,PeUiHistoryState *state){
    if(!state||!state->open||!valid(c,h))return;
    pe_ui_history_update(c,h,state,0);
    const PeTrainingUILayout *l=c->layout;float z=l->scale;PeTrainingUIRect p=l->history;
    /* NpcTalkHistoryWnd has no title widget. Its source header remains blank. */
    pe_ui_draw_sprite(c,"midbg",p,WHITE);pe_ui_draw_sprite(c,"midbg",p,(Color){0,33,255,77});
    pe_ui_draw_sprite(c,"insidebox2",(PeTrainingUIRect){p.x+10*z,p.y+34*z,640*z,400*z},WHITE);
    pe_ui_draw_sprite(c,"close1",l->history_close,pe_training_ui_contains(l->history_close,c->mouse_x,c->mouse_y)?PE_UI_BLUE:(Color){146,156,255,255});
    bool clear_hover=pe_training_ui_contains(l->history_clear,c->mouse_x,c->mouse_y);
    /* Source Clear texture is guang4, absent from the atlas; use the shared
     * supported button sprite at the exact authored80x37 bounds. */
    pe_ui_draw_sprite(c,"menulist",l->history_clear,clear_hover?PE_UI_BLUE:WHITE);
    pe_ui_draw_center(c,"Clear",l->history_clear,18,(Color){255,255,255,179},true);
    PeTrainingUIRect box=l->history_content;
    BeginScissorMode((int)floorf(box.x),(int)floorf(box.y),(int)ceilf(box.w),(int)ceilf(box.h));
    entries(c,h,state->scroll,true);EndScissorMode();
    if(state->content_height>box.h){
        PeTrainingUIRect track=l->history_scrollbar;
        pe_ui_draw_sprite(c,"pullOut_bg",track,(Color){255,255,255,150});
        pe_ui_draw_sprite(c,"pullOut",pe_ui_history_thumb(state,l),WHITE);
    }
}
