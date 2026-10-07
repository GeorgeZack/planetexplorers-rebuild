#!/usr/bin/env python3
"""Recover typed9077..9084 events without enabling quests or launching Unity."""
import argparse
from contextlib import closing
import hashlib
import json
from pathlib import Path
import re
import sys
from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, numeric_rows
OUTPUT=ROOT/'rebuild/generated'
IDS=tuple(range(9077,9085))
# Source and row guards are populated below and deliberately audited constants.
ROW_HASHES={'mission:9077': '8e219b14ea4a2ec6c49ae6e4787bf0d23c3e84192ea9aee85489d2c6f37dc585',
 'mission:9078': '2179073474c772d0762dcebe1c3f485e5fef75e4345e426fc1cf0e71c932f405',
 'mission:9079': 'c0baae7680a6a3573ba3a6cbf9d56c2b26d2a95bc422a4f2efae01decb87ec42',
 'mission:9080': 'ca631107601a0eb7f5027ddbc32cd9ca86b0eca516def5452542e198a97fd562',
 'mission:9081': 'ef54a6f916505d1e55f4f558078c64e82beac78b38ab61e586d7c9c32f1ec8d7',
 'mission:9082': '1d0b137b1ad407c7196d427059b3b7a4dac8e0bdcb7b12fe6fb571a214c29a56',
 'mission:9083': '76af8326ff127ca352c316165113125867ba3deda0e10c277278da92d15f37fd',
 'mission:9084': 'd3e5fb9974de2f9718536c7cce965c7180848738ed829de7dcef73d4594d8cf3',
 'plot:5': '1e1338125ee55b0dd3119203e4c491477946ef59afad0d4e89d5bc31b9d8110e',
 'talk:3164': '84fe561855aef90997d06166c9079d7c71bc76775040c0367b903f919ea57558',
 'talk:4107': '25ba6bcc8aa069bb97998dae4fa3cf3f588eb7c4f60dda70dc64b0017c689282',
 'talk:4108': 'dc5a157ff6e54b65632860af733b8d0f76ebe1da0d62e868b3ede18d553a04d2',
 'talk:4109': '08e31e091892d4a242ffb9b5fe7a93ac2b06bf2d5148a3a6230712e7158b11cb',
 'talk:4110': 'b132e18e6fc414381dd062f392a4fe9482f11176d756fb37a169d2190127f0da',
 'talk:4111': 'e2b477a603e0e90d64c9debee3feef38722d054593ecebb5cde4494558fd1376',
 'talk:4112': '107581a6312510834c1c8d1e7720135b60adada8717b6edcded8dd91c32f8f35',
 'talk:4113': 'f2bf98ead7241786e0bde2895121d4c90d6684b297fdc07b0abb208a63f0448a',
 'talk:4114': 'e0304f607b515366bff8307fe55deb0b2b41737ccd57a1215f9b1611f9b627ae',
 'talk:4115': 'c2028a90051e74716b86fe1da87251309af97f76a585622edaae2742fe3bcd0e',
 'talk:4116': '09491e42a987ae90e04bfc66396e1b2f4016ca7310929c1fea780475c95a06cd',
 'talk:4117': 'b14f589170bb05fd33249012d4ec478b8c3b57b4e4945e02a7f9ac1ed2916eaa',
 'talk:4118': '6dab68043de92e9c52981e807af76a91850964132cbc7b23d308609013a434bd',
 'talk:4119': '11ccb05d3754816d6b3d765ed4be56820646ec9ed5274c11dbb6936e01a5d3c8',
 'talk:4120': 'f2ce34a9a54a41fbc39dc792934d2316e683282c785ae596db09bbd7a453b178',
 'talk:4121': '7b923f98bf1ac3b9eb0978540821b2f137a7e47691cf86ea0ecbbe790f4727fb',
 'talk:4122': '492e551e94e8782f3af618660a2b2e673e6768d99cfe055bccf6fd60fcb1f415',
 'talk:4123': 'de0b2fef91f9ceec278c57b504a28f901c340b84f0e88e02cbfc83cff20fc0ee',
 'talk:4124': '1e6d4dba306ce0f7bd2cab6cc3a001cc3c4e64fb91f2da0bc09c1772f2ee35d7',
 'target:2534': '00900338d64d988b78ac744771b9370a883e06fb917f7aa885ad173b85499a64',
 'target:4504': '3271a9870fa8cf4e862efe7e04cedf2bf5550bfffd31d1862cefd924b26b31c1',
 'target:4505': 'f64edf520bc837b69e1868e75d415604895311242733412c99ebb4170a4721c8',
 'target:4506': '9da01e1dd77602f82f5837d128ab55feee70ad9a9196c90eaed8467de886ea96',
 'target:5509': '668006699bdecaa4448cfd125b456f265f2476f8cc8da4e797b9638e9ccdf8b8',
 'target:5510': 'f325258309fa30408182599c720fc42f76fc9530819a9fc507be25e59e669058',
 'target:5511': '6d20a548a613dcdd3e73b3bf41b35008cf248e6f47982dc45b2cc03230c51442',
 'target:7507': '553637b52c595717eb28be0195d06912d094dab6b314c7c8d48f90d51718cbcc'}
METHODS=(('Assets/Scripts/Mission/PlayerMission.cs',
  'public int SetQuestVariable1(',
  '97d00512f0dd4e4c6005d24180162ba3b07aac9f63072984cfed20aec215fb7e'),
 ('Assets/Scripts/Mission/PlayerMission.cs',
  'public void CompleteMission(',
  '28cbbeef654c138ec58da1e4c587310e90fa9e62ebade72a96fd1d81b8e34cca'),
 ('Assets/Scripts/Mission/PlayerMission.cs',
  'public void CompleteTarget(',
  '94fd790792951d6f0a3b9558f3721ea7c53ac0afea0a4c37e2546c8d93542016'),
 ('Assets/Scripts/Mission/PlayerMission.cs',
  'bool ForceCompleteMission(',
  '1771330b511baf22e7cfbf12f58345ee70f79b858eb604a46694da84128b37f0'),
 ('Assets/Scripts/Mission/PlayerMission.cs',
  'public bool IsGetTakeMission(',
  '89aedcc3df7149e3dfab8e8ba05846cf7f327f107b1f23d0d7ad3d4c1f6a7b8d'),
 ('Assets/Scripts/Mission/PlayerMission.cs',
  'public void Export(BinaryWriter bw)',
  '2ee536383ec334d8ef9e7ef0c565092a93fbfe78b010d73222e8afa362d52e94'),
 ('Assets/Scripts/Mission/PlayerMission.cs',
  'public void Import(byte[] buffer)',
  '21f02107198062015bf4638c249883316000200c850cec2ef4432ada21cfbd3e'),
 ('Assets/Scripts/Mission/StroyManager.cs',
  'void UpdateAdvPlot(',
  '6f32cc0d69175d7b885a10c1e3d724efef58048caa5215a8f356504e35afd38d'),
 ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINPCTalk.cs',
  'public void MatchTalkInfo(',
  'd8ea5753f1926be8f0ae32cc60eca46232c39ed0eb55795eb5421290a88c91a4'),
 ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINPCTalk.cs',
  'protected override void OnClose()',
  '64d9a52fc45ace32ed3ebb05c7b56bd830e1983f35374aca45176365832b0fe6'),
 ('Assets/Scripts/Mission/Mission.cs',
  'public enum Story_Info',
  'b0625ebf709412d328c3b610bc3603eaa675307da20f397d4b7a5b2499cd500e'),
 ('Assets/Scripts/Mission/MissionScript/MissionManager.cs',
  'public void SetGetTakeMission(',
  'f6b3dac11dbd780412dbd2b1f1b1d78383bf0f1dbcbd66749b60173e61e186ce'),
 ('Assets/Scripts/Mission/MissionScript/MissionManager.cs',
  'public void ProcessSingleMode(',
  'e13fb7d8cfdcaba351a3345dd74c7ce22db2d1534eeea17bb7d6fe7678f79724'),
 ('Assets/Scripts/Mission/MissionScript/MissionManager.cs',
  'public enum TakeMissionType',
  '7a10a7053f52a7eb978af8cb86fed71380993aef3459c3490cfac146caf82c59'),
 ('Assets/Scripts/Mission/PlayerMission.cs',
  'public int SetQuestVariable(',
  'f91db14b1d8800f5b9e187f6e51b433aa03718721784fccdffbb73d4757df247'),
 ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINPCTalk.cs',
  'public void UpdateNpcTalkInfo(int MissionID,',
  '25f3963e7230fca8dd65f3e6dde174c7f755f1e6e4ab32d80f65651e9ec56d1c'))

def digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def body_hash(source,signature):
    if source.count(signature)!=1:raise ValueError('Ambiguous mission method '+signature)
    start=source.index('{',source.index(signature));end=start+1;depth=1
    while end<len(source) and depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
    if depth:raise ValueError('Unclosed source method')
    return hashlib.sha256(re.sub(r'\s+','',source[start:end]).encode()).hexdigest()
def source_guards(root=ROOT):
    result=[]
    for path,signature,expected in METHODS:
        actual=body_hash((root/path).read_text(encoding='utf-8-sig',errors='replace'),signature)
        if actual!=expected:raise ValueError('Changed mission event method '+signature)
        result.append(dict(path=path,method=signature,normalized_sha256=actual))
    return result

def ids(value):return [] if value=='0' else list(map(int,value.split(',')))
def items(value):
    result=[]
    if value!='0':
        for pair in value.split(','):
            item,count=map(int,pair.split('_'))
            if item<=0 or count<=0:raise ValueError('Invalid item event')
            result.append(dict(item_id=item,count=count))
    return result

def bounded(values,n):
    if len(values)>n:raise ValueError('Mission source array exceeds bound')
    return '{'+','.join(map(str,values+[0]*(n-len(values))))+'}'
def item_array(values,n):
    if len(values)>n:raise ValueError('Mission item array exceeds bound')
    values=[f'{{{r["item_id"]},{r["count"]}}}' for r in values]
    return '{'+','.join(values+['{0,0}']*(n-len(values)))+'}'
def build_outputs(database=DEFAULT_DATABASE):
    guarded=source_guards()
    with closing(connect_readonly(database)) as db:
        quest,_=numeric_rows(db,'AdRandomQuest_List','ID')
        plots,_=numeric_rows(db,'AdvPlot','ID')
        tables={name:numeric_rows(db,name,'TargetID')[0] for name in ('AdQuest_TowerDefence','AdQuest_EnterArea','AdQuest_UseItem','AdQuest_LootItem')}
        scenario,_=numeric_rows(db,'Scenario','talk_id')
        raw={str(i):dict(quest[i]) for i in IDS};plot=dict(plots[5])
        targets={}
        for row in raw.values():
            for target in ids(row['TargetIDList']):
                matches=[(name,table[target]) for name,table in tables.items() if target in table]
                if len(matches)!=1:raise ValueError('Ambiguous mission target')
                name,data=matches[0];targets[str(target)]=dict(table=name,row=dict(data))
        for key,value in [*(('mission:'+k,v) for k,v in raw.items()),('plot:5',plot),*(('target:'+k,v) for k,v in targets.items())]:
            if ROW_HASHES.get(key)!=digest(value):raise ValueError('Changed mission event row '+key)
        talks=sorted({i for row in raw.values() for field in ('player_talk','TalkOP','TalkED') for i in ids(row[field]) if i})
        for i in talks:
            if ROW_HASHES.get('talk:'+str(i))!=digest(dict(scenario[i])):raise ValueError('Changed mission dialogue role '+str(i))
        dialogue=[dict(id=i,role=int(scenario[i]['npc_id'])+9901 if -9900<=int(scenario[i]['npc_id'])<=-9800 else 0,row=dict(scenario[i])) for i in talks]
    normalized=[]
    for identity in IDS:
        r=raw[str(identity)];pre=ids(r['PreLimit'].split(':')[-1])
        if len(pre)>1 or (pre and not r['PreLimit'].startswith('1:')):raise ValueError('Unsupported prerequisite')
        mutex=ids(r['MutexLimit'].split(':')[-1]);opening=ids(r['TalkOP']);ending=ids(r['TalkED']);target_ids=ids(r['TargetIDList'])
        if len(ending)>1:raise ValueError('Unsupported ending')
        plot_events=[] if r['AdvPlot']=='0' else [list(map(int,p.split('_'))) for p in r['AdvPlot'].split(',')]
        normalized.append(dict(mission=identity,prerequisite=pre[0] if pre else 0,mutex=mutex,choices=ids(r['GuanLianList']),
            player_talk=list(map(int,r['player_talk'].split(','))),opening=opening,ending=ending[0] if ending else 0,
            targets=target_ids,any_target=any(targets[str(i)]['row'].get('ComMission')=='1' for i in target_ids),
            auto_reply=r['AutoReply']=='1',repeatable=r['MaxNum']=='-1',increase_chain=r['IncreaseChain']=='1',
            accept_plot=next((p for kind,p in plot_events if kind==1),0),complete_plot=next((p for kind,p in plot_events if kind==3),0),
            next=int(r['NextQuest']),accept_items=items(r['Get_MissionItem']),complete_remove=items(r['CoRemoveItem']),complete_items=items(r['RewardItem'])))
    lines=['/* Generated by export_adventure_mission_events.py; disconnected metadata. */','#ifndef PE_ADVENTURE_MISSION_EVENTS_SOURCE_H','#define PE_ADVENTURE_MISSION_EVENTS_SOURCE_H',
           '#include "../src/pe_adventure_mission_events.h"','static const PeAdventureMissionEventSource pe_adventure_mission_event_sources[]={']
    for r in normalized:
        parts=[str(r['mission']),str(r['prerequisite']),bounded(r['mutex'],2),bounded(r['choices'],2),bounded(r['player_talk'],2),
               bounded(r['opening'],4),str(len(r['opening'])),str(r['ending']),bounded(r['targets'],3),str(len(r['targets']))]
        parts += [str(int(r[k])) for k in ('any_target','auto_reply','repeatable','increase_chain','accept_plot','complete_plot','next')]
        parts += [str(len(r[k])) for k in ('accept_items','complete_remove','complete_items')]
        parts += [item_array(r['accept_items'],3),item_array(r['complete_remove'],1),item_array(r['complete_items'],7)]
        lines.append('    {'+','.join(parts)+'},')
    lines+=['};','#endif','']
    manifest=dict(schema=1,evidence='Source-derived disconnected event contracts; no host or save integration.',source=describe_source(database),
        source_files=[describe_source(ROOT/p) for p in dict.fromkeys(p for p,_,_ in METHODS)],guarded_methods=guarded,
        missions=normalized,raw_missions=raw,targets=targets,plots={'5':plot},dialogue_roles=dialogue,
        lifecycle=dict(acceptance='active flag then Get plot enqueue then guarded acceptance items',
                       completion='chain advance then mission state then removal/rewards then Complete plot then ending/NextQuest',
                       grant_latch='PlayerMission.m_GetRewards separately exported/imported; loads never imply fresh grants'),
        limitations=['Repeatable choice shells9081/9083 are metadata only; concrete NPC/choice/geometry readiness remains external.',
                     'Atomic effect batches are a native retry policy, not source partial transaction ordering.',
                     '9080 target ComMission=1 permits any qualified assembly target, never arbitrary B-key geometry.',
                     '9082 item effects require a caller with complete item catalog and transaction support; no catalog is extended here.',
                     'Skill-tree rewards, broader quest chains and NextQuest9114 remain outside this bounded reducer.'])
    return {'pe_adventure_mission_events_source.h':'\n'.join(lines).encode(),'pe_adventure_mission_events_source.json':(json.dumps(manifest,indent=2)+'\n').encode()}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');p.add_argument('--output',type=Path,default=OUTPUT);a=p.parse_args()
    try:
        for name,data in build_outputs().items():
            path=a.output/name
            if a.check:
                if not path.exists() or path.read_bytes()!=data:raise ValueError('Stale '+name)
            else:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    except (ValueError,KeyError,OSError) as e:print(e,file=sys.stderr);return 1
    print('Adventure mission event metadata checked' if a.check else 'Adventure mission event metadata generated');return 0
if __name__=='__main__':raise SystemExit(main())
