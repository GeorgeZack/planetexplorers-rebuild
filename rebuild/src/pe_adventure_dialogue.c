#include "pe_adventure_dialogue.h"
#include "../generated/pe_adventure_dialogue_text.h"
#include <math.h>
#include <string.h>

static bool finite3(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static bool menu_allowed(const PeGame *g){
    return g&&g->mode==PE_GAME_ADVENTURE&&isfinite(g->hp)&&g->hp>0&&
        ((g->welcome_page<PE_ADVENTURE_WELCOME_PAGES&&!g->adventure_mission)||
         g->adventure_mission==PE_ADVENTURE_ARRIVAL||g->adventure_mission==PE_ADVENTURE_SUPPLIES);
}
static bool available(const PeGame *g){
    return menu_allowed(g)&&g->arrival_complete&&g->adventure_mission==PE_ADVENTURE_SUPPLIES&&!g->supplies_claimed;
}
static bool valid(const PeAdventureDialogue *d){
    if(!d)return false;
    if(d->stage==PE_ADVENTURE_DIALOGUE_IDLE)return !d->entity_id&&!d->page&&!d->role;
    return d->entity_id&&(d->role==1||d->role==2)&&((d->stage==PE_ADVENTURE_DIALOGUE_MENU&&!d->page)||
        (d->stage==PE_ADVENTURE_DIALOGUE_TALK&&d->page<5));
}
void pe_adventure_dialogue_init(PeAdventureDialogue *d){if(d)memset(d,0,sizeof *d);}
void pe_adventure_dialogue_cancel(PeAdventureDialogue *d){pe_adventure_dialogue_init(d);}
bool pe_adventure_arrival_step(PeGame *g,PeVec3 supplier){
    if(!g||g->mode!=PE_GAME_ADVENTURE||g->arrival_complete||g->adventure_mission!=PE_ADVENTURE_ARRIVAL||
       !finite3(g->player)||!finite3(supplier)||g->player.y<0||!(g->hp>0)||!isfinite(g->hp))return false;
    /* Original random-target initialization truncates XZ to IntVector2. */
    double ox=g->world.source?g->world.origin_x:0,oz=g->world.source?g->world.origin_z:0;
    double dx=(double)g->player.x+ox-trunc((double)supplier.x+ox),dz=(double)g->player.z+oz-trunc((double)supplier.z+oz);
    if(hypot(dx,dz)>=PE_ADVENTURE_ARRIVAL_RADIUS)return false;
    return pe_game_arrival_complete(g);
}
bool pe_adventure_dialogue_open(PeAdventureDialogue *d,const PeGame *g,int role,uint32_t entity,bool eligible){
    if(!valid(d)||d->stage!=PE_ADVENTURE_DIALOGUE_IDLE||!menu_allowed(g)||(role!=1&&role!=2)||!entity||!eligible)return false;
    *d=(PeAdventureDialogue){.stage=role==1&&g->welcome_page<PE_ADVENTURE_WELCOME_PAGES?PE_ADVENTURE_DIALOGUE_TALK:PE_ADVENTURE_DIALOGUE_MENU,.entity_id=entity,.role=role};return true;
}
bool pe_adventure_dialogue_choose_supply(PeAdventureDialogue *d,const PeGame *g){
    if(!valid(d)||d->role!=2||!available(g)||d->stage!=PE_ADVENTURE_DIALOGUE_MENU)return false;
    d->stage=PE_ADVENTURE_DIALOGUE_TALK;return true;
}
PeAdventureDialogueResult pe_adventure_dialogue_advance(PeAdventureDialogue *d,PeGame *g){
    if(valid(d)&&d->role==1&&d->stage==PE_ADVENTURE_DIALOGUE_TALK){
        if(!pe_game_welcome_advance(g,g?g->welcome_page:0))return PE_ADVENTURE_DIALOGUE_REJECTED;
        if(g->welcome_page==PE_ADVENTURE_WELCOME_PAGES){pe_adventure_dialogue_cancel(d);return PE_ADVENTURE_DIALOGUE_ACCEPTED;}
        return PE_ADVENTURE_DIALOGUE_ADVANCED;
    }
    if(!valid(d)||d->role!=2||!available(g)||d->stage!=PE_ADVENTURE_DIALOGUE_TALK)return PE_ADVENTURE_DIALOGUE_REJECTED;
    if(d->page<4){d->page++;return PE_ADVENTURE_DIALOGUE_ADVANCED;}
    /* The core function stages the entire supply inventory and owns idempotency.
     * Its message explains capacity failure; retaining this page permits retry. */
    if(!pe_game_accept_supplies(g))return PE_ADVENTURE_DIALOGUE_NO_SPACE;
    pe_adventure_dialogue_cancel(d);return PE_ADVENTURE_DIALOGUE_ACCEPTED;
}
const PeAdventureDialogueLine *pe_adventure_dialogue_line(int id){
    for(size_t i=0;i<sizeof pe_adventure_lines/sizeof pe_adventure_lines[0];i++)if(pe_adventure_lines[i].talk_id==id)return &pe_adventure_lines[i];
    return NULL;
}
const PeAdventureDialogueLine *pe_adventure_dialogue_current(const PeAdventureDialogue *d,const PeGame *g){
    if(!valid(d)||!menu_allowed(g))return NULL;
    if(d->role==1){
        if(d->stage!=PE_ADVENTURE_DIALOGUE_TALK||g->welcome_page>=PE_ADVENTURE_WELCOME_PAGES)return NULL;
        return pe_adventure_dialogue_line(g->welcome_page?4080+(int)g->welcome_page:1949);
    }
    if(!available(g))return NULL;
    if(d->stage==PE_ADVENTURE_DIALOGUE_MENU)return pe_adventure_dialogue_line(4089);
    if(d->stage==PE_ADVENTURE_DIALOGUE_TALK)return pe_adventure_dialogue_line(4090+(int)d->page);
    return NULL;
}
static const PeAdventureMissionText *mission(int id){
    for(size_t i=0;i<sizeof pe_adventure_missions/sizeof pe_adventure_missions[0];i++)if(pe_adventure_missions[i].id==id)return &pe_adventure_missions[i];
    return NULL;
}
const char *pe_adventure_mission_title(int id){const PeAdventureMissionText *m=mission(id);return m?m->title:NULL;}
const char *pe_adventure_mission_description(int id){const PeAdventureMissionText *m=mission(id);return m?m->description:NULL;}
const char *pe_adventure_mission_objective(int id){const PeAdventureMissionText *m=mission(id);return m?m->objective:NULL;}
static bool name_valid(const char *name){
    if(!name)return true;
    size_t i=0;for(;i<=128&&name[i];i++)if((unsigned char)name[i]<32||(unsigned char)name[i]==127)return false;
    return i<=128;
}
const char *pe_adventure_dialogue_speaker(const PeAdventureDialogueLine *line,const PeAdventureDialogueBindings *b){
    if(!line||!b)return NULL;
    const char *name=NULL;
    switch(line->speaker){
        case PE_ADVENTURE_SPEAKER_PLAYER:name=b->player_full_name?b->player_full_name:b->player_name;break;
        case PE_ADVENTURE_SPEAKER_WELCOME:name=b->welcome_full_name?b->welcome_full_name:b->welcome_name;break;
        case PE_ADVENTURE_SPEAKER_SUPPLIER:name=b->supplier_full_name?b->supplier_full_name:b->supplier_name;break;
        default:return NULL;
    }
    return name&&name[0]&&name_valid(name)?name:NULL;
}
bool pe_adventure_dialogue_format(const char *text,const PeAdventureDialogueBindings *b,char *output,size_t capacity){
    if(!text||!b||!output||!capacity||!name_valid(b->player_name)||!name_valid(b->welcome_name)||
       !name_valid(b->supplier_name)||!name_valid(b->town_name)||!name_valid(b->seed_name))return false;
    const char *tokens[]={"\"name%\"","\"AdvNPC%\"001","\"AdvNPC%\"002","\"Town%\"004","\"seedname%\""};
    const char *names[]={b->player_name,b->welcome_name,b->supplier_name,b->town_name,b->seed_name};
    char result[2048];size_t written=0,read=0;
    while(text[read]){
        if(read>=2047)return false;
        const char *piece=text+read;size_t length=1,consumed=1;
        for(size_t i=0;i<5;i++)if(names[i]&&names[i][0]&&strncmp(text+read,tokens[i],strlen(tokens[i]))==0){
            piece=names[i];length=strlen(piece);consumed=strlen(tokens[i]);break;
        }
        if(length>=sizeof result-written||length>=capacity-written)return false;
        memcpy(result+written,piece,length);written+=length;read+=consumed;
    }
    result[written]=0;memcpy(output,result,written+1);return true;
}
