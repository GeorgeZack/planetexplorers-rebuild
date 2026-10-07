#!/usr/bin/env python3
"""Exercise production frontend routing without a desktop or graphics.

Only raylib input queries are mocked; transactions link the real C core.
The bounded input block is extracted to avoid testing a duplicate implementation.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PREFIX = r"""
#include "pe_training.h"
#include "pe_training_ui_layout.h"
#include "pe_ui_input.h"
#include "pe_conversation_history.h"
#include "pe_ui_history_state.h"
#define log mission_log
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#undef assert
#define assert(x) do{if(!(x)){fprintf(stderr,"UI input:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
#include <string.h>
#include <stdio.h>
typedef struct {float x,y;} Vector2;
enum {MOUSE_BUTTON_LEFT,MOUSE_BUTTON_RIGHT,KEY_C,KEY_Y,KEY_I,KEY_Q,KEY_B,KEY_T,KEY_ONE=40,KEY_KP_1=80,KEY_ENTER=100,KEY_SPACE,KEY_ESCAPE};
static bool keys[128],left,right,down,released;
static bool IsKeyPressed(int k){return keys[k];}
static bool IsMouseButtonPressed(int k){return k==MOUSE_BUTTON_LEFT?left:right;}
static bool IsMouseButtonDown(int k){(void)k;return down;}
static bool IsMouseButtonReleased(int k){(void)k;return released;}
static Vector2 GetMouseDelta(void){return(Vector2){5,0};}
static PeTraining training;
static struct{PeTrainingUILayout layout;PeUiHistoryState history;PeConversationHistory conversation_history;}ui;
static float GetMouseWheelMove(void){return 0;}
static Vector2 pointer;
static Vector2 GetMousePosition(void){return pointer;}
static bool menu_open,paused,inventory,log,dragging,equipment_tab,dialogue_input_consumed;
static bool focused=true,smoke_frames,settings_consumed;
static int inventory_tab,build_rotation;
static float preview_yaw;
static void frame(void){
 int talk=pe_training_dialogue(&training);dialogue_input_consumed=false;
 bool pointer_release_pending=false;
 bool history_was_open=ui.history.open;
 bool ui_was_modal=ui.history.open||training.character_open||training.replicator_open||training.kit_open||inventory||log||menu_open;
"""
SUFFIX = r"""}

static void events(void){memset(keys,0,sizeof keys);left=right=down=released=false;}
static void reset(int mission,unsigned dialogue){
 memset(&training,0,sizeof training);memset(&ui,0,sizeof ui);events();
 training.ready=true;training.hp=training.max_hp=500;training.stamina=100;
 training.tutorial=(PeTutorial){(unsigned)mission,3,15,0};training.dialogue_index=dialogue;
 if(mission==PE_TUTORIAL_CAMERA)pe_tutorial_init(&training.tutorial);
 training.equipped_shovel=-1;
 menu_open=paused=inventory=log=dragging=equipment_tab=settings_consumed=false;
 inventory_tab=build_rotation=0;preview_yaw=0;pointer=(Vector2){0};
 assert(pe_training_ui_layout(1280,720,&ui.layout));assert(pe_tutorial_valid(&training.tutorial));
}
int main(void){
 /* Actual production response dispatch, both keyboards, and closing-frame
    ownership: key1 must not also consume the assigned healing quickslot. */
 reset(739,0);training.quickslot_item_id=916;training.juice_count=3;
 keys[KEY_ONE+8]=true;frame();assert(dialogue_input_consumed&&pe_training_dialogue(&training)==300215);
 events();keys[KEY_KP_1]=true;frame();assert(dialogue_input_consumed&&pe_training_dialogue(&training)==300258&&training.juice_count==3);
 events();keys[KEY_ONE]=true;keys[KEY_B]=true;frame();
 assert(dialogue_input_consumed&&!pe_training_dialogue(&training)&&training.juice_count==3&&!training.build_mode&&!training.juice_cooldown_ticks);
 events();keys[KEY_ONE]=true;frame();assert(!dialogue_input_consumed&&training.juice_count==2);
 reset(739,1);events();keys[KEY_ENTER]=true;frame();assert(dialogue_input_consumed&&!pe_training_dialogue(&training));
 reset(739,1);events();left=true;pointer=(Vector2){500,500};frame();assert(dialogue_input_consumed&&!pe_training_dialogue(&training));
 /* Source kit741, actual storage receipt and singleton5-stack conservation. */
 reset(741,1);training.position=(PeVec3){7,1.5f,12};training.kit_open=training.kit_intro_seen=true;
 training.tutorial.stage_objectives=1;training.inventory_open=inventory=true;
 right=true;pointer=(Vector2){202,285};frame();
 assert(training.kit_transferred&&training.juice_count==5&&training.tutorial.mission_id==742);
 events();right=true;frame();assert(training.juice_count==5);
 /* Source inventory/quickbar tutorial: actual pixel drag, required dialogue,
    right-click use and real healing/cooldown transaction. */
 reset(744,1);training.juice_count=10;training.inventory_open=inventory=true;
 left=true;pointer=(Vector2){874,183};frame();assert(inventory_tab==1&&equipment_tab);
 events();left=true;pointer=(Vector2){808,183};frame();assert(inventory_tab==0&&!equipment_tab);
 events();left=true;pointer=(Vector2){802,255};frame();assert(dragging);
 events();released=true;pointer=(Vector2){353,671};frame();
 assert(!dragging&&training.quickslot_item_id==916&&training.juice_count==10&&training.tutorial.mission_id==749);
 assert(pe_training_advance_dialogue(&training,4071)&&pe_training_advance_dialogue(&training,4298));
 events();right=true;frame();assert(training.juice_count==9&&training.tutorial.mission_id==745&&training.juice_cooldown_ticks==600);
 /* Actual bag knife equip then visible Character weapon unequip; inventory
    remains open, and weapon ownership never disappears or duplicates. */
 reset(745,5);training.inventory_open=inventory=true;training.knife_owned=training.equipment_lesson_accepted=true;
 left=true;pointer=(Vector2){874,183};frame();assert(equipment_tab);
 events();right=true;pointer=(Vector2){802,255};frame();
 assert(training.knife_owned&&training.knife_equipped&&training.tutorial.mission_id==746);
 assert(pe_training_advance_dialogue(&training,300229)&&pe_training_advance_dialogue(&training,4299));
 events();keys[KEY_C]=true;frame();assert(training.character_open&&inventory);
 events();right=true;pointer=(Vector2){314,453};frame();assert(!training.knife_equipped&&training.knife_owned);
 /* Real formula1/wood reservation: source lesson721, learnedformula and10wood
    produce one pending craft; repeated click never debits again. */
 reset(721,2);training.replicator_open=training.replicator_intro_seen=true;
 training.gathering.mission=721;training.gathering.accepted=15;
 training.gathering.formula1=true;training.gathering.wood=20;
 left=true;pointer=(Vector2){995,547};frame();assert(training.gathering.crafting&&training.gathering.wood==10);
 events();left=true;frame();assert(training.gathering.crafting&&training.gathering.wood==10);
 /* Real invalidcapacity/resource input rejects atomically. */
 reset(721,2);training.replicator_open=training.replicator_intro_seen=true;
 training.gathering.mission=721;training.gathering.accepted=15;training.gathering.formula1=true;training.gathering.wood=9;
 PeTraining before=training;left=true;pointer=(Vector2){995,547};frame();assert(!memcmp(&before,&training,sizeof before));

 reset(746,2);training.character_open=log=true;training.knife_owned=training.knife_equipped=true;
 right=true;pointer=(Vector2){314,453};frame();assert(training.knife_equipped);
 events();down=true;pointer=(Vector2){400,300};frame();assert(preview_yaw==0);
 reset(746,2);training.character_open=true;keys[KEY_Q]=true;frame();assert(log&&!training.character_open);
 reset(746,2);training.character_open=true;keys[KEY_Y]=true;frame();assert(training.replicator_open&&!training.character_open);
 reset(746,2);training.character_open=true;keys[KEY_I]=true;frame();assert(training.character_open&&inventory&&training.inventory_open);
 reset(744,1);training.juice_count=10;inventory=training.inventory_open=true;menu_open=true;left=true;pointer=(Vector2){802,255};frame();assert(!menu_open&&!dragging);
 reset(746,2);keys[KEY_C]=true;left=true;pointer=(Vector2){578,115};frame();assert(training.character_open);
 reset(746,2);keys[KEY_Q]=true;left=true;pointer=(Vector2){1129,119};frame();assert(log);
 reset(746,2);training.character_open=true;training.quickslot_item_id=916;training.juice_count=10;
 right=true;pointer=(Vector2){353,671};frame();assert(training.juice_count==10);
 events();keys[KEY_ONE]=true;frame();assert(training.juice_count==9);
 reset(744,1);training.juice_count=10;training.character_open=true;inventory=training.inventory_open=dragging=true;
 released=true;pointer=(Vector2){353,671};frame();assert(!training.quickslot_item_id&&!dragging);
 reset(746,2);training.quickslot_item_id=916;training.juice_count=10;menu_open=true;keys[KEY_ONE]=true;
 left=true;pointer=(Vector2){1150,624};frame();assert(paused&&training.juice_count==10);
 const float sizes[][2]={{1280,720},{640,360},{954,1048}};
 for(unsigned k=0;k<sizeof sizes/sizeof sizes[0];k++){
  reset(746,2);assert(pe_training_ui_layout(sizes[k][0],sizes[k][1],&ui.layout));
  left=true;pointer=(Vector2){ui.layout.health.x+1,ui.layout.health.y+1};frame();assert(training.character_open);
  reset(746,2);assert(pe_training_ui_layout(sizes[k][0],sizes[k][1],&ui.layout));
  left=true;pointer=(Vector2){ui.layout.mission_button.x+1,ui.layout.mission_button.y+1};frame();assert(log);
  reset(746,2);assert(pe_training_ui_layout(sizes[k][0],sizes[k][1],&ui.layout));
  menu_open=true;left=true;pointer=(Vector2){ui.layout.health.x+1,ui.layout.health.y+1};frame();assert(!menu_open&&!training.character_open);
 }
 /* Settings closing Escape belongs to the child modal, not Resume. */
 reset(746,2);paused=settings_consumed=true;keys[KEY_ESCAPE]=true;frame();assert(paused);
 events();settings_consumed=false;keys[KEY_ESCAPE]=true;frame();assert(!paused);
 /* Actual history opening owns simultaneous digit use; Escape owns its
    closing frame as well. No synthetic copy of the host Escape dispatch. */
 reset(746,2);training.quickslot_item_id=916;training.juice_count=10;training.hp=100;
 left=true;keys[KEY_ONE]=true;pointer=(Vector2){ui.layout.history_button.x+1,ui.layout.history_button.y+1};frame();
 assert(ui.history.open&&dialogue_input_consumed&&training.juice_count==10&&training.hp==100);
 ui.history.content_height=1600;ui.history.scroll=700;
 events();PeTrainingUIRect thumb=pe_ui_history_thumb(&ui.history,&ui.layout);
 left=true;pointer=(Vector2){thumb.x+thumb.w*.5f,thumb.y+thumb.h*.25f};frame();
 assert(ui.history.scroll_dragging&&dialogue_input_consumed&&fabsf(ui.history.scroll-700)<.01f);
 events();pointer.x=0;pointer.y-=80;frame();assert(ui.history.scroll_dragging&&ui.history.scroll<700&&dialogue_input_consumed);
 events();released=true;pointer.y=-100;frame();assert(!ui.history.scroll_dragging&&ui.history.scroll==0&&dialogue_input_consumed);
 events();keys[KEY_ESCAPE]=keys[KEY_ONE]=keys[KEY_C]=true;frame();
 assert(!ui.history.open&&dialogue_input_consumed&&training.juice_count==10&&training.hp==100&&!training.character_open);
 puts("Actual core transactions through extracted UI routing: storage, tabs, drag/use, equip/unequip, craft conservation passed");return 0;
}
"""

class TrainingUIInput(unittest.TestCase):
    def test_pause_commands_share_draw_geometry(self):
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("C compiler unavailable")
        harness = r"""
#include "pe_training_view.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#undef assert
#define assert(x) do{if(!(x)){fprintf(stderr,"UI input:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
#include <math.h>
int main(void) {
    PeTrainingUILayout layout;
    const float dimensions[][2]={{1280,720},{1920,1080},{1000,800}};
    for(unsigned d=0;d<3;d++) {
        assert(pe_training_ui_layout(dimensions[d][0],dimensions[d][1],&layout));
        for(int index=0;index<4;index++) {
            PeTrainingUIRect r=pe_training_pause_button(&layout,index);
            assert(pe_training_pause_command(&layout,r.x+r.w/2,r.y+r.h/2,true,false,false)==index+1);
            assert(pe_training_pause_command(&layout,r.x+r.w/2,r.y+r.h/2,false,false,false)==0);
            assert(pe_training_pause_command(&layout,r.x+r.w,r.y,true,false,false)==0);
        }
        assert(pe_training_pause_command(&layout,NAN,0,true,false,false)==0);
        assert(pe_training_pause_command(&layout,0,0,false,true,false)==2);
        assert(pe_training_pause_command(&layout,0,0,false,false,true)==3);
    }
    return 0;
}
"""
        parent = ROOT / "build"
        parent.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="pause-input-", dir=parent) as folder:
            temporary = Path(folder)
            source, binary = temporary / "pause.c", temporary / "pause"
            source.write_text(harness)
            environment = os.environ.copy()
            environment["TMPDIR"] = str(temporary)
            command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                       "-fsanitize=address,undefined", "-I" + str(ROOT / "src"), str(source),
                       str(ROOT / "src/pe_training_ui_layout.c"), "-lm", "-o", str(binary)]
            compiled = subprocess.run(command, capture_output=True, text=True, env=environment, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            ran = subprocess.run([str(binary)], capture_output=True, text=True, env=environment, timeout=60)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_actual_host_held_pointer_release(self):
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("C compiler unavailable")
        source = (ROOT / "src/pe_training_view.c").read_text()
        first = source.index("        unsigned pointer_buttons=0;")
        last = source.index("        bool was_captured = captured;", first)
        block = source[first:last]
        world = next(line for line in source.splitlines() if "bool world_pointer=" in line)
        dig = next(line for line in source.splitlines() if "bool dig_held=" in line)
        harness = r"""
#include "pe_ui_input.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#undef assert
#define assert(x) do{if(!(x)){fprintf(stderr,"UI input:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
#include <stdio.h>
typedef struct {float x,y;} Vector2;
enum {MOUSE_BUTTON_LEFT,MOUSE_BUTTON_RIGHT};
static unsigned held;
static bool IsMouseButtonDown(int button){return (held&(1u<<button))!=0;}
static Vector2 GetMousePosition(void){return (Vector2){0};}
static int ui;
static bool pe_training_ui_pointer_over(const int *u,Vector2 p){(void)u;(void)p;return false;}
static PeUiPointerGate pointer_gate;
static struct {int camera_mode;bool build_mode,shovel_held;} training={.shovel_held=true};
static bool focused=true,smoke_frames,paused,pause_was_active,modal,ui_was_modal;
static bool window_input_consumed,dialogue_input_consumed,captured,free_cursor;
static bool autorun,jump_pending,attack_pending;
static int talk;
static bool capture_allowed,dig_allowed;
static void frame(void){
""" + block + world + "\n" + dig + r"""
 capture_allowed=want_capture;dig_allowed=dig_held;
}
int main(void){
 for(unsigned button=1;button<=2;button++){
  pointer_gate=(PeUiPointerGate){0};held=button;ui_was_modal=true;frame();
  assert(!capture_allowed&&!dig_allowed&&pointer_gate.waiting_for_release);
  ui_was_modal=false;
  for(int i=0;i<3;i++){frame();assert(!capture_allowed&&!dig_allowed);}
  held=0;frame();assert(!capture_allowed&&!dig_allowed&&!pointer_gate.waiting_for_release);
  held=button;frame();assert(button==1?dig_allowed:capture_allowed);
 }
 puts("Production tutorial held-pointer gate blocks dig/capture through release");
}
"""
        with tempfile.TemporaryDirectory(prefix="ui-pointer-", dir=ROOT / "build") as folder:
            temporary = Path(folder)
            cfile, binary = temporary / "pointer.c", temporary / "pointer"
            cfile.write_text(harness)
            environment = os.environ.copy()
            environment["TMPDIR"] = str(temporary)
            command = [compiler,"-std=c11","-Wall","-Wextra","-Wpedantic","-Werror",
                       "-fsanitize=address,undefined","-I"+str(ROOT/"src"),str(cfile),
                       str(ROOT/"src/pe_ui_input.c"),str(ROOT/"src/pe_training_ui_layout.c"),"-lm","-o",str(binary)]
            result = subprocess.run(command,capture_output=True,text=True,env=environment,timeout=60)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result = subprocess.run([str(binary)],capture_output=True,text=True,env=environment,timeout=60)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_actual_input_routes_and_transactions(self):
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("C compiler unavailable")
        source = (ROOT / "src/pe_training_view.c").read_text()
        window_first = source.index("        unsigned visible_windows=")
        window_last = source.index("        bool cancel_window_drag=", window_first)
        first = source.index("            unsigned response=0;")
        last = source.index("                bool interact=", first)
        block = source[first:last] + "\n            }\n"
        line = next(line for line in (ROOT / "Makefile").read_text().splitlines() if line.startswith("CORE = "))
        core = [str(ROOT / path) for path in line.split(" = ", 1)[1].split()]
        parent = ROOT / "build"
        parent.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="ui-input-", dir=parent) as folder:
            temporary = Path(folder)
            harness, binary = temporary / "input.c", temporary / "input"
            escape_first = source.index("        if(focused&&!smoke_frames&&!settings_consumed&&IsKeyPressed(KEY_ESCAPE)) {")
            escape_last = source.index("        if(focused&&paused&&!smoke_frames&&!settings_consumed)", escape_first)
            harness.write_text(PREFIX + source[escape_first:escape_last] + source[window_first:window_last] + block + SUFFIX)
            environment = os.environ.copy()
            environment["TMPDIR"] = str(temporary)
            command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                       "-fsanitize=address,undefined", "-g", "-I" + str(ROOT / "src"),
                       str(harness), *core, "-llz4", "-lm", "-o", str(binary)]
            compiled = subprocess.run(command, capture_output=True, text=True, env=environment, timeout=120)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            ran = subprocess.run([str(binary)], capture_output=True, text=True, env=environment, timeout=60)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)
            self.assertIn("Actual core transactions", ran.stdout)

if __name__ == "__main__":
    unittest.main()
