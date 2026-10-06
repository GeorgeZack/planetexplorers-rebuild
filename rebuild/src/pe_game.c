#include "pe_game.h"
#include "pe_shield.h"
#include "pe_sword.h"
#include "pe_tree_collision.h"
#include "pe_settlement.h"
#include "../generated/pe_stamina_recovery_source.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void say(PeGame *g, const char *s) { snprintf(g->message, sizeof g->message, "%s", s); }
static void bump(uint32_t *v) { if (*v != UINT32_MAX) ++*v; }
static bool finite3(PeVec3 v) { return isfinite(v.x) && isfinite(v.y) && isfinite(v.z); }
static float length3(PeVec3 v) { return sqrtf(v.x*v.x+v.y*v.y+v.z*v.z); }
static bool normalize(PeVec3 *v) {
    if (!finite3(*v)) return false;
    float l = length3(*v);
    if (!isfinite(l) || l < 0.00001f) return false;
    v->x /= l; v->y /= l; v->z /= l;
    return true;
}
bool pe_game_static_segment_clear(const PeGame *g,PeVec3 start,PeVec3 end){
    if(!g||!finite3(start)||!finite3(end))return false;
    if(!pe_settlement_matches(&g->world))return true;
    const PeSceneCollision *scene=pe_settlement_collision_for_world(&g->world,NULL,0);if(!scene)return false;
    PeVec3 delta={end.x-start.x,end.y-start.y,end.z-start.z};float range=length3(delta);
    if(!isfinite(range))return false;
    /* Target contact is allowed; nearer immutable triangles block interaction. */
    return range<=.005f||!pe_scene_raycast(scene,start,delta,range-.005f).hit;
}
static bool static_box_clear(const PeGame *g,PeVec3 lo,PeVec3 hi){
    if(!pe_settlement_matches(&g->world))return true;
    const PeSceneCollision *scene=pe_settlement_collision_for_world(&g->world,NULL,0);
    return scene&&!pe_scene_overlaps_aabb(scene,lo,hi);
}
int pe_inventory_count(const PeInventory *p, int id) {
    int n = 0;
    for (int i=0;i<PE_INVENTORY_SLOTS;i++) if (p->slots[i].item_id == id) {
        if (p->slots[i].count < 0 || n > INT_MAX-p->slots[i].count) return INT_MAX;
        n += p->slots[i].count;
    }
    return n;
}
bool pe_inventory_add(PeInventory *p, int id, int count) {
    const PeItemDef *def=pe_item_def(id);
    if (!def || def->stack_limit < 1 || count <= 0) return false;
    PeInventory copy=*p;
    if(!copy.next_instance_id)copy.next_instance_id=1;
    int left=count;
    if(def->stack_limit==1){
        for(int i=0;i<PE_INVENTORY_SLOTS&&left;i++)if(copy.slots[i].count==0){
            if(copy.next_instance_id==UINT32_MAX)return false;
            copy.slots[i]=(PeSlot){id,1,copy.next_instance_id++,(float)def->durability_max};--left;
        }
    }else{
        for (int pass=0;pass<2;pass++) for (int i=0;i<PE_INVENTORY_SLOTS && left;i++) {
            PeSlot *slot=&copy.slots[i];
            if ((pass==0 && slot->item_id!=id) || (pass==1 && slot->count!=0)) continue;
            if (slot->count < 0 || slot->count > def->stack_limit) return false;
            int take=def->stack_limit-slot->count;
            if (take>left) take=left;
            slot->item_id=id; slot->count+=take; left-=take;
        }
    }
    if (left) return false;
    *p=copy; return true;
}
bool pe_inventory_remove(PeInventory *p, int id, int count) {
    if (count <= 0 || !pe_item_def(id) || pe_inventory_count(p,id)<count) return false;
    PeInventory copy=*p;
    for (int i=0;i<PE_INVENTORY_SLOTS && count;i++) {
        PeSlot *slot=&copy.slots[i];
        if (slot->item_id!=id) continue;
        int take=slot->count<count?slot->count:count;
        slot->count-=take; count-=take;
        if (!slot->count) memset(slot,0,sizeof *slot);
    }
    *p=copy; return true;
}
const PeSlot *pe_game_equipped_tool(const PeGame *g){
    if(!g->equipped_instance_id)return NULL;
    for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].instance_id==g->equipped_instance_id&&g->inventory.slots[i].count==1)return &g->inventory.slots[i];
    return NULL;
}
bool pe_game_equip_slot(PeGame *g,int index){
    if(!g||index<0||index>=PE_INVENTORY_SLOTS||!(g->hp>0))return false;
    PeSlot *slot=&g->inventory.slots[index];const PeItemDef *def=pe_item_def(slot->item_id);
    bool supported=def&&(def->id==10||def->tool_kind!=PE_TOOL_NONE||
        (g->mode==PE_GAME_ADVENTURE&&(def->equip_position&16)&&(def->item_type==9||def->item_type==10||def->item_type==14)));
    if(!supported||def->durability_max<=0||!slot->instance_id||slot->count!=1){say(g,"This equipment action is not implemented.");return false;}
    if(def->id==1&&!pe_sword_slot_valid(slot)){say(g,"Invalid sword instance.");return false;}
    if(slot->durability<=0&&def->id!=1){say(g,"That tool is broken.");return false;}
    g->equipped_instance_id=slot->instance_id;say(g,def->id==10?"Torch equipped: left click to hold, P to place, R to stow.":"Equipment selected.");return true;
}
void pe_game_unequip(PeGame *g){if(g){g->equipped_instance_id=0;say(g,"Equipment put away.");}}
bool pe_game_tool_stats(const PeGame *g,PeToolStats *stats){
    if(!stats)return false;
    memset(stats,0,sizeof *stats);const PeSlot *slot=pe_game_equipped_tool(g);
    const PeItemDef *def=slot?pe_item_def(slot->item_id):NULL;
    if(!def)return false;
    float attack=def->id==1?pe_sword_attack(slot,pe_player_defaults.attack):pe_player_defaults.attack+def->attack;
    if(!isfinite(attack))return false;
    stats->attack=attack;
    stats->mining_damage=stats->attack*(1.0f+pe_player_defaults.resource_damage+def->resource_damage)*5.0f;
    stats->mining_radius=pe_player_defaults.resource_range+def->resource_range;
    stats->stamina_cost=def->stamina_cost*pe_player_defaults.stamina_reduce_percent;
    stats->durability_cost=def->durability_cost;stats->tool_kind=def->tool_kind;
    return true;
}
static bool consume_materials(PeInventory *p,const PeRecipeDef *r){
    if(!r||r->workspace!=0||r->ingredient_count<1||r->ingredient_count>PE_RECIPE_INPUTS)return false;
    PeInventory copy=*p;
    for(int i=0;i<r->ingredient_count;i++)if(!pe_inventory_remove(&copy,r->ingredients[i].item_id,r->ingredients[i].count))return false;
    *p=copy;return true;
}
static bool output_count(const PeRecipeDef *r,int batches,int *count){
    if(!r||r->output_count<1||batches<1||batches>INT_MAX/r->output_count)return false;
    *count=batches*r->output_count;return true;
}
static bool deliver_pending(PeGame *g){
    if(!g->craft.batches_pending)return true;
    const PeRecipeDef *r=pe_recipe_def(g->craft.recipe_id);int count;
    if(!output_count(r,g->craft.batches_pending,&count)||!pe_inventory_add(&g->inventory,r->output_id,count))return false;
    g->craft.batches_pending=0;g->craft.delivery_elapsed=0;return true;
}
bool pe_game_start_craft(PeGame *g,int id,int batches){
    if(!g||!(g->hp>0))return false;
    const PeRecipeDef *r=pe_recipe_def(id);
    if(!pe_game_recipe_known(g,id)){say(g,"Learn this recipe from its script first.");return false;}
    if(g->craft.recipe_id){say(g,"A crafting job is already running.");return false;}
    if(!r||!isfinite(r->seconds)||r->seconds<=0||batches<1||batches>9999){say(g,"Invalid recipe or batch count.");return false;}
    PeInventory total=g->inventory;
    for(int i=0;i<r->ingredient_count;i++){
        int count=r->ingredients[i].count;
        if(count<=0||batches>INT_MAX/count||!pe_inventory_remove(&total,r->ingredients[i].item_id,count*batches)){
            say(g,"Not enough materials for that many batches.");return false;
        }
    }
    PeInventory probe=g->inventory;
    if(!consume_materials(&probe,r)||!pe_inventory_add(&probe,r->output_id,r->output_count)){say(g,"Craft needs ingredients and room for its output.");return false;}
    g->craft=(PeCraftJob){.recipe_id=id,.batches_left=batches};say(g,"Crafting started.");return true;
}
bool pe_game_cancel_craft(PeGame *g){
    if(!g->craft.recipe_id)return false;
    if(!deliver_pending(g)){say(g,"Make room for completed products before cancelling.");return false;}
    memset(&g->craft,0,sizeof g->craft);say(g,"Craft cancelled; completed products recovered.");return true;
}
static void step_craft(PeGame *g,float dt){
    if(!g->craft.recipe_id)return;
    const PeRecipeDef *r=pe_recipe_def(g->craft.recipe_id);
    if(!r||r->seconds<=0)return;
    g->craft.delivery_elapsed=fminf(6.0f,g->craft.delivery_elapsed+dt);
    g->craft.elapsed+=dt;
    while(g->craft.batches_left&&g->craft.elapsed>=r->seconds){
        PeInventory consumed=g->inventory,capacity;
        int count;
        if(!consume_materials(&consumed,r)||!output_count(r,g->craft.batches_pending+1,&count)){
            g->craft.elapsed=r->seconds;say(g,"Craft paused: missing materials.");break;
        }
        capacity=consumed;
        if(!pe_inventory_add(&capacity,r->output_id,count)){
            g->craft.elapsed=r->seconds;say(g,"Craft paused: inventory space needed.");break;
        }
        g->inventory=consumed;g->craft.elapsed-=r->seconds;
        --g->craft.batches_left;++g->craft.batches_pending;bump(&g->crafted);
        if((!g->craft.batches_left||g->craft.delivery_elapsed>5.0f)&&!deliver_pending(g))say(g,"Completed products waiting for inventory space.");
    }
    if(!g->craft.batches_left&&!deliver_pending(g))say(g,"Completed products waiting for inventory space.");
    if(!g->craft.batches_left){
        g->craft.elapsed=0;
        if(!g->craft.batches_pending){memset(&g->craft,0,sizeof g->craft);say(g,"Crafting complete.");}
    }
}
static bool overlap(PeVec3 p, const PeBlock *b) {
    return p.x+PE_PLAYER_RADIUS>b->x && p.x-PE_PLAYER_RADIUS<b->x+1 &&
           p.z+PE_PLAYER_RADIUS>b->z && p.z-PE_PLAYER_RADIUS<b->z+1 &&
           p.y+PE_PLAYER_HEIGHT>b->y && p.y<b->y+1;
}
static bool collides(const PeGame *g, PeVec3 p) {
    if (p.x<PE_PLAYER_RADIUS || p.z<PE_PLAYER_RADIUS || p.x>pe_world_width(&g->world)-1-PE_PLAYER_RADIUS ||
        p.z>pe_world_depth(&g->world)-1-PE_PLAYER_RADIUS || p.y<0 || p.y+PE_PLAYER_HEIGHT>pe_world_size_y(&g->world)-1) return true;
    if (pe_world_collides(&g->world,p,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT)) return true;
    if(pe_settlement_matches(&g->world)){
        const PeSceneCollision *scene=pe_settlement_collision_for_world(&g->world,NULL,0);
        if(!scene||pe_scene_capsule_overlap(scene,p,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT))return true;
    }
    for (int i=0;i<g->block_count;i++) if (overlap(p,&g->blocks[i])) return true;
    PeVec3 lo={p.x-PE_PLAYER_RADIUS,p.y,p.z-PE_PLAYER_RADIUS},hi={p.x+PE_PLAYER_RADIUS,p.y+PE_PLAYER_HEIGHT,p.z+PE_PLAYER_RADIUS};
    for(int i=0;i<g->cell_count;i++)if(pe_b45_overlaps_aabb(&g->cells[i],lo,hi))return true;
    return false;
}
bool pe_game_set_world(PeGame *g,const PeWorld *world,PeVec3 preferred){
    if(!g||!world||world->source>1||!pe_world_depth(world)||!finite3(preferred))return false;
    if(world->source==0)for(size_t i=0;i<pe_world_voxel_count(world);i++)if(pe_world_data_const(world)[i].material>PE_MAT_GRASS)return false;
    PeGame *next=calloc(1,sizeof *next);if(!next)return false;
    *next=*g;memset(&next->world,0,sizeof next->world);
    if(!pe_world_clone(&next->world,world)){free(next);return false;}next->block_count=0;next->tree_count=0;next->cell_count=0;
    next->deployed_count=0;memset(next->deployed,0,sizeof next->deployed);
    memset(next->blocks,0,sizeof next->blocks);memset(next->trees,0,sizeof next->trees);memset(next->cells,0,sizeof next->cells);
    PeSandboxCollision *collision=NULL;
    if(g->mode==PE_GAME_ADVENTURE||pe_settlement_matches(world)){
        char error[160];collision=pe_sandbox_collision_create();
        if(!collision||!pe_game_collision_update(next,collision,error,sizeof error)){pe_sandbox_collision_free(collision);pe_game_destroy(next);free(next);return false;}
    }
    int cx=(int)fminf(pe_world_width(world)-2,fmaxf(1,preferred.x));
    int cz=(int)fminf(pe_world_depth(world)-2,fmaxf(1,preferred.z));bool found=false;
    for(int ring=0;ring<pe_world_depth(world)&&!found;ring++)for(int z=cz-ring;z<=cz+ring&&!found;z++)for(int x=cx-ring;x<=cx+ring&&!found;x++){
        if(x<1||z<1||x>=pe_world_width(world)-2||z>=pe_world_depth(world)-2||(abs(x-cx)!=ring&&abs(z-cz)!=ring))continue;
        float h=pe_world_height(&next->world,(float)x,(float)z);
        if(h<=0||h+PE_PLAYER_HEIGHT+0.1f>=pe_world_size_y(world)-1)continue;
        PeVec3 p={(float)x,h+0.05f,(float)z};
        if(collision){next->player=p;found=pe_game_collision_settle(next,collision);}
        else for(int lift=0;lift<15;lift++,p.y+=0.05f)if(!collides(next,p)){next->player=p;found=true;break;}
    }
    if(found){next->velocity=(PeVec3){0};next->grounded=false;next->sprinting=false;next->jumping=false;next->jump_action_elapsed=0;pe_game_move(g,next);say(g,"World imported; inventory retained.");}
    pe_sandbox_collision_free(collision);pe_game_destroy(next);free(next);return found;
}
void pe_game_destroy(PeGame *g){if(g){pe_world_destroy(&g->world);memset(g,0,sizeof *g);}}
bool pe_game_clone(PeGame *out,const PeGame *g){
    if(!out||!g)return false;
    if(out==g)return true;
    PeWorld *w=calloc(1,sizeof *w);if(!w)return false;
    if(!pe_world_clone(w,&g->world)){free(w);return false;}
    pe_game_destroy(out);*out=*g;out->world=*w;free(w);return true;
}
void pe_game_move(PeGame *out,PeGame *g){if(out&&g&&out!=g){pe_game_destroy(out);*out=*g;memset(g,0,sizeof *g);}}
void pe_game_init(PeGame *g, uint32_t seed) {
    memset(g,0,sizeof *g); pe_world_init(&g->world,seed);
    g->player=(PeVec3){32,pe_world_height(&g->world,32,32)+0.05f,32};
    while(collides(g,g->player) && g->player.y<pe_world_size_y(&g->world)-PE_PLAYER_HEIGHT-1.1f)g->player.y+=0.05f;
    g->game_seconds=94.35*PE_DAY_SECONDS;
    g->inventory.next_instance_id=1;
    g->rng_state=seed?seed:UINT32_C(0x9e3779b9);
    g->stamina=g->stamina_max=pe_player_defaults.stamina_max;
    g->comfort=pe_stamina_recovery_source.initial_comfort;
    g->stamina_regen=pe_player_defaults.stamina_recover_speed;
    g->stamina_recover_delay=pe_player_defaults.stamina_recover_delay;
    g->hp=pe_player_defaults.hp;g->max_hp=pe_player_defaults.hp_max;
    g->tree_count=12;
    for (int i=0;i<g->tree_count;i++) {
        float angle=(float)i*2.3999632f+(float)(seed%100)*0.03f;
        float radius=i<3?2.1f:5.0f+(float)(i%5)*1.6f;
        float x=32+cosf(angle)*radius,z=32+sinf(angle)*radius;
        g->trees[i]=(PeTree){{x,pe_world_height(&g->world,x,z),z},8,255};
    }
    say(g,"Gather wood near trees, then craft or build.");
}
static bool grant_items(PeInventory *inventory,const PeIngredient *grants,size_t count){
    PeInventory staged=*inventory;
    for(size_t i=0;i<count;i++)if(!pe_inventory_add(&staged,grants[i].item_id,grants[i].count))return false;
    *inventory=staged;return true;
}
bool pe_game_init_mode(PeGame *g,uint32_t seed,PeGameMode mode){
    if(!g||(mode!=PE_GAME_LEGACY&&mode!=PE_GAME_ADVENTURE))return false;
    PeInventory starter={0};
    if(mode==PE_GAME_ADVENTURE&&!grant_items(&starter,pe_adventure_starter,pe_adventure_starter_count))return false;
    PeGame *staged=malloc(sizeof *staged);if(!staged)return false;
    pe_game_init(staged,seed);staged->mode=mode;
    if(mode==PE_GAME_ADVENTURE){
        PeSandboxCollision *collision=pe_sandbox_collision_create();char error[160];
        /* Settle against the pre-tree scene first, then escape only tree overlaps.
         * This keeps spawn recovery from climbing a trunk as though it were ground. */
        bool ready=collision&&pe_sandbox_collision_update(collision,&staged->world,NULL,0,NULL,0,error,sizeof error)&&
            pe_game_collision_settle(staged,collision)&&pe_game_collision_update(staged,collision,error,sizeof error)&&
            pe_game_collision_recover_trees(staged,collision)&&pe_game_collision_settle(staged,collision);
        pe_sandbox_collision_free(collision);
        if(!ready){pe_game_destroy(staged);free(staged);return false;}
        staged->inventory=starter;staged->money=500;staged->adventure_mission=0;
        if(!pe_outfit_init(staged)){pe_game_destroy(staged);free(staged);return false;}
        say(staged,"Welcome: speak with Westina. No recipes learned yet.");
    }
    *g=*staged;free(staged);
    return true;
}
bool pe_game_welcome_advance(PeGame *g,unsigned expected_page){
    if(!g||g->mode!=PE_GAME_ADVENTURE||!isfinite(g->hp)||g->hp<=0||
       g->welcome_page>=PE_ADVENTURE_WELCOME_PAGES||g->welcome_page!=expected_page||
       g->adventure_mission||g->arrival_complete||g->supplies_claimed)return false;
    g->welcome_page++;
    if(g->welcome_page==PE_ADVENTURE_WELCOME_PAGES){
        g->adventure_mission=PE_ADVENTURE_ARRIVAL;say(g,"Arrival: ask Eitai for the supplies.");
    }
    return true;
}
bool pe_game_arrival_complete(PeGame *g){
    if(!g||g->mode!=PE_GAME_ADVENTURE||g->welcome_page!=PE_ADVENTURE_WELCOME_PAGES||g->adventure_mission!=PE_ADVENTURE_ARRIVAL||g->arrival_complete||!(g->hp>0))return false;
    g->arrival_complete=true;g->adventure_mission=PE_ADVENTURE_SUPPLIES;
    say(g,"Arrival complete. Speak with the supplier about the starter pack.");return true;
}
bool pe_game_accept_supplies(PeGame *g){
    if(!g||g->mode!=PE_GAME_ADVENTURE||!g->arrival_complete||g->adventure_mission!=PE_ADVENTURE_SUPPLIES||g->supplies_claimed||!(g->hp>0))return false;
    if(!grant_items(&g->inventory,pe_adventure_supplies,pe_adventure_supplies_count)){
        say(g,"Make room for the complete supply package.");return false;
    }
    g->supplies_claimed=true;say(g,"Supplies received. Use the recipe scripts to learn crafting.");return true;
}
bool pe_game_recipe_known(const PeGame *g,int id){
    if(!g||!pe_recipe_def(id)||g->known_recipe_count<0||g->known_recipe_count>PE_KNOWN_RECIPES)return false;
    if(g->mode==PE_GAME_LEGACY)return true;
    for(int i=0;i<g->known_recipe_count;i++)if(g->known_recipes[i]==id)return true;
    return false;
}
static void clear_spent_references(PeGame *g){
    pe_shield_clear_missing(g);
    if(g->equipped_instance_id&&!pe_game_equipped_tool(g))g->equipped_instance_id=0;
    for(int i=0;i<PE_QUICKSLOTS;i++){
        PeQuickslot *q=&g->quickslots[i];bool exists=false;
        if(q->item_id&&!q->instance_id)continue; /* Stack shortcuts survive exhaustion. */
        for(int j=0;j<PE_INVENTORY_SLOTS;j++){
            PeSlot *s=&g->inventory.slots[j];
            if(s->count&&s->item_id==q->item_id&&s->instance_id==q->instance_id){exists=true;break;}
        }
        if(!exists)memset(q,0,sizeof *q);
    }
}
static void consume_slot(PeGame *g,int index){
    PeSlot *s=&g->inventory.slots[index];if(--s->count==0)memset(s,0,sizeof *s);
    clear_spent_references(g);
}
float pe_game_item_cooldown(const PeGame *g,int item_id){
    if(!g)return 0;
    double cooldown=item_id==916?g->juice_cooldown:item_id==PE_ITEM_MEDPACK?g->medpack_cooldown:0;
    return isfinite(cooldown)&&cooldown>0?(float)cooldown:0;
}
bool pe_game_use_slot(PeGame *g,int index){
    if(!g||index<0||index>=PE_INVENTORY_SLOTS||!isfinite(g->hp)||!(g->hp>0)||
       g->known_recipe_count<0||g->known_recipe_count>PE_KNOWN_RECIPES)return false;
    PeSlot *s=&g->inventory.slots[index];const PeItemDef *d=s->count?pe_item_def(s->item_id):NULL;
    if(!d)return false;
    if(d->learn_kind==PE_LEARN_RECIPE){
        int ids[PE_KNOWN_RECIPES],count=g->known_recipe_count;memcpy(ids,g->known_recipes,sizeof ids);bool changed=false;
        for(int i=0;i<d->learn_count;i++){
            int id=d->learn_ids[i];if(!pe_recipe_def(id))return false;
            bool known=g->mode==PE_GAME_LEGACY;
            for(int j=0;j<count;j++)if(ids[j]==id)known=true;
            if(!known){if(count>=PE_KNOWN_RECIPES)return false;ids[count++]=id;changed=true;}
        }
        if(!changed){say(g,"These recipes are already known.");return false;}
        memcpy(g->known_recipes,ids,sizeof ids);g->known_recipe_count=count;consume_slot(g,index);
        say(g,"Recipe script learned.");return true;
    }
    if(d->learn_kind==PE_LEARN_SCAN){
        uint32_t mask=g->scanner_mask;
        for(int i=0;i<d->learn_count;i++){int id=d->learn_ids[i];if(id<1||id>31)return false;mask|=UINT32_C(1)<<id;}
        if(mask==g->scanner_mask){say(g,"This scanner update is already installed.");return false;}
        g->scanner_mask=mask;consume_slot(g,index);say(g,"Scanner update installed; terrain scanner presentation is not implemented.");return true;
    }
    if(d->id==916){
        if(g->juice_cooldown>0||!isfinite(g->max_hp)||g->max_hp<=0)return false;
        consume_slot(g,index);g->juice_cooldown=10;g->juice_buff_elapsed=0;g->juice_buff_tick=1;g->juice_buff_active=true;
        g->hp=fminf(g->max_hp,g->hp+30);say(g,"Herbal Juice used.");return true;
    }
    if(d->id==PE_ITEM_MEDPACK){
        if(!isfinite(g->medpack_cooldown)||g->medpack_cooldown!=0||!isfinite(g->max_hp)||g->max_hp<=0)return false;
        consume_slot(g,index);g->medpack_cooldown=pe_medpack_cooldown_seconds;
        g->hp=fminf(g->max_hp,g->hp+pe_medpack_heal);say(g,"Portable Medpack used.");return true;
    }
    return pe_game_equip_slot(g,index);
}
bool pe_game_quickslot_assign(PeGame *g,int quick,int index){
    if(!g||quick<0||quick>=PE_QUICKSLOTS||index< -1||index>=PE_INVENTORY_SLOTS)return false;
    if(index== -1){memset(&g->quickslots[quick],0,sizeof g->quickslots[quick]);return true;}
    PeSlot s=g->inventory.slots[index];if(s.count<1||!pe_item_def(s.item_id))return false;
    g->quickslots[quick]=(PeQuickslot){s.item_id,s.instance_id};return true;
}
bool pe_game_quickslot_use(PeGame *g,int quick){
    if(!g||quick<0||quick>=PE_QUICKSLOTS)return false;
    PeQuickslot q=g->quickslots[quick];if(!q.item_id)return false;
    for(int i=0;i<PE_INVENTORY_SLOTS;i++){
        PeSlot *s=&g->inventory.slots[i];
        if(s->count&&s->item_id==q.item_id&&s->instance_id==q.instance_id)return pe_game_use_slot(g,i);
    }
    return false;
}
void pe_game_stamina_activity(PeGame *g){
    if(g&&g->mode==PE_GAME_ADVENTURE&&g->passive_recovery.initialized)
        pe_stamina_recovery_consume(&g->stamina_recovery,g->passive_recovery.last_time);
}
static void game_step(PeGame *g, PeInput input, float dt,const PeSandboxCollision *collision,const PeVec3 *motion) {
    if (!isfinite(dt) || dt<=0 || dt>0.25f) return;
    g->game_seconds+=(double)dt*12.0;
    double health_dt=dt==PE_FIXED_DT?1.0/60.0:(double)dt;
    g->medpack_cooldown=(float)fmax(0,(double)g->medpack_cooldown-health_dt);
    /* Float serialization/clock accumulation can leave ~20us after600 fixed
     * steps. Discard only sub-.1ms residue, not an extra display-frame delay. */
    if(g->medpack_cooldown<=.0001f)g->medpack_cooldown=0;
    g->juice_cooldown=fmax(0,g->juice_cooldown-health_dt);
    if(g->juice_cooldown<1e-9)g->juice_cooldown=0;
    if(g->juice_buff_active){
        g->juice_buff_elapsed+=health_dt;
        /* Source buff evaluates one tick per update, before checking expiry. */
        if(g->juice_buff_elapsed>(double)g->juice_buff_tick+1e-9){if(g->hp>0)g->hp=fminf(g->max_hp,g->hp+30);g->juice_buff_tick++;}
        if(g->juice_buff_elapsed>10+1e-9)g->juice_buff_active=false;
    }
    clear_spent_references(g);
    if(!(g->hp>0)){input=(PeInput){0};g->sprinting=false;motion=NULL;}
    if(motion){input.move_x=input.move_z=0;input.jump=input.sprint=false;input.stamina_action=true;}
    if(g->equipped_instance_id&&!pe_game_equipped_tool(g))g->equipped_instance_id=0;
    if (!isfinite(input.move_x)) input.move_x=0;
    if (!isfinite(input.move_z)) input.move_z=0;
    float mag=hypotf(input.move_x,input.move_z);
    if (mag>1) { input.move_x/=mag; input.move_z/=mag; }
    PeVec3 probe=g->player; probe.y-=0.08f;
    g->grounded=(collision?pe_sandbox_collision_support(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,0.08f,0.65f).hit:collides(g,probe)) && g->velocity.y<=0;
    if(g->grounded){g->jumping=false;g->jump_action_elapsed=0;}
    bool jump_was_active=g->jumping;
    if(g->jumping){g->jump_action_elapsed+=dt;if(g->jump_action_elapsed>3)g->jumping=false;}
    bool stamina_active=jump_was_active||input.stamina_action;
    if(!input.sprint||mag<0.0001f||!g->grounded)g->sprinting=false;
    else if(!g->sprinting && g->stamina>=PE_SPRINT_MIN_STAMINA)g->sprinting=true;
    if(g->sprinting){
        g->stamina=fmaxf(0,g->stamina-PE_SPRINT_STAMINA_PER_SECOND*pe_player_defaults.stamina_reduce_percent*dt);
        stamina_active=true;
        if(g->stamina<=0)g->sprinting=false;
    }
    float speed=g->sprinting?PE_SPRINT_SPEED:(input.walk?PE_WALK_SPEED:PE_RUN_SPEED);
    g->velocity.x=input.move_x*speed; g->velocity.z=input.move_z*speed;
    if(motion){g->velocity.x=motion->x;g->velocity.z=motion->z;}
    float jump_cost=PE_JUMP_STAMINA*pe_player_defaults.stamina_reduce_percent;
    if (input.jump && g->grounded && g->stamina>=jump_cost) {
        g->stamina-=jump_cost;g->jumping=true;g->sprinting=false;stamina_active=true;
        g->velocity.y=7;g->grounded=false;g->jump_action_elapsed=0;
    }
    g->velocity.y-=20*dt;
    if(collision){
        PeSandboxMotion motion;
        if(pe_sandbox_collision_move(collision,g->player,g->velocity,dt,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,
                                    g->grounded,g->grounded&&g->velocity.y<=0,&motion)){
            g->player=motion.position;g->velocity=motion.velocity;g->grounded=motion.grounded;
        }else {g->velocity=(PeVec3){0};say(g,"Movement blocked by invalid collision state.");}
    }else {
    /* Legacy substeps remain for old prototype callers and fixtures. */
    int steps=(int)ceilf((fabsf(g->velocity.y)+speed)*dt/0.08f);
    if (steps<1) steps=1;
    float sub=dt/(float)steps;
    for (int s=0;s<steps;s++) {
        for (int axis=0;axis<3;axis++) {
            PeVec3 next=g->player;
            if (axis==0) next.x+=g->velocity.x*sub;
            if (axis==1) next.z+=g->velocity.z*sub;
            if (axis==2) next.y+=g->velocity.y*sub;
            if (!collides(g,next)) g->player=next;
            else if (axis!=2 && g->grounded) {
                for(int lift=1;lift<=7;lift++) {
                    PeVec3 raised=next;raised.y+=(float)lift*0.05f;
                    if(!collides(g,raised)){g->player=raised;break;}
                }
            }
            else if (axis==2) { if (g->velocity.y<0) g->grounded=true; g->velocity.y=0; }
        }
    }
    }
    if(g->grounded)g->jumping=false;
    /* Move actions and active tool animations delay recovery. SkExpFuncSet
     * enforces at least .1 seconds after the last consumption/action frame. */
    float rest=dt;
    bool stamina_locked=stamina_active||g->tool_cooldown>0;
    if(g->tool_cooldown>0){rest=fmaxf(0,dt-g->tool_cooldown);g->stamina_idle=0;}
    g->tool_cooldown=fmaxf(0,g->tool_cooldown-dt);
    if(stamina_active){rest=0;g->stamina_idle=0;}
    float delay=fmaxf(0.1f,g->stamina_recover_delay),idle_before=g->stamina_idle;
    float recovered=fmaxf(0,idle_before+rest-delay)-fmaxf(0,idle_before-delay);
    g->stamina_idle=fminf(delay+dt,idle_before+rest);
    if(g->mode==PE_GAME_ADVENTURE){
        /* Adventure recovery belongs to display-frame buffs, including the
         * pre-mount/load interval. Never fall back to continuous recovery. */
        if(stamina_locked)pe_game_stamina_activity(g);
    }else g->stamina=fminf(g->stamina_max,g->stamina+g->stamina_regen*fmaxf(0,recovered));
    step_craft(g,dt);
    clear_spent_references(g);
}
void pe_game_step(PeGame *g,PeInput input,float dt){if(g)game_step(g,input,dt,NULL,NULL);}
bool pe_game_collision_update(const PeGame *g,PeSandboxCollision *collision,char *error,size_t capacity){
    if(!g||g->block_count<0||g->block_count>PE_MAX_BLOCKS||g->cell_count<0||g->cell_count>PE_MAX_B45_CELLS||g->tree_count<0||g->tree_count>PE_MAX_TREES)return false;
    PeSandboxBox boxes[PE_MAX_BLOCKS];
    for(int i=0;i<g->block_count;i++){
        PeBlock b=g->blocks[i];boxes[i]=(PeSandboxBox){{b.x,b.y,b.z},{b.x+1,b.y+1,b.z+1}};
    }
    PeTreeCapsule trees[PE_MAX_TREES];size_t count=0;
    if(g->mode==PE_GAME_ADVENTURE)for(int i=0;i<g->tree_count;i++)if(g->trees[i].remaining>0)
        trees[count++]=pe_tree_collision_source(g->trees[i].position);
    const PeSceneCollision *scene=NULL;
    if(pe_settlement_matches(&g->world)){scene=pe_settlement_collision_for_world(&g->world,error,capacity);if(!scene)return false;}
    return pe_sandbox_collision_update_with_static(collision,&g->world,g->cells,(size_t)g->cell_count,
        boxes,(size_t)g->block_count,trees,count,scene,error,capacity);
}
bool pe_game_step_collision(PeGame *g,PeInput input,float dt,const PeSandboxCollision *collision){
    if(!g||!collision||!isfinite(dt)||dt<=0||dt>0.25f||!finite3(g->velocity)||
       pe_sandbox_collision_overlap(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT))return false;
    game_step(g,input,dt,collision,NULL);return true;
}
bool pe_game_step_collision_motion(PeGame *g,PeInput input,float dt,const PeSandboxCollision *collision,PeVec3 motion){
    if(!g||!collision||!isfinite(dt)||dt<=0||dt>.25f||!finite3(g->velocity)||
       !finite3(motion)||motion.y!=0||hypotf(motion.x,motion.z)>100||
       hypot(hypot(motion.x,motion.z),(double)g->velocity.y-20*dt)>1000||
       pe_sandbox_collision_overlap(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT))return false;
    game_step(g,input,dt,collision,&motion);return true;
}
/* Version-4 compatibility/spawn policy, not ordinary movement or a Unity rule.
 * Same-height radial candidates every .05m/64 directions, at most 3m away.
 * Geometry is swept; initial tree penetrations must monotonically decrease. */
bool pe_game_collision_recover_trees(PeGame *g,const PeSandboxCollision *collision){
    if(!g||!collision||g->mode!=PE_GAME_ADVENTURE||g->tree_count<0||g->tree_count>PE_MAX_TREES||
       pe_sandbox_collision_overlap_geometry(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT))return false;
    if(!pe_sandbox_collision_overlap(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT))return true;
    PeTreeCapsule trees[PE_MAX_TREES];bool initial[PE_MAX_TREES];int count=0;bool embedded=false;
    for(int i=0;i<g->tree_count;i++)if(g->trees[i].remaining>0){
        trees[count]=pe_tree_collision_source(g->trees[i].position);
        initial[count]=pe_tree_collision_overlap(&trees[count],g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT);
        embedded|=initial[count++];
    }
    if(!embedded)return false;
    for(int ring=1;ring<=60;ring++)for(int a=0;a<64;a++){
        float angle=(float)a*0.09817477042468103f,distance=(float)ring*0.05f;
        PeVec3 delta={cosf(angle)*distance,0,sinf(angle)*distance};
        PeVec3 candidate={g->player.x+delta.x,g->player.y,g->player.z+delta.z};
        bool clear=true;
        for(int i=0;i<count&&clear;i++){
            if(initial[i]){
                float dx=g->player.x-trees[i].center.x,dz=g->player.z-trees[i].center.z;
                if(dx*delta.x+dz*delta.z < -0.000001f)clear=false;
            }else if(pe_tree_collision_sweep(&trees[i],g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,delta).hit)clear=false;
        }
        if(!clear||pe_sandbox_collision_overlap(collision,candidate,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT)||
           pe_sandbox_collision_sweep_geometry(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,delta).hit)continue;
        g->player=candidate; /* Preserve airborne velocity and all saved clocks. */
        g->grounded=g->velocity.y<=0&&pe_sandbox_collision_support(collision,candidate,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,.08f,.65f).hit;
        return true;
    }
    return false;
}
bool pe_game_collision_settle(PeGame *g,const PeSandboxCollision *collision){
    PeVec3 position;
    if(!g||!pe_sandbox_collision_settle(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT,4,5,&position))return false;
    g->player=position;g->velocity=(PeVec3){0};g->grounded=true;
    g->jumping=false;g->sprinting=false;g->jump_action_elapsed=0;
    return true;
}

/* Slab intersection, returning entry face for placement. */
static bool block_ray(const PeBlock *b, PeVec3 o, PeVec3 d, float *distance, PeVec3 *normal) {
    float lo=0,hi=3.0f; PeVec3 n={0,1,0};
    float origins[3]={o.x,o.y,o.z},dirs[3]={d.x,d.y,d.z};
    int mins[3]={b->x,b->y,b->z};
    for (int a=0;a<3;a++) {
        if (fabsf(dirs[a])<0.00001f) { if (origins[a]<mins[a] || origins[a]>mins[a]+1) return false; continue; }
        float t1=(mins[a]-origins[a])/dirs[a],t2=(mins[a]+1-origins[a])/dirs[a];
        float sign=-1;
        if (t1>t2) { float t=t1;t1=t2;t2=t;sign=1; }
        if (t1>lo) { lo=t1; n=(PeVec3){0,0,0}; if(a==0)n.x=sign; if(a==1)n.y=sign; if(a==2)n.z=sign; }
        if (t2<hi) hi=t2;
        if (lo>hi) return false;
    }
    *distance=lo; *normal=n; return true;
}
static int nearest_block(const PeGame *g,PeVec3 o,PeVec3 d,float *dist,PeVec3 *normal) {
    int found=-1; *dist=3.001f;
    for(int i=0;i<g->block_count;i++) {
        float t; PeVec3 n;
        if(block_ray(&g->blocks[i],o,d,&t,&n) && t<*dist) { *dist=t;*normal=n;found=i; }
    }
    return found;
}
static bool valid_ray(const PeGame *g, PeVec3 o, PeVec3 *d) {
    PeVec3 eye={g->player.x,g->player.y+1.5f,g->player.z};
    PeVec3 offset={o.x-eye.x,o.y-eye.y,o.z-eye.z};
    return finite3(o) && length3(offset)<=2.0f && normalize(d);
}
static bool remove_old_block(PeGame *g,PeVec3 o,PeVec3 d) {
    if(!valid_ray(g,o,&d)) return false;
    float dist; PeVec3 n;
    int i=nearest_block(g,o,d,&dist,&n);
    PeHit h=pe_world_raycast(&g->world,o,d,3);
    if(i<0 || (h.hit && h.distance+0.05f<dist)) return false;
    if(!pe_game_static_segment_clear(g,o,(PeVec3){o.x+d.x*dist,o.y+d.y*dist,o.z+d.z*dist}))return false;
    if(!pe_inventory_add(&g->inventory,g->blocks[i].item_id,1)) { say(g,"Inventory full; block kept in place.");return false; }
    g->blocks[i]=g->blocks[--g->block_count]; say(g,"Block recovered."); return true;
}
static int item_material(int item){
    for(int i=0;i<16;i++)if(pe_b45_material_item(i)==item)return i;
    return -1;
}
static bool same_cell(const PeB45Cell *a,const PeB45Cell *b){return a->x==b->x&&a->y==b->y&&a->z==b->z;}
static bool cell_bounds(const PeGame *g,const PeB45Cell *c){
    return pe_b45_valid(c)&&c->x>=0&&c->y>=0&&c->z>=0&&
           c->x<2*(pe_world_width(&g->world)-1)&&c->y<2*(pe_world_size_y(&g->world)-1)&&c->z<2*(pe_world_depth(&g->world)-1);
}
static bool cell_reachable(const PeGame *g,const PeB45Cell *c){
    PeVec3 eye={g->player.x,g->player.y+1.5f,g->player.z};
    float x=fminf((c->x+1)*0.5f,fmaxf(c->x*0.5f,eye.x))-eye.x;
    float y=fminf((c->y+1)*0.5f,fmaxf(c->y*0.5f,eye.y))-eye.y;
    float z=fminf((c->z+1)*0.5f,fmaxf(c->z*0.5f,eye.z))-eye.z;
    return x*x+y*y+z*z<=9.0f;
}
static bool cell_terrain_clear(const PeGame *g,const PeB45Cell *c){
    for(int x=0;x<3;x++)for(int y=0;y<3;y++)for(int z=0;z<3;z++){
        PeVec3 p={(float)c->x*0.5f+0.005f+x*0.245f,(float)c->y*0.5f+0.005f+y*0.245f,(float)c->z*0.5f+0.005f+z*0.245f};
        if(pe_b45_contains(c,p)&&pe_world_density(&g->world,p)>=PE_ISO_LEVEL)return false;
    }
    return true;
}
static bool prepare_build_cells(const PeGame *g,const PeB45Cell *cells,int count,
                                PeInventory *inventory,const char **message){
    if(!g||!(g->hp>0)||!cells||count<1||count>PE_MAX_BUILD_BATCH||g->cell_count>PE_MAX_B45_CELLS-count)return false;
    unsigned material_counts[16]={0};
    PeVec3 lo={g->player.x-PE_PLAYER_RADIUS,g->player.y,g->player.z-PE_PLAYER_RADIUS};
    PeVec3 hi={g->player.x+PE_PLAYER_RADIUS,g->player.y+PE_PLAYER_HEIGHT,g->player.z+PE_PLAYER_RADIUS};
    for(int i=0;i<count;i++){
        const PeB45Cell *c=&cells[i];
        if(!static_box_clear(g,(PeVec3){c->x*.5f+.001f,c->y*.5f+.001f,c->z*.5f+.001f},
                            (PeVec3){c->x*.5f+.499f,c->y*.5f+.499f,c->z*.5f+.499f})){
            *message="Construction intersects settlement geometry.";return false;
        }
        if(!cell_bounds(g,c)||!cell_reachable(g,c)||pe_b45_overlaps_aabb(c,lo,hi)||!cell_terrain_clear(g,c)){
            *message="Construction intersects terrain, player, or exceeds reach.";return false;
        }
        for(int j=0;j<g->cell_count;j++)if(same_cell(c,&g->cells[j])){*message="A construction cell is occupied.";return false;}
        for(int j=0;j<i;j++)if(same_cell(c,&cells[j]))return false;
        for(int j=0;j<g->block_count;j++){
            const PeBlock *b=&g->blocks[j];
            if(pe_b45_overlaps_aabb(c,(PeVec3){b->x,b->y,b->z},(PeVec3){b->x+1,b->y+1,b->z+1}))return false;
        }
        ++material_counts[c->material];
    }
    PeInventory next=g->inventory;
    for(int material=0;material<16;material++)if(material_counts[material]){
        int item=pe_b45_material_item(material);int cost=(int)pe_b45_build_cost(material_counts[material]);
        if(!pe_inventory_remove(&next,item,cost)){*message="Not enough resources for this brush.";return false;}
    }
    *inventory=next;return true;
}
bool pe_game_can_build_cells(const PeGame *g,const PeB45Cell *cells,int count){
    PeInventory inventory;const char *message=NULL;
    return prepare_build_cells(g,cells,count,&inventory,&message);
}
bool pe_game_build_cells(PeGame *g,const PeB45Cell *cells,int count){
    PeInventory next;const char *message=NULL;
    if(!prepare_build_cells(g,cells,count,&next,&message)){
        if(message)say(g,message);
        return false;
    }
    /* Copy input first to permit a caller-owned array that aliases game storage. */
    PeB45Cell copy[PE_MAX_BUILD_BATCH];memcpy(copy,cells,(size_t)count*sizeof *copy);
    g->inventory=next;
    memcpy(&g->cells[g->cell_count],copy,(size_t)count*sizeof *copy);g->cell_count+=count;
    bump(&g->built);say(g,"Construction placed (one resource per four cells, rounded up).");return true;
}
bool pe_game_remove_cells(PeGame *g,const int *indices,int count){
    if(!g||!(g->hp>0))return false;
    if(!indices||count<1||count>PE_MAX_BUILD_BATCH)return false;
    bool remove[PE_MAX_B45_CELLS]={false};unsigned material_counts[16]={0};
    for(int i=0;i<count;i++){
        int index=indices[i];if(index<0||index>=g->cell_count||remove[index]||!cell_reachable(g,&g->cells[index]))return false;
        remove[index]=true;++material_counts[g->cells[index].material];
    }
    PeInventory next=g->inventory;
    for(int material=0;material<16;material++){
        int refund=(int)pe_b45_remove_refund(material_counts[material]);
        if(refund&&!pe_inventory_add(&next,pe_b45_material_item(material),refund)){say(g,"Make room for construction refunds.");return false;}
    }
    int written=0;
    for(int i=0;i<g->cell_count;i++)if(!remove[i])g->cells[written++]=g->cells[i];
    memset(&g->cells[written],0,(size_t)(g->cell_count-written)*sizeof g->cells[0]);
    g->cell_count=written;g->inventory=next;say(g,"Construction removed (refunds round down per material).");return true;
}
static int nearest_cell(const PeGame *g,PeVec3 o,PeVec3 d,PeB45Hit *hit){
    int found=-1;*hit=(PeB45Hit){.distance=3.001f};
    for(int i=0;i<g->cell_count;i++){
        PeB45Hit candidate;
        if(pe_b45_raycast(&g->cells[i],o,d,3,&candidate)&&candidate.distance<hit->distance){*hit=candidate;found=i;}
    }
    return found;
}
static int normal_axis(PeVec3 n){
    if(fabsf(n.y)>=fabsf(n.x)&&fabsf(n.y)>=fabsf(n.z))return 1;
    return fabsf(n.x)>=fabsf(n.z)?0:2;
}
static PeB45Cell brush_offset(PeB45Cell c,int axis,int index){
    int a=index&1,b=(index>>1)&1;
    if(axis==1){c.x+=a;c.z+=b;}else if(axis==0){c.y+=a;c.z+=b;}else{c.x+=a;c.y+=b;}
    return c;
}
bool pe_game_remove_shape(PeGame *g,PeVec3 o,PeVec3 d,int brush){
    if(!g||!(g->hp>0))return false;
    if((brush!=1&&brush!=4)||!valid_ray(g,o,&d))return false;
    PeB45Hit hit;int index=nearest_cell(g,o,d,&hit);
    float old_distance;PeVec3 old_normal;int old=nearest_block(g,o,d,&old_distance,&old_normal);
    PeHit terrain=pe_world_raycast(&g->world,o,d,3);
    if(index<0||(old>=0&&old_distance<hit.distance))return remove_old_block(g,o,d);
    if(terrain.hit&&terrain.distance+0.01f<hit.distance)return false;
    if(!pe_game_static_segment_clear(g,o,hit.position))return false;
    int indices[4],count=0,axis=normal_axis(hit.normal);PeB45Cell anchor=g->cells[index];
    if(brush==4){if(axis==1){anchor.x&=~1;anchor.z&=~1;}else if(axis==0){anchor.y&=~1;anchor.z&=~1;}else{anchor.x&=~1;anchor.y&=~1;}}
    for(int i=0;i<brush;i++){
        PeB45Cell candidate=brush_offset(anchor,axis,i);
        for(int j=0;j<g->cell_count;j++)if(same_cell(&candidate,&g->cells[j])){indices[count++]=j;break;}
    }
    return count>0&&pe_game_remove_cells(g,indices,count);
}
bool pe_game_remove_block(PeGame *g,PeVec3 o,PeVec3 d){return pe_game_remove_shape(g,o,d,1);}
static bool preview_shape(const PeGame *g,PeVec3 o,PeVec3 d,int item,int shape,int rotation,int brush,
                          PeB45Cell out[4],size_t *count,const char **message){
    if(!g||!(g->hp>0)||!out||!count)return false;
    int material=item_material(item);
    if(material<0)return false;
    if((shape!=PE_B45_CUBE&&shape!=PE_B45_RAMP)||rotation<0||rotation>3||(brush!=1&&brush!=4)||!valid_ray(g,o,&d))return false;
    PeHit terrain=pe_world_raycast(&g->world,o,d,3);PeB45Hit hit;int cell=nearest_cell(g,o,d,&hit);
    float old_distance;PeVec3 old_normal;int old=nearest_block(g,o,d,&old_distance,&old_normal);
    PeVec3 position,normal;
    bool terrain_first=false;
    bool cell_first=cell>=0&&(!terrain.hit||hit.distance<=terrain.distance)&&(old<0||hit.distance<=old_distance);
    if(cell_first){position=hit.position;normal=hit.normal;}
    else if(old>=0&&(!terrain.hit||old_distance<=terrain.distance)){
        position=(PeVec3){o.x+d.x*old_distance,o.y+d.y*old_distance,o.z+d.z*old_distance};normal=old_normal;
    }else if(terrain.hit){position=terrain.position;normal=terrain.normal;terrain_first=true;}
    else{*message="Aim at nearby terrain or construction.";return false;}
    if(!pe_game_static_segment_clear(g,o,position)){*message="Settlement geometry blocks construction.";return false;}
    int axis=normal_axis(normal);
    PeB45Cell anchor={(int)floorf((position.x+normal.x*0.01f)*2),(int)floorf((position.y+normal.y*0.01f)*2),(int)floorf((position.z+normal.z*0.01f)*2),(uint8_t)((shape<<2)|rotation),(uint8_t)material};
    if(cell_first){
        anchor.x=g->cells[cell].x;anchor.y=g->cells[cell].y;anchor.z=g->cells[cell].z;
        if(axis==0)anchor.x+=normal.x>0?1:-1;else if(axis==1)anchor.y+=normal.y>0?1:-1;else anchor.z+=normal.z>0?1:-1;
    }
    if(brush==4){if(axis==1){anchor.x&=~1;anchor.z&=~1;}else if(axis==0){anchor.y&=~1;anchor.z&=~1;}else{anchor.x&=~1;anchor.y&=~1;}}
    PeB45Cell cells[4];
    for(int i=0;i<brush;i++)cells[i]=brush_offset(anchor,axis,i);
    if(terrain_first&&axis==1&&normal.y>0){
        float top=0;
        for(int i=0;i<brush;i++)for(int x=0;x<3;x++)for(int z=0;z<3;z++){
            float height=pe_world_height(&g->world,cells[i].x*0.5f+x*0.25f,cells[i].z*0.5f+z*0.25f);if(height>top)top=height;
        }
        for(int i=0;i<brush;i++)cells[i].y=(int)ceilf(top*2);
    }
    memcpy(out,cells,(size_t)brush*sizeof *out);*count=(size_t)brush;return true;
}
bool pe_game_preview_shape(const PeGame *g,PeVec3 o,PeVec3 d,int item,int shape,int rotation,int brush,
                           PeB45Cell out[4],size_t *count){
    if(item!=PE_ITEM_DIRT&&item!=PE_ITEM_STONE&&item!=PE_ITEM_WOOD)return false;
    const char *message=NULL;
    return preview_shape(g,o,d,item,shape,rotation,brush,out,count,&message);
}
bool pe_game_place_shape(PeGame *g,PeVec3 o,PeVec3 d,int item,int shape,int rotation,int brush){
    if(!g||!(g->hp>0))return false;
    if(item_material(item)<0)return pe_game_place(g,o,d,item);
    PeB45Cell cells[4];size_t count;const char *message=NULL;
    if(!preview_shape(g,o,d,item,shape,rotation,brush,cells,&count,&message)){
        if(message)say(g,message);
        return false;
    }
    return pe_game_build_cells(g,cells,(int)count);
}
static uint32_t random_next(uint32_t *state){
    uint32_t x=*state?*state:UINT32_C(0x9e3779b9);x^=x<<13;x^=x>>17;x^=x<<5;*state=x;return x;
}
static bool resource_yield(PeInventory *inventory,const PeTerrainResourceDef *resource,uint32_t *rng){
    if(resource->drop_count==0)return true;
    float quantity=resource->fixed_count>0?resource->fixed_count:resource->self_count;
    if(!isfinite(quantity)||quantity<0||quantity>10000||resource->drop_count<1||resource->drop_count>2)return false;
    int count=(int)quantity;
    float roll=(float)(random_next(rng)>>8)*(1.0f/16777216.0f);
    if(roll<quantity-count)count++;
    for(int i=0;i<count;i++){
        unsigned sample=random_next(rng)%100u;
        for(int j=0;j<resource->drop_count;j++)if(sample<resource->drops[j].threshold){
            if(!pe_inventory_add(inventory,resource->drops[j].item_id,1))return false;
            break;
        }
    }
    return true;
}
bool pe_game_dig_sample(PeGame *g,int sx,int sy,int sz,PeResourceEdit *out){
    if(!g||!out||!(g->hp>0)||sx<0||sy<0||sz<0||sx>=pe_world_width(&g->world)||sy>=pe_world_size_y(&g->world)||sz>=pe_world_depth(&g->world))return false;
    if(!pe_game_static_segment_clear(g,(PeVec3){g->player.x,g->player.y+1,g->player.z},(PeVec3){sx,sy,sz}))return false;
    PeToolStats stats;const PeSlot *equipped=pe_game_equipped_tool(g);
    if(!equipped||!isfinite(equipped->durability)||equipped->durability<=0||!pe_game_tool_stats(g,&stats)||
       (stats.tool_kind!=PE_TOOL_PICKAXE&&stats.tool_kind!=PE_TOOL_SHOVEL))return false;
    PeVoxel v=pe_world_get(&g->world,sx,sy,sz);
    if(!v.volume){*out=(PeResourceEdit){0};return true;}
    if(g->world.revision==UINT32_MAX)return false;
    int material=g->world.source==1?v.material:(v.material==PE_MAT_STONE?17:2);
    const PeTerrainResourceDef *resource=pe_terrain_resource(material);
    if(!resource)return false;
    float duration=pe_terrain_material_duration(material);
    if(duration<=0)return false;
    /* DigTerrainManager.DigVoxels: source hardness attenuation, byte damage,
     * and removal once density reaches <=127. Current base ResRange=.49
     * affects one selected sample; wider area tools are not yet exposed. */
    float damage=stats.mining_damage;
    float hp=37.14689f*duration*duration-14.12429f*duration+1.39f;
    float difference=hp-damage/1275.0f;
    if(difference>0.33f)damage*=1.0f-fminf(1.0f,fmaxf(0,(difference-0.33f)/0.33f));
    damage=fmaxf(2.5f,damage*duration);
    PeVoxel changed=v;
    changed.volume=damage>=changed.volume?0:(uint8_t)(changed.volume-(uint8_t)damage);
    if(changed.volume<PE_ISO_LEVEL)changed.volume=0;
    PeInventory next=g->inventory;uint32_t rng=g->rng_state;
    if(!changed.volume&&!resource_yield(&next,resource,&rng)){*out=(PeResourceEdit){.capacity_blocked=true};return true;}
    if(!changed.volume)changed.material=PE_MAT_AIR;
    /* Commit resource state only; the action host owns wear and stamina. */
    if(!pe_world_set(&g->world,sx,sy,sz,changed))return false;
    g->inventory=next;g->rng_state=rng;
    *out=(PeResourceEdit){.changed=true,.removed=!changed.volume};
    if(!changed.volume&&resource->drop_count)bump(&g->gathered);
    return true;
}

bool pe_game_mine(PeGame *g,PeVec3 o,PeVec3 d) {
    if(!g||!(g->hp>0))return false;
    if(!valid_ray(g,o,&d)) return false;
    float dist; PeVec3 n;
    int block=nearest_block(g,o,d,&dist,&n);
    PeHit h=pe_world_raycast(&g->world,o,d,3);PeB45Hit cell_hit;int cell=nearest_cell(g,o,d,&cell_hit);
    if(cell>=0&&(!h.hit||cell_hit.distance<=h.distance+0.01f)&&(block<0||cell_hit.distance<=dist))return pe_game_remove_shape(g,o,d,1);
    if(block>=0 && (!h.hit || dist<=h.distance+0.05f)) return pe_game_remove_block(g,o,d);
    PeHit dig=pe_world_dig_target(&g->world,o,d,g->player,3.0f);
    if(!dig.hit) { say(g,"No supported terrain sample within 3 m of your body.");return false; }
    PeToolStats stats;const PeSlot *equipped=pe_game_equipped_tool(g);
    if(!pe_game_tool_stats(g,&stats)||(stats.tool_kind!=PE_TOOL_PICKAXE&&stats.tool_kind!=PE_TOOL_SHOVEL)){
        say(g,"Equip a crafted pickaxe or shovel to mine.");return false;
    }
    if(equipped->durability<=0){say(g,"Your tool is broken.");return false;}
    if(g->tool_cooldown>0){say(g,"Wait for the tool's next swing.");return false;}
    /* DigVoxelIndicator chooses centered voxel boxes (including supported
     * sub-isosurface samples), not the nearest corner of the rendered mesh. */
    int sx=dig.x,sy=dig.y,sz=dig.z;
    PeVoxel v=pe_world_get(&g->world,sx,sy,sz);
    if(!v.volume){say(g,"Selected sample has no density to mine.");return false;}
    PeResourceEdit edit;
    if(!pe_game_dig_sample(g,sx,sy,sz,&edit))return false;
    if(edit.capacity_blocked){say(g,"Inventory full; terrain and tool preserved.");return false;}
    for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].instance_id==g->equipped_instance_id)
        g->inventory.slots[i].durability=fmaxf(0,g->inventory.slots[i].durability-stats.durability_cost);
    g->stamina=fmaxf(0,g->stamina-stats.stamina_cost);g->stamina_idle=0;pe_game_stamina_activity(g);
    float speed=1;if(g->stamina<=40)speed*=0.9f;if(g->stamina<=20)speed*=0.9f;
    g->tool_cooldown=0.5f/speed; /* Temporary event pacing replaces animation. */
    if(edit.removed)say(g,"Terrain sample removed.");
    else say(g,"Tool damaged terrain; keep mining this spot.");
    return true;
}

bool pe_game_cut_tree(PeGame *g,int index,PeResourceEdit *out){
    if(!g||!out||!(g->hp>0)||index<0||index>=g->tree_count||g->tree_count>PE_MAX_TREES)return false;
    const PeSlot *equipped=pe_game_equipped_tool(g);const PeItemDef *def=equipped?pe_item_def(equipped->item_id):NULL;
    if(!def||def->tool_kind!=PE_TOOL_AXE||!isfinite(equipped->durability)||equipped->durability<=0)return false;
    PeTree *tree=&g->trees[index];
    if(!pe_game_static_segment_clear(g,(PeVec3){g->player.x,g->player.y+1,g->player.z},
        (PeVec3){tree->position.x,tree->position.y+1,tree->position.z}))return false;
    if(!isfinite(tree->health)||tree->health<0||tree->health>255||tree->remaining<0)return false;
    if(!tree->remaining||tree->health==0){*out=(PeResourceEdit){0};return true;}
    /* Source HP255 and CutDamage, but procedural trees retain provisional unit
     * height/width/resource duration and stored wood yield, not a recovered
     * Adventure species table. The event clock does not establish data parity. */
    float health=fmaxf(0,tree->health-(pe_player_defaults.cut_damage+def->cut_damage));
    PeInventory next=g->inventory;
    if(!health&&!pe_inventory_add(&next,PE_ITEM_WOOD,tree->remaining)){*out=(PeResourceEdit){.capacity_blocked=true};return true;}
    tree->health=health;g->inventory=next;
    if(!health){tree->remaining=0;bump(&g->gathered);}
    *out=(PeResourceEdit){.changed=true,.removed=!health};return true;
}

bool pe_game_gather(PeGame *g) {
    if(!g||!(g->hp>0))return false;
    PeToolStats stats;const PeSlot *equipped=pe_game_equipped_tool(g);
    bool axe=pe_game_tool_stats(g,&stats)&&stats.tool_kind==PE_TOOL_AXE;
    float reach=axe?2.0f:3.0f;int best=-1;float dist=reach;
    for(int i=0;i<g->tree_count;i++){
        PeTree *t=&g->trees[i];float d=hypotf(t->position.x-g->player.x,t->position.z-g->player.z);
        if(t->remaining>0&&fabsf(t->position.y-g->player.y)<2.5f&&d<dist){dist=d;best=i;}
    }
    if(best<0){say(g,axe?"Move within 2 m of a tree to cut.":"Move within 3 m of a tree to gather wood.");return false;}
    if(g->tool_cooldown>0){say(g,"Wait for the next gathering action.");return false;}
    PeTree *tree=&g->trees[best];
    if(!pe_game_static_segment_clear(g,(PeVec3){g->player.x,g->player.y+1,g->player.z},
        (PeVec3){tree->position.x,tree->position.y+1,tree->position.z}))return false;
    if(!axe){
        if(!pe_inventory_add(&g->inventory,PE_ITEM_WOOD,1)){say(g,"Inventory full.");return false;}
        --tree->remaining;if(!tree->remaining)tree->health=0;
        g->tool_cooldown=0.35f;bump(&g->gathered);say(g,"Gathered loose wood (bootstrap gathering).");return true;
    }
    if(!equipped||equipped->durability<=0){say(g,"Your axe is broken.");return false;}
    PeResourceEdit edit;
    if(!pe_game_cut_tree(g,best,&edit))return false;
    if(edit.capacity_blocked){say(g,"Inventory full; tree and axe preserved.");return false;}
    for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].instance_id==g->equipped_instance_id)
        g->inventory.slots[i].durability=fmaxf(0,g->inventory.slots[i].durability-stats.durability_cost);
    g->stamina=fmaxf(0,g->stamina-stats.stamina_cost);g->stamina_idle=0;pe_game_stamina_activity(g);
    float speed=1;if(g->stamina<=40)speed*=0.9f;if(g->stamina<=20)speed*=0.9f;g->tool_cooldown=0.5f/speed;
    if(edit.removed)say(g,"Tree felled; remaining wood recovered.");
    else say(g,"Axe cut the tree.");
    return true;
}

bool pe_game_place(PeGame *g,PeVec3 o,PeVec3 d,int item) {
    if(!g||!(g->hp>0))return false;
    if(item_material(item)>=0)return pe_game_place_shape(g,o,d,item,PE_B45_CUBE,0,1);
    if(item!=PE_ITEM_DIRT && item!=PE_ITEM_WOOD && item!=PE_ITEM_STONE && item!=PE_ITEM_FENCE && item!=PE_ITEM_LADDER) return false;
    if(!valid_ray(g,o,&d) || g->block_count>=PE_MAX_BLOCKS) return false;
    PeHit h=pe_world_raycast(&g->world,o,d,3);
    float dist;PeVec3 n;int i=nearest_block(g,o,d,&dist,&n);
    PeBlock b={0,0,0,item};
    if(i>=0 && (!h.hit || dist<=h.distance)) {
        b.x=g->blocks[i].x+(int)n.x;b.y=g->blocks[i].y+(int)n.y;b.z=g->blocks[i].z+(int)n.z;
    } else if(h.hit) {
        b.x=(int)floorf(h.position.x+h.normal.x*0.55f);
        b.z=(int)floorf(h.position.z+h.normal.z*0.55f);
        b.y=(int)floorf(h.position.y+h.normal.y*0.55f);
        /* Terrain is smooth, so choose the first empty unit cell above a top hit. */
        if(h.normal.y>0.35f) {
            float top=0;
            for(int x=0;x<3;x++) for(int z=0;z<3;z++) {
                float y=pe_world_height(&g->world,b.x+(float)x*0.5f,b.z+(float)z*0.5f);
                if(y>top)top=y;
            }
            b.y=(int)ceilf(top);
        }
    } else {say(g,"Aim at nearby terrain or a block.");return false;}
    if(b.x<0||b.z<0||b.y<0||b.x>=pe_world_width(&g->world)-1||b.z>=pe_world_depth(&g->world)-1||b.y>=pe_world_size_y(&g->world)-2||overlap(g->player,&b)) {say(g,"Cannot place a block here.");return false;}
    if(!static_box_clear(g,(PeVec3){b.x+.001f,b.y+.001f,b.z+.001f},(PeVec3){b.x+.999f,b.y+.999f,b.z+.999f})){say(g,"Placement intersects settlement geometry.");return false;}
    if(!pe_game_static_segment_clear(g,o,(PeVec3){b.x+.5f,b.y+.5f,b.z+.5f}))return false;
    for(int k=0;k<g->block_count;k++) if(g->blocks[k].x==b.x&&g->blocks[k].y==b.y&&g->blocks[k].z==b.z) return false;
    for(int x=0;x<3;x++)for(int y=0;y<3;y++)for(int z=0;z<3;z++)
        if(pe_world_density(&g->world,(PeVec3){b.x+0.02f+(float)x*0.48f,b.y+0.02f+(float)y*0.48f,b.z+0.02f+(float)z*0.48f})>=PE_ISO_LEVEL) {say(g,"Placement intersects terrain.");return false;}
    for(int k=0;k<g->cell_count;k++)if(pe_b45_overlaps_aabb(&g->cells[k],(PeVec3){b.x,b.y,b.z},(PeVec3){b.x+1,b.y+1,b.z+1}))return false;
    if(!pe_inventory_remove(&g->inventory,item,1)) {say(g,"No selected building item in inventory.");return false;}
    clear_spent_references(g);
    g->blocks[g->block_count++]=b;bump(&g->built);say(g,"Block placed.");return true;
}
