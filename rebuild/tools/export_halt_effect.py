#!/usr/bin/env python3
"""Recover the female player's Halt stop effect without starting Unity.

Action_Halt holds its mask while IKMoveEffect's foot-stop effect runs. This
exports the checked-in action/effect rules, the serialized female curves and
relation row 50. It records effector offset requests only; the FinalIK solve
that turns them into a pose is a third-party plugin and is not recovered.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, numeric_rows
from export_hit_reaction import MODEL, f
from export_human_phy import read_objects
from export_starter_outfit import body, uncomment

MOVE = 'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_HumanMove.cs'
EFFECT = 'Assets/Scripts/Ik/AnimEffect/IKAnimEffect.cs'
MOVE_EFFECT = 'Assets/Scripts/Ik/AnimEffect/IKMoveEffect.cs'
CONTROL = 'Assets/Scripts/Ik/AnimEffect/IKAnimEffectCtrl.cs'
MODIFIER = 'Assets/Scripts/Ik/IKOffsetModifier.cs'
SOURCES = (MODEL, MOVE, EFFECT, MOVE_EFFECT, CONTROL, CONTROL+'.meta', MODIFIER)
METHODS = (
    (MOVE, r'public\s+class\s+Action_Halt', '8de474f6748d64966f07b993f74c6590a4702f356cda4df80bdd379d1d8673d7'),
    (EFFECT, r'public\s+class\s+IKAnimEffect', 'b71970fc2e94ca7e9b8e407a295656a130bd4aefa43c7e488bab342e0d61d76c'),
    (MOVE_EFFECT, r'public\s+class\s+IKMoveEffect', '1eaa28bf83df56a36ab633aef5be079293d9cfa89d858ed948d2d0c9f9cc0c55'),
    (CONTROL, r'public\s+bool\s+moveEffectRunning', '2cec3c7fd3e106cdca9ed7e98cce6b5afc14b190a2e6e1910b6bee87174c9854'),
    (MODIFIER, r'private\s+void\s+ModifyOffset', '39e897b06191e5dd508c50f8c3c95d4ff15aa373b38249863b3572d8afb9617a'),
)
# Both Move and Sprint request Halt from EndAction; Repulsed requests it at its stop time.
REQUESTS = ((MOVE, 'motionMgr.DoAction(PEActionType.Halt);', 2),
            ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Beat.cs', 'motionMgr.DoAction(PEActionType.Halt);', 1))
COMPONENT, CONTROL_GUID = 11400010, '85a060e6d790d3940b730a1fb42f6c2d'
HALT_ACTION, HALT_MASK = 50, 16
FORBIDDEN = (1,2,3,9,13,18,21,24,26,45,48,51,54,55)
END_IMMEDIATELY = (1,2,3)
BLOCKED_BY_MASK = ('Move','Jump','Gather','PickUpItem','Abnormal','Ride')
# FinalIK FullBodyBipedEffector ordinals used by the serialized links, in link order.
EFFECTORS = (7,8,0,1,2,5,6)
LINK_NAMES = {'left': ('LFoot','RFoot','Body','Lshoulder','RShoulder','LHand','RHand'),
              'right': ('LFoot','RFoot','Body','LShoulder','RShoulder','LHand','RHand')}
FADE = {'left': 0.009999999776482582, 'right': 0.30000001192092896}
CURVES_DIGEST = 'a914a97095ac5bf06ef9b19555c787ee6ce0759ecc760c1de59a45324941050e'


def require(value,message):
    if not value: raise ValueError(message)


def read_text(path):
    return path.read_text(encoding='utf-8-sig',errors='replace')


def validate_sources(root):
    texts={p:uncomment(read_text(root/p)) for p in SOURCES if p.endswith('.cs')}
    for path,pattern,expected in METHODS:
        if hashlib.sha256(body(texts[path],pattern).encode()).hexdigest()!=expected:
            raise ValueError('Halt effect source changed: '+path+' '+pattern)
    for path,needle,count in REQUESTS:
        if uncomment(read_text(root/path)).count(needle)!=count: raise ValueError('Halt request sites changed: '+path)
    meta=read_text(root/(CONTROL+'.meta'))
    require('guid: '+CONTROL_GUID in meta,'IKAnimEffectCtrl script identity changed')
    return [describe_source(root/p) for p in SOURCES]


def curve(data,label):
    require(data['m_PreInfinity']==2 and data['m_PostInfinity']==2 and len(data['m_Curve'])>=2,'Unsupported curve wrap: '+label)
    keys=[dict(time=k['time'],value=k['value'],in_slope=k['inSlope'],out_slope=k['outSlope']) for k in data['m_Curve']]
    for a,b in zip(keys,keys[1:]): require(b['time']>a['time'],'Unordered curve keys: '+label)
    for k in keys: require(all(isinstance(v,(int,float)) and abs(v)<1e6 for v in k.values()),'Unsupported curve key: '+label)
    return keys


def qualify(model):
    matches=[k for k,o in model.items() if o.get('script_guid')==CONTROL_GUID]
    require(matches==[COMPONENT],'Expected one IKAnimEffectCtrl')
    data=model[COMPONENT]['data']
    require(data['m_Enabled']==1 and data['m_Weight']==1.0,'IKAnimEffectCtrl master state changed')
    owner=model.get(data['m_GameObject']['m_PathID'])
    require(owner and owner['kind']=='GameObject' and owner['data']['m_IsActive'],'IKAnimEffectCtrl owner inactive')
    move=data['m_MoveEffect']; effects={}
    for side,field in (('left','m_LFootStopEffect'),('right','m_RFootStopEffect')):
        effect=move[field]
        require(effect['m_FadeTime']==FADE[side],'Stop effect fade changed')
        require([l['m_Name'] for l in effect['m_Effects']]==list(LINK_NAMES[side])
                and [l['m_Effector'] for l in effect['m_Effects']]==list(EFFECTORS)
                and not any(l['m_PinWordPos'] for l in effect['m_Effects']),'Stop effect links changed')
        effects[side]=dict(fade_time=effect['m_FadeTime'],
            # Overwritten by every DoEffect; kept only as evidence of editor leftovers.
            stale_serialized=dict(step_time=effect['m_StepTime'],weight=effect['m_Weight'],direction=effect['m_EffectDir']),
            links=[dict(name=l['m_Name'],effector=l['m_Effector'],force=curve(l['m_ForceDir'],l['m_Name']),
                        up=curve(l['m_UpDir'],l['m_Name'])) for l in effect['m_Effects']])
    result=dict(component=COMPONENT,owner=data['m_GameObject']['m_PathID'],master_weight=data['m_Weight'],effects=effects,
        speed_to_weight=curve(move['m_SpeedToWeight'],'m_SpeedToWeight'),speed_to_time=curve(move['m_SpeedToTime'],'m_SpeedToTime'),
        start_effect_links=len(move['m_StartEffect']['m_Effects']))
    digest=hashlib.sha256(json.dumps([result['effects'],result['speed_to_weight'],result['speed_to_time']],sort_keys=True,separators=(',',':')).encode()).hexdigest()
    require(digest==CURVES_DIGEST,'Female stop-effect curves changed')
    return result


def read_relation(database):
    db=connect_readonly(database)
    try:
        rows,_=numeric_rows(db,'ActionRelation','ActionType')
        row=dict(rows[HALT_ACTION]); fields=list(row)[5:]
        require(len(fields)==56 and row['Name']=='Halt' and fields[HALT_MASK]=='Halt' and row['PauseAction']=='0'
                and row['EndAction']=='0','Halt relation layout changed')
        expected={i:2 for i in FORBIDDEN}
        require(all(int(row[name])==expected.get(i,0) for i,name in enumerate(fields)),'Halt mask dependencies changed')
        require(tuple(map(int,row['EndImmediately'].split(',')))==END_IMMEDIATELY,'Halt relations changed')
        blocked=tuple(dict(v)['Name'] for v in rows.values() if int(dict(v)['Halt']))
        require(blocked==BLOCKED_BY_MASK,'Actions blocked by the Halt mask changed')
        return dict(row=row,blocked_by_mask=list(blocked))
    finally:
        db.close()


def build_outputs(root=ROOT,database=DEFAULT_DATABASE):
    sources=validate_sources(root); binding=qualify(read_objects(root/MODEL)); relation=read_relation(database)
    metadata=dict(schema=1,source=describe_source(database),source_files=sources,binding=binding,
        action_type=HALT_ACTION,mask=HALT_MASK,action_relation=relation['row'],blocked_by_mask=relation['blocked_by_mask'],
        method_guards=[dict(path=p,pattern=r,sha256=s) for p,r,s in METHODS],
        scope='Disconnected Halt lifecycle and stop-effect effector offset requests for the female player. No FinalIK solve, pose, live movement lock, animation or multiplayer branch.',
        lifecycle='Move and Sprint EndAction and Repulsed at its stop time request Halt. DoAction starts the foot-stop effect from the controller velocity; the action and its mask last until that effect stops running. With no effect or motor, or a non-positive master weight, it ends on its first Update.',
        effect='Speed selects weight and duration through two curves; weight is 0 at or below 4.5 m/s and 1 from 8, duration runs from .2 to .9 s. The stepping foot is the right one when the right-to-left foot vector is more than 90 degrees from the motion. Each frame every link adds (force(t)*direction + up(t)*up) * master*crossfade*weight to its effector offset.',
        engine='Offsets are additive requests to FinalIK FullBodyBipedIK, whose solver is not part of the checked-in source. Mono float evaluation is not claimed bit-exact.')
    counter=[0]; lines=[]
    def keys(rows):
        name='pe_halt_source_keys_%d'%counter[0]; counter[0]+=1
        lines.append('static const PeHaltCurveKey %s[%d]={%s};\n'%(name,len(rows),','.join(
            '{%s,%s,%s,%s}'%tuple(f(k[v]) for v in ('time','value','in_slope','out_slope')) for k in rows)))
        return '{%s,%d}'%(name,len(rows))
    tables=[]
    for side in ('left','right'):
        rows=['    {%d,%s,%s}, /* %s */\n'%(l['effector'],keys(l['force']),keys(l['up']),l['name']) for l in binding['effects'][side]['links']]
        tables.append('static const PeHaltLink pe_halt_source_%s_links[%d]={\n%s};\n'%(side,len(rows),''.join(rows)))
    weight,time=keys(binding['speed_to_weight']),keys(binding['speed_to_time'])
    header='/* Generated by export_halt_effect.py; do not edit. */\n#ifndef PE_HALT_SOURCE_H\n#define PE_HALT_SOURCE_H\n#include <stdint.h>\n'
    header+='typedef struct {float time,value,in_slope,out_slope;} PeHaltCurveKey;\n'
    header+='typedef struct {const PeHaltCurveKey *keys;unsigned count;} PeHaltCurve;\n'
    header+='typedef struct {unsigned effector;PeHaltCurve force,up;} PeHaltLink;\n'
    header+=''.join(lines)+''.join(tables)
    header+='static const PeHaltCurve pe_halt_source_speed_to_weight=%s;\n'%weight
    header+='static const PeHaltCurve pe_halt_source_speed_to_time=%s;\n'%time
    header+='static const float pe_halt_source_left_fade_time=%s;\n'%f(FADE['left'])
    header+='static const float pe_halt_source_right_fade_time=%s;\n'%f(FADE['right'])
    header+='static const uint64_t pe_halt_source_forbidden_masks=UINT64_C(%d);\n'%sum(1<<i for i in FORBIDDEN)
    header+='static const uint64_t pe_halt_source_end_immediately_actions=UINT64_C(%d);\n#endif\n'%sum(1<<i for i in END_IMMEDIATELY)
    return {'pe_halt_source.h':header.encode(),'pe_halt_source.json':(json.dumps(metadata,indent=2)+'\n').encode()}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    parser.add_argument('--output',type=Path,default=ROOT/'rebuild/generated')
    args=parser.parse_args(argv)
    try:
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
