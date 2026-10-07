#ifndef PE_UI_HISTORY_STATE_H
#define PE_UI_HISTORY_STATE_H
#include "pe_training_ui_layout.h"
#include <math.h>
/* Session-local presentation only. Hosts own open/clear/input, and set scroll
 * to -1 on opening or accepted history insertion to request the newest row. */
typedef struct {
    bool open;
    float scroll,content_height,width,height;
    bool scroll_dragging;
    float scroll_grab;
} PeUiHistoryState;
/* The same thumb bounds serve rendering, initial grab and held dragging. */
static inline PeTrainingUIRect pe_ui_history_thumb(const PeUiHistoryState *s,const PeTrainingUILayout *l){
    if(!s||!l)return (PeTrainingUIRect){0};
    PeTrainingUIRect track=l->history_scrollbar;
    float maximum=fmaxf(0,s->content_height-l->history_content.h);
    float height=s->content_height>0?fminf(track.h,fmaxf(18*l->scale,track.h*l->history_content.h/s->content_height)):track.h;
    float scroll=isfinite(s->scroll)&&s->scroll>=0?s->scroll:maximum;
    float offset=maximum>0?fminf(maximum,scroll)/maximum*(track.h-height):0;
    return (PeTrainingUIRect){track.x,track.y+offset,track.w,height};
}
/* CPU-only input mapping, using metrics from the last draw. No font/GPU call,
 * no history mutation; usable from both hosts' headless modal routes. */
static inline void pe_ui_history_scroll_input(PeUiHistoryState *s,const PeTrainingUILayout *l,float wheel,bool seek,float pointer_y){
    if(!s||!l||!isfinite(l->scale)||l->scale<=0||!isfinite(s->content_height)||!isfinite(l->history_content.h))return;
    float maximum=fmaxf(0,s->content_height-l->history_content.h);
    if(!isfinite(s->scroll)||s->scroll<0)s->scroll=maximum;
    if(isfinite(wheel))s->scroll-=wheel*40*l->scale;
    PeTrainingUIRect track=l->history_scrollbar;
    if(seek&&isfinite(pointer_y)&&isfinite(track.h)&&track.h>0){
        float thumb=pe_ui_history_thumb(s,l).h;
        s->scroll=track.h>thumb?maximum*fminf(1,fmaxf(0,(pointer_y-track.y-thumb*.5f)/(track.h-thumb))):0;
    }
    s->scroll=fminf(maximum,fmaxf(0,s->scroll));
}

/* A scrollbar press owns its held/release frames even outside the track.
 * Grabbing an existing thumb preserves the pointer offset; clicking the track
 * jumps its center then starts a drag. Caller still owns the whole modal. */
static inline bool pe_ui_history_pointer(PeUiHistoryState *s,const PeTrainingUILayout *l,float x,float y,float wheel,bool pressed,bool released,bool cancel){
    if(!s||!l)return false;
    bool owned=s->scroll_dragging;
    if(cancel||!s->open||!isfinite(x)||!isfinite(y)){
        s->scroll_dragging=false;return owned;
    }
    if(!s->scroll_dragging)pe_ui_history_scroll_input(s,l,
        pe_training_ui_contains(l->history,x,y)?wheel:0,false,0);
    if(pressed&&pe_training_ui_contains(l->history_scrollbar,x,y)&&s->content_height>l->history_content.h){
        PeTrainingUIRect thumb=pe_ui_history_thumb(s,l);
        if(pe_training_ui_contains(thumb,x,y))s->scroll_grab=y-thumb.y;
        else{
            pe_ui_history_scroll_input(s,l,0,true,y);
            s->scroll_grab=pe_ui_history_thumb(s,l).h*.5f;
        }
        s->scroll_dragging=true;owned=true;
    }
    if(s->scroll_dragging){
        PeTrainingUIRect thumb=pe_ui_history_thumb(s,l),track=l->history_scrollbar;
        float maximum=fmaxf(0,s->content_height-l->history_content.h);
        s->scroll=track.h>thumb.h?maximum*fminf(1,fmaxf(0,(y-track.y-s->scroll_grab)/(track.h-thumb.h))):0;
        if(released)s->scroll_dragging=false;
    }
    return owned;
}
#endif
