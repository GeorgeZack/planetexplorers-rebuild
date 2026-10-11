#!/usr/bin/env python3
"""Recover admitted female-player Repulsed requests without starting Unity."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

import export_hit_reaction as reaction
from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, numeric_rows
from export_starter_outfit import body, uncomment
from export_capsule_history import DEFAULT_UNITY, UNITY_SHA256

BEAT_ACTION = 'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Beat.cs'
MANAGER = 'Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs'
HUMAN = 'Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Move_Human.cs'
MOTOR = 'Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Move_Motor.cs'
ANGLE_EVIDENCE = 'docs/reference/repulsed-unity-vector-angle.json'
SOURCES = tuple(dict.fromkeys((*reaction.SOURCES, BEAT_ACTION, MANAGER,
                              HUMAN, HUMAN+'.meta', MOTOR, MOTOR+'.meta', ANGLE_EVIDENCE)))
METHODS = (
    (reaction.BEAT, r'public\s+override\s+void\s+Start\s*\(', '26487b70b6f579e662db5d2266abe616c1d3f728006afbe1248df65f9e044bf1'),
    (reaction.BEAT, r'public\s+void\s+OnMsg\s*\(', 'da2592018cd8f26703dad2dce118fc8d6666e9a21baa2e7af61e53424942d3b1'),
    (BEAT_ACTION, r'public\s+class\s+Action_Repulsed', 'eac786e4b02e200b7832fa6e94ed4b9345a64eac314a6f8720483b2c7fbfdd3b'),
    (MANAGER, r'public\s+bool\s+DoAction\s*\(', 'f58a6d9b88dc13c74170eeb3c230d55da2a7258d8f45ed591d6bdddaba54bd85'),
    (MANAGER, r'class\s+RunningAction', '9578ae9ef45f7372e68b12e71fed364e4fe6b707cb87c3557c95382f16033034'),
    (reaction.ACTION, r'public\s+abstract\s+class\s+PEAction', 'c443537080d22ef14706226c3e35a351a17389c010ca95a06bb180cf938ae5f2'),
)
CURVES = {
    'm_TimeVelocityScale': '9bb8ee1357ac56fd6b238fcb4c10e14a246e3b54b1d1487500b51f5f9b25f808',
    'm_ForceToVelocity': 'eb2501fef2881255eaa45c57791e91a7f5dddbb6d50e4eda1bd5e44bafca1315',
    'm_ForceToMoveTime': '5fe3a3edb73d732972aa3842a205cb778b90c699d03730fa463d26c289ce6b49',
    'm_ApplyMoveStopTime': '1767331c6b5dc6f9419978d8becdd761236bff489fbe174fb6aa1aed704333b0',
    'm_WentflyTimeCurve': 'fa95c2bf314996eab872ffedda2110f57dcc42ad55ed806bc620157f7a486354',
}
FORBIDDEN = (1,5,7,8,9,10,11,17,24,31,32,33,34,35,40,41,44,45,46,48,51,55)
PAUSE = (13,15,18,19,22,24)
END_IMMEDIATELY = (1,2,3,4,14,16,20,21,23,25,27,44,48,49,56,57,58,60,61)


def digest(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()


def bits(indices):
    return sum(1 << i for i in indices)


def validate_sources(root):
    reaction.validate_sources(root)
    for path, pattern, expected in METHODS:
        text=uncomment((root/path).read_text(encoding='utf-8-sig',errors='replace'))
        if hashlib.sha256(body(text,pattern).encode()).hexdigest()!=expected:
            raise ValueError('Repulsed source method changed: '+path+' '+pattern)
    text=re.sub(r'\s+','',uncomment((root/reaction.BEAT).read_text(encoding='utf-8-sig',errors='replace')))
    for binding in ('m_Repulsed.m_Move=Entity.GetCmpt<Motion_Move_Motor>();',
                    'm_Repulsed.m_Param=m_Param;', 'm_Repulsed.phyCtrl=m_PhyCtrl;'):
        if binding not in text:
            raise ValueError('Repulsed live component binding changed')
    return [describe_source(root/path) for path in SOURCES]


def qualify(model,player,root=ROOT):
    reaction.qualify(model,player,dict(param=reaction.guid(root,reaction.PARAM),
                                     view=reaction.guid(root,reaction.VIEW),beat=reaction.guid(root,reaction.BEAT)))
    param=model[11471154]['data']
    curves={}
    for field, expected in CURVES.items():
        curve=param[field]
        if digest(curve)!=expected or len(curve['m_Curve'])!=4:
            raise ValueError('Unqualified active female Repulsed curve: '+field)
        curves[field]=curve
    if param['m_ToWentflyForceScale']!=0.800000011920929 or param['m_ApplyWentflyBone']!={'m_FileID':0,'m_PathID':400106}:
        raise ValueError('Female Wentfly request binding changed')
    bone=model[400106]
    owner=model[reaction.pointer(bone['data']['m_GameObject'])]['data']
    if bone['kind']!='Transform' or owner['m_Name']!='Bip01 Spine2' or not owner['m_IsActive']:
        raise ValueError('Female Wentfly bone changed')
    view_go=reaction.pointer(model[11424092]['data']['m_GameObject'])
    transforms={i:o['data'] for i,o in model.items() if o['kind']=='Transform'}
    root_transform=[i for i,d in transforms.items() if reaction.pointer(d['m_GameObject'])==view_go]
    if len(root_transform)!=1:
        raise ValueError('Female view transform changed')
    tid=400106; seen=set()
    while tid!=root_transform[0]:
        if not tid or tid in seen or tid not in transforms:
            raise ValueError('Wentfly bone outside female hierarchy')
        seen.add(tid); transform=transforms[tid]
        if transform['m_GameObject']['m_FileID'] or transform['m_Father']['m_FileID'] or not model[reaction.pointer(transform['m_GameObject'])]['data']['m_IsActive']:
            raise ValueError('Inactive or external Wentfly bone hierarchy')
        tid=reaction.pointer(transform['m_Father'])
    # The generic PEMotor force branch is absent from this player. HumanPhyCtrl
    # receives ApplyMoveRequest; its engine physics remain a separate gate.
    hid,human=reaction.one(player,reaction.guid(root,HUMAN))
    if hid!=11400024 or reaction.pointer(human['m_GameObject'])!=100000:
        raise ValueError('Player human motion owner changed')
    motor_guid=reaction.guid(root,MOTOR)
    if any(o.get('script_guid')==motor_guid and o['data']['m_Enabled']
           for o in player.values()):
        raise ValueError('Unexpected generic motor on EntityPlayer')
    return dict(curves=curves,model_component=11471154,entity_component=11400038,
                human_motion_component=hid,generic_motor_present=False,
                wentfly_bone=400106,wentfly_bone_name=owner['m_Name'],
                wentfly_force_scale=param['m_ToWentflyForceScale'],
                unused_entity_curves=player[11400038]['data']['m_Repulsed'])


def read_relation(database):
    db=connect_readonly(database)
    try:
        rows,_=numeric_rows(db,'ActionRelation','ActionType')
        row=dict(rows[6]); fields=list(row)[5:]
        if len(fields)!=56 or row['Name']!='Repulsed' or row['EndAction']!='0':
            raise ValueError('Repulsed relation layout changed')
        expected={i:2 for i in FORBIDDEN}
        if any(int(row[name])!=expected.get(i,0) for i,name in enumerate(fields)):
            raise ValueError('Repulsed ordinal mask dependencies changed')
        if tuple(map(int,row['PauseAction'].split(',')))!=PAUSE or tuple(map(int,row['EndImmediately'].split(',')))!=END_IMMEDIATELY:
            raise ValueError('Repulsed action relations changed')
        return row
    finally:
        db.close()


def qualify_unity(unity_engine=DEFAULT_UNITY,root=ROOT):
    raw=unity_engine.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=UNITY_SHA256:
        raise ValueError('Unqualified Repulsed UnityEngine assembly')
    evidence=json.loads((root/ANGLE_EVIDENCE).read_text())
    if evidence['artifact']['sha256']!=UNITY_SHA256 or evidence['module']['mvid']!='5cf16eb2-32c9-483e-8fb7-9072104d5826':
        raise ValueError('Unqualified Unity angle evidence identity')
    methods={m['token']:m for m in evidence['methods']}
    angle=methods['0x060004cc']; normalize=methods['0x060004c3']
    if angle['body']['il_sha256']!='dfd1d9fda3a0c9dd644e7c0092186ee705030f5e2562cb56da88d859c25458f2' or normalize['body']['il_sha256']!='42f7bb97933b393992990f5ad60c0edcda1ba770de0674c566d2f1ed18f58b25':
        raise ValueError('Unqualified Unity angle/normalize methods')
    for method in methods.values():
        data=bytes.fromhex(method['raw_il_hex']); body_info=method['body']
        offset=body_info['file_offset']
        header_size=1 if body_info['header_format']=='tiny' else (raw[offset+1]>>4)*4
        if len(data)!=body_info['il_size'] or hashlib.sha256(data).hexdigest()!=body_info['il_sha256'] or raw[offset+header_size:offset+header_size+len(data)]!=data:
            raise ValueError('Unity method evidence differs from installed bytes')
    return dict(name='UnityEngine.dll',sha256=UNITY_SHA256,
                mvid=evidence['module']['mvid'],rea_evidence_id=evidence['rea_evidence_id'],
                zero_direction_angle_degrees=90,
                qualification='Static CIL-derived zero-direction angle; no managed/game execution.',
                method_il_sha256={m['token']:m['body']['il_sha256'] for m in methods.values()})


def build_outputs(root=ROOT,database=DEFAULT_DATABASE,unity_engine=DEFAULT_UNITY):
    sources=validate_sources(root)
    binding=qualify(reaction.read_prefab(root/reaction.MODEL),reaction.read_prefab(root/reaction.PLAYER),root)
    relation=read_relation(database)
    metadata=dict(schema=1,source=describe_source(database),source_files=sources,
                  unity_engine=qualify_unity(unity_engine,root),
                  binding=binding,action_relation=relation,
                  method_guards=[dict(path=p,pattern=r,sha256=s) for p,r,s in METHODS],
                  scope='Disconnected admitted Repulsed lifecycle and velocity requests; no live stun, knockback, shielding, action scheduler, humanoid pose, IK, ragdoll or physical motor equivalence.',
                  clock='One Update call, testing the old elapsed time before adding deltaTime; Wentfly and Halt use strict greater-than boundaries.',
                  repeat='MotionMgr calls inherited no-op ResetAction for a running Repulsed; it does not restart, rebind or recheck dependencies.',
                  shield='Shield angle uses the previous m_MoveDir before assigning/projecting the new hit direction. Unity5 Angle with its initial zero direction is90. This is not equipment-based automatic shielding.',
                  parameters='Caller snapshots PEActionParamVVF.param when DoAction executes; source ignores its para argument and reads the shared singleton.',
                  motor='Active EntityPlayer has Motion_Move_Human and no Motion_Move_Motor. ApplyMoveRequest is a velocity request to HumanPhyCtrl, not a direct displacement or an additive generic-motor impulse.')
    header='''/* Generated by export_repulsed_action.py; do not edit. */
#ifndef PE_REPULSED_SOURCE_H
#define PE_REPULSED_SOURCE_H
#include <stdint.h>
typedef struct {float time,value,in_slope,out_slope;} PeRepulsedCurveKey;
'''
    for name,field in zip(('time_velocity','force_velocity','move_time','stop_time','wentfly_time'),CURVES):
        header+='static const PeRepulsedCurveKey pe_repulsed_'+name+'_curve[4]={\n'
        header+=''.join('    {'+','.join(reaction.f(k[v]) for v in ('time','value','inSlope','outSlope'))+'},\n' for k in binding['curves'][field]['m_Curve'])
        header+='};\n'
    for name,indices in (('forbidden_masks',FORBIDDEN),('pause_actions',PAUSE),('end_immediately_actions',END_IMMEDIATELY)):
        header+='static const uint64_t pe_repulsed_source_'+name+'=UINT64_C('+str(bits(indices))+');\n'
    header+='static const float pe_repulsed_wentfly_force_scale='+reaction.f(binding['wentfly_force_scale'])+';\n#endif\n'
    return {'pe_repulsed_source.h':header.encode(),
            'pe_repulsed_source.json':(json.dumps(metadata,indent=2)+'\n').encode()}


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
    except (OSError,ValueError,KeyError,ImportError) as error:
        print(error,file=sys.stderr); return 1
    return 0


if __name__=='__main__': raise SystemExit(main())
