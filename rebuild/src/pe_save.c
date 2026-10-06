#define _POSIX_C_SOURCE 200809L
#include "pe_game.h"
#include "pe_settlement.h"
#include "pe_town_interaction.h"
#include "pe_adventure_medicine.h"
#include "pe_adventure_hunt.h"
#include "pe_deployable.h"
#include "pe_shield.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SAVE_CAPACITY (PE_WORLD_LIMIT_COUNT*2u+PE_MAX_VOXEL_COUNT*2u+32768u)
#define SAVE_VERSION 15u
_Static_assert(PE_SAVE_V15_APPEND==4,"V15 appends only the shield reference");
_Static_assert(PE_OUTFIT_SLOTS==3&&PE_SAVE_V14_APPEND==36,"V14 has exactly three 12-byte worn records");
/* All numbers are little-endian; floats are IEEE-754 bit patterns. No ABI structs. */
typedef struct { unsigned char *p;size_t pos,size;bool ok; } Stream;
static void put32(Stream *s,uint32_t v) { if(s->pos+4>s->size){s->ok=false;return;} for(int i=0;i<4;i++)s->p[s->pos++]=(unsigned char)(v>>(i*8)); }
static uint32_t get32(Stream *s) { if(s->pos+4>s->size){s->ok=false;return 0;} uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)s->p[s->pos++]<<(i*8);return v; }
static void putf(Stream *s,float f) {uint32_t v;memcpy(&v,&f,4);put32(s,v);}
static float getf(Stream *s) {uint32_t v=get32(s);float f;memcpy(&f,&v,4);return f;}
static void putd(Stream *s,double d) {uint64_t v;memcpy(&v,&d,8);put32(s,(uint32_t)v);put32(s,(uint32_t)(v>>32));}
static double getd(Stream *s) {uint64_t v=get32(s);v|=(uint64_t)get32(s)<<32;double d;memcpy(&d,&v,8);return d;}
static void putvec(Stream *s,PeVec3 v) {putf(s,v.x);putf(s,v.y);putf(s,v.z);}
static PeVec3 getvec(Stream *s) {PeVec3 v;v.x=getf(s);v.y=getf(s);v.z=getf(s);return v;}
static uint32_t checksum(const unsigned char *p,size_t n) {uint32_t h=2166136261u;for(size_t i=0;i<n;i++){h^=p[i];h*=16777619u;}return h;}
static void err(char *out,size_t cap,const char *msg) {if(out&&cap)snprintf(out,cap,"%s",msg);}
static bool vecfinite(PeVec3 v) {return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}
static bool valid(const PeGame *g,bool trees,bool outfit) {
    if(outfit&&!pe_outfit_state_valid(g))return false;
    if(!pe_shield_state_valid(g))return false;
    if((g->mode!=PE_GAME_LEGACY&&g->mode!=PE_GAME_ADVENTURE)||
       !isfinite(g->hp)||!isfinite(g->max_hp)||g->max_hp<=0||g->max_hp>1000000||g->hp<0||g->hp>g->max_hp||
       !isfinite(g->medpack_cooldown)||g->medpack_cooldown<0||g->medpack_cooldown>10||
       !isfinite(g->juice_cooldown)||g->juice_cooldown<0||g->juice_cooldown>10||
       !isfinite(g->juice_buff_elapsed)||g->juice_buff_elapsed<0||g->juice_buff_elapsed>10.251||
       g->juice_buff_tick>12||g->scanner_mask&~UINT32_C(0xffe)||
       g->known_recipe_count<0||g->known_recipe_count>PE_KNOWN_RECIPES)return false;
    if(g->juice_buff_active&&(g->juice_buff_tick<1||g->juice_buff_elapsed>10.000000001))return false;
    if(g->mode==PE_GAME_LEGACY){if(g->welcome_page||g->adventure_mission||g->arrival_complete||g->supplies_claimed)return false;}
    else if(g->welcome_page<PE_ADVENTURE_WELCOME_PAGES){
        if(g->adventure_mission||g->arrival_complete||g->supplies_claimed)return false;
    }
    else if(g->welcome_page!=PE_ADVENTURE_WELCOME_PAGES||(g->adventure_mission!=PE_ADVENTURE_ARRIVAL&&g->adventure_mission!=PE_ADVENTURE_SUPPLIES)||
            g->arrival_complete!=(g->adventure_mission==PE_ADVENTURE_SUPPLIES)||
            (g->supplies_claimed&&!g->arrival_complete))return false;
    for(int i=0;i<g->known_recipe_count;i++){
        if(!pe_recipe_def(g->known_recipes[i]))return false;
        for(int j=0;j<i;j++)if(g->known_recipes[j]==g->known_recipes[i])return false;
    }
    for(int i=0;i<PE_QUICKSLOTS;i++){
        PeQuickslot q=g->quickslots[i];if(!q.item_id){if(q.instance_id)return false;continue;}
        const PeItemDef *qd=pe_item_def(q.item_id);if(!qd)return false;
        if(qd->stack_limit>1){if(q.instance_id)return false;continue;}
        bool found=false;
        for(int j=0;j<PE_INVENTORY_SLOTS;j++){
            PeSlot s=g->inventory.slots[j];if(s.count>0&&s.item_id==q.item_id&&s.instance_id==q.instance_id){found=true;break;}
        }
        if(!found)return false;
    }
    if(!vecfinite(g->player)||!vecfinite(g->velocity)||!isfinite(g->yaw)||!isfinite(g->pitch)||
       !isfinite(g->game_seconds)||g->game_seconds<0||g->game_seconds>1e15||
       g->player.x<PE_PLAYER_RADIUS||g->player.x>pe_world_width(&g->world)-1-PE_PLAYER_RADIUS||
       g->player.z<PE_PLAYER_RADIUS||g->player.z>pe_world_depth(&g->world)-1-PE_PLAYER_RADIUS||
       g->player.y<0||g->player.y+PE_PLAYER_HEIGHT>pe_world_size_y(&g->world)-1||
       fabsf(g->velocity.x)>1000||fabsf(g->velocity.y)>1000||fabsf(g->velocity.z)>1000||
       g->block_count<0||g->block_count>PE_MAX_BLOCKS||g->tree_count<0||g->tree_count>PE_MAX_TREES||
       g->cell_count<0||g->cell_count>PE_MAX_B45_CELLS) return false;
    if(g->world.source>1||!pe_world_data_const(&g->world)||!pe_town_state_valid(g)||!pe_adventure_medicine_state_valid(g)||!pe_adventure_hunt_state_valid(g)||!pe_deployable_state_valid(g))return false;
    if(g->world.source==0)for(size_t i=0;i<pe_world_voxel_count(&g->world);i++)if(pe_world_data_const(&g->world)[i].material>PE_MAT_GRASS)return false;
    if(!g->inventory.next_instance_id||!g->rng_state||
       !isfinite(g->stamina)||!isfinite(g->stamina_max)||!isfinite(g->stamina_regen)||
       !isfinite(g->stamina_recover_delay)||!isfinite(g->stamina_idle)||!isfinite(g->tool_cooldown)||
       g->stamina_max<=0||g->stamina_max>1000000||g->stamina<0||g->stamina>g->stamina_max||
       g->stamina_regen<0||g->stamina_regen>1000000||g->stamina_recover_delay<0||g->stamina_recover_delay>1000||
       g->stamina_idle<0||g->stamina_idle>1001||g->tool_cooldown<0||g->tool_cooldown>10)return false;
    if(g->equipped_instance_id){
        const PeSlot *slot=pe_game_equipped_tool(g);const PeItemDef *def=slot?pe_item_def(slot->item_id):NULL;
        if(!def||def->durability_max<=0||(def->tool_kind==PE_TOOL_NONE&&
           !(g->mode==PE_GAME_ADVENTURE&&(def->equip_position&16)&&(def->item_type==9||def->item_type==10||def->item_type==14))))return false;
    }
    if(g->mode==PE_GAME_LEGACY&&pe_world_collides(&g->world,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT))return false;
    for(int i=0;i<PE_INVENTORY_SLOTS;i++) {
        PeSlot s=g->inventory.slots[i];
        if(s.count==0) {if(s.item_id!=0||s.instance_id||s.durability!=0)return false;continue;}
        const PeItemDef *d=pe_item_def(s.item_id);
        if(!d||s.count<1||s.count>d->stack_limit||!isfinite(s.durability))return false;
        if(d->stack_limit==1){
            if(!s.instance_id||s.instance_id>=g->inventory.next_instance_id||s.durability<0||s.durability>d->durability_max)return false;
            for(int j=0;j<i;j++)if(g->inventory.slots[j].instance_id==s.instance_id)return false;
        }else if(s.instance_id||s.durability!=0)return false;
    }
    if(!isfinite(g->craft.elapsed)||g->craft.elapsed<0||g->craft.batches_left<0||g->craft.batches_left>9999||
       g->craft.batches_pending<0||g->craft.batches_pending>9999||g->craft.batches_left+g->craft.batches_pending>9999||
       !isfinite(g->craft.delivery_elapsed)||g->craft.delivery_elapsed<0||g->craft.delivery_elapsed>6)return false;
    if(g->craft.recipe_id){
        const PeRecipeDef *r=pe_recipe_def(g->craft.recipe_id);
        if(!r||!pe_game_recipe_known(g,g->craft.recipe_id)||r->seconds<=0||g->craft.elapsed>r->seconds+0.25f||(!g->craft.batches_left&&!g->craft.batches_pending))return false;
    }else if(g->craft.batches_left||g->craft.batches_pending||g->craft.elapsed!=0||g->craft.delivery_elapsed!=0)return false;
    for(int i=0;i<g->block_count;i++) {
        const PeBlock *b=&g->blocks[i];
        if(b->x<0||b->x>=pe_world_width(&g->world)-1||b->z<0||b->z>=pe_world_depth(&g->world)-1||b->y<0||b->y>=pe_world_size_y(&g->world)-2||
           (b->item_id!=PE_ITEM_WOOD&&b->item_id!=PE_ITEM_DIRT&&b->item_id!=PE_ITEM_STONE&&b->item_id!=PE_ITEM_FENCE&&b->item_id!=PE_ITEM_LADDER))return false;
        if(g->mode==PE_GAME_LEGACY&&g->player.x+PE_PLAYER_RADIUS>b->x&&g->player.x-PE_PLAYER_RADIUS<b->x+1&&
           g->player.z+PE_PLAYER_RADIUS>b->z&&g->player.z-PE_PLAYER_RADIUS<b->z+1&&
           g->player.y+PE_PLAYER_HEIGHT>b->y&&g->player.y<b->y+1)return false;
        for(int j=0;j<i;j++)if(g->blocks[j].x==b->x&&g->blocks[j].y==b->y&&g->blocks[j].z==b->z)return false;
    }
    for(int i=0;i<g->cell_count;i++){
        const PeB45Cell *c=&g->cells[i];
        if(!pe_b45_valid(c)||c->x<0||c->y<0||c->z<0||c->x>=2*(pe_world_width(&g->world)-1)||c->y>=2*(pe_world_size_y(&g->world)-1)||c->z>=2*(pe_world_depth(&g->world)-1))return false;
        PeVec3 lo={g->player.x-PE_PLAYER_RADIUS,g->player.y,g->player.z-PE_PLAYER_RADIUS};
        PeVec3 hi={g->player.x+PE_PLAYER_RADIUS,g->player.y+PE_PLAYER_HEIGHT,g->player.z+PE_PLAYER_RADIUS};
        if(g->mode==PE_GAME_LEGACY&&pe_b45_overlaps_aabb(c,lo,hi))return false;
        for(int j=0;j<i;j++)if(g->cells[j].x==c->x&&g->cells[j].y==c->y&&g->cells[j].z==c->z)return false;
        for(int j=0;j<g->block_count;j++){
            PeBlock b=g->blocks[j];if(pe_b45_overlaps_aabb(c,(PeVec3){b.x,b.y,b.z},(PeVec3){b.x+1,b.y+1,b.z+1}))return false;
        }
        for(int x=0;x<3;x++)for(int y=0;y<3;y++)for(int z=0;z<3;z++){
            PeVec3 p={c->x*0.5f+0.005f+x*0.245f,c->y*0.5f+0.005f+y*0.245f,c->z*0.5f+0.005f+z*0.245f};
            if(pe_b45_contains(c,p)&&pe_world_density(&g->world,p)>=PE_ISO_LEVEL)return false;
        }
    }
    for(int i=0;i<g->tree_count;i++) {
        const PeTree *t=&g->trees[i];
        if(!vecfinite(t->position)||t->position.x<0||t->position.x>=pe_world_width(&g->world)||t->position.z<0||t->position.z>=pe_world_depth(&g->world)||
           t->position.y<0||t->position.y>=pe_world_size_y(&g->world)||t->remaining<0||t->remaining>1000000||
           !isfinite(t->health)||t->health<0||t->health>255||(t->remaining>0&&t->health<=0)||(t->remaining==0&&t->health!=0))return false;
    }
    if(g->mode==PE_GAME_ADVENTURE||pe_settlement_matches(&g->world)){
        PeSandboxCollision *collision=pe_sandbox_collision_create();
        if(!collision)return false;
        bool clear=pe_game_collision_update(g,collision,NULL,0)&&
            !(trees?pe_sandbox_collision_overlap(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT):
                pe_sandbox_collision_overlap_geometry(collision,g->player,PE_PLAYER_RADIUS,PE_PLAYER_HEIGHT));
        pe_sandbox_collision_free(collision);if(!clear)return false;
    }
    return true;
}
static void encode(Stream *s,const PeGame *g) {
    put32(s,g->world.seed);put32(s,g->world.revision);
    putvec(s,g->player);putvec(s,g->velocity);put32(s,g->grounded?1u:0u);putf(s,g->yaw);putf(s,g->pitch);putd(s,g->game_seconds);
    for(int i=0;i<PE_INVENTORY_SLOTS;i++){put32(s,(uint32_t)g->inventory.slots[i].item_id);put32(s,(uint32_t)g->inventory.slots[i].count);}
    put32(s,(uint32_t)g->craft.recipe_id);put32(s,(uint32_t)g->craft.batches_left);putf(s,g->craft.elapsed);
    put32(s,(uint32_t)g->block_count);
    for(int i=0;i<g->block_count;i++){put32(s,(uint32_t)g->blocks[i].x);put32(s,(uint32_t)g->blocks[i].y);put32(s,(uint32_t)g->blocks[i].z);put32(s,(uint32_t)g->blocks[i].item_id);}
    put32(s,(uint32_t)g->tree_count);
    for(int i=0;i<g->tree_count;i++){putvec(s,g->trees[i].position);put32(s,(uint32_t)g->trees[i].remaining);}
    put32(s,g->gathered);put32(s,g->crafted);put32(s,g->built);
    if(s->pos+PE_VOXEL_COUNT*2u>s->size){s->ok=false;return;}
    for(int z=0;z<PE_WORLD_Z;z++)for(int y=0;y<PE_WORLD_Y;y++)for(int x=0;x<PE_WORLD_X;x++){PeVoxel v=pe_world_get(&g->world,x,y,z);s->p[s->pos++]=v.volume;s->p[s->pos++]=v.material;}
    /* Version 2 extends the v1 payload without reinterpreting its prefix. */
    put32(s,g->world.source);put32(s,(uint32_t)g->world.origin_x);put32(s,(uint32_t)g->world.origin_y);put32(s,(uint32_t)g->world.origin_z);
    put32(s,g->inventory.next_instance_id);
    for(int i=0;i<PE_INVENTORY_SLOTS;i++){put32(s,g->inventory.slots[i].instance_id);putf(s,g->inventory.slots[i].durability);}
    put32(s,g->equipped_instance_id);put32(s,g->rng_state);
    putf(s,g->stamina);putf(s,g->stamina_max);putf(s,g->stamina_regen);putf(s,g->stamina_recover_delay);putf(s,g->stamina_idle);putf(s,g->tool_cooldown);
    put32(s,(uint32_t)g->craft.batches_pending);putf(s,g->craft.delivery_elapsed);
    for(int i=0;i<g->tree_count;i++)putf(s,g->trees[i].health);
    put32(s,(uint32_t)g->cell_count);
    for(int i=0;i<g->cell_count;i++){
        put32(s,(uint32_t)g->cells[i].x);put32(s,(uint32_t)g->cells[i].y);put32(s,(uint32_t)g->cells[i].z);
        if(s->pos+2>s->size){s->ok=false;return;}
        s->p[s->pos++]=g->cells[i].type;s->p[s->pos++]=g->cells[i].material;
    }
    put32(s,(uint32_t)g->mode);put32(s,(uint32_t)g->adventure_mission);
    put32(s,g->arrival_complete?1:0);put32(s,g->supplies_claimed?1:0);put32(s,g->money);put32(s,g->scanner_mask);
    put32(s,(uint32_t)g->known_recipe_count);
    for(int i=0;i<g->known_recipe_count;i++)put32(s,(uint32_t)g->known_recipes[i]);
    for(int i=0;i<PE_QUICKSLOTS;i++){put32(s,(uint32_t)g->quickslots[i].item_id);put32(s,g->quickslots[i].instance_id);}
    putf(s,g->hp);putf(s,g->max_hp);putd(s,g->juice_cooldown);putd(s,g->juice_buff_elapsed);
    put32(s,g->juice_buff_tick);put32(s,g->juice_buff_active?1:0);
    put32(s,g->welcome_page);
    /* V7 preserves every older byte offset, then appends depth and extra samples. */
    unsigned legacy_depth=g->world.extended?PE_WORLD_Z:(unsigned)pe_world_depth(&g->world);
    put32(s,legacy_depth);
    size_t prefix_count=(size_t)PE_WORLD_X*PE_WORLD_Y*legacy_depth;
    if(s->pos+(prefix_count-PE_VOXEL_COUNT)*2u>s->size){s->ok=false;return;}
    for(size_t i=PE_VOXEL_COUNT;i<prefix_count;i++){s->p[s->pos++]=g->world.voxels[i].volume;s->p[s->pos++]=g->world.voxels[i].material;}
    /* V8 preserves the V7 prefix and adds dimensions. Nonlegacy dimensions
     * carry their full x-fast payload; the prefix remains a bounded crop. */
    put32(s,(uint32_t)pe_world_width(&g->world));put32(s,(uint32_t)pe_world_size_y(&g->world));put32(s,(uint32_t)pe_world_depth(&g->world));
    if(g->world.extended){
        size_t count=pe_world_voxel_count(&g->world);const PeVoxel *v=pe_world_data_const(&g->world);
        if(s->pos+count*2u>s->size){s->ok=false;return;}
        for(size_t i=0;i<count;i++){s->p[s->pos++]=v[i].volume;s->p[s->pos++]=v[i].material;}
    }
    put32(s,g->annot_complete?1u:0u);
    for(int i=0;i<PE_TOWN_V8_STOCK_CAPACITY;i++){put32(s,g->town_stock[i].initialized?1u:0u);put32(s,g->town_stock[i].remaining);putd(s,g->town_stock[i].create_time);}
    /* V9 retains the exact V8 payload and its first seventeen stable offers. */
    put32(s,g->medicine_state);
    put32(s,PE_TOWN_STOCK_CAPACITY-PE_TOWN_V8_STOCK_CAPACITY);
    for(int i=PE_TOWN_V8_STOCK_CAPACITY;i<PE_TOWN_STOCK_CAPACITY;i++){
        put32(s,g->town_stock[i].initialized?1u:0u);put32(s,g->town_stock[i].remaining);putd(s,g->town_stock[i].create_time);
    }
    /* V10 preserves every V9 byte, then stores hunt progression and deployed instances. */
    put32(s,g->hunt_state);putf(s,g->hunt_destination.x);putf(s,g->hunt_destination.y);putf(s,g->hunt_destination.z);
    put32(s,g->deployed_count);
    for(unsigned i=0;i<PE_MAX_DEPLOYABLES;i++){
        const PeDeployable *d=&g->deployed[i];put32(s,(uint32_t)d->item_id);put32(s,d->instance_id);putf(s,d->durability);
        putf(s,d->position.x);putf(s,d->position.y);putf(s,d->position.z);put32(s,d->quarter_turn);
    }
    /* V11 preserves the literal V10 payload and appends only quest death credit. */
    put32(s,g->hunt_kills);
    /* V12 retains exactly V11's bytes; only new hunt-state values are enabled. */
    /* V13 appends the independent group9 Med Pack cooldown, never the juice clock. */
    putf(s,g->medpack_cooldown);
    /* V14 preserves the literal V13 payload, then three fixed worn instances. */
    for(unsigned i=0;i<3;i++){
        const PeWornItem *w=&g->outfit.items[i];
        put32(s,(uint32_t)w->item_id);put32(s,w->instance_id);putf(s,w->durability);
    }
    /* Bag retains the exact shield item and wear; never serialize a second owner. */
    put32(s,g->shield_instance_id);
}
/* Decode signed-domain values by range check before C conversion. */
static int getint(Stream *s) {uint32_t v=get32(s);if(v>2147483647u){s->ok=false;return 0;}return (int)v;}
static int32_t geti32(Stream *s){uint32_t u=get32(s);int32_t v;memcpy(&v,&u,sizeof v);return v;}
static void decode(Stream *s,PeGame *g,uint32_t version) {
    g->hp=pe_player_defaults.hp;g->max_hp=pe_player_defaults.hp_max;
    g->world.seed=get32(s);g->world.revision=get32(s);
    g->player=getvec(s);g->velocity=getvec(s);uint32_t grounded=get32(s);if(grounded>1)s->ok=false;g->grounded=grounded!=0;
    g->yaw=getf(s);g->pitch=getf(s);g->game_seconds=getd(s);
    for(int i=0;i<PE_INVENTORY_SLOTS;i++){g->inventory.slots[i].item_id=getint(s);g->inventory.slots[i].count=getint(s);}
    g->craft.recipe_id=getint(s);g->craft.batches_left=getint(s);g->craft.elapsed=getf(s);
    g->block_count=getint(s);if(g->block_count>PE_MAX_BLOCKS){s->ok=false;return;}
    for(int i=0;i<g->block_count;i++){g->blocks[i].x=getint(s);g->blocks[i].y=getint(s);g->blocks[i].z=getint(s);g->blocks[i].item_id=getint(s);}
    g->tree_count=getint(s);if(g->tree_count>PE_MAX_TREES){s->ok=false;return;}
    for(int i=0;i<g->tree_count;i++){g->trees[i].position=getvec(s);g->trees[i].remaining=getint(s);}
    g->gathered=get32(s);g->crafted=get32(s);g->built=get32(s);
    if(s->pos+PE_VOXEL_COUNT*2u>s->size){s->ok=false;return;}
    for(int i=0;i<PE_VOXEL_COUNT;i++){g->world.voxels[i].volume=s->p[s->pos++];g->world.voxels[i].material=s->p[s->pos++];}
    if(version>=2){
        g->world.source=get32(s);g->world.origin_x=geti32(s);g->world.origin_y=geti32(s);g->world.origin_z=geti32(s);
        g->inventory.next_instance_id=get32(s);
        for(int i=0;i<PE_INVENTORY_SLOTS;i++){g->inventory.slots[i].instance_id=get32(s);g->inventory.slots[i].durability=getf(s);}
        g->equipped_instance_id=get32(s);g->rng_state=get32(s);
        if(version==2&&g->equipped_instance_id){
            const PeSlot *slot=pe_game_equipped_tool(g);const PeItemDef *def=slot?pe_item_def(slot->item_id):NULL;
            if(def&&def->tool_kind==PE_TOOL_NONE)g->equipped_instance_id=0;
        }
        g->stamina=getf(s);g->stamina_max=getf(s);g->stamina_regen=getf(s);g->stamina_recover_delay=getf(s);g->stamina_idle=getf(s);g->tool_cooldown=getf(s);
        g->craft.batches_pending=getint(s);g->craft.delivery_elapsed=getf(s);
        for(int i=0;i<g->tree_count;i++)g->trees[i].health=getf(s);
        if(version>=3){
            g->cell_count=getint(s);if(g->cell_count>PE_MAX_B45_CELLS){s->ok=false;return;}
            for(int i=0;i<g->cell_count;i++){
                g->cells[i].x=getint(s);g->cells[i].y=getint(s);g->cells[i].z=getint(s);
                if(s->pos+2>s->size){s->ok=false;return;}
                g->cells[i].type=s->p[s->pos++];g->cells[i].material=s->p[s->pos++];
            }
        }
    }else{
        /* V1 had no identity/wear/stamina fields: assign unique instances and
         * source player defaults. Existing quantities and craft progress stay. */
        g->inventory.next_instance_id=1;
        for(int i=0;i<PE_INVENTORY_SLOTS;i++){
            PeSlot *slot=&g->inventory.slots[i];const PeItemDef *def=pe_item_def(slot->item_id);
            if(slot->count&&def&&def->stack_limit==1){slot->instance_id=g->inventory.next_instance_id++;slot->durability=(float)def->durability_max;}
        }
        g->rng_state=g->world.seed?g->world.seed:UINT32_C(0x9e3779b9);
        g->stamina=g->stamina_max=pe_player_defaults.stamina_max;
        g->stamina_regen=pe_player_defaults.stamina_recover_speed;g->stamina_recover_delay=pe_player_defaults.stamina_recover_delay;
        for(int i=0;i<g->tree_count;i++)g->trees[i].health=g->trees[i].remaining?255:0;
    }
    if(version>=4){
        uint32_t mode=get32(s);if(mode>PE_GAME_ADVENTURE){s->ok=false;return;}g->mode=(PeGameMode)mode;
        g->adventure_mission=getint(s);
        uint32_t arrival=get32(s),supplies=get32(s);if(arrival>1||supplies>1)s->ok=false;
        g->arrival_complete=arrival!=0;g->supplies_claimed=supplies!=0;g->money=get32(s);g->scanner_mask=get32(s);
        g->known_recipe_count=getint(s);if(g->known_recipe_count>PE_KNOWN_RECIPES){s->ok=false;return;}
        for(int i=0;i<g->known_recipe_count;i++)g->known_recipes[i]=getint(s);
        for(int i=0;i<PE_QUICKSLOTS;i++){g->quickslots[i].item_id=getint(s);g->quickslots[i].instance_id=get32(s);}
        g->hp=getf(s);g->max_hp=getf(s);g->juice_cooldown=getd(s);g->juice_buff_elapsed=getd(s);
        g->juice_buff_tick=get32(s);uint32_t active=get32(s);if(active>1)s->ok=false;g->juice_buff_active=active!=0;
    }
    g->welcome_page=version>=6?get32(s):(g->mode==PE_GAME_ADVENTURE?PE_ADVENTURE_WELCOME_PAGES:0);
    g->world.depth=version>=7?get32(s):PE_WORLD_Z;
    if(g->world.depth!=PE_WORLD_Z&&g->world.depth!=PE_WORLD_MAX_Z){s->ok=false;return;}
    size_t count=pe_world_voxel_count(&g->world);
    if(s->pos+(count-PE_VOXEL_COUNT)*2u>s->size){s->ok=false;return;}
    for(size_t i=PE_VOXEL_COUNT;i<count;i++){g->world.voxels[i].volume=s->p[s->pos++];g->world.voxels[i].material=s->p[s->pos++];}
    if(version>=8){
        unsigned x=get32(s),y=get32(s),z=get32(s);
        if(x<2||x>PE_WORLD_LIMIT_X||y<2||y>PE_WORLD_LIMIT_Y||z<2||z>PE_WORLD_LIMIT_Z){s->ok=false;return;}
        bool inline_world=x==PE_WORLD_X&&y==PE_WORLD_Y&&(z==PE_WORLD_Z||z==PE_WORLD_MAX_Z);
        if(inline_world){if(z!=g->world.depth){s->ok=false;return;}}
        else {
            size_t full=(size_t)x*y*z;
            if(s->pos+full*2u>s->size){s->ok=false;return;}
            uint32_t revision=g->world.revision;
            if(!pe_world_resize(&g->world,(int)x,(int)y,(int)z)){s->ok=false;return;}
            g->world.revision=revision;PeVoxel *v=pe_world_data(&g->world);
            for(size_t i=0;i<full;i++){v[i].volume=s->p[s->pos++];v[i].material=s->p[s->pos++];}
        }
        uint32_t complete=get32(s);if(complete>1)s->ok=false;g->annot_complete=complete!=0;
        for(int i=0;i<PE_TOWN_V8_STOCK_CAPACITY;i++){
            uint32_t initialized=get32(s);if(initialized>1)s->ok=false;
            g->town_stock[i].initialized=initialized!=0;g->town_stock[i].remaining=get32(s);g->town_stock[i].create_time=getd(s);
        }
    }
    if(version>=9){
        g->medicine_state=get32(s);
        if(get32(s)!=PE_TOWN_STOCK_CAPACITY-PE_TOWN_V8_STOCK_CAPACITY){s->ok=false;return;}
        for(int i=PE_TOWN_V8_STOCK_CAPACITY;i<PE_TOWN_STOCK_CAPACITY;i++){
            uint32_t initialized=get32(s);if(initialized>1)s->ok=false;
            g->town_stock[i].initialized=initialized!=0;g->town_stock[i].remaining=get32(s);g->town_stock[i].create_time=getd(s);
        }
    }
    if(version>=10){
        g->hunt_state=get32(s);g->hunt_destination=(PeVec3){getf(s),getf(s),getf(s)};
        g->deployed_count=get32(s);if(g->deployed_count>PE_MAX_DEPLOYABLES){s->ok=false;return;}
        for(unsigned i=0;i<PE_MAX_DEPLOYABLES;i++){
            PeDeployable *d=&g->deployed[i];d->item_id=getint(s);d->instance_id=get32(s);d->durability=getf(s);
            d->position=(PeVec3){getf(s),getf(s),getf(s)};d->quarter_turn=get32(s);
        }
    }
    if(version>=11)g->hunt_kills=get32(s);
    else if(g->hunt_state>PE_HUNT_ENCOUNTER_PENDING)s->ok=false;
    if(version<12&&g->hunt_state>PE_HUNT_RETURN_PENDING)s->ok=false;
    g->medpack_cooldown=version>=13?getf(s):0;
    if(version>=14)for(unsigned i=0;i<3;i++){
        PeWornItem *w=&g->outfit.items[i];
        w->item_id=getint(s);w->instance_id=get32(s);w->durability=getf(s);
    }
    g->shield_instance_id=version>=15?get32(s):0;

}
bool pe_game_save(const PeGame *g,const char *path,char *error,size_t capacity) {
    _Static_assert(sizeof(float)==4&&sizeof(double)==8,"Save format needs 32/64-bit floats");
    if(!g||!path||!path[0]||!valid(g,true,true)){err(error,capacity,"Invalid save path or game state.");return false;}
    size_t buffer_size=pe_world_voxel_count(&g->world)*2u+PE_MAX_VOXEL_COUNT*2u+32768u;
    unsigned char *data=malloc(buffer_size);
    if(!data){err(error,capacity,"Out of memory.");return false;}
    Stream s={data,16,buffer_size,true};encode(&s,g);
    memcpy(data,"PECS",4);Stream header={data,4,16,true};put32(&header,SAVE_VERSION);put32(&header,(uint32_t)(s.pos-16));put32(&header,checksum(data+16,s.pos-16));
    size_t n=strlen(path);
    char *tmp=malloc(n+16);
    if(!tmp){free(data);err(error,capacity,"Out of memory.");return false;}
    snprintf(tmp,n+16,"%s.tmp.XXXXXX",path);
    int fd=mkstemp(tmp);FILE *f=fd>=0?fdopen(fd,"wb"):NULL;
    bool ok=s.ok&&f!=NULL;
    if(f){if(fwrite(data,1,s.pos,f)!=s.pos||fflush(f)!=0||fsync(fileno(f))!=0)ok=false;if(fclose(f)!=0)ok=false;}
    else if(fd>=0)close(fd);
    if(ok&&rename(tmp,path)!=0)ok=false;
    if(!ok){unlink(tmp);err(error,capacity,"Could not write save file.");}else err(error,capacity,"");
    free(tmp);free(data);return ok;
}
bool pe_game_load(PeGame *g,const char *path,char *error,size_t capacity) {
    if(!g||!path){err(error,capacity,"Invalid load path.");return false;}
    FILE *f=fopen(path,"rb");if(!f){err(error,capacity,"Save file not found.");return false;}
    unsigned char *data=malloc(SAVE_CAPACITY+1u);PeGame *next=calloc(1,sizeof *next);
    if(!data||!next){free(data);free(next);fclose(f);err(error,capacity,"Out of memory.");return false;}
    size_t size=fread(data,1,SAVE_CAPACITY+1u,f);bool ok=!ferror(f)&&size>=16&&size<=SAVE_CAPACITY;
    fclose(f);
    uint32_t version=0;
    if(ok){Stream h={data,4,size,true};version=get32(&h);uint32_t payload=get32(&h),sum=get32(&h);
        ok=memcmp(data,"PECS",4)==0&&(version>=1&&version<=SAVE_VERSION)&&payload==size-16&&sum==checksum(data+16,size-16);
    }
    bool migration_failed=false;float migration_distance=0;
    if(ok){Stream s={data,16,size,true};decode(&s,next,version);ok=s.ok&&s.pos==size&&valid(next,version!=4,version>=14);
        /* Old native actors displayed these clothes without owning their wear.
         * Validate every existing field first: allocating new IDs must never
         * launder a malformed counter, alias, inventory or collision state. */
        if(ok&&version<14)ok=pe_outfit_init(next)&&pe_outfit_state_valid(next);}
    if(ok&&version==4&&next->mode==PE_GAME_ADVENTURE){
        PeVec3 previous=next->player;
        PeSandboxCollision *collision=pe_sandbox_collision_create();
        ok=collision&&pe_game_collision_update(next,collision,NULL,0)&&pe_game_collision_recover_trees(next,collision);
        pe_sandbox_collision_free(collision);migration_failed=!ok;
        if(ok){migration_distance=hypotf(next->player.x-previous.x,next->player.z-previous.z);ok=valid(next,true,true);}
    }
    if(ok){pe_game_move(g,next);
        if(migration_distance>0)snprintf(g->message,sizeof g->message,"Game loaded; moved %.2fm to clear a legacy tree overlap.",(double)migration_distance);
        else snprintf(g->message,sizeof g->message,"Game loaded.");
        err(error,capacity,"");
    }
    else err(error,capacity,migration_failed?"Version 4 player overlaps trees; no safe bounded escape. Save unchanged.":"Save is corrupt, incompatible, or contains invalid state.");
    pe_game_destroy(next);free(next);free(data);return ok;
}
