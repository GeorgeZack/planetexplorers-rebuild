/* Headless transaction fixture from an existing, physically reached Annot save.
 * No item grants, currency edits, terrain writes or position assignments. The
 * vendor API is exercised directly: this does not claim physical shop travel. */
#include "pe_adventure_medicine.h"
#include "pe_town_interaction.h"
#include "pe_settlement.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static void json_string(const char *s){
    putchar('"');for(const unsigned char *p=(const unsigned char *)s;*p;p++){
        if(*p=='"'||*p=='\\')printf("\\%c",*p);
        else if(*p<32)printf("\\u%04x",*p);else putchar(*p);
    }putchar('"');
}
static void inspect(const PeGame *g){
    printf("{\"medicineState\":%u,\"ready\":%s,\"money\":%u,\"crafted\":%u,\"gameSeconds\":%.17g,\"worldHash\":\"%016" PRIx64 "\",\"sourcePosition\":[%.9g,%.9g,%.9g],\"velocity\":[%.9g,%.9g,%.9g],\"yaw\":%.9g,\"pitch\":%.9g,\"items\":[",
        g->medicine_state,pe_adventure_medicine_ready(g)?"true":"false",g->money,g->crafted,g->game_seconds,pe_world_hash(&g->world),
        (double)g->player.x+g->world.origin_x,(double)g->player.y+g->world.origin_y,(double)g->player.z+g->world.origin_z,
        (double)g->velocity.x,(double)g->velocity.y,(double)g->velocity.z,(double)g->yaw,(double)g->pitch);
    bool comma=false;
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].count){
        const PeSlot *s=&g->inventory.slots[i];printf("%s{\"id\":%d,\"count\":%d,\"instance\":%u}",comma?",":"",s->item_id,s->count,s->instance_id);comma=true;
    }
    printf("],\"knownRecipes\":[");for(int i=0;i<g->known_recipe_count;i++)printf("%s%d",i?",":"",g->known_recipes[i]);printf("]}");
}
int main(int argc,char **argv){
    bool prepare=argc==4&&!strcmp(argv[1],"--prepare"),read_only=argc==3&&!strcmp(argv[1],"--inspect");
    if(!prepare&&!read_only){fprintf(stderr,"Usage: %s --inspect INPUT.pesave | --prepare INPUT.pesave OUTPUT.pesave\n",argv[0]);return 2;}
    struct stat source_stat,output_stat;
    if(prepare&&(!strcmp(argv[2],argv[3])||(stat(argv[2],&source_stat)==0&&stat(argv[3],&output_stat)==0&&source_stat.st_dev==output_stat.st_dev&&source_stat.st_ino==output_stat.st_ino))){fprintf(stderr,"Input must remain separate from output\n");return 2;}
    PeGame *g=calloc(1,sizeof *g),*loaded=calloc(1,sizeof *loaded);PeSandboxCollision *collision=NULL;
    char error[256]={0};bool ok=false;unsigned ticks=0,spent=0;PeVec3 initial={0};uint64_t world_hash=0;uint32_t crafted=0;
    if(!g||!loaded){snprintf(error,sizeof error,"Allocation failed");goto done;}
    if(!pe_game_load(g,argv[2],error,sizeof error))goto done;
    if(read_only){inspect(g);puts("");ok=true;goto done;}
    if(!pe_settlement_is_route(&g->world)||!g->annot_complete||g->medicine_state!=PE_MEDICINE_NONE||g->money!=500||g->craft.recipe_id||pe_inventory_count(&g->inventory,917)){
        snprintf(error,sizeof error,"Require normal-money500, idle, unaccepted medicine route checkpoint at Annot");goto done;
    }
    initial=g->player;world_hash=pe_world_hash(&g->world);crafted=g->crafted;
    printf("{\"schema\":1,\"kind\":\"headless-C-medicine-transactions\",\"input\":");json_string(argv[2]);printf(",\"output\":");json_string(argv[3]);
    printf(",\"physicalVendorTravel\":false,\"before\":");inspect(g);printf(",\"learnedScripts\":[");
    bool comma=false;
    /* Learn existing scripts through the same inventory action as the UI.
     * Each consumed source book releases capacity; no inventory is cleared. */
    for(int i=0;i<pe_inventory_capacity(&g->inventory);i++){
        const PeSlot *slot=&g->inventory.slots[i];const PeItemDef *def=pe_item_def(slot->item_id);
        if(slot->count&&def&&def->learn_kind==PE_LEARN_RECIPE){
            int id=slot->item_id;if(!pe_game_use_slot(g,i)){snprintf(error,sizeof error,"Cannot learn source script%d: %s",id,g->message);goto report;}
            printf("%s%d",comma?",":"",id);comma=true;
        }
    }
    if(!pe_adventure_medicine_accept(g)){snprintf(error,sizeof error,"Acceptance failed: %s",g->message);goto report;}
    int script=-1;for(int i=0;i<pe_inventory_capacity(&g->inventory);i++)if(g->inventory.slots[i].item_id==877&&g->inventory.slots[i].count){script=i;break;}
    if(script<0||!pe_game_use_slot(g,script)||!pe_game_recipe_known(g,337)){snprintf(error,sizeof error,"Cannot learn accepted Herbal Drink script");goto report;}
    printf("%s877],\"purchases\":[",comma?",":"");comma=false;
    const PeRecipeDef *recipe=pe_recipe_def(337);
    if(!recipe||recipe->output_id!=917||recipe->output_count!=1||recipe->ingredient_count!=5||!pe_town_shop_open(g,20028)){snprintf(error,sizeof error,"Recipe337/Xing source contract missing");goto purchase_report;}
    for(int i=0;i<recipe->ingredient_count;i++){
        PeIngredient ingredient=recipe->ingredients[i];unsigned count=(unsigned)ingredient.count*5;const PeTownOffer *offer=NULL;
        for(size_t j=0;j<pe_town_shop_count(20028);j++){const PeTownOffer *candidate=pe_town_shop_offer(20028,j);if(candidate->item_id==ingredient.item_id){offer=candidate;break;}}
        if(!offer||!pe_town_shop_buy(g,20028,offer->store_id,count)){snprintf(error,sizeof error,"Cannot buy required ingredient%d: %s",ingredient.item_id,g->message);goto purchase_report;}
        spent+=offer->price*count;printf("%s{\"vendor\":20028,\"store\":%d,\"item\":%d,\"count\":%u,\"unitPrice\":%u,\"remainingStock\":%u}",comma?",":"",offer->store_id,offer->item_id,count,offer->price,pe_town_shop_stock(g,20028,offer->store_id));comma=true;
    }
    collision=pe_sandbox_collision_create();
    if(!collision||!pe_game_collision_update(g,collision,error,sizeof error)||!pe_game_start_craft(g,337,5)){if(!error[0])snprintf(error,sizeof error,"Craft start failed: %s",g->message);goto purchase_report;}
    while(g->craft.recipe_id&&ticks<1200){
        if(!pe_game_step_collision(g,(PeInput){0},PE_FIXED_DT,collision)){snprintf(error,sizeof error,"Idle simulation rejected state");goto purchase_report;}
        ticks++;
    }
    if(g->craft.recipe_id||!pe_adventure_medicine_ready(g)||pe_inventory_count(&g->inventory,917)!=5||g->crafted!=crafted+5||g->money!=500-spent||pe_world_hash(&g->world)!=world_hash||hypotf(g->player.x-initial.x,g->player.z-initial.z)>.001f||fabsf(g->player.y-initial.y)>.01f){
        snprintf(error,sizeof error,"Timed craft/position/world/money invariant failed");goto purchase_report;
    }
    if(!pe_game_save(g,argv[3],error,sizeof error)||!pe_game_load(loaded,argv[3],error,sizeof error))goto purchase_report;
    ok=pe_adventure_medicine_ready(loaded)&&!loaded->craft.recipe_id&&g->medicine_state==loaded->medicine_state&&g->money==loaded->money&&g->crafted==loaded->crafted&&
        !memcmp(&g->inventory,&loaded->inventory,sizeof g->inventory)&&!memcmp(&g->player,&loaded->player,sizeof g->player)&&!memcmp(&g->velocity,&loaded->velocity,sizeof g->velocity)&&
        !memcmp(g->town_stock,loaded->town_stock,sizeof g->town_stock)&&!memcmp(g->known_recipes,loaded->known_recipes,sizeof g->known_recipes)&&g->known_recipe_count==loaded->known_recipe_count&&g->game_seconds==loaded->game_seconds&&pe_world_hash(&loaded->world)==world_hash;
    if(!ok)snprintf(error,sizeof error,"Save/reload did not preserve medicine transaction state");
 purchase_report:printf("]");goto result;
 report:printf("],\"purchases\":[]");
 result:printf(",\"ticks\":%u,\"fixedDt\":%.9g,\"spent\":%u,\"success\":%s,\"saveReload\":%s,\"error\":",ticks,(double)PE_FIXED_DT,spent,ok?"true":"false",ok?"true":"false");json_string(error);printf(",\"after\":");inspect(g);puts("}");
 done:
    if(!ok)fprintf(stderr,"Medicine fixture: %s\n",error);
    pe_sandbox_collision_free(collision);pe_game_destroy(g);pe_game_destroy(loaded);free(g);free(loaded);return ok?0:1;
}
