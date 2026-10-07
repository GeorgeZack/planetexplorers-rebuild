#include "pe_town_defense_spawn.h"
#include "../generated/pe_adventure_town_defense_source.h"
#include <math.h>
#include <string.h>
_Static_assert(sizeof pe_town_defense_source_groups/sizeof pe_town_defense_source_groups[0]==4,"Two source groups per wave required");

static bool finite3(PeVec3 p){return isfinite(p.x)&&isfinite(p.y)&&isfinite(p.z);}
static bool zero3(PeVec3 p){return p.x==0&&p.y==0&&p.z==0;}
static bool same3(PeVec3 a,PeVec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
static bool frame_valid(PeVec3 center,PeVec3 forward){
    return finite3(center)&&finite3(forward)&&hypot((double)forward.x,forward.z)>1e-5;
}
static const PeTownDefenseSourceGroup *group(unsigned wave,unsigned index){
    if(wave>1||index>1)return NULL;
    return &pe_town_defense_source_groups[wave*2+index];
}
static PeVec3 anchor(PeVec3 center,PeVec3 forward,float angle,float radius){
    /* Positive Unity Y rotation sends forward+Z toward+X. Flatten before
     * rotation and normalize before radius; radius is not sqrt-distributed. */
    double radians=(double)angle*0.01745329251994329577;
    double scale=radius/hypot((double)forward.x,forward.z),c=cos(radians),s=sin(radians);
    return (PeVec3){(float)(center.x+(forward.x*c+forward.z*s)*scale),
        PE_TOWN_DEFENSE_DEFERRED_Y,(float)(center.z+(forward.z*c-forward.x*s)*scale)};
}
static bool request_zero(PeTownDefenseSpawnRequest r){
    return !r.spawn_type&&!r.prototype&&!r.can_ride&&r.angle_degrees==0&&r.radius==0&&zero3(r.anchor);
}
static bool request_valid(PeTownDefenseSpawnRequest r,const PeTownDefenseSourceGroup *g,PeVec3 center,PeVec3 forward){
    if(!g||r.can_ride||r.spawn_type!=g->spawn_type||r.prototype!=g->prototype||!isfinite(r.angle_degrees)||!isfinite(r.radius)||
       r.angle_degrees<g->angle_min||r.angle_degrees>g->angle_max||r.radius<PE_TOWN_DEFENSE_RADIUS_MIN||r.radius>PE_TOWN_DEFENSE_RADIUS_MAX||!finite3(r.anchor))return false;
    return same3(r.anchor,anchor(center,forward,r.angle_degrees,r.radius));
}
bool pe_town_defense_plan_valid(const PeTownDefenseWavePlan *p){
    if(!p||p->wave>1||!frame_valid(p->center,p->forward)||p->count<10||p->count>PE_TOWN_DEFENSE_WAVE_CAPACITY)return false;
    unsigned first=p->count-2;
    const PeTownDefenseSourceGroup *g0=group(p->wave,0),*g1=group(p->wave,1);
    if(first<(unsigned)g0->count_min||first>=(unsigned)g0->count_max_exclusive)return false;
    for(unsigned i=0;i<PE_TOWN_DEFENSE_WAVE_CAPACITY;i++){
        if(i>=p->count){if(!request_zero(p->requests[i]))return false;}
        else if(!request_valid(p->requests[i],i<first?g0:g1,p->center,p->forward))return false;
    }
    return true;
}
bool pe_town_defense_plan_wave(unsigned wave,PeVec3 center,PeVec3 forward,PeTownDefenseDraws draws,PeTownDefenseWavePlan *out){
    if(!out||wave>1||!frame_valid(center,forward)||!draws.integer||!draws.real)return false;
    PeTownDefenseWavePlan plan={.wave=wave,.center=center,.forward=forward};
    for(unsigned j=0;j<2;j++){
        const PeTownDefenseSourceGroup *g=group(wave,j);int count=0;
        if(!draws.integer(draws.context,g->count_min,g->count_max_exclusive,&count)||count<g->count_min||count>=g->count_max_exclusive)return false;
        if((unsigned)count>PE_TOWN_DEFENSE_WAVE_CAPACITY-plan.count)return false;
        for(int k=0;k<count;k++){
            PeTownDefenseSpawnRequest r={.spawn_type=g->spawn_type,.prototype=g->prototype};
            if(!draws.real(draws.context,(float)g->angle_min,(float)g->angle_max,&r.angle_degrees)||!isfinite(r.angle_degrees)||r.angle_degrees<g->angle_min||r.angle_degrees>g->angle_max)return false;
            if(!draws.real(draws.context,PE_TOWN_DEFENSE_RADIUS_MIN,PE_TOWN_DEFENSE_RADIUS_MAX,&r.radius)||!isfinite(r.radius)||r.radius<PE_TOWN_DEFENSE_RADIUS_MIN||r.radius>PE_TOWN_DEFENSE_RADIUS_MAX)return false;
            r.anchor=anchor(center,forward,r.angle_degrees,r.radius);if(!finite3(r.anchor))return false;
            plan.requests[plan.count++]=r;
        }
    }
    if(!pe_town_defense_plan_valid(&plan))return false;
    *out=plan;return true;
}
bool pe_town_defense_spawn_init(PeTownDefenseSpawn *s,uint32_t beacon,PeVec3 center,PeVec3 forward){
    if(!s||!beacon||!frame_valid(center,forward))return false;
    *s=(PeTownDefenseSpawn){.beacon_id=beacon,.center=center,.forward=forward};return true;
}
bool pe_town_defense_spawn_valid(const PeTownDefenseSpawn *s){
    if(!s||!s->beacon_id||!frame_valid(s->center,s->forward)||s->waves_admitted>2||s->count>PE_TOWN_DEFENSE_AGENT_CAPACITY||s->deaths>PE_TOWN_DEFENSE_KILL_GOAL||s->complete!=(s->deaths==PE_TOWN_DEFENSE_KILL_GOAL))return false;
    unsigned deaths=0,wave_count[2]={0};PeTownDefenseWavePlan plans[2]={{.wave=0,.center=s->center,.forward=s->forward},{.wave=1,.center=s->center,.forward=s->forward}};
    for(unsigned i=0;i<PE_TOWN_DEFENSE_AGENT_CAPACITY;i++){
        const PeTownDefenseMember *m=&s->members[i];
        if(i>=s->count){
            if(m->agent_id||m->entity_id||m->wave||m->state!=PE_TOWN_DEFENSE_AGENT_EMPTY||!request_zero(m->request)||!zero3(m->position)||m->default_player_id||m->camp_id)return false;
            continue;
        }
        if(m->agent_id!=i+1||m->wave>=s->waves_admitted||m->wave>1||(i&&m->wave<s->members[i-1].wave)||wave_count[m->wave]>=PE_TOWN_DEFENSE_WAVE_CAPACITY)return false;
        plans[m->wave].requests[wave_count[m->wave]++]=m->request;
        if(m->state==PE_TOWN_DEFENSE_AGENT_PENDING){
            if(m->entity_id||!zero3(m->position)||m->default_player_id||m->camp_id)return false;
        }else if(m->state==PE_TOWN_DEFENSE_AGENT_CREATED||m->state==PE_TOWN_DEFENSE_AGENT_DEAD){
            if(!m->entity_id||m->entity_id==s->beacon_id||!finite3(m->position)||m->position.x!=m->request.anchor.x||m->position.z!=m->request.anchor.z||m->default_player_id!=PE_TOWN_DEFENSE_PLAYER_ID||m->camp_id!=PE_TOWN_DEFENSE_CAMP_ID)return false;
            for(unsigned j=0;j<i;j++)if(s->members[j].entity_id==m->entity_id)return false;
            deaths+=m->state==PE_TOWN_DEFENSE_AGENT_DEAD;
        }else return false;
    }
    for(unsigned wave=0;wave<s->waves_admitted;wave++){plans[wave].count=wave_count[wave];if(!pe_town_defense_plan_valid(&plans[wave]))return false;}
    return deaths==s->deaths&&(s->waves_admitted||!s->count);
}
bool pe_town_defense_spawn_admit(PeTownDefenseSpawn *s,const PeTownDefenseWavePlan *p){
    if(!pe_town_defense_spawn_valid(s)||s->complete||!pe_town_defense_plan_valid(p)||p->wave!=s->waves_admitted||!same3(p->center,s->center)||!same3(p->forward,s->forward)||p->count>PE_TOWN_DEFENSE_AGENT_CAPACITY-s->count)return false;
    PeTownDefenseSpawn staged=*s;
    for(unsigned i=0;i<p->count;i++){
        unsigned index=staged.count++;
        staged.members[index]=(PeTownDefenseMember){.agent_id=index+1,.wave=p->wave,.state=PE_TOWN_DEFENSE_AGENT_PENDING,.request=p->requests[i]};
    }
    staged.waves_admitted++;if(!pe_town_defense_spawn_valid(&staged))return false;
    *s=staged;return true;
}
bool pe_town_defense_spawn_created(PeTownDefenseSpawn *s,uint32_t agent,uint32_t entity,int prototype,PeVec3 position,bool supported){
    if(!pe_town_defense_spawn_valid(s)||s->complete||!supported||!agent||agent>s->count||!entity||entity==s->beacon_id||!finite3(position))return false;
    PeTownDefenseMember *m=&s->members[agent-1];
    if(m->state!=PE_TOWN_DEFENSE_AGENT_PENDING||m->request.prototype!=prototype||position.x!=m->request.anchor.x||position.z!=m->request.anchor.z)return false;
    for(unsigned i=0;i<s->count;i++)if(s->members[i].entity_id==entity)return false;
    m->entity_id=entity;m->position=position;m->default_player_id=PE_TOWN_DEFENSE_PLAYER_ID;m->camp_id=PE_TOWN_DEFENSE_CAMP_ID;m->state=PE_TOWN_DEFENSE_AGENT_CREATED;return true;
}
PeTownDefenseDeathResult pe_town_defense_spawn_death(PeTownDefenseSpawn *s,uint32_t beacon,uint32_t entity){
    if(!pe_town_defense_spawn_valid(s)||s->complete||beacon!=s->beacon_id||!entity)return PE_TOWN_DEFENSE_DEATH_REJECTED;
    for(unsigned i=0;i<s->count;i++){
        PeTownDefenseMember *m=&s->members[i];if(m->entity_id!=entity)continue;
        if(m->state!=PE_TOWN_DEFENSE_AGENT_CREATED)return PE_TOWN_DEFENSE_DEATH_REJECTED;
        m->state=PE_TOWN_DEFENSE_AGENT_DEAD;s->deaths++;s->complete=s->deaths==PE_TOWN_DEFENSE_KILL_GOAL;
        return s->complete?PE_TOWN_DEFENSE_DEATH_COMPLETED:PE_TOWN_DEFENSE_DEATH_CREDITED;
    }
    return PE_TOWN_DEFENSE_DEATH_REJECTED;
}
