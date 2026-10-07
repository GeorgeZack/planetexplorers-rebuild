/* CPU-only actual sandbox gathering dispatch and input ownership. */
#include "../src/pe_sandbox_view.c"
#include <assert.h>
static PeSandboxUIResult plant_route(PeSandboxUI *ui,PeGame *g){
    return pe_sandbox_ui_route(ui,g,(PeSandboxUIInput){.focused=true,.quickslot=-1,.mouse_x=100,.mouse_y=300});
}
int main(void){
    PeGame *g=calloc(1,sizeof *g);assert(g&&pe_game_init_mode(g,221,PE_GAME_ADVENTURE));
    for(int z=0;z<pe_world_depth(&g->world);z++)for(int y=0;y<pe_world_size_y(&g->world);y++)for(int x=0;x<pe_world_width(&g->world);x++)
        pe_world_set(&g->world,x,y,z,(PeVoxel){y<10?255:0,y<10?PE_MAT_DIRT:PE_MAT_AIR});
    g->tree_count=g->cell_count=g->block_count=0;g->player=(PeVec3){32,10.05f,32};g->velocity=(PeVec3){0};
    g->vegetation=(PeVegetation){.installed=true,.count=1,.plants={{.prototype_id=15,.position={33,10,32}}}};
    assert(pe_vegetation_state_valid(g));const PePlantSource *source=pe_vegetation_source(15);assert(source&&source->harvestable);
    PeSandboxCollision *collision=pe_sandbox_collision_create();char error[160]={0};assert(collision&&pe_game_collision_update(g,collision,error,sizeof error));
    PeSandboxCollisionStats before=pe_sandbox_collision_stats(collision);
    PeVec3 origin={g->player.x,g->player.y+1,g->player.z};
    PeVec3 target={33+(source->minimum.x+source->maximum.x)*.5f,10+(source->minimum.y+source->maximum.y)*.5f,32+(source->minimum.z+source->maximum.z)*.5f};
    PeVec3 direction={target.x-origin.x,target.y-origin.y,target.z-origin.z};
    unsigned index=PE_MAX_PLANTS;float distance=-1;assert(pe_vegetation_pick(g,origin,direction,INFINITY,&index,&distance)&&index==0);
    PeSandboxUI ui;pe_sandbox_ui_init(&ui,1280,720);PeSandboxPointerGate gate={0};
    PeSandboxUIResult action=plant_route(&ui,g);assert(action.npc_interaction&&!action.consume_world);
    PeSandboxMelee melee={0};PeSandboxActions tool={0};
    SandboxVegetationInput input={.index=index,.distance=distance,.other_distance=INFINITY,.enabled=true,.available=true,.requested=true,.melee=&melee,.tool=&tool};
    /* Visual availability, nearer NPC/deployable and automatic welcome all win. */
    input.available=false;assert(!sandbox_vegetation_interact(g,collision,input,&action,&gate,2));input.available=true;
    input.other_distance=distance;assert(!sandbox_vegetation_interact(g,collision,input,&action,&gate,2));input.other_distance=INFINITY;
    input.modal_priority=true;assert(!sandbox_vegetation_interact(g,collision,input,&action,&gate,2));input.modal_priority=false;
    ui.inventory_open=true;action=plant_route(&ui,g);assert(!sandbox_vegetation_interact(g,collision,input,&action,&gate,2));ui.inventory_open=false;
    action=plant_route(&ui,g);input.enabled=false;assert(!sandbox_vegetation_interact(g,collision,input,&action,&gate,2));input.enabled=true;
    int count=pe_inventory_count(&g->inventory,source->item_id);uint32_t gathered=g->gathered;
    for(int busy=0;busy<3;busy++){
        melee.action.active=busy==0;melee.tick_pending=busy==1;tool.stage=busy==2?PE_SANDBOX_ACTION_LOOP:PE_SANDBOX_ACTION_IDLE;
        PeSandboxMelee old_melee=melee;PeSandboxActions old_tool=tool;action=plant_route(&ui,g);
        assert(sandbox_vegetation_interact(g,collision,input,&action,&gate,0));
        assert(!g->vegetation.plants[0].depleted&&pe_inventory_count(&g->inventory,source->item_id)==count&&g->gathered==gathered);
        assert(!memcmp(&old_melee,&melee,sizeof melee)&&!memcmp(&old_tool,&tool,sizeof tool));
        assert(action.consume_actions&&!action.consume_world&&sandbox_hunt_runs(&ui,action)&&strstr(g->message,"Finish"));
    }
    melee=(PeSandboxMelee){0};tool=(PeSandboxActions){0};action=plant_route(&ui,g);
    assert(sandbox_vegetation_interact(g,collision,input,&action,&gate,2));
    assert(g->vegetation.plants[0].depleted&&pe_inventory_count(&g->inventory,source->item_id)==count+(int)source->drop_count&&g->gathered==gathered+1);
    assert(action.consume_actions&&!action.consume_world&&!action.npc_interaction&&gate.actions_only&&gate.waiting_for_release);
    assert(sandbox_hunt_runs(&ui,action)); /* same production AI admission */
    assert(!sandbox_vegetation_interact(g,collision,input,&action,&gate,2));
    float start=g->player.x;double seconds=g->game_seconds;
    assert(pe_game_step_collision(g,(PeInput){.move_x=1},PE_FIXED_DT,collision));
    assert(g->player.x>start&&g->game_seconds>seconds); /* no gathering freeze */
    for(int held=0;held<2;held++){
        action=plant_route(&ui,g);assert(pe_sandbox_pointer_route(&gate,&action,2)&&action.consume_actions&&!action.consume_world);
        assert(!sandbox_vegetation_interact(g,collision,input,&action,&gate,2)&&sandbox_hunt_runs(&ui,action));
    }
    action=plant_route(&ui,g);assert(pe_sandbox_pointer_route(&gate,&action,0)&&action.consume_actions&&!action.consume_world);
    action=plant_route(&ui,g);assert(!pe_sandbox_pointer_route(&gate,&action,0));
    assert(!pe_vegetation_pick(g,origin,direction,INFINITY,&index,&distance));
    assert(pe_game_collision_update(g,collision,error,sizeof error));
    PeSandboxCollisionStats after=pe_sandbox_collision_stats(collision);
    assert(before.triangles==after.triangles&&before.construction_rebuilds==after.construction_rebuilds);
    /* A full bag rejects atomically but still consumes its action. */
    g->vegetation.plants[0].depleted=false;
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id};g->equipped_instance_id=g->shield_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    const PeItemDef *wood=pe_item_def(PE_ITEM_WOOD);assert(wood&&pe_inventory_add(&g->inventory,PE_ITEM_WOOD,wood->stack_limit*PE_INVENTORY_SLOTS));
    PeInventory bag=g->inventory;action=plant_route(&ui,g);
    assert(sandbox_vegetation_interact(g,collision,input,&action,&gate,0));
    assert(!g->vegetation.plants[0].depleted&&!memcmp(&bag,&g->inventory,sizeof bag)&&g->gathered==gathered+1);
    assert(strstr(g->message,"room")&&action.consume_actions&&!action.consume_world&&sandbox_hunt_runs(&ui,action));
    pe_sandbox_collision_free(collision);pe_game_destroy(g);free(g);
    puts("Sandbox vegetation host: PASS (no graphics or asset skips)");return 0;
}
