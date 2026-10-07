/* Real sandbox routing, source dialogue and saved-history owner. No GPU. */
#include "pe_sandbox_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"conversation UI:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){
    PeGame *g=calloc(1,sizeof *g);PeSandboxUI *u=calloc(1,sizeof *u);
    CHECK(g&&u&&pe_game_init_mode(g,217,PE_GAME_ADVENTURE));
    pe_sandbox_ui_init(u,1280,720);u->camera_cursor=true;
    CHECK(pe_sandbox_ui_open_welcome(u,g,20008,true));
    PeSandboxUIInput input={.focused=true,.quickslot=-1};
    (void)pe_sandbox_ui_route(u,g,input);
    CHECK(g->conversation_history.count==1);
    CHECK(!strcmp(g->conversation_history.entries[0].speaker,"Westina Nangle"));
    CHECK(!strstr(g->conversation_history.entries[0].content,"\"playername\""));
    (void)pe_sandbox_ui_route(u,g,input);CHECK(g->conversation_history.count==1);
    input.response=1;(void)pe_sandbox_ui_route(u,g,input);
    CHECK(g->conversation_history.count==2);
    input.response=0;input.escape=true;(void)pe_sandbox_ui_route(u,g,input);
    input.escape=false;CHECK(u->dialogue.stage==PE_ADVENTURE_DIALOGUE_IDLE);
    PeGame *before=calloc(1,sizeof *before);CHECK(before&&pe_game_clone(before,g));
    CHECK(pe_inventory_add(&g->inventory,916,2));
    int juice_count=pe_inventory_count(&g->inventory,916);
    int juice=-1;for(int k=0;k<pe_inventory_capacity(&g->inventory);k++)if(g->inventory.slots[k].item_id==916){juice=k;break;}
    CHECK(juice>=0);g->quickslots[0]=(PeQuickslot){916,g->inventory.slots[juice].instance_id};g->hp=100;
    input.quickslot=0;input.left_pressed=true;input.mouse_x=u->layout.history_button.x+1;input.mouse_y=u->layout.history_button.y+1;
    PeSandboxUIResult r=pe_sandbox_ui_route(u,g,input);CHECK(u->history.open&&r.consume_world&&!r.npc_interaction&&!r.camera_cycle_allowed);
    CHECK(pe_inventory_count(&g->inventory,916)==juice_count&&g->hp==100);input.quickslot=-1;
    pe_game_destroy(before);CHECK(pe_game_clone(before,g));
    CHECK(!memcmp(&g->conversation_history,&before->conversation_history,sizeof g->conversation_history));
    /* Wheel is presentation-only; no quest advancement or inventory mutation. */
    u->history.content_height=1600;u->history.scroll=1000;input.left_pressed=false;input.wheel=2;
    input.mouse_x=u->layout.history_content.x+5;input.mouse_y=u->layout.history_content.y+5;
    r=pe_sandbox_ui_route(u,g,input);CHECK(r.consume_world&&u->history.scroll==920);
    CHECK(!memcmp(&g->inventory,&before->inventory,sizeof g->inventory)&&g->welcome_page==before->welcome_page);
    /* Real modal scrollbar press/hold/release, preserving the grab offset
       and ownership while the pointer leaves the track/window. */
    input.wheel=0;input.left_pressed=true;
    PeTrainingUIRect thumb=pe_ui_history_thumb(&u->history,&u->layout);
    input.mouse_x=thumb.x+thumb.w*.5f;input.mouse_y=thumb.y+thumb.h*.25f;
    float scroll_before=u->history.scroll;
    r=pe_sandbox_ui_route(u,g,input);CHECK(r.consume_world&&u->history.scroll_dragging&&fabsf(u->history.scroll-scroll_before)<.01f);
    input.left_pressed=false;input.mouse_x=0;input.mouse_y-=90;
    r=pe_sandbox_ui_route(u,g,input);CHECK(r.consume_world&&u->history.scroll_dragging&&u->history.scroll<scroll_before);
    input.left_released=true;input.mouse_y=-100;
    r=pe_sandbox_ui_route(u,g,input);CHECK(r.consume_world&&!u->history.scroll_dragging&&u->history.scroll==0);
    input.left_released=false;input.left_pressed=true;input.mouse_x=u->layout.history_scrollbar.x+1;input.mouse_y=u->layout.history_scrollbar.y+100;
    (void)pe_sandbox_ui_route(u,g,input);CHECK(u->history.scroll_dragging);
    input.left_pressed=false;input.focused=false;
    r=pe_sandbox_ui_route(u,g,input);CHECK(r.consume_world&&!u->history.scroll_dragging);
    input.focused=true;input.escape=true;input.quickslot=0;
    r=pe_sandbox_ui_route(u,g,input);CHECK(r.consume_world&&!u->history.open&&!r.camera_cycle_allowed);
    CHECK(pe_inventory_count(&g->inventory,916)==juice_count&&g->hp==100);
    input.escape=false;input.quickslot=-1;input.left_pressed=true;
    input.mouse_x=u->layout.history_button.x+1;input.mouse_y=u->layout.history_button.y+1;
    (void)pe_sandbox_ui_route(u,g,input);CHECK(u->history.open&&!u->history.scroll_dragging);
    input.wheel=0;input.left_pressed=true;input.mouse_x=u->layout.history_clear.x+1;input.mouse_y=u->layout.history_clear.y+1;
    r=pe_sandbox_ui_route(u,g,input);CHECK(r.consume_world&&g->conversation_history.count==0&&u->history.open);
    PeSandboxPointerGate gate={0};CHECK(pe_sandbox_pointer_route(&gate,&r,1));
    u->history.scroll_dragging=true; /* closing cancels an outstanding drag too */
    input.mouse_x=u->layout.history_close.x+1;input.mouse_y=u->layout.history_close.y+1;
    r=pe_sandbox_ui_route(u,g,input);CHECK(!u->history.open&&!u->history.scroll_dragging&&r.consume_world);
    CHECK(pe_sandbox_pointer_route(&gate,&r,1)&&r.consume_world);
    input.left_pressed=false;input.mouse_x=10;input.mouse_y=300;r=pe_sandbox_ui_route(u,g,input);
    CHECK(pe_sandbox_pointer_route(&gate,&r,0)&&r.consume_world);
    /* Source menu offers are not history until actually selected. */
    g->welcome_page=9;g->arrival_complete=true;g->adventure_mission=PE_ADVENTURE_SUPPLIES;
    CHECK(pe_sandbox_ui_open_supplier(u,g,20016,true));
    (void)pe_sandbox_ui_route(u,g,input);CHECK(!g->conversation_history.count);
    input.response=1;(void)pe_sandbox_ui_route(u,g,input);
    CHECK(g->conversation_history.count==2&&!strcmp(g->conversation_history.entries[0].speaker,"You"));
    CHECK(!strcmp(g->conversation_history.entries[1].speaker,"Eitai Takayama"));
    pe_game_destroy(before);free(before);pe_game_destroy(g);free(g);free(u);
    puts("conversation UI: presented pages, selected response, shared history input and held release passed");return 0;
}
