/* Staged draw-only native UI capture. Run solely on a dedicated private display.
 * No sandbox host, input injection, saves or original-game process is involved. */
#include "pe_sandbox_ui.h"
#include "pe_settlement.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"render UI:%d %s\n",__LINE__,#x);exit(1);}}while(0)
static PeSandboxUIInput input(void){return(PeSandboxUIInput){.focused=true,.quickslot=-1};}
static void hover(PeSandboxUI *u,PeTrainingUIRect r){u->mouse_x=r.x+r.w*.5f;u->mouse_y=r.y+r.h*.5f;}
static void click(PeSandboxUI *u,PeGame *g,PeTrainingUIRect r){PeSandboxUIInput i=input();i.left_pressed=true;i.mouse_x=r.x+r.w*.5f;i.mouse_y=r.y+r.h*.5f;pe_sandbox_ui_route(u,g,i);}
static int bag(const PeGame *g,int id){for(int k=0;k<pe_inventory_capacity(&g->inventory);k++)if(g->inventory.slots[k].item_id==id)return k;return -1;}
static void shop(PeSandboxUI *u,PeGame *g,PeSandboxShopMode mode,int item){
    pe_sandbox_ui_cancel_town(u);pe_sandbox_ui_cancel_repair(u);u->inventory_open=false;
    REQUIRE(pe_sandbox_ui_open_town(u,g,20002,"Vendor",(Texture2D){0},true));PeSandboxUIInput i=input();i.response=1;pe_sandbox_ui_route(u,g,i);
    click(u,g,pe_sandbox_ui_shop_tab(u,mode));PeSandboxShopRow row;int index=-1;
    for(int k=0;k<pe_sandbox_ui_shop_count(u,g);k++)if(pe_sandbox_ui_shop_item(u,g,k,&row)&&row.item.item_id==item){index=k;break;}
    REQUIRE(index>=0);u->shop_scroll=index>7?index-7:0;click(u,g,pe_sandbox_ui_shop_row(u,index-u->shop_scroll));hover(u,pe_sandbox_ui_shop_row(u,index-u->shop_scroll));
}
static void capture(PeSandboxUI *u,PeGame *g,const char *directory,const char *name,int w,int h){
    for(int frame=0;frame<3;frame++){BeginDrawing();ClearBackground((Color){28,45,54,255});pe_sandbox_ui_draw(u,g,false);EndDrawing();}
    char path[1024];snprintf(path,sizeof path,"%s/%s-%dx%d.png",directory,name,w,h);
    Image pixels=LoadImageFromScreen();REQUIRE(pixels.data&&pixels.width==w&&pixels.height==h);REQUIRE(ExportImage(pixels,path));UnloadImage(pixels);
}
int main(int argc,char **argv){
    const char *display=getenv("DISPLAY"),*private_display=getenv("PE_UI_PRIVATE_DISPLAY");
    REQUIRE(argc==2&&display&&private_display&&!strcmp(display,private_display)&&!getenv("WAYLAND_DISPLAY"));
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(1280,720,"Private UI render fixture");REQUIRE(IsWindowReady());
    PeGame *g=calloc(1,sizeof *g);REQUIRE(g&&pe_game_init_mode(g,911,PE_GAME_ADVENTURE));char error[256];REQUIRE(pe_settlement_start(g,error,sizeof error));
    g->inventory=(PeInventory){.next_instance_id=g->inventory.next_instance_id,.categorized=true};g->equipped_instance_id=g->shield_instance_id=0;memset(g->quickslots,0,sizeof g->quickslots);
    REQUIRE(pe_inventory_add(&g->inventory,26,1)&&pe_inventory_add(&g->inventory,877,1)&&pe_inventory_add(&g->inventory,924,7)&&pe_inventory_add(&g->inventory,279,40));g->money=2000;
    g->inventory.slots[bag(g,26)].durability=119;g->known_recipes[g->known_recipe_count++]=pe_item_def(877)->learn_ids[0];
    PeSandboxUI u={0};REQUIRE(pe_sandbox_ui_load(&u,PE_SOURCE_ROOT "/rebuild/build/reference-assets",error,sizeof error));
    const int sizes[][2]={{1280,720},{640,480},{320,240}};
    for(unsigned k=0;k<3;k++){
        int w=sizes[k][0],h=sizes[k][1];SetWindowSize(w,h);pe_sandbox_ui_resize(&u,(float)w,(float)h);
        shop(&u,g,PE_SANDBOX_SHOP_BUY,1);click(&u,g,pe_sandbox_ui_shop_quantity_rect(&u));PeSandboxUIInput i=input();i.typed="2";pe_sandbox_ui_route(&u,g,i);capture(&u,g,argv[1],"shop-quantity",w,h);
        shop(&u,g,PE_SANDBOX_SHOP_BUY,1);capture(&u,g,argv[1],"shop-fresh-details",w,h);
        shop(&u,g,PE_SANDBOX_SHOP_SELL,26);capture(&u,g,argv[1],"shop-worn-details",w,h);
        pe_sandbox_ui_cancel_town(&u);u.inventory_open=true;int script=bag(g,877);u.inventory_tab=script/420;u.inventory_page=script%420/42;hover(&u,u.layout.inventory_slots[script%42]);capture(&u,g,argv[1],"inventory-learned",w,h);
        u.inventory_open=false;REQUIRE(pe_sandbox_ui_open_repair(&u,g,true));int axe=bag(g,26);PeSlot expected=g->inventory.slots[axe];REQUIRE(pe_game_repair_insert(g,axe,&expected));hover(&u,pe_sandbox_ui_repair_reserved(&u));capture(&u,g,argv[1],"repair-details",w,h);REQUIRE(pe_game_repair_take(g,NULL));pe_sandbox_ui_cancel_repair(&u);
    }
    pe_sandbox_ui_unload(&u);pe_game_destroy(g);free(g);CloseWindow();puts("private staged UI captures complete");return 0;
}
