#!/usr/bin/env python3
"""Recover the player's Whacked/Repulsed animation contract without starting Unity.

Reads the installed FemaleAnimator2 controller graph, its state behaviours and
clip identities, the Whacked action relation row and a committed static CIL
excerpt. It records which clips play and when the original ends the hit
reaction. It does not sample humanoid poses or port a blend-weight algorithm.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

import cil_disasm
import cil_evidence
from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, numeric_rows
from export_hit_reaction import f
from export_starter_outfit import body, uncomment

GAME = Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers'
MANAGED = GAME/'PE_Client_Data/Managed'
ASSET, SCRIPTS = 'sharedassets1.assets', 'sharedassets0.assets'
ASSET_SHA256 = 'd9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67'
SCRIPTS_SHA256 = '7266577940dc7ecf4392a241d668248410f98fdf17cad3573f38a27daed13b1b'
EVIDENCE = 'docs/reference/reaction-presentation-il.json'
BEAT_ACTION = 'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Beat.cs'
SENDER = 'Assets/Scripts/Anim/AnimatorEventSender.cs'
FADER = 'Assets/Scripts/Anim/AnimatorLayerWeightCtr.cs'
SOURCES = (BEAT_ACTION, SENDER, FADER, EVIDENCE)
METHODS = (
    (BEAT_ACTION, r'public\s+class\s+Action_Whacked', '6a1e6e8cfa2568272daae5d219e85768892a276fd97c34bd05e6ece1fd6b0825'),
    (SENDER, r'public\s+class\s+AnimatorEventSender', '211f9c7c44b0ddd17231f6811f4cc149799053b8843d3a7487a62e1d1dc6de8a'),
    (FADER, r'public\s+class\s+AnimatorLayerWeightCtr', '5c90ea3c64fd68da202aca4e457d7c1a8bd2fbb6d9dc91c224b6b7bb36355efb'),
)
ASSEMBLIES = {'Assembly-CSharp.dll': ('56d90949e732331be7644fc9185dc088c78ad4888a100f60d96b7aa093f4d7d3',
                                      '7ddcc464-5353-4750-b969-fe628ee474da')}
EXCERPT = {'Assembly-CSharp.dll': lambda m: m['declaring_type'] in (
    'Pathea.Action_Whacked', 'AnimatorEventSender', 'AnimatorLayerWeightCtr') or (
    m['declaring_type']=='Pathea.Motion_Move_Human' and m['name']=='UpdateAnimState')}
METHODS_DIGEST = '7e282ded98aab008b9c31ed2009b05ade06011a6357e7826aa9e49c4428b6b4b'
METHOD_IL = {
    'Pathea.Action_Whacked::get_ActionType': '54fc02efedc1c846',
    'Pathea.Action_Whacked::DoAction': '89148e444fb021e8',
    'Pathea.Action_Whacked::Update': '5fc5d411745f7e09',
    'Pathea.Action_Whacked::EndImmediately': 'ed29d2d0b5fd180a',
    'Pathea.Action_Whacked::OnAnimEvent': '0751da1896044a25',
    'AnimatorEventSender::OnStateExit': '21a1cc17b200ef46',
    'AnimatorEventSender::OnStateUpdate': '008106395c716506',
    'AnimatorEventSender::SendMsg': '2657007c9b47e19e',
    'AnimatorLayerWeightCtr::OnStateUpdate': '1b0dfe5ce94639c1',
    'AnimatorLayerWeightCtr::OnStateExit': '4d2042223a7e57e0',
    'Pathea.Motion_Move_Human::UpdateAnimState': '2c96e7723e6f8f6b81432c42ca5460b89bb6b7375422037fec61940de27e6991',
}
CONTROLLER, OVERRIDE = 779, 780
SENDER_SCRIPT, FADER_SCRIPT = 1916, 557
WHACKED_ACTION, WHACKED_MASK = 5, 47
FORBIDDEN = (6,7,8,9,10,11,24,31,40,41,44,45,46,47,48,55)
PAUSE = (13,15,18,19,22,28,47)
END_IMMEDIATELY = (1,2,3,4,14,20,24,25,27,30,31,32,33,42,43,44,45,46,48,49,50,58,60,61)
BE_HIT = 'FullBody.BeHit.Blend Tree'
# MoveStyle threshold -> female clip. Styles 7 and 8 (Carry, BeCarry) reuse the idle clip.
BE_HIT_CLIPS = (('IdleBeHit',139),('SwordBeHit',157),('RifleBeHit',188),('HandgunBeHit',173),('BowBeHit',200),
                ('ShotgunBeHit',220),('MineBeHit',210),('IdleBeHit',139),('IdleBeHit',139),('GrenadeBeHit',227),
                ('GlovesBeHit',116))
# BeatWeight 1 branch of the ground Move tree: (RightSpeed, ForwardSpeed) -> leaf.
# The two straight-back points blend to BlockRepulsed by SheildBlockF.
BEAT = (((0,0),'NormalIdle',684),((0,5),'ForwardFast',226),((3.5,3.5),'FowardRightFast',235),
        ((5,0),'RightFast',243),((3.5,-3.5),'BackRightFast',250),((0,-5),('BackFast',257),('BlockRepulsed',691)),
        ((-3.5,3.5),'ForwardLeftFast',265),((-5,0),'LeftFast',271),((-3.5,-3.5),'BackLeftFast',278),
        ((0,3),'Forward',109),((2.1,2.1),'ForwardRight',154),((3,0),'Right',136),((2.1,-2.1),'BackRight',170),
        ((0,-3),('Back',186),('BlockRepulsed',691)),((-2.1,2.1),'ForwardLeft',198),((-3,0),'Left',209),
        ((-2.1,-2.1),'BackLeft',218))
WATER_BEAT = (((0,0),'SwimIdle',351),((0,2),'SwimFF',303),((0,-2),'SwimFB',319),((2,0),'SwimFR',313),((-2,0),'SwimFL',308))
MIRRORED = {235,243,250,154,136,170,313}
UNLOOPED = {139,157,188,173,200,220,210,227,116,235}
MALE_OVERRIDES = {684:688, 210:110} # NormalIdle and MineBeHit; male MineBeHit is .9 s, not .9333


def require(value,message):
    if not value: raise ValueError(message)


def digest(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream,'sha256').hexdigest()


def bits(indices):
    return sum(1<<i for i in indices)


def validate_sources(root):
    texts={p:uncomment((root/p).read_text(encoding='utf-8-sig',errors='replace')) for p in SOURCES if p.endswith('.cs')}
    for path,pattern,expected in METHODS:
        if hashlib.sha256(body(texts[path],pattern).encode()).hexdigest()!=expected:
            raise ValueError('Reaction presentation source changed: '+path)
    return [describe_source(root/p) for p in SOURCES]


def load_evidence(root=ROOT):
    evidence,found=cil_evidence.load(root/EVIDENCE,ASSEMBLIES,METHODS_DIGEST,METHOD_IL)
    code=lambda name:cil_disasm.decode(bytes.fromhex(found[name]['raw_il_hex']))
    literal=lambda name:cil_evidence.strings(found[name])
    require(code('Pathea.Action_Whacked::get_ActionType')==[(0,'ldc.i4.5',None),(1,'ret',None)],'Whacked action ordinal changed')
    require(literal('Pathea.Action_Whacked::DoAction')==['BeHit'],'Whacked trigger changed')
    require(literal('Pathea.Action_Whacked::EndImmediately')==['ResetFullBody','BeHit'],'Whacked cleanup changed')
    require(literal('Pathea.Action_Whacked::OnAnimEvent')==['HitAnimEnd'],'Whacked end event changed')
    for name in ('DoAction','EndImmediately'):
        require((WHACKED_MASK in [o for _,m,o in code('Pathea.Action_Whacked::'+name) if m=='ldc.i4.s']),'Whacked mask changed')
    # MoveStyle = Lerp(MoveStyle, (float)style, 5 * deltaTime), as in the checked-in C#.
    update=code('Pathea.Motion_Move_Human::UpdateAnimState'); names=found['Pathea.Motion_Move_Human::UpdateAnimState']['strings']
    at=[i for i,(_,m,o) in enumerate(update) if m=='ldstr' and names['0x%08x'%o]=='MoveStyle']
    require(len(at)==2 and at[1]==at[0]+2,'MoveStyle smoothing changed')
    tail=[(m,o if m=='ldc.r4' else None) for _,m,o in update[at[1]+1:at[1]+10]]
    require(tail==[('callvirt',None),('ldarg.0',None),('callvirt',None),('conv.r4',None),('ldc.r4',5.0),
                   ('call',None),('mul',None),('call',None),('callvirt',None)],'MoveStyle smoothing changed')
    return evidence,found


def read_relation(database):
    db=connect_readonly(database)
    try:
        rows,_=numeric_rows(db,'ActionRelation','ActionType')
        row=dict(rows[WHACKED_ACTION]); fields=list(row)[5:]
        if len(fields)!=56 or row['Name']!='Whacked' or row['EndAction']!='0' or fields[WHACKED_MASK]!='Whacked':
            raise ValueError('Whacked relation layout changed')
        expected={i:2 for i in FORBIDDEN}
        if any(int(row[name])!=expected.get(i,0) for i,name in enumerate(fields)):
            raise ValueError('Whacked ordinal mask dependencies changed')
        if tuple(map(int,row['PauseAction'].split(',')))!=PAUSE or tuple(map(int,row['EndImmediately'].split(',')))!=END_IMMEDIATELY:
            raise ValueError('Whacked action relations changed')
        return row
    finally:
        db.close()


def clip_row(path_id,data):
    muscle=data['m_MuscleClip']
    return dict(path_id=path_id,name=data['m_Name'],length=muscle['m_StopTime']-muscle['m_StartTime'],
        frame_rate=data['m_SampleRate'],loop=bool(muscle['m_LoopTime']),mirror=bool(muscle['m_Mirror']),
        humanoid_bindings=sum(b['classID']==95 and b['customType']==8 for b in data['m_ClipBindingConstant']['genericBindings']),
        events=len(data['m_Events']))


def read_installed(game=GAME):
    """Controller, behaviour bytes and clip summaries from the pinned installed assets."""
    import UnityPy
    data=game/'PE_Client_Data'
    if digest(data/ASSET)!=ASSET_SHA256 or digest(data/SCRIPTS)!=SCRIPTS_SHA256:
        raise ValueError('Unqualified installed animation assets')
    serialized=next(iter(UnityPy.load(str(data/ASSET)).files.values()))
    scripts=next(iter(UnityPy.load(str(data/SCRIPTS)).files.values()))
    controller=serialized.objects[CONTROLLER].read_typetree(); override=serialized.objects[OVERRIDE].read_typetree()
    require([Path(e.path).name for e in serialized.externals][:1]==[SCRIPTS],'Unexpected script asset reference')
    wanted={i for _,i in BE_HIT_CLIPS}|{i for _,_,i in WATER_BEAT}
    for _,*leaves in BEAT: wanted|={l[1] for l in leaves if isinstance(l,tuple)}|{l for l in leaves if isinstance(l,int)}
    behaviours={}
    for pointer in controller['m_StateMachineBehaviours']:
        require(pointer['m_FileID']==0,'External state behaviour')
        behaviours[pointer['m_PathID']]=serialized.objects[pointer['m_PathID']].get_raw_data().hex()
    replaced={x['m_OriginalClip']['m_PathID']:x['m_OverrideClip']['m_PathID'] for x in override['m_Clips']
              if x['m_OriginalClip']['m_PathID'] in wanted and x['m_OverrideClip']['m_PathID']!=x['m_OriginalClip']['m_PathID']}
    return dict(controller=controller,overridden=replaced,override_controller=override['m_Controller'],
        clips={i:clip_row(i,serialized.objects[i].read_typetree()) for i in sorted(wanted|set(replaced.values()))},
        behaviours=behaviours,
        scripts={i:scripts.objects[i].read_typetree()['m_ClassName'] for i in (SENDER_SCRIPT,FADER_SCRIPT)})


def behaviour(raw_hex,scripts):
    """Decode one build-serialized StateMachineBehaviour of the two known scripts."""
    raw=bytes.fromhex(raw_hex)
    try:
        game_file,game_path,enabled,script_file,script_path,name=struct.unpack_from('<iqIiqI',raw,0)
        require((game_file,game_path,enabled,script_file,name)==(0,0,1,1,0),'Unexpected state behaviour header')
        kind=scripts.get(script_path); cursor=32
        if kind=='AnimatorLayerWeightCtr':
            require(len(raw)==36,'Unexpected layer fade payload')
            return dict(script=kind,fadeout_speed=struct.unpack_from('<f',raw,32)[0])
        require(kind=='AnimatorEventSender','Unsupported state behaviour script')
        texts=[]
        for _ in range(2):
            size=struct.unpack_from('<I',raw,cursor)[0]; cursor+=4
            require(cursor+size<=len(raw),'Truncated behaviour string')
            texts.append(raw[cursor:cursor+size].decode('utf-8')); cursor=(cursor+size+3)&~3
        count=struct.unpack_from('<I',raw,cursor)[0]
        # Timed events would need their own decoder and clock; none are present on these states.
        require(count==0 and cursor+4==len(raw),'Unsupported timed state events')
        return dict(script=kind,enter_event=texts[0],exit_event=texts[1],timed_events=0)
    except struct.error as error:
        raise ValueError('Truncated state behaviour') from error


def qualify_controller(installed):
    controller=installed['controller']; graph=controller['m_Controller']; names=dict(controller['m_TOS'])
    refs=controller['m_AnimationClips']; clips=installed['clips']
    require(controller['m_Name']=='FemaleAnimator2' and installed['override_controller']=={'m_FileID':0,'m_PathID':CONTROLLER},'Unsupported controller identities')
    # The male override lists every clip but really replaces only these two.
    require(installed['overridden']==MALE_OVERRIDES,'Male reaction clip overrides changed')
    for path_id,row in clips.items():
        female={v:k for k,v in MALE_OVERRIDES.items()}.get(path_id,path_id)
        require(row['humanoid_bindings']==127 and row['events']==0 and row['frame_rate']==30.0
                and row['name']==clips[female]['name'],'Reaction clip changed: '+row['name'])
        require(row['mirror']==(female in MIRRORED) and row['loop']==(female not in UNLOOPED),'Reaction clip flags changed: '+row['name'])
    values=graph['m_Values']['data']['m_ValueArray']; defaults=graph['m_DefaultValues']['data']
    parameter={names.get(v['m_ID']):(v['m_Type'],v['m_Index']) for v in values}
    for name in ('BeHit','ResetFullBody'): require(parameter.get(name,(0,))[0]==9,'Missing trigger '+name)
    for name in ('BeatWeight','SheildBlockF','MoveStyle','ForwardSpeed','RightSpeed'):
        require(parameter.get(name,(0,))[0]==1 and defaults['m_FloatValues'][parameter[name][1]]==0.0,'Unexpected float '+name)
    layer=graph['m_LayerArray'][1]['data']
    require(layer['m_StateMachineIndex']==1 and layer['(int&)m_LayerBlendingMode']==0 and layer['m_DefaultWeight']==1.0
            and layer['m_BodyMask']=={'word0':0xffffffff,'word1':0xffffffff,'word2':3}
            and not layer['m_SkeletonMask']['data']['m_Data'],'FullBody layer changed')
    machine=graph['m_StateMachineArray'][1]['data']; states=[s['data'] for s in machine['m_StateConstantArray']]
    path=lambda index:names.get(states[index]['m_FullPathID'])
    index={path(i):i for i in range(len(states))}
    def conditions(transition):
        return [(names.get(c['data']['m_EventID']),c['data']['m_ConditionMode']) for c in transition['m_ConditionConstantArray']]
    def any_state(trigger,target):
        found=[t['data'] for t in machine['m_AnyStateTransitionConstantArray'] if (trigger,1) in conditions(t['data'])]
        require(len(found)==1 and conditions(found[0])==[(trigger,1)] and found[0]['m_DestinationState']==index[target]
                and found[0]['m_TransitionDuration']==0.0 and found[0]['m_TransitionOffset']==0.0
                and not found[0]['m_HasExitTime'] and found[0]['m_CanTransitionToSelf'],trigger+' entry changed')
    any_state('BeHit',BE_HIT); any_state('ResetFullBody','FullBody.Empty')
    require(machine['m_DefaultState']==index['FullBody.DoNothing'],'FullBody default state changed')
    def leaf(nodes,node,expected):
        data=nodes[node]['data']
        require(not data['m_ChildIndices'] and refs[data['m_ClipID']]=={'m_FileID':0,'m_PathID':expected[1]}
                and clips[expected[1]]['name']==expected[0] and data['m_Duration']==1.0 and data['m_CycleOffset']==0.0
                and not data['m_Mirror'],'Reaction leaf changed: '+expected[0])
        return dict(node=node,name=expected[0],path_id=expected[1])
    def tree(state):
        require(len(state['m_BlendTreeConstantArray'])==1,'Unexpected reaction motion count')
        return state['m_BlendTreeConstantArray'][0]['data']['m_NodeArray']
    hit=states[index[BE_HIT]]; nodes=tree(hit); top=nodes[0]['data']
    require(hit['m_Speed']==1.0 and not hit['m_Loop'] and not hit['m_Mirror'] and not hit['m_SpeedParamID']
            and names.get(top['m_BlendEventID'])=='MoveStyle' and top['m_BlendType']==0
            and top['m_Blend1dData']['data']['m_ChildThresholdArray']==[float(i) for i in range(len(BE_HIT_CLIPS))]
            and top['m_ChildIndices']==list(range(1,len(BE_HIT_CLIPS)+1)),'BeHit blend changed')
    hit_leaves=[dict(threshold=i,**leaf(nodes,i+1,expected)) for i,expected in enumerate(BE_HIT_CLIPS)]
    require(len(hit['m_TransitionConstantArray'])==1,'BeHit exits changed')
    leave=hit['m_TransitionConstantArray'][0]['data']
    selector=machine['m_SelectorStateConstantArray'][leave['m_DestinationState']-30000]['data']['m_TransitionConstantArray']
    require(not conditions(leave) and leave['m_HasExitTime'] and not leave['m_HasFixedDuration']
            and len(selector)==1 and not selector[0]['data']['m_ConditionConstantArray']
            and selector[0]['data']['m_Destination']==index['FullBody.DoNothing'],'BeHit exit changed')
    empty=states[index['FullBody.Empty']]
    require(not empty['m_BlendTreeConstantArray'] and len(empty['m_TransitionConstantArray'])==1,'FullBody.Empty changed')
    rest=empty['m_TransitionConstantArray'][0]['data']
    require(rest['m_DestinationState']==index['FullBody.DoNothing'] and not conditions(rest) and rest['m_HasExitTime']
            and rest['m_TransitionDuration']==0.0,'FullBody.Empty exit changed')
    ranges={}
    for entry in controller['m_StateMachineBehaviourVectorDescription']['m_StateMachineBehaviourRanges']:
        state,span=(entry['first'],entry['second']) if isinstance(entry,dict) else entry
        ranges[(state['m_StateID'],state['m_LayerIndex'])]=span
    order=controller['m_StateMachineBehaviourVectorDescription']['m_StateMachineBehaviourIndices']
    def behaviours(name):
        span=ranges.get((states[index[name]]['m_FullPathID'],1))
        if not span: return []
        return [behaviour(installed['behaviours'][controller['m_StateMachineBehaviours'][i]['m_PathID']],installed['scripts'])
                for i in order[span['m_StartIndex']:span['m_StartIndex']+span['m_Count']]]
    hit_behaviours,idle_behaviours=behaviours(BE_HIT),behaviours('FullBody.DoNothing')
    require(hit_behaviours==[dict(script='AnimatorEventSender',enter_event='',exit_event='HitAnimEnd',timed_events=0)],'BeHit behaviour changed')
    require(idle_behaviours==[dict(script='AnimatorEventSender',enter_event='OnEndFullAnim',exit_event='',timed_events=0),
                              dict(script='AnimatorLayerWeightCtr',fadeout_speed=5.0)] and not behaviours('FullBody.Empty'),
            'FullBody idle behaviours changed')
    base=graph['m_StateMachineArray'][0]['data']
    def move(full,child,expected):
        found=[s['data'] for s in base['m_StateConstantArray'] if names.get(s['data']['m_FullPathID'])==full]
        require(len(found)==1 and found[0]['m_Speed']==1.0,'Missing '+full)
        nodes=tree(found[0]); top=nodes[0]['data']
        require(names.get(top['m_BlendEventID'])=='BeatWeight' and top['m_BlendType']==0 and top['m_ChildIndices']==[1,child]
                and top['m_Blend1dData']['data']['m_ChildThresholdArray']==[0.0,1.0],'BeatWeight blend changed')
        beat=nodes[child]['data']; points=beat['m_Blend2dData']['data']['m_ChildPositionArray']
        require(beat['m_BlendType']==2 and names.get(beat['m_BlendEventID'])=='RightSpeed'
                and names.get(beat['m_BlendEventYID'])=='ForwardSpeed' and len(points)==len(expected)==len(beat['m_ChildIndices']),
                'Beat direction blend changed')
        rows=[]
        for point,node,(position,*leaves) in zip(points,beat['m_ChildIndices'],expected):
            require(abs(point['x']-position[0])<1e-5 and abs(point['y']-position[1])<1e-5,'Beat direction point changed')
            row=dict(right_speed=point['x'],forward_speed=point['y'])
            if len(leaves)==2 and isinstance(leaves[0],str): row['clip']=leaf(nodes,node,tuple(leaves))
            else:
                shield=nodes[node]['data']
                require(names.get(shield['m_BlendEventID'])=='SheildBlockF' and shield['m_BlendType']==0 and len(shield['m_ChildIndices'])==2
                        and shield['m_Blend1dData']['data']['m_ChildThresholdArray']==[0.0,1.0],'Shield block blend changed')
                row['clip']=leaf(nodes,shield['m_ChildIndices'][0],leaves[0])
                row['shield_block_clip']=leaf(nodes,shield['m_ChildIndices'][1],leaves[1])
            rows.append(row)
        return dict(state=full,beat_node=child,blend_type=beat['m_BlendType'],leaves=rows)
    return dict(controller=dict(name=controller['m_Name'],path_id=CONTROLLER,male_override=OVERRIDE,
            male_clip_overrides={str(k):v for k,v in MALE_OVERRIDES.items()}),
        clips=[clips[i] for i in sorted(clips)],
        be_hit=dict(state=BE_HIT,layer=1,entry=dict(trigger='BeHit',any_state=True,duration=0.0,can_transition_to_self=True),
            speed=hit['m_Speed'],parameter='MoveStyle',leaves=hit_leaves,
            exit=dict(exit_time=leave['m_ExitTime'],duration=leave['m_TransitionDuration'],fixed_duration=False,destination='FullBody.DoNothing'),
            behaviours=hit_behaviours),
        full_body=dict(reset=dict(trigger='ResetFullBody',destination='FullBody.Empty',duration=0.0),
            empty_exit_time=rest['m_ExitTime'],idle='FullBody.DoNothing',idle_behaviours=idle_behaviours,
            default_weight=layer['m_DefaultWeight'],blending='override',body_mask='all'),
        repulsed=dict(ground=move('Base Layer.GroundLocomotion.Move',193,BEAT),
                      water=move('Base Layer.WaterLocomotion.Move',25,WATER_BEAT)))


def build_outputs(root=ROOT,database=DEFAULT_DATABASE,game=GAME):
    sources=validate_sources(root); evidence,found=load_evidence(root)
    cil_evidence.qualify_installed(evidence,ASSEMBLIES,game/'PE_Client_Data/Managed')
    graph=qualify_controller(read_installed(game)); relation=read_relation(database)
    hit=graph['be_hit']; clips={c['path_id']:c for c in graph['clips']}
    metadata=dict(schema=1,source=describe_source(database),source_files=sources,
        installed_assets={ASSET:ASSET_SHA256,SCRIPTS:SCRIPTS_SHA256},
        assemblies={n:dict(sha256=d,mvid=m,rea_evidence_id=evidence['assemblies'][n]['rea_evidence_id'])
                    for n,(d,m) in ASSEMBLIES.items()},
        methods_digest=METHODS_DIGEST,method_il_sha256={n:found[n]['body']['il_sha256'] for n in METHOD_IL},
        method_guards=[dict(path=p,pattern=r,sha256=s) for p,r,s in METHODS],
        whacked=dict(action_type=WHACKED_ACTION,mask=WHACKED_MASK,action_relation=relation,
                     move_style_lerp_speed=5.0,**graph['be_hit']),
        full_body=graph['full_body'],repulsed=graph['repulsed'],controller=graph['controller'],clips=graph['clips'],
        scope='Disconnected Whacked lifecycle and hit/Repulsed clip selection contract. No humanoid poses, blend-weight algorithm, Animator transition evaluation, hit IK, ragdoll, live stun or knockback.',
        end='Whacked has no timer. It ends when AnimatorEventSender on the BeHit state sends its exit event HitAnimEnd; the clips carry no events. The only authored exit starts at normalized time .9 and lasts .1 of the state, so the nominal end is one full state length.',
        selection='BeHit blends per-style clips by the MoveStyle float, which Motion_Move_Human smooths toward the MoveStyle enum by Lerp(5*deltaTime). Repulsed plays the BeatWeight 1 branch of the locomotion tree, selected by local RightSpeed/ForwardSpeed; straight-back leaves blend to BlockRepulsed by SheildBlockF.',
        engine='Freeform-directional blend weights, the frame on which a transition or state exit is evaluated, and layer mixing are Unity behavior and are not recovered. Poses need an engine bake.')
    header='/* Generated by export_reaction_presentation.py; do not edit. */\n#ifndef PE_REACTION_PRESENTATION_SOURCE_H\n#define PE_REACTION_PRESENTATION_SOURCE_H\n#include <stdint.h>\n'
    header+='typedef struct {uint32_t clip;float length;} PeReactionHitLeaf;\n'
    header+='typedef struct {float right_speed,forward_speed;uint32_t clip,shield_block_clip;} PeReactionBeatLeaf;\n'
    header+='static const PeReactionHitLeaf pe_whacked_source_leaves[%d]={\n'%len(hit['leaves'])
    header+=''.join('    {%d,%s}, /* %d %s */\n'%(l['path_id'],f(clips[l['path_id']]['length']),l['threshold'],l['name']) for l in hit['leaves'])+'};\n'
    for name,branch in (('ground',graph['repulsed']['ground']),('water',graph['repulsed']['water'])):
        header+='static const PeReactionBeatLeaf pe_repulsed_source_%s_leaves[%d]={\n'%(name,len(branch['leaves']))
        header+=''.join('    {%s,%s,%d,%d}, /* %s */\n'%(f(l['right_speed']),f(l['forward_speed']),l['clip']['path_id'],
            l.get('shield_block_clip',{}).get('path_id',0),l['clip']['name']) for l in branch['leaves'])+'};\n'
    header+='static const float pe_whacked_source_exit_time=%s;\n'%f(hit['exit']['exit_time'])
    header+='static const float pe_whacked_source_exit_duration=%s;\n'%f(hit['exit']['duration'])
    header+='static const float pe_whacked_source_move_style_lerp_speed=5.0f;\n'
    header+='static const float pe_reaction_source_full_body_fadeout_speed=5.0f;\n'
    for name,indices in (('forbidden_masks',FORBIDDEN),('pause_actions',PAUSE),('end_immediately_actions',END_IMMEDIATELY)):
        header+='static const uint64_t pe_whacked_source_'+name+'=UINT64_C('+str(bits(indices))+');\n'
    header+='#endif\n'
    return {'pe_reaction_presentation_source.h':header.encode(),
            'pe_reaction_presentation_source.json':(json.dumps(metadata,indent=2)+'\n').encode()}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    parser.add_argument('--output',type=Path,default=ROOT/'rebuild/generated')
    parser.add_argument('--excerpt',type=Path,metavar='CSHARP_INVENTORY',
                        help='rewrite the committed excerpt from a saved REA inventory, then exit')
    args=parser.parse_args(argv)
    try:
        if args.excerpt:
            data=cil_evidence.excerpt({'Assembly-CSharp.dll':args.excerpt},MANAGED,EXCERPT,
                'Focused static method excerpt for the Whacked action and its animator state behaviours. No assembly or game execution.')
            (ROOT/EVIDENCE).write_text(json.dumps(data,indent=1)+'\n'); print(cil_evidence.digest(data)); return 0
        outputs=build_outputs()
        if args.check:
            for name,data in outputs.items():
                if not (args.output/name).is_file() or (args.output/name).read_bytes()!=data:
                    raise ValueError('Stale or missing '+name)
        else:
            args.output.mkdir(parents=True,exist_ok=True)
            for name,data in outputs.items(): (args.output/name).write_bytes(data)
    except (OSError,ValueError,KeyError,ImportError,IndexError) as error:
        print(error,file=sys.stderr); return 1
    return 0


if __name__=='__main__': raise SystemExit(main())
