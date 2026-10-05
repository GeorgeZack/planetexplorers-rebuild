#!/usr/bin/env python3
"""Read installed player clip identities for the optional isolated Unity sampler.

Requires optional UnityPy. No game launch or original-file writes.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
# Verified FemaleAnimator2 leaves: ground Move, Jump/Fall/Land and Attack6–9.
# NormalSprint is deliberately absent: two distinct clips share its name/length.
CLIPS = {'NormalIdle': 684, 'NormalWF': 531, 'NormalRF': 539, 'Jump': 263,
         'Fall': 500, 'Land': 505, 'Attack6': 231, 'Attack7': 240,
         'Attack8': 247, 'Attack9': 254,
         'Felling1': 244, 'Felling2': 252, 'Felling3': 259,
         'Spade1': 189, 'Spade2': 201, 'Spade3': 211,
         'Mine1': 221, 'Mine2': 228, 'Mine3': 237}


def direct_states(controller, prefix, selected):
    """Verify direct controller leaves, retaining transition evidence separately."""
    names=dict(controller['m_TOS']);result={}
    for machine in controller['m_Controller']['m_StateMachineArray']:
        for entry in machine['data']['m_StateConstantArray']:
            state=entry['data'];name=names.get(state['m_NameID'])
            if name not in selected:continue
            path=names[state['m_FullPathID']]
            trees=state['m_BlendTreeConstantArray']
            if name in result or path!=prefix+name or len(trees)!=1:
                raise ValueError('Unexpected direct controller state')
            nodes=trees[0]['data']['m_NodeArray']
            if len(nodes)!=1 or nodes[0]['data']['m_ChildIndices']:
                raise ValueError('State is not a direct clip leaf')
            index=nodes[0]['data']['m_ClipID'];reference=controller['m_AnimationClips'][index]
            if reference!={'m_FileID':0,'m_PathID':CLIPS[name]}:
                raise ValueError('State references a different source clip')
            result[name]={'path':path,'clip_index':index,'speed':state['m_Speed'],
                          'transitions':[t['data'] for t in state['m_TransitionConstantArray']]}
    if len(result)!=len(selected):raise ValueError('Missing verified direct states')
    return result


def felling_states(controller):
    return direct_states(controller,'FullBody.Fell.',('Felling1','Felling2','Felling3'))


def spade_states(controller):
    return direct_states(controller,'FullBody.Dig.',('Spade1','Spade2','Spade3'))


def mine_states(controller):
    return direct_states(controller,'FullBody.Dig.',('Mine1','Mine2','Mine3'))


def inventory(asset):
    import UnityPy
    env = UnityPy.load(str(asset))
    serialized = next(iter(env.files.values()))
    controller = serialized.objects[779].read_typetree()
    override = serialized.objects[780].read_typetree()
    if controller['m_Name'] != 'FemaleAnimator2' or override['m_Name'] != 'MaleAnimator':
        raise ValueError('Unsupported installed controller identities')
    if override['m_Controller'] != {'m_FileID': 0, 'm_PathID': 779}:
        raise ValueError('Unexpected male controller reference')
    refs = {x['m_PathID'] for x in controller['m_AnimationClips'] if x['m_FileID'] == 0}
    replacements = {x['m_OriginalClip']['m_PathID']: x['m_OverrideClip']['m_PathID']
                    for x in override['m_Clips']}
    states={**felling_states(controller),**spade_states(controller),**mine_states(controller)}
    with asset.open('rb') as stream:
        asset_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
    result = {'schema': 1, 'asset': str(asset), 'asset_sha256': asset_hash,
              'unity_version': serialized.unity_version, 'genders': {}}
    for sex, avatar, name in [('female', 'FemaleAvatar', 'FemaleAnimator2'), ('male', 'MaleAvatar', 'MaleAnimator')]:
        clips = []
        runtime_ids={replacements.get(i,i) if sex=='male' else i for i in refs}
        runtime_clips={i:serialized.objects[i].read_typetree() for i in runtime_ids}
        for clip_name, female_id in CLIPS.items():
            if female_id not in refs:
                raise ValueError('Expected clip absent from controller')
            path_id = replacements.get(female_id, female_id) if sex == 'male' else female_id
            data = serialized.objects[path_id].read_typetree()
            if data['m_Name'] != clip_name:
                raise ValueError('Unexpected clip name')
            muscle = data['m_MuscleClip']
            humanoid = sum(b['classID'] == 95 and b['customType'] == 8 for b in data['m_ClipBindingConstant']['genericBindings'])
            if not humanoid:
                raise ValueError('Selected source clip has no humanoid bindings')
            length=muscle['m_StopTime']-muscle['m_StartTime']
            matches=[i for i,d in runtime_clips.items() if d['m_Name']==clip_name and
                     abs((d['m_MuscleClip']['m_StopTime']-d['m_MuscleClip']['m_StartTime'])-length)<=.0001 and
                     abs(d['m_SampleRate']-data['m_SampleRate'])<=.001]
            if matches!=[path_id]:
                raise ValueError('Clip identity is ambiguous in runtime controller: '+clip_name)
            clips.append({'name': clip_name, 'path_id': path_id, 'length': muscle['m_StopTime'] - muscle['m_StartTime'],
                          'frame_rate': data['m_SampleRate'], 'loop': muscle['m_LoopTime'], 'humanoid_bindings': humanoid,
                          'events': [{'time': e['time'], 'function': e['functionName'], 'data': e['data']}
                                     for e in data['m_Events']]})
            if clip_name in states:clips[-1]['controller_state']=states[clip_name]
        result['genders'][sex] = {'avatar': avatar, 'controller': name, 'clips': clips}
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir', type=Path, default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--output', type=Path, default=ROOT/'rebuild/build/animator-bake/clip-plan.json')
    a = p.parse_args()
    game = a.game_dir.expanduser().resolve(); output = a.output.expanduser().resolve()
    if output == game or game in output.parents:
        p.error('Output must not modify installed game')
    try:
        report = inventory(game/'PE_Client_Data/sharedassets1.assets')
        output.parent.mkdir(parents=True, exist_ok=True)
        temp = output.with_suffix('.json.tmp')
        temp.write_text(json.dumps(report, indent=2)+'\n')
        temp.replace(output)
    except (OSError, ValueError, ImportError, KeyError) as e:
        p.error(str(e))
    print(output)


if __name__ == '__main__':
    main()
