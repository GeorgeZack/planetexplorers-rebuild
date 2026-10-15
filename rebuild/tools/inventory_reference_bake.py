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
# Sword style's idle leaf is AttackIdle, not a nonexistent SwordIdle clip.
# Ten authored clips fit the unchanged isolated sampler retention bound.
SWORD_CLIPS = {'AttackIdle': 90, 'SwordWF': 133, 'SwordRF': 435, 'SwordSprint': 113,
               'SwordJump': 756, 'SwordFall': 757, 'SwordLand': 758,
               'SwordRunJump': 272, 'SwordRunFall': 284, 'SwordRunLand': 279}
CLIPS.update(SWORD_CLIPS)
# Separate backward pair: never aliases NormalWB/RB or expands torch-sword.
SWORD_BACKWARD_CLIPS = {'SwordWB': 441, 'SwordRB': 708}
CLIPS.update(SWORD_BACKWARD_CLIPS)
# Ground Move Normal branch: cardinal strafe/back leaves plus authored third-
# person turn alternatives. The unusual WR parent really uses RightSpeed.
DIRECTIONAL_CLIPS = {'NormalWB': 547, 'NormalWL': 517, 'NormalWR': 524,
                     'NormalRB': 696, 'NormalRL': 447, 'NormalRR': 510,
                     'NormalWalkTurnLeft': 751, 'NormalWalkTurnRight': 752,
                     'NormalRunTurnLeft': 753, 'NormalRunTurnRight': 754}
CLIPS.update(DIRECTIONAL_CLIPS)
# Bounded running first-stage pose; root motion is sampled separately.
CLIPS['RunSwordAttack'] = 408
# Hit-reaction leaves, kept apart from every locomotion preset. Whacked plays
# FullBody.BeHit, a MoveStyle blend: thresholds 0 and 1 are the unarmed and
# Sword (knife) clips. Repulsed plays the BeatWeight 1 branch of ground Move;
# its two straight-back points blend to BlockRepulsed by SheildBlockF.
# Each value is (clip, state path, (parameter, parameter_y, child value)...).
_MOVE='Base Layer.GroundLocomotion.Move'
def _beat(clip,point,shield=None):
    steps=[('BeatWeight',None,1),('RightSpeed','ForwardSpeed',point)]
    return (clip,_MOVE,tuple(steps+([('SheildBlockF',None,shield)] if shield is not None else [])))
REACTION_BRANCHES = {
    'IdleBeHit': (139,'FullBody.BeHit.Blend Tree',(('MoveStyle',None,0),)),
    'SwordBeHit': (157,'FullBody.BeHit.Blend Tree',(('MoveStyle',None,1),)),
    'ForwardFast': _beat(226,(0,5)), 'FowardRightFast': _beat(235,(3.5,3.5)),
    'RightFast': _beat(243,(5,0)), 'BackRightFast': _beat(250,(3.5,-3.5)),
    'BackFast': _beat(257,(0,-5),0), 'ForwardLeftFast': _beat(265,(-3.5,3.5)),
    'LeftFast': _beat(271,(-5,0)), 'BackLeftFast': _beat(278,(-3.5,-3.5)),
    'Forward': _beat(109,(0,3)), 'ForwardRight': _beat(154,(2.1,2.1)),
    'Right': _beat(136,(3,0)), 'BackRight': _beat(170,(2.1,-2.1)),
    'Back': _beat(186,(0,-3),0), 'ForwardLeft': _beat(198,(-2.1,2.1)),
    'Left': _beat(209,(-3,0)), 'BackLeft': _beat(218,(-2.1,-2.1)),
    'BlockRepulsed': _beat(691,(0,-5),1),
}
REACTION_CLIPS = {name: row[0] for name,row in REACTION_BRANCHES.items()}
CLIPS.update(REACTION_CLIPS)
# Ground get-up leaves entered from the ragdoll, and the sprint blade attack.
# The controller crosses names: state GetupBack plays clip GetUpBackMirror and
# state GetupBackMirror plays clip GetUpBack; likewise for Front.
GETUP_STATES = {'GetUpBackMirror': (322,'Base Layer.Ragdoll.GetupBack','GetUpBack'),
                'GetUpFrontMirror': (326,'Base Layer.Ragdoll.GetupFront','GetUpFront'),
                'GetUpBack': (311,'Base Layer.Ragdoll.GetupBackMirror','GetUpBackMirror'),
                'GetUpFront': (317,'Base Layer.Ragdoll.GetupFrontMirror','GetUpFrontMirror')}
RECOVERY_CLIPS = {name: row[0] for name,row in GETUP_STATES.items()}
RECOVERY_CLIPS['SprintSwordAttack'] = 401
CLIPS.update(RECOVERY_CLIPS)
# Female FullBody.Gather leaves (controller states 22-24). Leaf poses only:
# the Herb/HerbUp state-exit events and transition timing are not sampled.
GATHER_CLIPS = {'HerbDown': 267, 'Herb': 273, 'HerbUp': 280}
CLIPS.update(GATHER_CLIPS)
DIRECTIONAL_BRANCHES = {
    'NormalWB': ((0,1,2,33), (0,-2), None),
    'NormalWL': ((0,1,2,34,36), (-2,0), ('FirstPerson',1)),
    'NormalWR': ((0,1,2,30,32), (2,0), ('RightSpeed',1)),
    'NormalRB': ((0,1,2,16), (0,-5), None),
    'NormalRL': ((0,1,2,20,22), (-5,0), ('FirstPerson',1)),
    'NormalRR': ((0,1,2,10,12), (5,0), ('FirstPerson',1)),
    'NormalWalkTurnLeft': ((0,1,2,34,35), (-2,0), ('FirstPerson',0)),
    'NormalWalkTurnRight': ((0,1,2,30,31), (2,0), ('RightSpeed',0)),
    'NormalRunTurnLeft': ((0,1,2,20,21), (-5,0), ('FirstPerson',0)),
    'NormalRunTurnRight': ((0,1,2,10,11), (5,0), ('FirstPerson',0)),
}


def _directional_states(controller,selected,clip_ids,style):
    """Require exact installed branch ancestry, parameters, points and IDs."""
    names=dict(controller['m_TOS']);path='Base Layer.GroundLocomotion.Move'
    states=[e['data'] for m in controller['m_Controller']['m_StateMachineArray']
            for e in m['data']['m_StateConstantArray'] if names.get(e['data']['m_FullPathID'])==path]
    if len(states)!=1 or len(states[0]['m_BlendTreeConstantArray'])!=1:
        raise ValueError('Missing/duplicate directional Move tree')
    state=states[0];nodes=state['m_BlendTreeConstantArray'][0]['data']['m_NodeArray'];result={}
    for name,(indices,point,variant) in selected.items():
        expected=[('BeatWeight',None,0),('MoveStyleH','MoveStyleV',style),
                  ('RightSpeed','ForwardSpeed',point)]
        if variant:expected.append((variant[0],None,variant[1]))
        branches=[]
        for (parent,child),(parameter,parameter_y,value) in zip(zip(indices,indices[1:]),expected):
            node=nodes[parent]['data'];children=node['m_ChildIndices']
            if children.count(child)!=1:raise ValueError('Directional ancestry differs')
            slot=children.index(child)
            if names.get(node['m_BlendEventID'])!=parameter or names.get(node['m_BlendEventYID'])!=parameter_y:
                raise ValueError('Directional branch parameter differs')
            branch=dict(node=parent,child=child,parameter=parameter,parameter_y=parameter_y,blend_type=node['m_BlendType'])
            if isinstance(value,tuple):
                actual=node['m_Blend2dData']['data']['m_ChildPositionArray'][slot]
                if any(abs(actual[k]-v)>1e-5 for k,v in zip(('x','y'),value)):
                    raise ValueError('Directional branch point differs')
                branch['point']=actual
            else:
                actual=node['m_Blend1dData']['data']['m_ChildThresholdArray'][slot]
                if abs(actual-value)>1e-5:raise ValueError('Directional branch threshold differs')
                branch['threshold']=actual
            branches.append(branch)
        leaf=nodes[indices[-1]]['data'];index=leaf['m_ClipID']
        if leaf['m_ChildIndices'] or controller['m_AnimationClips'][index]!={'m_FileID':0,'m_PathID':clip_ids[name]}:
            raise ValueError('Directional leaf differs from exact source clip')
        result[name]=dict(path=path,clip_index=index,speed=state['m_Speed'],node_path=list(indices),branches=branches)
    return result


def reaction_states(controller):
    """Require each reaction leaf at its exact installed parameters, values and clip."""
    names=dict(controller['m_TOS']);states={}
    for machine in controller['m_Controller']['m_StateMachineArray']:
        for entry in machine['data']['m_StateConstantArray']:
            path=names.get(entry['data']['m_FullPathID'])
            if path in states:raise ValueError('Duplicate source state path')
            states[path]=entry['data']
    result={}
    for name,(clip,path,steps) in REACTION_BRANCHES.items():
        state=states.get(path)
        if not state or len(state['m_BlendTreeConstantArray'])!=1 or state['m_Speed']!=1:
            raise ValueError('Reaction state differs')
        nodes=state['m_BlendTreeConstantArray'][0]['data']['m_NodeArray'];index=0;node_path=[0];branches=[]
        for parameter,parameter_y,value in steps:
            node=nodes[index]['data']
            if names.get(node['m_BlendEventID'])!=parameter or (parameter_y and names.get(node['m_BlendEventYID'])!=parameter_y):
                raise ValueError('Reaction branch parameter differs')
            if isinstance(value,tuple):
                slots=[i for i,p in enumerate(node['m_Blend2dData']['data']['m_ChildPositionArray'])
                       if abs(p['x']-value[0])<=1e-5 and abs(p['y']-value[1])<=1e-5]
            else:
                slots=[i for i,t in enumerate(node['m_Blend1dData']['data']['m_ChildThresholdArray']) if abs(t-value)<=1e-5]
            if len(slots)!=1 or len(node['m_ChildIndices'])<=slots[0]:raise ValueError('Reaction branch value differs')
            child=node['m_ChildIndices'][slots[0]]
            branches.append(dict(node=index,child=child,parameter=parameter,parameter_y=parameter_y,
                                 blend_type=node['m_BlendType'],value=list(value) if isinstance(value,tuple) else value))
            index=child;node_path.append(child)
        leaf=nodes[index]['data'];clip_index=leaf['m_ClipID']
        if leaf['m_ChildIndices'] or clip_index>=len(controller['m_AnimationClips']) or \
                controller['m_AnimationClips'][clip_index]!={'m_FileID':0,'m_PathID':clip}:
            raise ValueError('Reaction leaf differs from exact source clip')
        result[name]=dict(path=path,clip_index=clip_index,speed=state['m_Speed'],node_path=node_path,branches=branches)
    return result


def getup_states(controller):
    """Each ground get-up state is a direct leaf entered by its own Any State trigger."""
    names=dict(controller['m_TOS']);result={}
    for machine in controller['m_Controller']['m_StateMachineArray']:
        states=[e['data'] for e in machine['data']['m_StateConstantArray']]
        for name,(clip,path,trigger) in GETUP_STATES.items():
            found=[i for i,s in enumerate(states) if names.get(s['m_FullPathID'])==path]
            if not found:continue
            if name in result or len(found)!=1:raise ValueError('Duplicate get-up state')
            state=states[found[0]];trees=state['m_BlendTreeConstantArray']
            if len(trees)!=1 or len(trees[0]['data']['m_NodeArray'])!=1 or state['m_Speed']!=1 or state['m_Mirror']:
                raise ValueError('Get-up state is not a direct speed-1 leaf')
            leaf=trees[0]['data']['m_NodeArray'][0]['data'];index=leaf['m_ClipID']
            if leaf['m_ChildIndices'] or index>=len(controller['m_AnimationClips']) or \
                    controller['m_AnimationClips'][index]!={'m_FileID':0,'m_PathID':clip}:
                raise ValueError('Get-up state references a different source clip')
            entries=[[names.get(c['data']['m_EventID']) for c in t['data']['m_ConditionConstantArray']]
                     for t in machine['data']['m_AnyStateTransitionConstantArray'] if t['data']['m_DestinationState']==found[0]]
            if len(entries)!=1 or trigger not in entries[0]:raise ValueError('Get-up entry trigger differs')
            result[name]=dict(path=path,clip_index=index,speed=state['m_Speed'],entry_conditions=entries[0])
    if len(result)!=len(GETUP_STATES):raise ValueError('Missing verified get-up states')
    return result


def sprint_attack_states(controller):
    return direct_states(controller,'FullBody.Attack.',('SprintSwordAttack',))


def directional_states(controller):
    return _directional_states(controller,DIRECTIONAL_BRANCHES,DIRECTIONAL_CLIPS,(0,0))


def sword_backward_states(controller):
    """Exact Sword backward branches, not Normal backward or turn substitutes."""
    branches={'SwordWB':((0,1,41,68),(0,-2),None),
              'SwordRB':((0,1,41,53),(0,-5),None)}
    result=_directional_states(controller,branches,SWORD_BACKWARD_CLIPS,(.94,.342))
    if any(value['speed']!=1 for value in result.values()):
        raise ValueError('Sword backward source state speed differs')
    return result



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


def gather_states(controller):
    result=direct_states(controller,'FullBody.Gather.',tuple(GATHER_CLIPS))
    if any(value['speed']!=1 for value in result.values()):
        raise ValueError('Gather source state speed differs')
    return result


def gather_controller(controller):
    """Bind the FullBody Gather entry, leaf chain and exit to exact state hashes.

    Only serialized controller values are recorded. How Unity schedules them
    (motionless-state length, frame quantization) is left to the replay.
    """
    names=dict(controller['m_TOS']);layers=controller['m_Controller']['m_LayerArray']
    found=[i for i,l in enumerate(layers) if names.get(l['data']['m_Binding'])=='FullBody']
    if len(found)!=1:raise ValueError('Missing/duplicate FullBody layer')
    layer=found[0];machine=controller['m_Controller']['m_StateMachineArray'][layers[layer]['data']['m_StateMachineIndex']]['data']
    states=[e['data'] for e in machine['m_StateConstantArray']]
    index={names.get(s['m_FullPathID']):i for i,s in enumerate(states)}
    chain=('FullBody.DoNothing',)+tuple('FullBody.Gather.'+n for n in GATHER_CLIPS)
    if len(index)!=len(states) or any(p not in index for p in chain):raise ValueError('Gather state graph differs')
    selectors=[s['data'] for s in machine['m_SelectorStateConstantArray']]
    def selector(value,entry):
        if value<30000 or value-30000>=len(selectors):raise ValueError('Expected a Gather selector')
        s=selectors[value-30000];routes=s['m_TransitionConstantArray']
        if bool(s['m_IsEntry'])!=entry or len(routes)!=1 or routes[0]['data']['m_ConditionConstantArray']:
            raise ValueError('Gather selector differs')
        return s,routes[0]['data']['m_Destination']
    def timing(t):
        if t['m_HasFixedDuration'] or t['m_TransitionOffset']!=0:raise ValueError('Unsupported Gather transition timing')
        return dict(exit_time=t['m_ExitTime'],has_exit_time=bool(t['m_HasExitTime']),duration=t['m_TransitionDuration'],fixed_duration=False,offset=0.0)
    idle=states[index[chain[0]]]
    if idle['m_BlendTreeConstantArray']:raise ValueError('FullBody idle is not a motionless state')
    entries=[t['data'] for t in idle['m_TransitionConstantArray']
             if any(names.get(c['data']['m_EventID'])=='Gather' for c in t['data']['m_ConditionConstantArray'])]
    if len(entries)!=1:raise ValueError('Gather entry transition differs')
    entry=entries[0];conditions=entry['m_ConditionConstantArray']
    if len(conditions)!=1 or conditions[0]['data']['m_ConditionMode']!=1 or entry['m_HasExitTime']:
        raise ValueError('Gather entry condition differs')
    entry_selector,first=selector(entry['m_DestinationState'],True)
    if first!=index[chain[1]]:raise ValueError('Gather entry destination differs')
    unsigned=lambda v:v&0xffffffff
    def identity(path):
        # Speed, cycle offset and their parameter bindings all move the state clock.
        s=states[index[path]]
        if s['m_Speed']!=1 or s['m_CycleOffset']!=0 or s['m_SpeedParamID'] or s['m_CycleOffsetParamID'] or s['m_MirrorParamID']:
            raise ValueError('Gather state clock configuration differs')
        return dict(path=path,name_hash=unsigned(s['m_NameID']),full_path_hash=unsigned(s['m_FullPathID']),speed=s['m_Speed'],
                    cycle_offset=s['m_CycleOffset'],speed_parameter=None,cycle_offset_parameter=None,mirror=bool(s['m_Mirror']),
                    motion=bool(s['m_BlendTreeConstantArray']))
    result=dict(layer_index=layer,layer='FullBody',idle=identity(chain[0]),
                entry=dict(trigger='Gather',selector_full_path_hash=unsigned(entry_selector['m_FullPathID']),**timing(entry)),
                states=[identity(p) for p in chain[1:]],transitions=[])
    for source,target in zip(chain[1:],chain[2:]+(None,)):
        moves=states[index[source]]['m_TransitionConstantArray']
        if len(moves)!=1 or moves[0]['data']['m_ConditionConstantArray'] or not moves[0]['data']['m_HasExitTime']:
            raise ValueError('Gather leaf transition differs')
        move=moves[0]['data'];destination=move['m_DestinationState']
        if target is None:
            exit_selector,destination=selector(destination,False)
            if exit_selector['m_FullPathID']!=entry_selector['m_FullPathID']:raise ValueError('Gather exit selector differs')
            target=chain[0]
        if destination!=index[target]:raise ValueError('Gather leaf destination differs')
        result['transitions'].append(dict(source=source,destination=target,**timing(move)))
    return result


def felling_states(controller):
    return direct_states(controller,'FullBody.Fell.',('Felling1','Felling2','Felling3'))


def spade_states(controller):
    return direct_states(controller,'FullBody.Dig.',('Spade1','Spade2','Spade3'))


def running_attack_states(controller):
    return direct_states(controller,'FullBody.Attack.',('RunSwordAttack',))


def mine_states(controller):
    return direct_states(controller,'FullBody.Dig.',('Mine1','Mine2','Mine3'))


def sword_states(controller):
    """Pin source Sword blend leaves, retaining their branch coordinates.

    Motion_Move_Human maps style1 to cos/sin20 degrees, selecting the authored
    (.94,.342) MoveStyleH/V point. This records leaves, not a blend-graph port.
    """
    names=dict(controller['m_TOS']);states={}
    for machine in controller['m_Controller']['m_StateMachineArray']:
        for entry in machine['data']['m_StateConstantArray']:
            state=entry['data'];path=names.get(state['m_FullPathID'])
            if path in states:raise ValueError('Duplicate source state path')
            states[path]=state
    move='Base Layer.GroundLocomotion.Move'
    paths={'AttackIdle':(move,(0,1,41,42)), 'SwordWF':(move,(0,1,41,61,63)),
           'SwordRF':(move,(0,1,41,43,45)), 'SwordSprint':(move,(0,1,41,74))}
    for phase in ('Jump','Fall','Land'):
        path='Base Layer.GroundLocomotion.Jump.'+phase
        paths['Sword'+phase]=(path,(0,1,3))
        paths['SwordRun'+phase]=(path,(0,17,19))
    result={}
    for name,(path,indices) in paths.items():
        state=states[path];trees=state['m_BlendTreeConstantArray']
        if len(trees)!=1:raise ValueError('Sword state blend-tree count differs')
        nodes=trees[0]['data']['m_NodeArray'];branches=[];sword_branch=False
        for parent,child in zip(indices,indices[1:]):
            node=nodes[parent]['data'];children=node['m_ChildIndices']
            if children.count(child)!=1:raise ValueError('Sword blend ancestry differs')
            slot=children.index(child);thresholds=node['m_Blend1dData']['data']['m_ChildThresholdArray']
            points=node['m_Blend2dData']['data']['m_ChildPositionArray']
            parameter=names.get(node['m_BlendEventID']);parameter_y=names.get(node['m_BlendEventYID'])
            branch=dict(node=parent,child=child,blend_type=node['m_BlendType'],parameter=parameter,parameter_y=parameter_y)
            if thresholds:branch['threshold']=thresholds[slot]
            if points:branch['point']=points[slot]
            if parameter=='MoveStyleH':
                point=points[slot]
                if parameter_y!='MoveStyleV' or abs(point['x']-.94)>1e-5 or abs(point['y']-.342)>1e-5:
                    raise ValueError('Sword style branch coordinates differ')
                sword_branch=True
            branches.append(branch)
        leaf=nodes[indices[-1]]['data'];index=leaf['m_ClipID']
        if not sword_branch or leaf['m_ChildIndices'] or controller['m_AnimationClips'][index]!={'m_FileID':0,'m_PathID':SWORD_CLIPS[name]}:
            raise ValueError('Sword blend leaf differs from exact source clip')
        result[name]=dict(path=path,clip_index=index,speed=state['m_Speed'],node_path=list(indices),branches=branches)
    return result


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
    states={**felling_states(controller),**spade_states(controller),**mine_states(controller),**sword_states(controller),**sword_backward_states(controller),**directional_states(controller),**running_attack_states(controller),**reaction_states(controller),**getup_states(controller),**sprint_attack_states(controller),**gather_states(controller)}
    with asset.open('rb') as stream:
        asset_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
    result = {'schema': 1, 'asset': str(asset), 'asset_sha256': asset_hash,
              'unity_version': serialized.unity_version, 'gather_controller': gather_controller(controller), 'genders': {}}
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
            if clip_name in REACTION_CLIPS:clips[-1]['clip_mirror']=bool(muscle['m_Mirror'])
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
