#include "pe_training_ui_layout.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL training UI layout %d: %s\n",__LINE__,#x); failures++; } } while (0)
static bool near(float a,float b) { return fabsf(a-b)<.001f; }
static bool inside(PeTrainingUIRect inner,PeTrainingUIRect outer) {
    return inner.x>=outer.x && inner.y>=outer.y && inner.x+inner.w<=outer.x+outer.w+.001f && inner.y+inner.h<=outer.y+outer.h+.001f;
}
int pe_test_training_ui_layout(void) {
    failures=0; PeTrainingUILayout base;
    CHECK(pe_training_ui_layout(1280,720,&base));
    CHECK(base.scale==1 && base.inventory.x==752 && base.inventory.y==110);
    CHECK(base.mission.x==120 && base.mission.w==1040 && base.mission_close.x==1117);
    CHECK(base.health.w==164 && base.stamina.y==678.5f);
    CHECK(base.history.x==310&&base.history.y==114&&base.history.w==660&&base.history.h==492);
    CHECK(near(base.history_content.x,325)&&near(base.history_content.y,151.2f)&&base.history_content.w==630&&base.history_content.h==393);
    CHECK(inside(base.history_content,base.history)&&inside(base.history_clear,base.history)&&inside(base.history_close,base.history)&&inside(base.history_scrollbar,base.history));
    CHECK(inside(base.history_button,base.minimap));
    CHECK(pe_training_ui_contains(base.history_button,1254,149));
    CHECK(!pe_training_ui_contains(base.history_button,1254,175));

    for(int i=0;i<PE_UI_MENU_ITEMS;i++){CHECK(inside(base.menu_items[i],base.menu_panel));CHECK(pe_training_ui_hit(base.menu_items,PE_UI_MENU_ITEMS,base.menu_items[i].x+1,base.menu_items[i].y+1)==i);}
    CHECK(base.replicator.x==200 && base.replicator.w==880 && base.replicator.h==480);
    CHECK(base.equipment.x==-20 && base.equipment.w==620 && base.equipment.h==602);
    CHECK(base.equipment_weapon.x==290 && base.equipment_weapon.y==429);
    CHECK(base.equipment_slots[9].x==525 && base.equipment_slots[9].y==429);
    CHECK(inside(base.replicator_craft,base.replicator));
    CHECK(base.storage.x==152 && base.storage.y==134 && base.dialogue.y==470);
    /* Hand is never one of the assignable nine slots; indices remain source
       hotkey order across the large central health-panel gap. */
    CHECK(pe_training_ui_hit(base.quickslots,9,302,671)==-1);
    CHECK(pe_training_ui_hit(base.quickslots,9,353,671)==0);
    CHECK(pe_training_ui_hit(base.quickslots,9,404,671)==1);
    CHECK(pe_training_ui_hit(base.quickslots,9,772,671)==4);
    CHECK(pe_training_ui_hit(base.quickslots,9,976,671)==8);
    CHECK(pe_training_ui_hit(base.quickslots,9,640,671)==-1);
    CHECK(pe_training_ui_hit(base.quickslots,9,1019,671)==-1);
    CHECK(pe_training_ui_contains(base.build_button,1019,671));
    CHECK(pe_training_ui_contains(base.build_button,1048,696));
    CHECK(!pe_training_ui_contains(base.build_button,1049,696));
    for (int i=0;i<42;i++) {
        PeTrainingUIRect r=base.inventory_slots[i];
        CHECK(inside(r,base.inventory));
        CHECK(pe_training_ui_hit(base.inventory_slots,42,r.x+r.w*.5f,r.y+r.h*.5f)==i);
        CHECK(pe_training_ui_hit(base.inventory_slots,42,r.x+r.w+1,r.y+1)==-1);
        CHECK(pe_training_ui_hit(base.inventory_slots,42,r.x+1,r.y+r.h+1)==-1);
    }
    const float sizes[][2]={{954,1048},{2560,1440},{2560,1080},{640,360},{320,1000}};
    for (size_t j=0;j<sizeof sizes/sizeof sizes[0];j++) {
        PeTrainingUILayout l; CHECK(pe_training_ui_layout(sizes[j][0],sizes[j][1],&l));
        CHECK(l.mission_tracker.y-7*l.scale>=l.minimap.y+l.minimap.h+8*l.scale);
        CHECK(near(l.dialogue.w/l.dialogue.h,base.dialogue.w/base.dialogue.h));
        CHECK(near(l.height-l.hud.y-l.hud.h,(720-base.hud.y-base.hud.h)*l.scale));
        CHECK(near(l.width-l.minimap.x-l.minimap.w,(1280-base.minimap.x-base.minimap.w)*l.scale));
        CHECK(near(l.inventory.y,base.inventory.y*l.scale));
        CHECK(near(l.width-l.inventory.x-l.inventory.w,(1280-base.inventory.x-base.inventory.w)*l.scale));
        PeTrainingUIRect screen={0,0,l.width,l.height};
        CHECK(inside(l.hud,screen) && inside(l.dialogue,screen) && inside(l.inventory,screen) && inside(l.storage,screen));
        for (int i=0;i<30;i++) {
            PeTrainingUIRect r=l.storage_slots[i]; CHECK(inside(r,l.storage));
            CHECK(pe_training_ui_hit(l.storage_slots,30,r.x+r.w*.5f,r.y+r.h*.5f)==i);
        }
        for (int a=PE_UI_TOP_LEFT;a<=PE_UI_BOTTOM_RIGHT;a++) {
            PeTrainingUIRect r=pe_training_ui_map(&l,(PeTrainingUIRect){345,234,48,48},(PeTrainingUIAnchor)a);
            float x=-1,y=-1;
            CHECK(pe_training_ui_unmap(&l,(PeTrainingUIAnchor)a,r.x+r.w*.5f,r.y+r.h*.5f,&x,&y));
            CHECK(near(x,369) && near(y,258));
            CHECK(near(r.w,r.h));
        }
    }
    PeTrainingUILayout tall; CHECK(pe_training_ui_layout(954,1048,&tall));
    CHECK(tall.hud.y>960 && tall.inventory.y<90 && tall.dialogue.y>860);
    CHECK(pe_training_ui_contains((PeTrainingUIRect){10,20,48,48},10,20));
    CHECK(!pe_training_ui_contains((PeTrainingUIRect){10,20,48,48},58,20));
    CHECK(!pe_training_ui_contains((PeTrainingUIRect){10,20,48,48},10,68));
    CHECK(!pe_training_ui_contains((PeTrainingUIRect){10,20,48,48},NAN,21));
    CHECK(!pe_training_ui_contains((PeTrainingUIRect){10,20,INFINITY,48},11,21));
    CHECK(pe_training_ui_hit(NULL,42,1,1)==-1);
    PeTrainingUILayout old=base;
    CHECK(!pe_training_ui_layout(NAN,720,&base) && !memcmp(&old,&base,sizeof old));
    CHECK(!pe_training_ui_layout(1280,0,&base) && !memcmp(&old,&base,sizeof old));
    CHECK(!pe_training_ui_layout(1280,INFINITY,&base) && !memcmp(&old,&base,sizeof old));
    CHECK(!pe_training_ui_layout(1280,720,NULL));
    PeTrainingUIRect uv={9,8,7,6};
    CHECK(pe_training_ui_minimap_uv(27,11,27,11,&uv) && uv.x==.25f && uv.y==.25f && uv.w==.5f && uv.h==.5f);
    CHECK(pe_training_ui_minimap_uv(62,-62,0,0,&uv) && uv.x==.5f && uv.y==0);
    CHECK(pe_training_ui_minimap_uv(620,-620,0,0,&uv) && uv.x==.5f && uv.y==0);
    PeTrainingUIRect before=uv;
    CHECK(!pe_training_ui_minimap_uv(NAN,0,0,0,&uv) && !memcmp(&before,&uv,sizeof uv));
    CHECK(!pe_training_ui_minimap_uv(0,0,0,0,NULL));
    float x=2,y=3;
    CHECK(!pe_training_ui_unmap(&base,(PeTrainingUIAnchor)99,1,1,&x,&y) && x==2 && y==3);
    CHECK(!pe_training_ui_unmap(&base,PE_UI_CENTER,INFINITY,1,&x,&y) && x==2 && y==3);
    /* Moved controls and drawn roots share the same geometry. Resizing keeps
       the whole panel reachable, including its close button and item slots. */
    for(int w=0;w<PE_UI_WINDOW_COUNT;w++){
        PeUIWindows windows={0};PeTrainingUILayout l;
        CHECK(pe_training_ui_windows_layout(1280,720,&windows,&l));
        PeTrainingUIRect start=pe_training_ui_window_rect(&l,w),hud=l.hud;
        CHECK(pe_training_ui_window_drag(&windows,&l,w,start.x+80,start.y+24,true,false,false));
        CHECK(windows.dragging==w+1);
        CHECK(pe_training_ui_window_drag(&windows,&l,-1,start.x+105,start.y+44,false,true,false));
        PeTrainingUIRect moved=pe_training_ui_window_rect(&l,w);
        CHECK(near(moved.x,start.x+25)&&near(moved.y,start.y+20)&&!windows.dragging);
        CHECK(!memcmp(&hud,&l.hud,sizeof hud));
        CHECK(pe_training_ui_window_at(&l,1u<<w,moved.x+10,moved.y+10)==w);
        CHECK(pe_training_ui_window_drag(&windows,&l,w,moved.x+80,moved.y+24,true,false,false));
        CHECK(pe_training_ui_window_drag(&windows,&l,w,-1000,10000,false,true,false));
        CHECK(pe_training_ui_windows_layout(1000,700,&windows,&l));
        CHECK(inside(pe_training_ui_window_rect(&l,w),(PeTrainingUIRect){0,0,1000,700}));
    }
    PeUIWindows windows={0};PeTrainingUILayout moved;
    CHECK(pe_training_ui_windows_layout(1280,720,&windows,&moved));
    PeTrainingUIRect close=moved.inventory_close,slot=moved.inventory_slots[0];
    CHECK(!pe_training_ui_window_drag(&windows,&moved,PE_UI_WINDOW_INVENTORY,close.x+1,close.y+1,true,false,false));
    CHECK(!pe_training_ui_window_drag(&windows,&moved,PE_UI_WINDOW_INVENTORY,slot.x+1,slot.y+1,true,false,false));
    CHECK(pe_training_ui_window_drag(&windows,&moved,PE_UI_WINDOW_INVENTORY,moved.inventory.x+80,moved.inventory.y+24,true,false,false));
    CHECK(pe_training_ui_window_drag(&windows,&moved,-1,400,300,false,false,false));
    CHECK(near(moved.inventory_slots[0].x-moved.inventory.x,26)&&near(moved.inventory_close.y-moved.inventory.y,16));
    CHECK(pe_training_ui_window_drag(&windows,&moved,-1,400,300,false,false,true)&&!windows.dragging);
    PeTrainingUILayout kept=moved;windows.x[0]=NAN;
    CHECK(!pe_training_ui_windows_layout(1000,700,&windows,&moved)&&!memcmp(&kept,&moved,sizeof kept));
    /* History is the highest ordinary window and all dependent hit/draw
       rectangles move together. Source content starts37px below the top,
       so the native32px header must not steal the first history row. */
    for(size_t i=0;i<sizeof sizes/sizeof sizes[0];i++){
        PeUIWindows windows={0};PeTrainingUILayout l;
        CHECK(pe_training_ui_windows_layout(sizes[i][0],sizes[i][1],&windows,&l));
        PeTrainingUIRect original=l.history_content,button=l.history_button;
        unsigned visible=(1u<<PE_UI_WINDOW_HISTORY)|(1u<<PE_UI_WINDOW_MISSION);
        CHECK(pe_training_ui_window_at(&l,visible,l.history.x+25*l.scale,l.history.y+20*l.scale)==PE_UI_WINDOW_HISTORY);
        CHECK(!pe_training_ui_window_drag(&windows,&l,PE_UI_WINDOW_HISTORY,l.history_close.x+l.scale,l.history_close.y+l.scale,true,false,false));
        CHECK(!pe_training_ui_window_drag(&windows,&l,PE_UI_WINDOW_HISTORY,l.history_content.x+l.scale,l.history_content.y+l.scale,true,false,false));
        float old_x=l.history.x,old_y=l.history.y;
        CHECK(pe_training_ui_window_drag(&windows,&l,PE_UI_WINDOW_HISTORY,old_x+20*l.scale,old_y+15*l.scale,true,false,false));
        CHECK(pe_training_ui_window_drag(&windows,&l,-1,l.width+100,l.height+100,false,true,false));
        CHECK(!windows.dragging);
        CHECK(near(l.history_content.x-original.x,l.history.x-old_x));
        CHECK(near(l.history_content.y-original.y,l.history.y-old_y));
        CHECK(inside(l.history,(PeTrainingUIRect){0,0,l.width,l.height}));
        CHECK(inside(l.history_content,l.history)&&inside(l.history_close,l.history)&&inside(l.history_clear,l.history)&&inside(l.history_scrollbar,l.history));
        CHECK(!memcmp(&button,&l.history_button,sizeof button));
        CHECK(pe_training_ui_window_drag(&windows,&l,PE_UI_WINDOW_HISTORY,l.history.x+20*l.scale,l.history.y+15*l.scale,true,false,false));
        CHECK(pe_training_ui_window_drag(&windows,&l,-1,0,0,false,false,true)&&!windows.dragging);
    }
    return failures;
}
