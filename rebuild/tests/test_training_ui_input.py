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
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef struct {float x,y;} Vector2;
enum {MOUSE_BUTTON_LEFT,MOUSE_BUTTON_RIGHT,KEY_C,KEY_Y,KEY_I,KEY_Q,KEY_B,KEY_T,KEY_ONE};
static bool keys[16],left,right,down,released;
static bool IsKeyPressed(int k){return keys[k];}
static bool IsMouseButtonPressed(int k){return k==MOUSE_BUTTON_LEFT?left:right;}
static bool IsMouseButtonDown(int k){(void)k;return down;}
static bool IsMouseButtonReleased(int k){(void)k;return released;}
static Vector2 GetMouseDelta(void){return(Vector2){5,0};}
static PeTraining training;
static struct{PeTrainingUILayout layout;}ui;
static Vector2 pointer;
static Vector2 GetMousePosition(void){return pointer;}
static bool menu_open,paused,inventory,log,dragging,equipment_tab;
static int inventory_tab,build_rotation;
static float preview_yaw;
static void frame(void){
 bool ui_was_modal=training.character_open||training.replicator_open||training.kit_open||inventory||log||menu_open;
"""
SUFFIX = r"""}

static void events(void){memset(keys,0,sizeof keys);left=right=down=released=false;}
static void reset(int mission,unsigned dialogue){
 memset(&training,0,sizeof training);events();
 training.ready=true;training.hp=training.max_hp=500;training.stamina=100;
 training.tutorial=(PeTutorial){(unsigned)mission,3,15,0};training.dialogue_index=dialogue;
 training.equipped_shovel=-1;
 menu_open=paused=inventory=log=dragging=equipment_tab=false;
 inventory_tab=build_rotation=0;preview_yaw=0;pointer=(Vector2){0};
 assert(pe_training_ui_layout(1280,720,&ui.layout));assert(pe_tutorial_valid(&training.tutorial));
}
int main(void){
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
#include <math.h>
int main(void) {
    PeTrainingUILayout layout;
    const float dimensions[][2]={{1280,720},{1920,1080},{1000,800}};
    for(unsigned d=0;d<3;d++) {
        assert(pe_training_ui_layout(dimensions[d][0],dimensions[d][1],&layout));
        for(int index=0;index<3;index++) {
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

    def test_actual_input_routes_and_transactions(self):
        compiler = shutil.which("cc")
        if not compiler:
            self.skipTest("C compiler unavailable")
        source = (ROOT / "src/pe_training_view.c").read_text()
        window_first = source.index("        unsigned visible_windows=")
        window_last = source.index("        bool cancel_window_drag=", window_first)
        first = source.index("                const bool menu_was_open=")
        last = source.index("                bool interact=", first)
        block = source[first:last]
        line = next(line for line in (ROOT / "Makefile").read_text().splitlines() if line.startswith("CORE = "))
        core = [str(ROOT / path) for path in line.split(" = ", 1)[1].split()]
        parent = ROOT / "build"
        parent.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="ui-input-", dir=parent) as folder:
            temporary = Path(folder)
            harness, binary = temporary / "input.c", temporary / "input"
            harness.write_text(PREFIX + source[window_first:window_last] + block + SUFFIX)
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
