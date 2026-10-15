/* Read-only current-loader interpretation of one native hunt checkpoint.
 * Does not advance simulation, save, create a fixture or initialize graphics.
 * Older-save migrations happen in memory only, exactly as in the game loader. */
#include "pe_paths.h"
#include "pe_game.h"
#include "pe_block.h"
#include "pe_settlement.h"
#include "pe_adventure_hunt.h"
#include "pe_town_interaction.h"
#include "pe_town_wallet.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
    if(argc<2||argc>3){fprintf(stderr,"Usage: %s SAVE [ASSETS]\n",argv[0]);return 2;}
    char default_assets[4096];
    if(argc==2&&!pe_paths_default_assets(default_assets,sizeof default_assets)){
        fprintf(stderr,"Cannot locate assets beside the executable; supply ASSETS.\n");return 1;
    }
    const char *assets=argc==3?argv[2]:default_assets;
    char error[256];PeGame *g=calloc(1,sizeof *g);if(!g)return 1;
    if(!pe_settlement_configure(assets,error,sizeof error)||!pe_game_load(g,argv[1],error,sizeof error)){
        fprintf(stderr,"%s\n",error);pe_game_destroy(g);free(g);return 1;
    }
    printf("{\"schema\":6,\"interpretation\":\"current-loader\",\"mode\":%u,"
        "\"hunt_state\":%u,\"hunt_kills\":%u,\"pending_return_talk\":%d,"
        "\"source_position\":[%.17g,%.17g,%.17g],\"hunt_destination\":[%.9g,%.9g,%.9g],"
        "\"hp\":%.9g,\"max_hp\":%.9g,\"stamina\":%.9g,\"comfort\":%.9g,"
        "\"game_seconds\":%.17g,\"money\":%u,\"equipped_instance\":%u,\"outfit_defense\":%.9g,"
        "\"medpacks\":%d,\"juice\":%d,\"revival_shots\":%d,"
        "\"medpack_cooldown\":%.9g,\"juice_cooldown\":%.17g,\"reward_scripts\":[%d,%d,%d,%d],\"outfit\":[",
        (unsigned)g->mode,g->hunt_state,g->hunt_kills,pe_adventure_hunt_return_talk(g),
        (double)g->player.x+g->world.origin_x,(double)g->player.y+g->world.origin_y,(double)g->player.z+g->world.origin_z,
        (double)g->hunt_destination.x,(double)g->hunt_destination.y,(double)g->hunt_destination.z,
        (double)g->hp,(double)g->max_hp,(double)g->stamina,(double)g->comfort,g->game_seconds,g->money,
        g->equipped_instance_id,(double)pe_outfit_defense(&g->outfit),
        pe_inventory_count(&g->inventory,925),pe_inventory_count(&g->inventory,916),pe_inventory_count(&g->inventory,937),
        (double)g->medpack_cooldown,g->juice_cooldown,pe_inventory_count(&g->inventory,592),
        pe_inventory_count(&g->inventory,593),pe_inventory_count(&g->inventory,594),pe_inventory_count(&g->inventory,595));
    for(unsigned i=0;i<PE_OUTFIT_SLOTS;i++)printf("%s{\"item_id\":%d,\"instance_id\":%u,\"durability\":%.9g}",
        i?",":"",g->outfit.items[i].item_id,g->outfit.items[i].instance_id,(double)g->outfit.items[i].durability);
    printf("],\"yaw\":%.9g,\"pitch\":%.9g,\"velocity\":[%.9g,%.9g,%.9g],"
        "\"grounded\":%s,\"inventory_capacity\":%d,\"inventory_categorized\":%s,\"inventory\":[",
        (double)g->yaw,(double)g->pitch,(double)g->velocity.x,(double)g->velocity.y,(double)g->velocity.z,
        g->grounded?"true":"false",pe_inventory_capacity(&g->inventory),g->inventory.categorized?"true":"false");
    bool comma=false;
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].count){
        const PeSlot *s=&g->inventory.slots[i];
        printf("%s{\"slot\":%d,\"category\":%d,\"item_id\":%d,\"count\":%d,\"instance_id\":%u,\"durability\":%.9g}",
            comma?",":"",i,pe_item_def(s->item_id)->tab,s->item_id,s->count,s->instance_id,(double)s->durability);comma=true;
    }
    printf("],\"quickslots\":[");
    for(int i=0;i<PE_QUICKSLOTS;i++)printf("%s{\"key\":%d,\"item_id\":%d,\"instance_id\":%u}",
        i?",":"",i+1,g->quickslots[i].item_id,g->quickslots[i].instance_id);
    printf("],\"hunt_loot\":[");
    for(unsigned i=0;i<g->hunt_loot.count;i++){
        const PeHuntLootDrop *d=&g->hunt_loot.drops[i];
        printf("%s{\"instance_id\":%u,\"item_id\":%d,\"count\":%d,\"source_position\":[%.9g,%.9g,%.9g],\"created_seconds\":%.17g}",
            i?",":"",d->instance_id,d->item_id,d->count,(double)d->position.x+g->world.origin_x,
            (double)d->position.y+g->world.origin_y,(double)d->position.z+g->world.origin_z,d->created_seconds);
    }
    /* Schema 3: saved hunger and the stock of every supported food offer. */
    printf("],\"hunger\":{\"value\":%.9g,\"starving\":%s},\"food_offers\":[",
        (double)g->hunger.hunger,g->hunger.starving?"true":"false");
    comma=false;
    for(size_t i=0;i<PE_TOWN_STOCK_CAPACITY;i++){
        const PeTownOffer *o=pe_town_shop_offer_at(i);
        if(!o||!pe_hunger_is_food(o->item_id))continue;
        const PeTownStock *t=&g->town_stock[i];
        printf("%s{\"offer\":%zu,\"entity_id\":%u,\"item_id\":%d,\"price\":%u,\"limit\":%u,"
            "\"initialized\":%s,\"remaining\":%u,\"create_time\":%.17g}",
            comma?",":"",i,o->entity_id,o->item_id,o->price,o->limit,t->initialized?"true":"false",t->remaining,t->create_time);
        comma=true;
    }
    /* Schema 4: every persisted vendor purse, in stable source order. Buyback rows are
     * transient and never saved, so none are reported. */
    printf("],\"town_wallets\":[");
    comma=false;
    for(size_t i=0;i<PE_TOWN_WALLET_CAPACITY;i++){
        const PeTownWalletSource *w=pe_town_wallet_source_at(i);if(!w)continue;
        printf("%s{\"index\":%zu,\"entity_id\":%u,\"balance\":%u,\"added\":%s}",
            comma?",":"",i,w->entity_id,g->town_wallet[i].balance,g->town_wallet[i].added?"true":"false");
        comma=true;
    }
    /* Schema 5: the camp repair machine's saved job (PECSv20): its one reserved
     * item and the remaining CounterScript ticks; all zero when empty. */
    const PeCampRepair *r=&g->repair;
    printf("],\"repair\":{\"occupied\":%s,\"item_id\":%d,\"count\":%d,\"instance_id\":%u,"
        "\"durability\":%.9g,\"running\":%s,\"remaining_ticks\":%lld}",
        r->item.count>0?"true":"false",r->item.item_id,r->item.count,r->item.instance_id,(double)r->item.durability,
        r->running?"true":"false",(long long)r->remaining_ticks);
    /* Schema 6: the saved Block45 cells (PeGame.cells) with the original two-byte
     * encoding split into shape (type>>2) and rotation (type&3), the material's
     * item, and the built statistic. Construction history is transient and never
     * saved, so it is not reported. */
    printf(",\"built\":%u,\"cell_count\":%d,\"cells\":[",g->built,g->cell_count);
    for(int i=0;i<g->cell_count&&i<PE_MAX_B45_CELLS;i++){
        const PeB45Cell *c=&g->cells[i];
        printf("%s{\"x\":%d,\"y\":%d,\"z\":%d,\"type\":%u,\"shape\":%u,\"rotation\":%u,\"material\":%u,\"item_id\":%d}",
            i?",":"",c->x,c->y,c->z,(unsigned)c->type,(unsigned)(c->type>>2),(unsigned)(c->type&3),(unsigned)c->material,pe_b45_material_item(c->material));
    }
    puts("]}");
    pe_game_destroy(g);free(g);return 0;
}
