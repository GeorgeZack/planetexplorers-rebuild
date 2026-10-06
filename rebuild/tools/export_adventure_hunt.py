#!/usr/bin/env python3
"""Export checked Bug Hunt travel rules and an optional observed reference point."""
import argparse
from contextlib import closing
import json
import math
from pathlib import Path
import sqlite3
import sys
from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, numeric_rows
DEFAULT_CAPTURE=ROOT/'docs/reference/hunt-original-save.json'
DEFAULT_RETURN_CAPTURE=ROOT/'docs/reference/hunt-original-return.json'
DEFAULT_OUTPUT=ROOT/'rebuild/generated'


def build_outputs(database,capture,return_capture=DEFAULT_RETURN_CAPTURE):
    with closing(connect_readonly(database)) as db:
        quests,_=numeric_rows(db,'AdRandomQuest_List','ID')
        targets,_=numeric_rows(db,'AdQuest_EnterArea','TargetID')
        monsters,_=numeric_rows(db,'AdQuest_KillMonster','TargetID')
        contracts={9074:dict(PreLimit='1:9072',player_talk='4101,0',TalkOP='4102',GuanLianList='9075,9076'),
            9075:dict(TargetIDList='4524',player_talk='4103,0',TalkOP='0',AutoReply='1',NextQuest='9126',IncreaseChain='1',Get_MissionItem='0',RewardItem='0',bGiveUp='0'),
            9076:dict(player_talk='4104,0',MaxNum='-1'),9126:dict(TargetIDList='1518',NextQuest='9127'),
            9127:dict(TargetIDList='4525',AutoReply='1',RewardItem='592_1,593_1,594_1,595_1',TalkED='4105,4106',NextQuest='9077',AdvPlot='3_4',Get_MissionItem='0',CoRemoveItem='0')}
        for identity,expected in contracts.items():
            for key,value in expected.items():
                if quests[identity][key]!=value:raise ValueError(f'Hunt{identity} source contract changed: {key}')
        if targets[4524]['DistPos']!='1_0_130_15' or targets[4524]['NotForDungeon']!='1':raise ValueError('Hunt discovery contract changed')
        if targets[4525]['DistPos']!='3_3_0_5' or targets[4525]['NotForDungeon']!='1':raise ValueError('Hunt return discovery contract changed')
        if monsters[1518]['MonsterID']!='13_4' or monsters[1518]['PathIDtemp']!='0_13_6':raise ValueError('Hunt encounter contract changed')
        source=dict(missions={i:dict(quests[i]) for i in contracts},discovery=dict(targets[4524]),encounter=dict(monsters[1518]),return_discovery=dict(targets[4525]))
    observation=None;position=[0.,0.,0.]
    if capture is not None:
        evidence=json.loads(capture.read_text());target=evidence.get('hunt_target')
        if evidence.get('schema')!=1 or evidence.get('archives_unchanged') is not True or not evidence.get('input_sha256'):raise ValueError('Missing validated original archive evidence')
        if not isinstance(target,dict) or target.get('id')!=4524 or target.get('kind')!=4 or target.get('present') is not True:raise ValueError('No observed hunt discovery4524')
        if '9075' not in evidence.get('active_missions',{}) or evidence.get('completed_missions',{}).get('9072',0)<1:raise ValueError('Expected completed medicine and active hunt')
        quest=evidence.get('random_missions',{}).get('9075',{})
        if quest.get('npc')!=20018:raise ValueError('Expected archived Naoko binding')
        targets=quest.get('targets',[])
        if len(targets)!=1 or any(targets[0].get(key)!=target.get(key) for key in ('id','kind','present','position')):
            raise ValueError('Observed target differs from random mission archive')
        position=target.get('position')
        if not isinstance(position,list) or len(position)!=3 or any(type(v) not in (int,float) or not math.isfinite(v) for v in position):raise ValueError('Invalid observed destination')
        if position[0]!=math.trunc(position[0]) or position[2]!=math.trunc(position[2]):raise ValueError('Expected source integer XZ destination')
        observation=dict(capture=describe_source(capture),archive_guid=evidence['archive_guid'],input_sha256=evidence['input_sha256'],target=target)
    return_observation=None;return_position=[0.,0.,0.]
    if return_capture is not None:
        evidence=json.loads(return_capture.read_text());quest=evidence.get('random_missions',{}).get('9127',{})
        if evidence.get('schema')!=1 or evidence.get('archives_unchanged') is not True or not evidence.get('input_sha256'):
            raise ValueError('Missing validated original return archive evidence')
        if '9127' not in evidence.get('active_missions',{}) or evidence.get('completed_missions',{}).get('9126',0)<1:
            raise ValueError('Expected active return and completed kill quest')
        targets=quest.get('targets',[])
        if quest.get('npc')!=20018 or quest.get('reply_npc')!=20018 or len(targets)!=1:
            raise ValueError('Expected original return Naoko binding')
        target=targets[0]
        if target.get('id')!=4525 or target.get('kind')!=4 or target.get('present') is not True:
            raise ValueError('No observed return discovery4525')
        return_position=target.get('position')
        if not isinstance(return_position,list) or len(return_position)!=3 or any(type(v) not in (int,float) or not math.isfinite(v) for v in return_position):
            raise ValueError('Invalid observed return destination')
        if return_position[0]!=math.trunc(return_position[0]) or return_position[2]!=math.trunc(return_position[2]):
            raise ValueError('Expected source integer return XZ destination')
        if quest.get('items',{}).get('reward')!=[[592,1],[593,1],[594,1],[595,1]] or quest.get('ending_talk')!=[4105,4106] or quest.get('ending_quests')!=[9077]:
            raise ValueError('Original return reward/dialogue contract differs')
        return_observation=dict(capture=describe_source(return_capture),archive_guid=evidence['archive_guid'],input_sha256=evidence['input_sha256'],target=target)
    def cfloat(value):
        text=format(float(value),'.9g')
        if '.' not in text and 'e' not in text:text+='.0'
        return text+'f'
    header='\n'.join(['/* Generated by tools/export_adventure_hunt.py; do not edit. */','#ifndef PE_ADVENTURE_HUNT_SOURCE_H','#define PE_ADVENTURE_HUNT_SOURCE_H',
        '#define PE_HUNT_MISSION 9075','#define PE_HUNT_TARGET 4524','#define PE_HUNT_RADIUS 15.0',
        '#define PE_HUNT_REFERENCE_AVAILABLE '+('1' if observation else '0'),
        'static const PeVec3 pe_hunt_reference_destination={'+','.join(cfloat(v) for v in position)+'};',
        '#define PE_HUNT_RETURN_MISSION 9127','#define PE_HUNT_RETURN_TARGET 4525','#define PE_HUNT_RETURN_RADIUS 5.0',
        '#define PE_HUNT_RETURN_REFERENCE_AVAILABLE '+('1' if return_observation else '0'),
        'static const PeVec3 pe_hunt_return_destination={'+','.join(cfloat(v) for v in return_position)+'};',
        'static const PeIngredient pe_hunt_return_rewards[]={{592,1},{593,1},{594,1},{595,1}};','#endif',''])
    manifest=dict(schema=1,source=describe_source(database),contract=source,reference=observation,
        available=bool(observation),return_reference=return_observation,scope='Pinned original travel4524 and return4525; source reward/ending contracts, not observed delivery timing.',
        absent_reference_policy='Unavailable: zero placeholder coordinates are never accepted.',
        arrival='Strict source XZ distance below15m; sourceY>=0; no items; advances to encounter9126.',
        return_arrival='Strict source XZ distance below5m; sourceY>=0; automatic completion grants scripts before4105/4106. Native inventory capacity retry and omitted plot4/9077 are explicit adapters.')
    return {'pe_adventure_hunt_source.h':header.encode(),'pe_adventure_hunt_source.json':(json.dumps(manifest,indent=2)+'\n').encode()}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--database',type=Path,default=DEFAULT_DATABASE)
    p.add_argument('--capture',type=Path,default=DEFAULT_CAPTURE);p.add_argument('--output',type=Path,default=DEFAULT_OUTPUT)
    p.add_argument('--return-capture',type=Path,default=DEFAULT_RETURN_CAPTURE)
    p.add_argument('--check',action='store_true');a=p.parse_args()
    capture=a.capture if a.capture.is_file() else None
    if capture is None and a.capture!=DEFAULT_CAPTURE:p.error('Explicit capture is missing')
    if not a.return_capture.is_file() and a.return_capture!=DEFAULT_RETURN_CAPTURE:p.error('Explicit return capture is missing')
    try:
        for name,data in build_outputs(a.database,capture,a.return_capture if a.return_capture.is_file() else None).items():
            path=a.output/name
            if a.check:
                if not path.is_file() or path.read_bytes()!=data:raise ValueError('Stale generated '+name)
            else:a.output.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        print('Hunt export checked' if a.check else 'Hunt export generated', '(observed reference)' if capture else '(unavailable until observed reference)')
    except (OSError,ValueError,KeyError,sqlite3.Error) as e:print(e,file=sys.stderr);return 1
    return 0

if __name__=='__main__':raise SystemExit(main())
