#include "pe_deployable.h"
#include "pe_town_interaction.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"deployable:%d: %s\n",__LINE__,#c);failures++;}}while(0)
static int torch_slot(const PeGame *g){for(int i=0;i<PE_INVENTORY_SLOTS;i++)if(g->inventory.slots[i].item_id==10&&g->inventory.slots[i].count)return i;return -1;}
static void floor_world(PeGame *g){
    memset(pe_world_data(&g->world),0,pe_world_voxel_count(&g->world)*sizeof(PeVoxel));
    for(int z=0;z<pe_world_depth(&g->world);z++)for(int y=0;y<8;y++)for(int x=0;x<pe_world_width(&g->world);x++)pe_world_set(&g->world,x,y,z,(PeVoxel){255,PE_MAT_DIRT});
    g->tree_count=0;g->player=(PeVec3){20,7.5f,20};g->velocity=(PeVec3){0};g->grounded=true;
}
int pe_test_deployable(void){
    failures=0;PeGame *g=calloc(1,sizeof *g);PeSandboxCollision *collision=pe_sandbox_collision_create();char error[256];
    if(!g||!collision){free(g);pe_sandbox_collision_free(collision);return 1;}
    CHECK(pe_game_init_mode(g,91,PE_GAME_ADVENTURE));floor_world(g);CHECK(pe_game_collision_update(g,collision,error,sizeof error));
    PeSceneHit floor=pe_sandbox_collision_ray_geometry(collision,(PeVec3){20,20,20},(PeVec3){0,-1,0},30);
    CHECK(floor.hit);g->player.y=floor.position.y;
    const PeTownOffer *offer=NULL;for(size_t i=0;i<PE_TOWN_STOCK_CAPACITY;i++){const PeTownOffer *o=pe_town_shop_offer_at(i);if(o&&o->item_id==10){offer=o;break;}}
    CHECK(offer!=NULL);if(!offer)goto done;
    CHECK(pe_town_shop_open(g,offer->entity_id));CHECK(pe_town_shop_buy(g,offer->entity_id,offer->store_id,1));
    int slot=torch_slot(g);CHECK(slot>=0);if(slot<0)goto done;
    g->inventory.slots[slot].durability=1200.25f;uint32_t id=g->inventory.slots[slot].instance_id,next=g->inventory.next_instance_id,money=g->money;
    CHECK(pe_game_equip_slot(g,slot));CHECK(pe_game_quickslot_assign(g,0,slot));
    PeInventory before=g->inventory;PeVec3 out={-1,-1,-1},origin={20,g->player.y+1.58f,20},direction={2,-1.58f,0};uint64_t hash=pe_world_hash(&g->world);
    CHECK(pe_deployable_state_valid(g));CHECK(pe_deployable_preview(g,collision,id,origin,direction,&out));
    CHECK(fabsf(out.x-22)<.001f&&fabsf(out.y-floor.position.y)<.001f&&!g->deployed_count&&!memcmp(&before,&g->inventory,sizeof before));
    CHECK(!pe_deployable_preview(g,collision,id,(PeVec3){50,20,20},(PeVec3){0,-1,0},&out));
    CHECK(!pe_deployable_preview(g,collision,id,(PeVec3){NAN,0,0},direction,&out));
    CHECK(!pe_deployable_preview(g,collision,id,origin,(PeVec3){0},&out));
    CHECK(pe_deployable_preview(g,collision,id,(PeVec3){30,20,20},(PeVec3){0,-1,0},&out)); /* inclusive10m */
    CHECK(!pe_deployable_preview(g,collision,id,(PeVec3){30.01f,20,20},(PeVec3){0,-1,0},&out));
    CHECK(pe_deployable_place(g,collision,id,origin,direction));CHECK(g->deployed_count==1&&pe_deployable_state_valid(g));
    CHECK(!g->equipped_instance_id&&!g->quickslots[0].instance_id&&!pe_inventory_count(&g->inventory,10));
    CHECK(g->deployed[0].instance_id==id&&g->deployed[0].durability==1200.25f&&g->inventory.next_instance_id==next&&g->money==money&&pe_world_hash(&g->world)==hash);
    CHECK(!pe_deployable_place(g,collision,id,origin,direction));
    PeVec3 p=g->deployed[0].position,probe={p.x-1,p.y+.2f,p.z};uint32_t picked=0;float distance=-1;
    CHECK(pe_deployable_pick(g,probe,(PeVec3){1,0,0},INFINITY,&picked,&distance)&&picked==id&&fabsf(distance-.92f)<.001f);
    CHECK(!pe_deployable_pick(g,(PeVec3){p.x,p.y+.2f,p.z},(PeVec3){1,0,0},INFINITY,&picked,&distance)); /* no interior exit selection */
    CHECK(!pe_deployable_pick(g,(PeVec3){p.x-20,p.y+.2f,p.z},(PeVec3){1,0,0},0,&picked,&distance));
    CHECK(pe_deployable_pick(g,(PeVec3){p.x-20,p.y+.2f,p.z},(PeVec3){1,0,0},11,&picked,&distance)); /* source obstacle+10 */
    CHECK(pe_game_collision_update(g,collision,error,sizeof error));
    CHECK(!pe_sandbox_collision_ray_geometry(collision,probe,(PeVec3){1,0,0},2).hit); /* trigger is not a blocker */
    PeVec3 local={0,0,1},point=pe_deployable_point(&g->deployed[0],local);CHECK(fabsf(point.y-p.y-1)<.001f);
    CHECK(pe_deployable_turn(g,id));point=pe_deployable_point(&g->deployed[0],local);CHECK(fabsf(point.x-p.x-1)<.001f&&fabsf(point.y-p.y)<.001f);
    for(int i=0;i<3;i++){CHECK(pe_deployable_turn(g,id));}
    CHECK(g->deployed[0].quarter_turn==0);
    PeVec3 player=g->player;g->player=(PeVec3){p.x+4.079f,p.y+.2f,p.z};CHECK(pe_deployable_in_range(g,id));
    g->player.x=p.x+4.081f;CHECK(!pe_deployable_in_range(g,id));CHECK(!pe_deployable_get(g,id));g->player=player;
    PeDeployable saved=g->deployed[0];g->deployed[0].quarter_turn=4;CHECK(!pe_deployable_state_valid(g));g->deployed[0]=saved;
    g->deployed[0].durability=NAN;CHECK(!pe_deployable_state_valid(g));g->deployed[0]=saved;
    g->deployed[0].instance_id=next;CHECK(!pe_deployable_state_valid(g));g->deployed[0]=saved;
    g->deployed[1]=saved;g->deployed_count=2;CHECK(!pe_deployable_state_valid(g));g->deployed_count=1;CHECK(!pe_deployable_state_valid(g));g->deployed[1]=(PeDeployable){0};
    PeSlot prior=g->inventory.slots[slot];g->inventory.slots[slot]=(PeSlot){10,1,id,1200.25f};CHECK(!pe_deployable_state_valid(g));g->inventory.slots[slot]=prior;
    while(pe_inventory_add(&g->inventory,1,1)){}before=g->inventory;
    CHECK(!pe_deployable_get(g,id)&&g->deployed_count==1&&!memcmp(&before,&g->inventory,sizeof before));
    CHECK(pe_inventory_remove(&g->inventory,1,1));int charcoal=pe_inventory_count(&g->inventory,1001);
    CHECK(pe_deployable_get(g,id)&&g->deployed_count==0&&pe_inventory_count(&g->inventory,1001)==charcoal+1&&!pe_inventory_count(&g->inventory,10));
    CHECK(pe_deployable_state_valid(g)&&!pe_deployable_get(g,id)&&!pe_deployable_turn(g,id));
    /* A surface disappearing after preview must not commit the old position. */
    CHECK(pe_inventory_remove(&g->inventory,1,1));CHECK(pe_town_shop_buy(g,offer->entity_id,offer->store_id,1));slot=torch_slot(g);CHECK(slot>=0);
    if(slot>=0){
        id=g->inventory.slots[slot].instance_id;CHECK(pe_deployable_preview(g,collision,id,origin,direction,&out));before=g->inventory;
        memset(pe_world_data(&g->world),0,pe_world_voxel_count(&g->world)*sizeof(PeVoxel));g->world.revision++;
        CHECK(pe_game_collision_update(g,collision,error,sizeof error));CHECK(!pe_deployable_place(g,collision,id,origin,direction));CHECK(!g->deployed_count&&!memcmp(&before,&g->inventory,sizeof before));
    }
done:
    pe_game_destroy(g);free(g);pe_sandbox_collision_free(collision);return failures;
}
