#include "pe_hare_skill.h"
#include "../generated/pe_hare_skill_source.h"
#include <math.h>
#include <string.h>
const PeHareSkillDef *pe_hare_skill_def(unsigned attack){return attack<5?&pe_hare_skill_source[attack]:NULL;}
static bool valid(const PeHareSkill*s){
    return s&&s->initialized&&pe_hare_skill_def(s->attack)&&s->guide<pe_hare_skill_source[s->attack].guide_count&&
        s->phase>=PE_HARE_SKILL_IDLE&&s->phase<=PE_HARE_SKILL_DONE&&s->skip_flags<=7&&
        isfinite(s->seconds)&&s->seconds>=0&&s->seconds<=86400&&isfinite(s->phase_seconds)&&s->phase_seconds>=0&&s->phase_seconds<=86400&&
        s->contact_enabled==(s->active&&s->phase==PE_HARE_SKILL_MAIN)&&
        (!s->active||(s->phase!=PE_HARE_SKILL_IDLE&&s->phase!=PE_HARE_SKILL_DONE&&!s->completed));
}
void pe_hare_skill_init(PeHareSkill*s){if(s){memset(s,0,sizeof *s);s->initialized=true;}}
static double duration(const PeHareSkill*s){
    const PeHareSkillDef*d=pe_hare_skill_def(s->attack);
    switch(s->phase){
    case PE_HARE_SKILL_PREPARE_PRE:return d->prepare_pre;
    case PE_HARE_SKILL_PREPARE_POST:return d->prepare_post;
    case PE_HARE_SKILL_PRE:return d->pre[s->guide];
    case PE_HARE_SKILL_MAIN:return d->main[s->guide];
    case PE_HARE_SKILL_POST:return d->post[s->guide];
    case PE_HARE_SKILL_ENDING_PRE:return d->ending_pre;
    case PE_HARE_SKILL_ENDING_POST:return d->ending_post;
    default:return 0;
    }
}
static unsigned skip_bit(PeHareSkillPhase p){return p==PE_HARE_SKILL_PRE?PE_HARE_SKILL_SKIP_PRE:p==PE_HARE_SKILL_MAIN?PE_HARE_SKILL_SKIP_MAIN:p==PE_HARE_SKILL_POST?PE_HARE_SKILL_SKIP_POST:0;}
static bool settle(PeHareSkill*s){
    for(unsigned guard=0;guard<16&&s->active;guard++){
        double wait=duration(s);
        if(wait>0&&s->phase_seconds<wait&&!(s->skip_flags&skip_bit(s->phase))){s->contact_enabled=s->phase==PE_HARE_SKILL_MAIN;return true;}
        s->phase_seconds=0;s->contact_enabled=false;
        if(s->phase==PE_HARE_SKILL_POST){
            if(s->guide+1<pe_hare_skill_def(s->attack)->guide_count){s->guide++;s->skip_flags=0;s->phase=PE_HARE_SKILL_PRE;}
            else s->phase=PE_HARE_SKILL_ENDING_PRE;
        }else if(s->phase==PE_HARE_SKILL_ENDING_POST){s->phase=PE_HARE_SKILL_DONE;s->active=false;s->completed=true;}
        else s->phase=(PeHareSkillPhase)(s->phase+1);
    }
    return !s->active;
}
bool pe_hare_skill_begin(PeHareSkill*s,unsigned attack){
    if(!valid(s)||s->active||!pe_hare_skill_def(attack))return false;
    PeHareSkill next={.initialized=true,.active=true,.attack=attack,.phase=PE_HARE_SKILL_PREPARE_PRE};
    if(!settle(&next))return false;
    *s=next;return true;
}
bool pe_hare_skill_step(PeHareSkill*s,unsigned skip_requests){
    if(!valid(s)||skip_requests>7)return false;
    if(!s->active)return true;
    PeHareSkill next=*s;next.seconds+=PE_HARE_SKILL_DT;next.phase_seconds+=PE_HARE_SKILL_DT;next.skip_flags|=skip_requests;
    if(!settle(&next)||!valid(&next))return false;
    *s=next;return true;
}
void pe_hare_skill_cancel(PeHareSkill*s){if(s&&s->initialized){s->active=s->completed=s->contact_enabled=false;s->phase=PE_HARE_SKILL_IDLE;s->phase_seconds=0;s->skip_flags=0;}}
