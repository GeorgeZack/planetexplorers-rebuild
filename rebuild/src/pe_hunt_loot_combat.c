#include "pe_hunt_loot_combat.h"
#include "pe_hunt_loot.h"

bool pe_hunt_loot_combat_step(PeGame *game,PeHuntCombat *combat,
    const PeSandboxCollision *collision,PeHuntCombatPlayer player,
    PeSandboxMelee *melee,const PeSandboxMeleeFrame *frame,
    PeSandboxMeleeUpdate *update,bool *completed){
    if(!game||!combat||!melee||!frame||!update||!completed||
       game->mode!=PE_GAME_ADVENTURE)return false;
    /* All copied objects below own scalar/transient state only. */
    PeHuntCombat next=*combat;PeSandboxMelee blade=*melee;
    PeOutfit outfit=player.outfit?*player.outfit:(PeOutfit){0};
    PeSlot shield=player.shield?*player.shield:(PeSlot){0};
    PeSlot weapon=player.weapon?*player.weapon:(PeSlot){0};
    PeHuntCombatPlayer staged=player;
    if(player.outfit)staged.outfit=&outfit;
    if(player.shield)staged.shield=&shield;
    if(player.weapon)staged.weapon=&weapon;
    float hp=game->hp,stamina=game->stamina;unsigned kills=game->hunt_kills;
    PeSandboxMeleeUpdate result;bool finished=false;
    if(!pe_hunt_combat_step(&next,&game->world,collision,staged,&blade,frame,
        &hp,&stamina,&kills,&result,&finished))return false;
    PeVec3 positions[PE_HUNT_ENCOUNTER_COUNT];size_t count=0;
    for(size_t i=0;i<PE_HUNT_ENCOUNTER_COUNT;i++){
        if(result.status[i]!=PE_KNIFE_CONTACT_HIT||!result.contact[i].died)continue;
        const PeHuntEncounterActor *before=&combat->encounter.actors[i];
        const PeHuntEncounterActor *after=&next.encounter.actors[i];
        if(!before->present||before->prototype!=13||before->hp<=0||
           before->entity_id!=after->entity_id||after->hp!=0)return false;
        positions[count++]=after->motion.position;
    }
    /* spawn stages loot, RNG and the shared identity counter internally. There
     * are no fallible operations after it succeeds, so no death loses its loot. */
    if(count&&!pe_hunt_loot_spawn(game,positions,count))return false;
    if(player.outfit)*player.outfit=outfit;
    if(player.shield)*player.shield=shield;
    if(player.weapon)*player.weapon=weapon;
    game->hp=hp;game->stamina=stamina;game->hunt_kills=kills;
    *combat=next;*melee=blade;*update=result;*completed=finished;
    return true;
}
