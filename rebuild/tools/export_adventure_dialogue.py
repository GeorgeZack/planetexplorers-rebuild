#!/usr/bin/env python3
"""Export bounded Adventure9027..9032/9071..9076 dialogue and9126/9127 mission text.

Reads only original SQLite inputs. No fixed NPC names/prototypes/placements are
introduced; generated AdvNPC/Town tokens remain for the runtime binding layer.
"""
from contextlib import closing
import argparse
import json
from pathlib import Path
import sqlite3
import sys
from export_content import (ROOT,DEFAULT_DATABASE,DEFAULT_LOCALIZATION,c_string,
    connect_readonly,describe_source,integer,numeric_rows)

DEFAULT_OUTPUT=ROOT/'rebuild/generated'
TALKS=(1949,)+tuple(range(4081,4107))
SPEAKERS={0:1,-9999:0,-9900:1,-9899:2,-9898:3}

def build_outputs(database,localization):
    with closing(connect_readonly(database)) as db:
        quests,_=numeric_rows(db,'AdRandomQuest_List','ID')
        scenarios,_=numeric_rows(db,'Scenario','talk_id')
        targets,_=numeric_rows(db,'AdQuest_EnterArea','TargetID')
        chains,_=numeric_rows(db,'AdQuestChain','QC_ID')
        collect,_=numeric_rows(db,'AdQuest_LootItem','TargetID')
        monsters,_=numeric_rows(db,'AdQuest_KillMonster','TargetID')
        translations,_=numeric_rows(db,'Translation','ID',allow_duplicates=True)
        english={i:r['English'] for i,r in translations.items()}
    if localization is not None:
        with closing(connect_readonly(localization)) as db:
            rows,_=numeric_rows(db,'Translation','ID',allow_duplicates=True)
            english.update({i:r['English'] for i,r in rows.items() if r['English']})
    def text(identity):
        value=english.get(identity)
        if not isinstance(value,str) or not value or '\0' in value or len(value.encode())>1800:
            raise ValueError(f'missing/invalid English {identity}')
        return value
    if (quests[9031]['TalkOP']!='4088' or quests[9032]['TalkOP']!='4090,4091,4092,4093,4094'
            or quests[9032]['player_talk']!='4089,0' or quests[9032]['PreLimit']!='1:9031'
            or chains[31]['PreLimit']!='9031' or chains[31]['QusetGroup1']!='9032,100'
            or targets[4502]['DistPos']!='3_2_0_5' or targets[4502]['NotForDungeon']!='1'):
        raise ValueError('Adventure dialogue/arrival source contract changed')
    for mission,opening,ending in ((9027,1949,4081),(9028,4082,4083),(9029,4084,4085),(9030,4086,4087)):
        row=quests[mission]
        if row['TalkOP']!=str(opening) or row['TalkED']!=str(ending) or row['NextQuest']!=str(mission+1) or row['AutoReply']!='1' or row['TargetIDList']!='0':
            raise ValueError('Adventure welcome chain changed')
    if (quests[9071]['TalkOP']!='4096' or quests[9071]['player_talk']!='4095,0' or quests[9071]['PreLimit']!='1:9032' or quests[9071]['TargetIDList']!='0' or quests[9032]['AutoReply']!='1' or quests[9032]['RewardItem']!='0' or quests[9032]['NextQuest']!='0'):
        raise ValueError('Annot completion/continuation changed')
    if (quests[9071]['GuanLianList']!='9072,9073' or quests[9072]['player_talk']!='4097,4099'
            or quests[9072]['TalkOP']!='0' or quests[9072]['TalkED']!='4100'
            or quests[9072]['TargetIDList']!='2531' or quests[9073]['player_talk']!='4098,0'
            or quests[9073]['MaxNum']!='-1' or collect[2531]['ItemID']!='917' or collect[2531]['ItemNum']!='5'):
        raise ValueError('Medicine choices/delivery changed')
    if (quests[9074]['PreLimit']!='1:9072' or quests[9074]['player_talk']!='4101,0'
            or quests[9074]['TalkOP']!='4102' or quests[9074]['GuanLianList']!='9075,9076'
            or quests[9075]['player_talk']!='4103,0' or quests[9075]['TalkOP']!='0'
            or quests[9075]['TargetIDList']!='4524' or quests[9075]['AutoReply']!='1'
            or quests[9075]['NextQuest']!='9126' or quests[9075]['IncreaseChain']!='1'
            or quests[9075]['Get_MissionItem']!='0' or quests[9075]['RewardItem']!='0'
            or quests[9076]['player_talk']!='4104,0' or quests[9076]['MaxNum']!='-1'
            or targets[4524]['DistPos']!='1_0_130_15' or targets[4524]['NotForDungeon']!='1'
            or quests[9126]['TargetIDList']!='1518' or quests[9126]['NextQuest']!='9127'
            or quests[9127]['TargetIDList']!='4525' or targets[4525]['DistPos']!='3_3_0_5'
            or quests[9127]['TalkED']!='4105,4106' or quests[9127]['NextQuest']!='9077'
            or quests[9127]['TalkOP']!='0' or quests[9127]['AutoReply']!='1'
            or quests[9077]['PreLimit']!='1:9127'
            or monsters[1518]['MonsterID']!='13_4'
            or monsters[1518]['PathIDtemp']!='0_13_6'):
        raise ValueError('Post-medicine hunt contract changed')
    lines=[]
    for talk in TALKS:
        row=scenarios[talk];npc=int(row['npc_id'])
        expected=0 if talk==1949 else (-9999 if talk%2 else (-9900 if talk<=4088 else (-9899 if talk<=4094 else -9898)))
        if talk in (4097,4098,4099,4103,4104):expected=-9999
        if talk==4100:expected=-9898
        if npc!=expected:raise ValueError(f'unexpected speaker for {talk}')
        parts=str(row['talk_content']).split(':')
        if len(parts)!=3:raise ValueError(f'unsupported dialogue layout {talk}')
        identity=integer(parts[0],'localization',1)
        lines.append(dict(talk_id=talk,localization_id=identity,npc_id=npc,speaker=SPEAKERS[npc],text=text(identity)))
    missions=[]
    for identity,target in ((9031,4502),(9032,4503),(9075,4524),(9127,4525)):
        row=quests[identity]
        if row['TargetIDList']!=str(target):raise ValueError('Adventure target changed')
        missions.append(dict(id=identity,title=text(integer(row['MissionName'],'title',1)),
            description=text(integer(row['Description'],'description',1)),
            objective=text(integer(targets[target]['Desc'],'objective',1)),target_id=target))
    row=quests[9072]
    missions.append(dict(id=9072,title=text(integer(row['MissionName'],'title',1)),
        description=text(integer(row['Description'],'description',1)),
        objective=text(integer(collect[2531]['Desc'],'objective',1)),target_id=2531))
    row=quests[9126]
    missions.append(dict(id=9126,title=text(integer(row['MissionName'],'title',1)),
        description=text(integer(row['Description'],'description',1)),
        objective=text(integer(monsters[1518]['Desc'],'objective',1)),target_id=1518))
    header=['/* Generated by tools/export_adventure_dialogue.py; do not edit. */',
        '#ifndef PE_ADVENTURE_DIALOGUE_TEXT_H','#define PE_ADVENTURE_DIALOGUE_TEXT_H',
        '#include "pe_adventure_dialogue.h"','#define PE_ADVENTURE_ARRIVAL_RADIUS 5.0',
        'static const PeAdventureDialogueLine pe_adventure_lines[]={']
    for row in lines:
        header.append('    {%d,%d,%d,%s},'%(row['talk_id'],row['localization_id'],row['speaker'],c_string(row['text'])))
    header+=['};','typedef struct {int id;const char *title,*description,*objective;} PeAdventureMissionText;',
        'static const PeAdventureMissionText pe_adventure_missions[]={']
    for row in missions:header.append('    {%d,%s,%s,%s},'%(row['id'],c_string(row['title']),c_string(row['description']),c_string(row['objective'])))
    header+=['};','#endif','']
    manifest=dict(schema=1,source=describe_source(database),localization=describe_source(localization) if localization else None,
        scope='Transient source menu/pages; generated role bindings; existing PeGame owns grants.',
        arrival_target=dict(id=4502,role=2,radius=5,strict=True,axes='XZ',truncate_target_to_integer=True,not_for_dungeon=True),
        acceptance='UINPCTalk.MatchTalkInfo attaches trigger only to final page; OnClose dispatches TakeMissionType_Get.',
        lines=lines,missions=missions)
    return {'pe_adventure_dialogue_text.h':'\n'.join(header).encode(),
        'pe_adventure_dialogue_text.json':(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--database',type=Path,default=DEFAULT_DATABASE)
    parser.add_argument('--localization',type=Path,default=DEFAULT_LOCALIZATION)
    parser.add_argument('--embedded-only',action='store_true')
    parser.add_argument('--output',type=Path,default=DEFAULT_OUTPUT)
    parser.add_argument('--check',action='store_true');args=parser.parse_args()
    try:
        outputs=build_outputs(args.database,None if args.embedded_only else args.localization)
        if args.check:
            for name,data in outputs.items():
                if not (args.output/name).is_file() or (args.output/name).read_bytes()!=data:raise ValueError('stale generated '+name)
        else:
            args.output.mkdir(parents=True,exist_ok=True)
            for name,data in outputs.items():(args.output/name).write_bytes(data)
        print('Adventure dialogue checked' if args.check else 'Adventure dialogue generated');return 0
    except (OSError,ValueError,KeyError,sqlite3.Error) as error:
        print(error,file=sys.stderr);return 1

if __name__=='__main__':raise SystemExit(main())
