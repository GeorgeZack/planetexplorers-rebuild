#include "pe_build_history.h"
#include "../generated/pe_build_history_source.h"
#include <stdio.h>
#include <string.h>
_Static_assert(PE_BUILD_HISTORY_CAPACITY==PE_BUILD_HISTORY_SOURCE_CAPACITY,"Source construction history capacity changed");
_Static_assert(PE_BUILD_HISTORY_SOURCE_BLOCK_DIVISOR==4,"Source construction material divisor changed");

static bool same_position(const PeB45Cell *a,const PeB45Cell *b){return a->x==b->x&&a->y==b->y&&a->z==b->z;}
static bool empty_cell(const PeB45Cell *c){return !c->x&&!c->y&&!c->z&&!c->type&&!c->material;}
static bool cells_valid(const PeGame *g,const PeB45Cell *cells,unsigned count){
    if(!cells||!count||count>PE_MAX_BUILD_BATCH)return false;
    for(unsigned i=0;i<count;i++){
        const PeB45Cell *c=&cells[i];
        if(!pe_b45_valid(c)||c->x<0||c->y<0||c->z<0||
           c->x>=2*(pe_world_width(&g->world)-1)||c->y>=2*(pe_world_size_y(&g->world)-1)||c->z>=2*(pe_world_depth(&g->world)-1))return false;
        for(unsigned j=0;j<i;j++)if(same_position(c,&cells[j]))return false;
    }
    return true;
}
bool pe_build_history_state_valid(const PeGame *g){
    if(!g)return false;
    const PeBuildHistory *h=&g->build_history;
    if(h->count>PE_BUILD_HISTORY_CAPACITY||h->cursor>h->count)return false;
    for(unsigned i=0;i<PE_BUILD_HISTORY_CAPACITY;i++){
        const PeBuildEdit *e=&h->edits[i];
        if(i<h->count){if(!cells_valid(g,e->cells,e->count))return false;}
        else if(e->count||e->addition)return false;
        for(unsigned j=i<h->count?e->count:0;j<PE_MAX_BUILD_BATCH;j++)if(!empty_cell(&e->cells[j]))return false;
    }
    return true;
}
void pe_build_history_clear(PeGame *g){if(g)memset(&g->build_history,0,sizeof g->build_history);}
bool pe_build_history_can_undo(const PeGame *g){return pe_build_history_state_valid(g)&&g->build_history.cursor>0;}
bool pe_build_history_can_redo(const PeGame *g){return pe_build_history_state_valid(g)&&g->build_history.cursor<g->build_history.count;}
bool pe_build_history_record(PeGame *g,const PeB45Cell *cells,int count,bool addition){
    if(!pe_build_history_state_valid(g)||count<1||!cells_valid(g,cells,(unsigned)count))return false;
    PeBuildEdit edit={0};edit.addition=addition;edit.count=(unsigned)count;
    memcpy(edit.cells,cells,(size_t)count*sizeof *cells);
    PeBuildHistory *h=&g->build_history;
    if(h->cursor==PE_BUILD_HISTORY_CAPACITY){
        memmove(h->edits,h->edits+1,(PE_BUILD_HISTORY_CAPACITY-1)*sizeof h->edits[0]);
        h->cursor--;
    }
    h->edits[h->cursor++]=edit;h->count=h->cursor;
    memset(h->edits+h->count,0,(PE_BUILD_HISTORY_CAPACITY-h->count)*sizeof h->edits[0]);
    return true;
}
static bool replay(PeGame *g,bool undo){
    if(!(undo?pe_build_history_can_undo(g):pe_build_history_can_redo(g))){
        if(g)snprintf(g->message,sizeof g->message,"Nothing to %s.",undo?"undo":"redo");
        return false;
    }
    PeBuildHistory *h=&g->build_history;
    const PeBuildEdit *e=&h->edits[undo?h->cursor-1:h->cursor];
    snprintf(g->message,sizeof g->message,"Cannot %s construction here.",undo?"undo":"redo");
    if(!pe_game_replay_build_cells(g,e->cells,(int)e->count,undo?!e->addition:e->addition))return false;
    if(undo)h->cursor--;else h->cursor++;
    snprintf(g->message,sizeof g->message,"Construction %s.",undo?"undone":"redone");return true;
}
bool pe_build_history_undo(PeGame *g){return replay(g,true);}
bool pe_build_history_redo(PeGame *g){return replay(g,false);}
