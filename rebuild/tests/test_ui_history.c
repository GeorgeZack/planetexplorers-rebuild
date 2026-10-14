/* Headless shared presentation contract: real renderer/wrapping, recorded GPU
 * calls and synthetic font metrics. No display, assets or game mutation. */
#include "pe_ui_history.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#c);exit(1);}}while(0)
typedef struct {int kind;char text[2048];Rectangle box;Color tint;int font;} Draw;
static Draw calls[4096];static size_t count;static int scissors;
static void record(int kind,const char *text,Rectangle r,Color color,int font){CHECK(count<4096);Draw *d=&calls[count++];memset(d,0,sizeof *d);d->kind=kind;d->box=r;d->tint=color;d->font=font;if(text)snprintf(d->text,sizeof d->text,"%s",text);}
bool pe_ui_sprite(const PeUiAssets *a,const char *s,Rectangle r,Color c,float z){(void)a;(void)z;record(1,s,r,c,0);return true;}
bool pe_ui_sprite_flip(const PeUiAssets *a,const char *s,Rectangle r,Color c,bool h,bool v){(void)a;(void)h;(void)v;record(1,s,r,c,0);return true;}
Font pe_ui_font(const PeUiAssets *a,bool bold){(void)a;return (Font){.baseSize=bold?19:18};}
Font pe_ui_font_small(const PeUiAssets *a,bool bold){(void)a;return (Font){.baseSize=bold?15:14};}
Font pe_ui_font_heading(const PeUiAssets *a){(void)a;return (Font){.baseSize=32};}
Vector2 MeasureTextEx(Font f,const char *s,float size,float spacing){(void)f;(void)spacing;return (Vector2){strlen(s)*size*.55f,size};}
void DrawTextEx(Font f,const char *s,Vector2 p,float size,float spacing,Color c){(void)spacing;record(2,s,(Rectangle){p.x,p.y,MeasureTextEx(f,s,size,0).x,size},c,f.baseSize);}
void DrawRectangleRec(Rectangle r,Color c){record(3,NULL,r,c,0);}
void DrawRectangleLinesEx(Rectangle r,float width,Color c){(void)width;record(4,NULL,r,c,0);}
void BeginScissorMode(int x,int y,int w,int h){CHECK(w>=0&&h>=0);scissors++;record(5,NULL,(Rectangle){x,y,w,h},WHITE,0);}
void EndScissorMode(void){CHECK(scissors>0);scissors--;record(6,NULL,(Rectangle){0},WHITE,0);}
void pe_training_ui_shader_draw(PeTrainingUiShader *fx,PeUiEffect role,Rectangle r,float seconds,float alpha,bool refresh){(void)fx;(void)seconds;(void)alpha;(void)refresh;record(7,NULL,r,WHITE,role);}
static const Draw *find(int kind,const char *text){for(size_t i=0;i<count;i++)if(calls[i].kind==kind&&!strcmp(calls[i].text,text))return &calls[i];return NULL;}
static void reset(void){CHECK(scissors==0);count=0;memset(calls,0,sizeof calls);}
static bool near(float a,float b){return fabsf(a-b)<.01f;}
int main(void){
    PeConversationHistory *history=calloc(1,sizeof *history);CHECK(history);
    CHECK(pe_conversation_history_add(history,"Andy","one\ntwo",NULL));
    const int sizes[][2]={{1280,720},{640,480},{320,240}};
    for(size_t k=0;k<sizeof sizes/sizeof *sizes;k++){
        PeTrainingUILayout layout;PeUIWindows windows={0};CHECK(pe_training_ui_windows_layout(sizes[k][0],sizes[k][1],&windows,&layout));
        PeUiCanvas canvas={.layout=&layout,.mouse_x=-1,.mouse_y=-1};
        PeUiHistoryState state={.open=true,.scroll=-1};
        reset();pe_ui_history_draw(&canvas,history,&state);CHECK(scissors==0);
        const Draw *speaker=find(2,"Andy :"),*body=find(2,"onetwo");CHECK(speaker&&body);
        CHECK(speaker->tint.r==69&&speaker->tint.g==208&&speaker->tint.b==255&&speaker->font==19);
        CHECK(body->tint.r==255&&body->tint.g==255&&body->tint.b==255);
        CHECK(!strcmp(history->entries[0].content,"one\ntwo"));CHECK(state.scroll==0);
        CHECK(near(state.content_height,40*layout.scale));
        CHECK(find(2,"Clear")&&find(1,"close1"));
        int clip_depth=0,entry_text=0,begin_count=0;
        for(size_t i=0;i<count;i++){
            if(calls[i].kind==5){begin_count++;clip_depth++;CHECK(near(calls[i].box.x,floorf(layout.history_content.x)));CHECK(near(calls[i].box.width,ceilf(layout.history_content.w)));}
            if(calls[i].kind==6)clip_depth--;
            if(calls[i].kind==2&&strcmp(calls[i].text,"Clear")){entry_text++;CHECK(clip_depth==1);}
        }
        CHECK(entry_text==2&&begin_count==1&&!clip_depth);
    }
    pe_conversation_history_clear(history);
    char content[PE_CONVERSATION_CONTENT_CAPACITY];
    for(unsigned i=0;i<PE_CONVERSATION_HISTORY_CAPACITY;i++){
        snprintf(content,sizeof content,"Record %u contains several wrapped words to fill this scrolling conversation history. Older entries stay available when scrolling upward, while the newest entry remains at the bottom.",i);
        CHECK(pe_conversation_history_add(history,"A source speaker",content,NULL));
    }
    PeUIWindows windows={0};PeTrainingUILayout layout;CHECK(pe_training_ui_windows_layout(1280,720,&windows,&layout));
    PeUiCanvas canvas={.layout=&layout,.mouse_x=-1,.mouse_y=-1};PeUiHistoryState state={.open=true,.scroll=-1};
    reset();pe_ui_history_draw(&canvas,history,&state);CHECK(scissors==0&&state.content_height>state.height);
    float bottom=state.content_height-state.height;CHECK(near(state.scroll,bottom));CHECK(find(1,"pullOut")&&find(1,"pullOut_bg"));
    pe_ui_history_update(&canvas,history,&state,1);CHECK(near(state.scroll,bottom-40));
    pe_ui_history_update(&canvas,history,&state,0);CHECK(near(state.scroll,bottom-40));
    PeUiHistoryState pure=state;pe_ui_history_scroll_input(&pure,&layout,2,false,0);
    pe_ui_history_update(&canvas,history,&state,2);CHECK(near(state.scroll,pure.scroll));
    pe_ui_history_seek(&canvas,history,&state,layout.history_scrollbar.y);CHECK(state.scroll==0);
    pe_ui_history_seek(&canvas,history,&state,layout.history_scrollbar.y+layout.history_scrollbar.h);CHECK(near(state.scroll,bottom));
    pe_ui_history_scroll_input(&state,&layout,1000,false,0);CHECK(state.scroll==0);
    PeTrainingUIRect thumb=pe_ui_history_thumb(&state,&layout);
    CHECK(pe_ui_history_pointer(&state,&layout,thumb.x+1,thumb.y+thumb.h*.25f,0,true,false,false));
    CHECK(state.scroll_dragging&&state.scroll==0);
    CHECK(pe_ui_history_pointer(&state,&layout,-100,layout.history_scrollbar.y+layout.history_scrollbar.h+100,0,false,false,false));
    CHECK(near(state.scroll,bottom)&&state.scroll_dragging);
    CHECK(pe_ui_history_pointer(&state,&layout,-100,layout.history_scrollbar.y+layout.history_scrollbar.h+100,0,false,true,false));
    CHECK(!state.scroll_dragging&&near(state.scroll,bottom));
    thumb=pe_ui_history_thumb(&state,&layout);
    CHECK(pe_ui_history_pointer(&state,&layout,thumb.x+1,thumb.y+1,0,true,false,false));
    CHECK(pe_ui_history_pointer(&state,&layout,thumb.x+1,thumb.y+1,0,false,false,true));
    CHECK(!state.scroll_dragging);
    state.scroll_dragging=true;
    CHECK(pe_training_ui_windows_layout(640,480,&windows,&layout));pe_ui_history_update(&canvas,history,&state,0);
    CHECK(near(state.scroll,state.content_height-state.height)&&!state.scroll_dragging); /* reflow bottom and cancel obsolete grab */
    state.scroll=NAN;pe_ui_history_update(&canvas,history,&state,0);CHECK(isfinite(state.scroll));
    state.open=false;reset();pe_ui_history_draw(&canvas,history,&state);CHECK(count==0);
    pe_conversation_history_clear(history);state.open=true;pe_ui_history_update(&canvas,history,&state,0);CHECK(state.scroll==0&&state.content_height==0);
    /* Maximum native text bound, including a single long UTF-8 token. Shared
       flow's draw and measurement must agree without altering owned bytes. */
    size_t n=0;while(n+2<sizeof content){content[n++]=(char)0xc3;content[n++]=(char)0xa9;}content[n]=0;
    CHECK(pe_conversation_history_add(history,"Name",content,NULL));
    reset();pe_ui_history_draw(&canvas,history,&state);CHECK(scissors==0&&state.content_height>0&&!strcmp(history->entries[0].content,content));
    history->count=PE_CONVERSATION_HISTORY_CAPACITY+1;reset();pe_ui_history_draw(&canvas,history,&state);CHECK(count==0);
    free(history);puts("conversation history shared layout, clipping, LF display and scroll pass");return 0;
}
