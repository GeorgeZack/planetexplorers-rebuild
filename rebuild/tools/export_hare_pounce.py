#!/usr/bin/env python3
"""Guard the isolated source hare pounce lifecycle and controller binding."""
import argparse
from contextlib import closing
import hashlib
import json
import sqlite3
import struct
from pathlib import Path
import xml.etree.ElementTree as ET
from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, c_float
from export_starter_outfit import body, uncomment
from export_vegetation_source import source_text
BT='Assets/Scripts/Behave/Action/BTAttack.cs'
XML='Assets/Resources/AiPrefab/EntityData/Monster/M_lepus_hare.xml'
EPS='Assets/Scripts/PETools/PEMath.cs'
TRIGGER='Assets/Scripts/Assist/AnimationStateInfo/PEAnimatorAttackTrigger.cs'
CONTROLLER='Assets/Resources/AiPrefab/EntityControllers/Monster/Land/lepus_hare.controller'
PREFAB='Assets/Prefabs/AiPrefab/AiPrefabEntity/AiPrefabMonster/Monster/M_lepus_hare.prefab'
BUNDLE_SHA='94409ad1bf9570048a52c42671986907e2a5ca131f55f73737e5b7d7cff76c35'
CLIP_ID=1506232535475238862
STATE_ID=336979320
ATTRIBUTES={'Name':'Pounce','pounce':'attack5','prob':'0.5','cdTime':'5','minRange':'2.0','maxRange':'5.0','minAngle':'-15','maxAngle':'15','startTime':'0.267','endTime':'0.6','stopTime':'0.967','skillID':'30100053'}
SKILL={'_id':'30100053','_interruptable':'1','_coolingTime':'.967','_coolingTimeType':'1','_coolingTimeShared':'0','_pretimeOfPrepare':'0','_postimeOfPrepare':'0','_effPrepare':'0','_cond':'False','_effMainOneTime':'0','_pretimeOfMain':'0','_timeOfMain':'.967','_postimeOfMain':'0','_effMainEachTime':'0','_triggerEvents':'10250','_pretimeOfEnding':'0','_postimeOfEnding':'0','_effEnding':'0'}
PINS={'Assets/Scripts/PETools/PEMath.cs': '3f1aad65bb9928affcc3ad60c3ad7f4518abe3cf7f56f41aab9d4119433cbf3a', 'Assets/Scripts/Assist/AnimationStateInfo/PEAnimatorAttackTrigger.cs': '3a4440daceaad6e444990bd355fa52ca12a89c6a1d30ddbefab38ea0e8ba85b5', 'Assets/Resources/AiPrefab/EntityControllers/Monster/Land/lepus_hare.controller': 'a0e8eda8989a5447a27d019b6949354c12536a6d76be21cc7790b283869ed024', 'Assets/Prefabs/AiPrefab/AiPrefabEntity/AiPrefabMonster/Monster/M_lepus_hare.prefab': 'bfebf9c0d9c846ec1fc9a0764910fa14bb652fdbbcedcae204d1075b2973b1e5'}
BODY_HASH='a343446ae0102c89ee984df471abec7d0248bebf16d7ca9549f3c3e12a230c28'

def f32(x):return struct.unpack('<f',struct.pack('<f',float(x)))[0]
def source_contract(root=ROOT,database=DEFAULT_DATABASE):
    text=uncomment(source_text(root/BT))
    if '#define ATTACK_OLD' in text:raise ValueError('Old pounce branch enabled')
    if hashlib.sha256(body(text,r'public\s+class\s+BTPounce').encode()).hexdigest()!=BODY_HASH:raise ValueError('Pounce lifecycle source changed')
    for rel,digest in PINS.items():
        if hashlib.sha256((root/rel).read_bytes()).hexdigest()!=digest:raise ValueError('Pounce source changed: '+rel)
    xml=ET.fromstring((root/XML).read_bytes().decode('utf-8',errors='replace'))
    nodes=xml.findall('.//Action[@Type="Pounce"]/Data')
    if len(nodes)!=1 or nodes[0].attrib!=ATTRIBUTES:raise ValueError('Pounce XML contract changed')
    with closing(connect_readonly(database)) as db:
        skill=dict(db.execute("SELECT * FROM skMain WHERE _id='30100053'").fetchone())
        trigger=dict(db.execute("SELECT * FROM skTriggerEvent WHERE _id='10250'").fetchone())
    if any(skill.get(k)!=v for k,v in SKILL.items()):raise ValueError('Pounce skill contract changed')
    if trigger['_cond']!='colinfo' or trigger['_force']!='250':raise ValueError('Pounce external trigger changed')
    return dict(xml=nodes[0].attrib,skill=skill,trigger=trigger)

def installed_provenance(objects):
    from export_reference_actor import CONTROLLER_ID
    from export_reference_assets import pointer
    c=objects[CONTROLLER_ID].read_typetree();names=dict(c['m_TOS'])
    states={names[s['data']['m_FullPathID']]:s['data'] for m in c['m_Controller']['m_StateMachineArray'] for s in m['data']['m_StateConstantArray']}
    state=states['Base Layer.Attack.attack5'];leaves=[n['data'] for b in state['m_BlendTreeConstantArray'] for n in b['data']['m_NodeArray'] if not n['data']['m_ChildIndices']]
    if len(leaves)!=1 or state['m_FullPathID']!=STATE_ID or state['m_Loop'] or state['m_Speed']!=1:raise ValueError('Pounce controller state changed')
    clip_id=pointer(c['m_AnimationClips'][leaves[0]['m_ClipID']]);clip=objects[clip_id].read_typetree();m=clip['m_MuscleClip'];dense=m['m_Clip']['data']['m_DenseClip']
    if clip_id!=CLIP_ID or clip['m_Name']!='attack5_pouncefront' or m['m_LoopTime'] or m['m_StartTime']!=0 or m['m_StopTime']!=0.9666671752929688 or dense['m_FrameCount']!=31 or dense['m_CurveCount']!=302 or dense['m_SampleRate']!=30 or m['m_Clip']['data']['m_StreamedClip']['curveCount']!=0:raise ValueError('Pounce clip contract changed')
    desc=c['m_StateMachineBehaviourVectorDescription'];span,=[v for k,v in desc['m_StateMachineBehaviourRanges'] if k['m_StateID']==STATE_ID and k['m_LayerIndex']==0]
    behaviours=[];window=None;effect=None
    for i in desc['m_StateMachineBehaviourIndices'][span['m_StartIndex']:span['m_StartIndex']+span['m_Count']]:
        id=pointer(c['m_StateMachineBehaviours'][i]);t=objects[id].read_typetree();kind=objects[pointer(t['m_Script'])].read_typetree()['m_ClassName'];behaviours.append(dict(path_id=id,kind=kind,fields=t))
        if kind=='PEAnimatorAttackTrigger':
            if t['startTime']!=f32(.575) or t['endTime']!=f32(.802) or t['bones']!=['Bip01 L Forearm','Bip01 R Forearm']:raise ValueError('Pounce collision window changed')
            window=dict(start_normalized=t['startTime'],end_normalized=t['endTime'],start_inclusive=True,end_inclusive=True,bones=t['bones'],behaviour_path_id=id)
        if kind=='PEAnimatorTrigger':effect=t['triggers']
    expected=[{'time':f32(.32),'effects':[{'effect':1,'data':'850'}]},{'time':f32(.66),'effects':[{'effect':1,'data':'891'}]}]
    if window is None or effect!=expected or len(behaviours)!=3:raise ValueError('Pounce behaviour contract changed')
    contract=source_contract()
    return dict(controller_path_id=CONTROLLER_ID,state='Base Layer.Attack.attack5',state_full_path_id=STATE_ID,state_speed=1,clip_path_id=clip_id,skill=contract['skill'],prepare_effect=None,animator_owner='BTPounce.SetBool(attack5,true), not skEffect',state_behaviours=behaviours,attack_windows=[window],clip_animation_events=clip['m_Events'])

def build_outputs(root=ROOT,database=DEFAULT_DATABASE):
    contract=source_contract(root,database)
    header=['/* Generated by export_hare_pounce.py; isolated source owner, no hunt activation. */','#ifndef PE_HARE_POUNCE_SOURCE_H','#define PE_HARE_POUNCE_SOURCE_H',
        '#define PE_HARE_POUNCE_EPSILON 0x1p-23f',
        'static const PeHarePounceDef pe_hare_pounce_source={30100053,10250,'+','.join(c_float(f32(ATTRIBUTES[k])) for k in ('prob','cdTime','minRange','maxRange','startTime','endTime','stopTime'))+','+c_float(f32(.967))+','+c_float(f32(.575))+','+c_float(f32(.802))+'};','#endif','']
    report=dict(schema=1,sources=[describe_source(root/p) for p in (BT,XML,EPS,TRIGGER,CONTROLLER,PREFAB)]+[describe_source(database)],contract=contract,installed=dict(bundle_sha256=BUNDLE_SHA,controller_id=4131577587453613724,state_id=STATE_ID,clip_id=CLIP_ID,clip_name='attack5_pouncefront',duration=0.9666671752929688,frames=31,fps=30,dense_channels=302),scope=['Disconnected lifecycle and staged animation; no production pounce admission, aggression or collision activation.','Absolute scaled float timestamps preserve source strict comparisons; host must supply actual ReadyAttack scope and bounds-gap geometry.','First aligned tick starts the independently owned skill; movement success and Reset never stop that skill.','Reset retains the original Linux epsilon guard; initialization at exactly time0 has its original no-restore edge.'])
    return {'pe_hare_pounce_source.h':'\n'.join(header).encode(),'pe_hare_pounce_source.json':(json.dumps(report,indent=2)+'\n').encode()}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');a=p.parse_args()
    for name,data in build_outputs().items():
        path=ROOT/'rebuild/generated'/name
        if a.check:
            if not path.is_file() or path.read_bytes()!=data:raise ValueError('Stale '+name)
        else:path.write_bytes(data)
    print('Pounce source checked' if a.check else 'Pounce source generated')
if __name__=='__main__':main()
