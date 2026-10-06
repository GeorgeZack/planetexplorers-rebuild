#!/usr/bin/env python3
"""Extract bounded production host policy for a CPU integration test.

No substitute world/action admission is invented in the fixture. Fail closed
when host structure changes: review and update extraction with that change.
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def block(text, marker):
    start = text.index(marker)
    opening = text.index('{', start)
    depth = 0
    for end in range(opening, len(text)):
        if text[end] == '{': depth += 1
        elif text[end] == '}':
            depth -= 1
            if depth == 0: return text[start:end+1]
    raise ValueError('unbalanced host block')

def extract():
    text = (ROOT/'src/pe_sandbox_view.c').read_text()
    movement = block(text, 'if(world_input) {\n                float forward=')
    actions = block(text, 'if(action_input){\n                if(IsKeyPressed(KEY_E))')
    assert 'attack_pending=true' in actions and 'tool_drawn=true' in actions
    assert 'IsMouseButtonPressed(MOUSE_BUTTON_LEFT)' in actions
    assert 'if(action.consume_actions)attack_pending=false;' in text
    assert '.input_allowed=(!action.consume_world||cycle_from_tab)&&!action.paused&&!loaded&&!resized&&game->hp>0' in text
    assert 'bool world_input=camera_control.world_input;' in text
    assert 'bool action_input=world_input&&!action.consume_actions;' in text
    assert 'hunt.encounter.active&&!action.consume_world' in text
    sensor = block(text, 'if(hunt.encounter.active&&!action.consume_world){')
    assert 'pe_hunt_combat_sense' in sensor
    fixed = block(text, 'if(hunt.encounter.active){\n                    pe_sandbox_actor_update')
    contact = block(fixed, 'if(!action.consume_world){')
    assert 'pe_hunt_combat_step' in contact
    cancel = block(text, 'if(!world_input||build_mode||!third_person||game->hp<=0||!knife_ready){')
    return '''/* Generated from production pe_sandbox_view.c; do not edit. */
static PeInput host_motion(PeGame *game, PeSandboxUIResult action,
    PeSandboxCameraControlResult camera_control, PeSandboxMelee *blade,
    bool *drawn, bool *new_action, bool *pending) {
    bool world_input=camera_control.world_input;
    bool action_input=world_input&&!action.consume_actions;
    bool attack_pending=*pending,tool_drawn=*drawn;
    bool build_mode=false,third_person=camera_control.third_person,knife_ready=true,knife_tool=true;
    bool autorun=false,jump_pending=false,walk=false;
    PeSandboxMelee melee=*blade;
    if(action.consume_actions)attack_pending=false;
''' + cancel + '\n    PeInput input={0};\n' + movement + '''
    *blade=melee;*drawn=tool_drawn;*new_action=action_input;*pending=attack_pending;
    return input;
}
static bool host_hunt_enabled(PeHuntCombat hunt,PeSandboxUIResult action){
    return hunt.encounter.active&&!action.consume_world;
}
'''

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args()
    output=Path(a.output);output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(extract())
