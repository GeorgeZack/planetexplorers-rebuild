#!/usr/bin/env python3
"""Offline source-bound shield layer capture plan; emits no poses or launch commands.

This is deliberately separate from the known-working controller-free clip bake.
The plan does not authorize attaching the gameplay controller to a clone.
"""
import argparse
import hashlib
import json
from pathlib import Path

from inventory_reference_bake import sword_states

ROOT = Path(__file__).resolve().parents[2]
CLIPS = {90: 'AttackIdle', 133: 'SwordWF', 435: 'SwordRF',
         413: 'SwordSheildBlock', 420: 'NormalSheildBlock',
         270: 'SwordPutOn', 275: 'SwordPutOff'}
MASK = {'word0': 0, 'word1': 41947128, 'word2': 1}


def equal(value, wanted, label):
    if value != wanted:
        raise ValueError(label + ' source contract changed')


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def contract(controller):
    equal(controller['m_Name'], 'FemaleAnimator2', 'Controller')
    names = dict(controller['m_TOS'])
    layers = controller['m_Controller']['m_LayerArray']
    layer = layers[3]['data']
    equal(names[layer['m_Binding']], 'LArmOnly', 'Layer name/index')
    equal(layer['m_StateMachineIndex'], 3, 'Layer machine')
    equal(layer['m_BodyMask'], MASK, 'Humanoid mask')
    equal(layer['m_SkeletonMask']['data']['m_Data'], [], 'Transform mask')
    equal(layer['(int&)m_LayerBlendingMode'], 0, 'Override blend mode')
    equal(layer['m_DefaultWeight'], 1., 'Layer weight')
    equal(layer['m_IKPass'], False, 'Layer IK')
    equal(layer['m_SyncedLayerAffectsTiming'], False, 'Layer synchronization')
    machines = controller['m_Controller']['m_StateMachineArray']
    states = {names[x['data']['m_FullPathID']]: x['data']
              for m in machines for x in m['data']['m_StateConstantArray']}
    block = states['LArmOnly.SheildBlock']
    equal(block['m_Speed'], 1., 'Block speed')
    equal(block['m_IKOnFeet'], False, 'Block IK')
    nodes = block['m_BlendTreeConstantArray'][0]['data']['m_NodeArray']
    equal(len(nodes), 3, 'Block blend shape')
    root = nodes[0]['data']
    equal(names[root['m_BlendEventID']], 'MoveStyle', 'Block blend parameter')
    equal(root['m_BlendType'], 0, 'Block blend type')
    equal(root['m_ChildIndices'], [1, 2], 'Block leaves')
    equal(root['m_Blend1dData']['data']['m_ChildThresholdArray'], [0., 1.], 'Block style thresholds')
    for node, identity in zip(nodes[1:], (420, 413)):
        equal(node['data']['m_ChildIndices'], [], 'Block leaf')
        equal(controller['m_AnimationClips'][node['data']['m_ClipID']],
              {'m_FileID': 0, 'm_PathID': identity}, 'Block clip identity')
    entry = states['LArmOnly.DoNothing']['m_TransitionConstantArray'][0]['data']
    exit_transition, = block['m_TransitionConstantArray']
    exit_transition = exit_transition['data']
    for transition, mode, destination, duration in ((entry, 1, 1, .09999996423721313),
                                                  (exit_transition, 2, 0, .10209385305643082)):
        condition, = transition['m_ConditionConstantArray']
        equal(names[condition['data']['m_EventID']], 'SheildBlock', 'Block condition')
        equal(condition['data']['m_ConditionMode'], mode, 'Block condition mode')
        equal(transition['m_DestinationState'], destination, 'Block transition destination')
        equal(transition['m_HasFixedDuration'], False, 'Normalized transition duration')
        equal(transition['m_HasExitTime'], False, 'Conditional transition')
        equal(transition['m_TransitionDuration'], duration, 'Block transition duration')
    params = {names[v['m_ID']]: v for v in controller['m_Controller']['m_Values']['data']['m_ValueArray']}
    equal(params['SheildBlock']['m_Type'], 4, 'Shield boolean parameter')
    equal(params['MoveStyle']['m_Type'], 1, 'MoveStyle float parameter')
    related = {}
    for name in ('LArmOnly.SheildBlock', 'UpbodyOnly.SwordPutOn', 'UpbodyOnly.SwordPutOff'):
        state = states[name]
        ranges = controller['m_StateMachineBehaviourVectorDescription']
        references = []
        for key, span in ranges['m_StateMachineBehaviourRanges']:
            if key['m_StateID'] == state['m_FullPathID']:
                for index in ranges['m_StateMachineBehaviourIndices'][span['m_StartIndex']:span['m_StartIndex']+span['m_Count']]:
                    references.append(controller['m_StateMachineBehaviours'][index])
        related[name] = {'state': state, 'behaviour_references': references}
        if name.startswith('UpbodyOnly.'):
            equal(state['m_Speed'], 1.2000000476837158, 'Upper-body draw/stow speed')
            draw_nodes = state['m_BlendTreeConstantArray'][0]['data']['m_NodeArray']
            equal(names[draw_nodes[0]['data']['m_BlendEventID']], 'InWater', 'Draw/stow blend parameter')
            equal(draw_nodes[0]['data']['m_Blend1dData']['data']['m_ChildThresholdArray'], [0., 1.], 'Draw/stow thresholds')
            equal(draw_nodes[0]['data']['m_ChildIndices'], [1, 2], 'Draw/stow leaves')
            equal(controller['m_AnimationClips'][draw_nodes[1]['data']['m_ClipID']],
                  {'m_FileID': 0, 'm_PathID': 270 if name.endswith('PutOn') else 275}, 'Ground draw/stow identity')
    return {'layer_index': 3, 'layer': layer, 'entry_transition': entry,
            'exit_transition': exit_transition, 'states': related,
            'parameters': {key: params[key] for key in ('SheildBlock', 'MoveStyle', 'MoveStyleH', 'MoveStyleV', 'InWater')},
            'controller_behaviour_count': len({(r['m_FileID'], r['m_PathID']) for r in controller['m_StateMachineBehaviours']})}


def inventory(asset):
    import UnityPy
    serialized = next(iter(UnityPy.load(str(asset)).files.values()))
    controller = serialized.objects[779].read_typetree()
    result = contract(controller)
    locomotion = sword_states(controller)
    clips = []
    for identity, name in CLIPS.items():
        c = serialized.objects[identity].read_typetree()
        equal(c['m_Name'], name, 'Clip name')
        m = c['m_MuscleClip']
        bindings = sum(b['classID'] == 95 and b['customType'] == 8 for b in c['m_ClipBindingConstant']['genericBindings'])
        if not bindings:
            raise ValueError('Clip has no humanoid bindings')
        clips.append({'path_id': identity, 'name': name, 'duration': m['m_StopTime']-m['m_StartTime'],
                      'sample_rate': c['m_SampleRate'], 'loop': m['m_LoopTime'], 'humanoid_bindings': bindings,
                      'events': c['m_Events'], 'locomotion_branch': locomotion.get(name)})
    result.update(schema='pe-shield-layer-plan-1', asset=str(asset), asset_sha256=sha(asset),
                  unity_version=serialized.unity_version, controller_path_id=779, clips=clips,
                  pure_clone_capture_ready=False, runtime_pose_export_ready=False,
                  capture_policy='Read-only live simultaneous Animator observation until masked clone isolation is qualified',
                  source_files=[{'path': p, 'sha256': sha(ROOT/p)} for p in (
                      'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sheild.cs',
                      'Assets/Scripts/Items/PEEquipment/PESheild.cs')])
    return result


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir', type=Path, default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--output', type=Path, default=ROOT/'rebuild/build/shield-layer/plan.json')
    a = p.parse_args(argv)
    game, output = a.game_dir.expanduser().resolve(), a.output.expanduser().resolve()
    if output == game or game in output.parents:
        p.error('Output must not modify installed game')
    try:
        result = inventory(game/'PE_Client_Data/sharedassets1.assets')
        output.parent.mkdir(parents=True, exist_ok=True)
        # This preparatory plan is never installed or treated as an animation bake.
        output.write_text(json.dumps(result, indent=2)+'\n')
    except (OSError, ValueError, KeyError, ImportError, IndexError) as error:
        p.error(str(error))
    print(output)


if __name__ == '__main__':
    main()
