#include "pe_town_wallet.h"
#include "pe_game.h"
#include "../generated/pe_town_wallets.h"
#include <limits.h>
#include <math.h>
#include <string.h>

_Static_assert(sizeof pe_town_wallet_sources/sizeof pe_town_wallet_sources[0]==PE_TOWN_WALLET_CAPACITY,"wallet source count");
const PeTownWalletSource *pe_town_wallet_source_at(size_t index){
    return index<PE_TOWN_WALLET_CAPACITY?&pe_town_wallet_sources[index]:NULL;
}
static bool mode_valid(const PeGame *g){
    return g&&(g->mode==PE_GAME_LEGACY||g->mode==PE_GAME_ADVENTURE);
}
bool pe_town_wallet_state_valid(const PeGame *g){
    if(!mode_valid(g))return false;
    for(size_t i=0;i<PE_TOWN_WALLET_CAPACITY;i++){
        const PeTownWallet *w=&g->town_wallet[i];
        if(w->balance>INT32_MAX||(g->mode!=PE_GAME_ADVENTURE&&(w->balance||w->added)))return false;
    }
    return true;
}
bool pe_town_wallet_init(PeGame *g){
    if(!mode_valid(g))return false;
    PeTownWallet next[PE_TOWN_WALLET_CAPACITY]={0};
    for(size_t i=0;i<PE_TOWN_WALLET_CAPACITY;i++){
        if(g->town_wallet[i].balance||g->town_wallet[i].added)return false;
        const PeTownWalletSource *s=pe_town_wallet_source_at(i);
        if(!s->entity_id||s->initial_balance>INT32_MAX||s->serialized_max>INT32_MAX||s->daily_increase>INT32_MAX)return false;
        for(size_t j=0;j<i;j++)if(pe_town_wallet_sources[j].entity_id==s->entity_id)return false;
        if(g->mode==PE_GAME_ADVENTURE)next[i]=(PeTownWallet){s->initial_balance,s->initial_added};
    }
    memcpy(g->town_wallet,next,sizeof next);return true;
}
bool pe_town_wallet_step(PeGame *g){
    if(!pe_town_wallet_state_valid(g)||!isfinite(g->game_seconds)||g->game_seconds<0)return false;
    if(g->mode!=PE_GAME_ADVENTURE)return true;
    /* UTimer.TimeInDay uses divide/floor, then SecondInDay multiplies back.
     * Preserve that order instead of replacing it with remainder arithmetic. */
    double day=g->game_seconds/PE_TOWN_WALLET_SOURCE_DAY_SECONDS;
    double seconds_in_day=(day-floor(day))*PE_TOWN_WALLET_SOURCE_DAY_SECONDS;
    bool morning=seconds_in_day<PE_TOWN_WALLET_SOURCE_HALF_DAY_SECONDS;
    PeTownWallet next[PE_TOWN_WALLET_CAPACITY];memcpy(next,g->town_wallet,sizeof next);
    for(size_t i=0;i<PE_TOWN_WALLET_CAPACITY;i++){
        if(morning)next[i].added=false;
        else if(!next[i].added){
            uint32_t increment=pe_town_wallet_sources[i].daily_increase;
            if(increment>(uint32_t)INT32_MAX-next[i].balance)return false;
            next[i].balance+=increment;next[i].added=true;
        }
    }
    memcpy(g->town_wallet,next,sizeof next);return true;
}
bool pe_town_wallet_balance(const PeGame *g,uint32_t entity,uint32_t *out){
    if(!out||!pe_town_wallet_state_valid(g))return false;
    for(size_t i=0;i<PE_TOWN_WALLET_CAPACITY;i++)if(pe_town_wallet_sources[i].entity_id==entity){
        *out=g->town_wallet[i].balance;return true;
    }
    return false;
}
