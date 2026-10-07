#!/usr/bin/env python3
"""Export source9077 metadata only; never synthesize beacon positions or cohorts."""
import argparse
from contextlib import closing
import json
import hashlib
import re
from pathlib import Path
import sqlite3
import sys
from export_content import ROOT, DEFAULT_DATABASE, DEFAULT_LOCALIZATION, connect_readonly, describe_source, numeric_rows, c_string

DEFAULT_OUTPUT = ROOT/'rebuild/generated'
SOURCE_FILES = (
    'Assets/Scripts/Scene/SceneEntityMissionPointTowerDefense.cs',
    'Assets/Scripts/Scene/EntityMonsterBeacon.cs',
    'Assets/Scripts/Scene/SceneEntityPosAgent.cs',
    'Assets/Scripts/Scene/MonsterEntityCreator.cs',
    'Assets/Scripts/AiScripts/AiSpawn/SPClass.cs',
    'Assets/Scripts/AiScripts/AiHelper/AiUtil.cs',
    'Assets/Scripts/Mission/MissionTowerDefense.cs',
    'Assets/Scripts/Mission/PlayerMission.cs',
    'Assets/Scripts/Mission/AdRandomMission.cs',
    'Assets/Scripts/Mission/MissionScript/MissionManager.cs',
    'Assets/Scripts/PuJi/Town/Tool/VArtifactUtil.cs',
)
WAVE_CONTRACT = '0_501_8_10_0_360,503_2_3_0_360;20_510_8_10_0_360,503_2_3_0_360'


# Whitespace-normalized method guards pin the active branch, including draw
# order and deferred creation. A changed body requires a renewed source audit.
SPAWN_METHODS = (
    ('Assets/Scripts/Scene/EntityMonsterBeacon.cs', 'public static EntityMonsterBeacon CreateMonsterBeaconByTDID(', '1c8cada20bb1f2ca53a0714873e67efadec26a6cb058d79abc50a8b0ced49029'),
    ('Assets/Scripts/Scene/EntityMonsterBeacon.cs', 'IEnumerator RefreshTowerMission()', '53e291f5913d7a1d30918c59b7dd17438b72203e2eae82790200b0a8871ea259'),
    ('Assets/Scripts/Scene/EntityMonsterBeacon.cs', 'public virtual void OnMonsterCreated(PeEntity e)', 'f019d61a9250cd729b63df9c8f1b7a850b6bc1e55a02f5331e9872a4be256cb6'),
    ('Assets/Scripts/AiScripts/AiHelper/AiUtil.cs', 'public static Vector3 GetRandomPosition(Vector3 center, float minRadius, float maxRadius, Vector3 direction, float minAngle, float maxAngle)', 'eeab7dc61ad6e4efae7d3bdbccfb2b4b2f253c934164699754b9e30035a9c162'),
    ('Assets/Scripts/Scene/SceneEntityMissionPointTowerDefense.cs', 'void OnOneDeath()', '207eda8c028b17b4cbf22ff35558e614651461ccbc385799dd484e33768ae6ba'),
    ('Assets/Scripts/Scene/SceneEntityPosAgent.cs', 'IEnumerator TryCreateEntityUnstatic()', '1d63249b872e96aee0b84311a8462bc2ae9ab419b5ac11fcb00c657c4234bb46'),
)


def validate_spawn_sources(root=ROOT):
    methods=[]
    for path,signature,expected in SPAWN_METHODS:
        source=(root/path).read_text(encoding='utf-8-sig')
        if source.count(signature)!=1:raise ValueError('Ambiguous source method '+signature)
        start=source.index('{',source.index(signature));end=start+1;depth=1
        while end<len(source) and depth:
            depth+=(source[end]=='{')-(source[end]=='}');end+=1
        if depth:raise ValueError('Unclosed source method '+signature)
        digest=hashlib.sha256(re.sub(r'\s+','',source[start:end]).encode()).hexdigest()
        if digest!=expected:raise ValueError('Defense planner source changed: '+signature)
        methods.append(dict(path=path,method=signature,normalized_sha256=digest))
    beacon=re.sub(r'\s+','',(root/'Assets/Scripts/Scene/EntityMonsterBeacon.cs').read_text(encoding='utf-8-sig'))
    agent=re.sub(r'\s+','',(root/'Assets/Scripts/Scene/SceneEntityPosAgent.cs').read_text(encoding='utf-8-sig'))
    for expected in ('constintMonsterBeaconPlayerID=8;','constintMonsterBeaconCampID=26;',
                     'go.transform.rotation=Quaternion.identity;','bcn._forward=go.transform.forward;'):
        if expected not in beacon:raise ValueError('Defense permission/orientation constant changed')
    if 'publicconstfloatPosYTBD=0.0f;' not in agent:raise ValueError('Deferred Y constant changed')
    return methods


def expect(row, values, label):
    for key, value in values.items():
        if row[key] != str(value):
            raise ValueError(f'{label} contract changed: {key}')


def parse_waves(value):
    waves=[]
    for index, part in enumerate(value.split(';')):
        fields=part.replace(',', '_').split('_')
        if len(fields)<6 or (len(fields)-1)%5:
            raise ValueError('Invalid wave descriptor arity')
        nums=list(map(int,fields));delay=nums[0]
        if delay<0:raise ValueError('Negative wave delay')
        groups=[]
        for start in range(1,len(nums),5):
            sp,low,high,lo_angle,hi_angle=nums[start:start+5]
            if sp<500 or low<1 or high<=low or not 0<=lo_angle<=hi_angle<=360:
                raise ValueError('Invalid bounded wave group')
            groups.append(dict(spawn_type=sp,minimum_count=low,maximum_count_exclusive=high,
                               angle_min_degrees=lo_angle,angle_max_degrees=hi_angle))
        waves.append(dict(index=index,delay_seconds=delay,groups=groups))
    return waves


def build_outputs(database=DEFAULT_DATABASE, localization=DEFAULT_LOCALIZATION):
    guarded_methods=validate_spawn_sources()
    with closing(connect_readonly(database)) as db:
        quests,_=numeric_rows(db,'AdRandomQuest_List','ID')
        targets,_=numeric_rows(db,'AdQuest_TowerDefence','TargetID')
        discovery,_=numeric_rows(db,'AdQuest_EnterArea','TargetID')
        scenario,_=numeric_rows(db,'Scenario','talk_id')
        monsters,_=numeric_rows(db,'PrototypeMonster','id')
        chains,_=numeric_rows(db,'AdQuestChain','QC_ID')
        translations,_=numeric_rows(db,'Translation','ID',allow_duplicates=True)
        english={i:r['English'] for i,r in translations.items()}
        # dif_coef/sp_type repeat generally. Demand one exact scripted match.
        besiege=[dict(r) for r in db.execute("SELECT * FROM MonsterBesiege WHERE dif_coef='520'")]
        if len(besiege)!=1:raise ValueError('Expected one scripted MonsterBesiege520')
        wave_row=besiege[0]
        spawn={}
        for identity in (501,503,510):
            matches=[dict(r) for r in db.execute('SELECT * FROM MonsterBesiegeSpawn WHERE sp_type=?',(str(identity),))]
            if len(matches)!=1:raise ValueError('Ambiguous scripted spawn type')
            spawn[identity]=matches[0]
        expect(quests[9077],dict(TargetIDList=7507,PreLimit='1:9127',TalkOP='4107,4108,4109,4110',
                               AutoReply=1,RewardItem=0,TalkED=0,NextQuest=0,bGiveUp=0),'Quest9077')
        expect(targets[7507],dict(Time=30,Pos='5_4',NpcList=0,ObjectList=0,Count=15,td_id=520,ReceiveQuest=0),'Target7507')
        expect(wave_row,dict(spawn_type=-1,weights=1,cd_time=10,start_time=30,remaining_time=-1,sp_type=WAVE_CONTRACT),'MonsterBesiege520')
        for sp,prototype in ((501,13),(503,4),(510,7)):
            expect(spawn[sp],dict(spawn_type=-1,area_type_R=-1,area_type_S=-1,dif_lv=-1,
                                 land=f'0_{prototype}',water=f'0_{prototype}',hole=f'0_{prototype}',sky=f'0_{prototype}'),f'Spawn{sp}')
        expect(quests[9078],dict(PreLimit='1:9077',TargetIDList=4504,player_talk='4111,0',TalkOP=4112,NextQuest=0),'Followup9078')
        expect(quests[9079],dict(PreLimit='1:9078',TargetIDList=4505,player_talk='4113,0',TalkOP='4114,4115',AdvPlot='1_5',NextQuest=0),'Followup9079')
        expect(discovery[4504],dict(DistPos='3_2_0_5',NotForDungeon=1),'Discovery4504')
        expect(discovery[4505],dict(DistPos='3_5_0_5',NotForDungeon=1),'Discovery4505')
        expect(chains[32],dict(QusetGroup3='9078,100'),'Chain32')
        expect(chains[31],dict(QusetGroup2='9079,100'),'Chain31')
        raw=dict(missions={str(i):dict(quests[i]) for i in (9077,9078,9079)},
                 target=dict(targets[7507]),waves=wave_row,spawn_types=spawn,
                 followup_targets={str(i):dict(discovery[i]) for i in (4504,4505)},
                 chains={str(i):dict(chains[i]) for i in (31,32,33,40)})
        species=[dict(monsters[i]) for i in (13,4,7)]
        talks=[dict(scenario[i]) for i in range(4107,4116)]
    if localization is not None:
        with closing(connect_readonly(localization)) as db:
            translations,_=numeric_rows(db,'Translation','ID',allow_duplicates=True)
            english.update({i:r['English'] for i,r in translations.items() if r['English']})
    def text(identity):
        value=english.get(identity)
        if not isinstance(value,str) or not value or '\0' in value or len(value)>1800:
            raise ValueError('Missing bounded English text')
        return value
    dialogue=[]
    for row in talks:
        identity=int(row['talk_id']);speaker=int(row['npc_id'])
        expected={4107:-9897,4108:-9898,4109:-9897,4110:-9898,4111:-9999,4112:-9898,4113:-9999,4114:-9899,4115:-9899}[identity]
        if speaker!=expected:raise ValueError('Changed dialogue role binding')
        localization_id=int(row['talk_content'].split(':')[0])
        dialogue.append(dict(id=identity,npc_token=speaker,role=0 if speaker==-9999 else speaker+9901,
                             localization_id=localization_id,text=text(localization_id)))
    waves=parse_waves(wave_row['sp_type'])
    proto={501:13,503:4,510:7}
    for wave in waves:
        for group in wave['groups']:group['prototype']=proto[group['spawn_type']]
    total_min=sum(g['minimum_count'] for w in waves for g in w['groups'])
    total_max=sum(g['maximum_count_exclusive']-1 for w in waves for g in w['groups'])
    mission_text=[dict(id=i,title=text(int(raw['missions'][str(i)]['MissionName'])),
                       description=text(int(raw['missions'][str(i)]['Description']))) for i in (9077,9078,9079)]
    lines=['/* Generated by export_adventure_town_defense.py; metadata only. */',
           '#ifndef PE_ADVENTURE_TOWN_DEFENSE_SOURCE_H','#define PE_ADVENTURE_TOWN_DEFENSE_SOURCE_H',
           '#define PE_TOWN_DEFENSE_MISSION 9077','#define PE_TOWN_DEFENSE_TARGET 7507',
           '#define PE_TOWN_DEFENSE_KILL_GOAL 15','#define PE_TOWN_DEFENSE_BEACON_TOWN_ID 4',
           '#define PE_TOWN_DEFENSE_INITIAL_SECONDS 30','#define PE_TOWN_DEFENSE_COOLDOWN_SECONDS 10',
           '#define PE_TOWN_DEFENSE_COHORT_AVAILABLE 0',
           '#define PE_TOWN_DEFENSE_RADIUS_MIN 20.0f','#define PE_TOWN_DEFENSE_RADIUS_MAX 80.0f',
           '#define PE_TOWN_DEFENSE_DEFERRED_Y 0.0f',
           '#define PE_TOWN_DEFENSE_PLAYER_ID 8','#define PE_TOWN_DEFENSE_CAMP_ID 26',
           'typedef struct {int wave,delay_seconds,spawn_type,prototype,count_min,count_max_exclusive,angle_min,angle_max;} PeTownDefenseSourceGroup;',
           'static const PeTownDefenseSourceGroup pe_town_defense_source_groups[]={']
    for w in waves:
        for g in w['groups']:
            lines.append('    {%d,%d,%d,%d,%d,%d,%d,%d},'%(w['index'],w['delay_seconds'],g['spawn_type'],g['prototype'],g['minimum_count'],g['maximum_count_exclusive'],g['angle_min_degrees'],g['angle_max_degrees']))
    lines+=['};','typedef struct {int talk,role;const char *text;} PeTownDefenseSourceTalk;',
            'static const PeTownDefenseSourceTalk pe_town_defense_source_talks[]={']
    for row in dialogue:lines.append('    {%d,%d,%s},'%(row['id'],row['role'],c_string(row['text'])))
    lines+=['};','#endif','']
    manifest=dict(schema=1,source=describe_source(database),localization=describe_source(localization) if localization else None,
                  source_files=[describe_source(ROOT/p) for p in SOURCE_FILES],guarded_spawn_methods=guarded_methods,
                  evidence='Source-derived metadata; no live beacon/cohort capture.',
                  planner_rule=dict(draw_order='group count then each angle then uniform radius',integer_maximum='exclusive',
                                    real_endpoints='inclusive',positive_y_rotation='forward +Z rotates toward +X',
                                    deferred_y_value=0,can_ride=False,created_permissions=dict(default_player_id=8,camp_id=26),
                                    credit='actual beacon-owned member deaths regardless of killer',
                                    native_adapters=['Atomic whole-wave admission, stable per-beacon agent IDs and death deduplication.',
                                                     'Pending anchors retained on failed support; no source retry clock is ported.',
                                                     'Completion latches at15; survivor cleanup and mission completion remain host responsibilities.']),
                  contract=raw,mission_text=mission_text,dialogue=dialogue,waves=waves,species=species,
                  kill_goal=15,total_possible_spawns=dict(minimum=total_min,maximum=total_max),
                  reference=dict(available=False,beacon_position=None,agents=None),
                  position_rule=dict(kind='town_trans_pos',town_id=4,fallback='current player position if GetTownPos fails',
                                     spawn_radius_min=20,spawn_radius_max=80,deferred_y=True),
                  limitations=['Counts are source spawning ranges and quest goals, never agent/entity IDs.',
                               'MonsterSiege_SpawnId and AdQuest_TD_Info belong to other/legacy paths; their row520 is not this beacon schedule.',
                               'No random anchors, scales or HP are synthesized; prototypes4 and7 need assets, collision, attacks and AI recovery.',
                               'No active9077 reload/completion evidence is supplied by this exporter.'])
    return {'pe_adventure_town_defense_source.h':'\n'.join(lines).encode(),
            'pe_adventure_town_defense_source.json':(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--database',type=Path,default=DEFAULT_DATABASE)
    p.add_argument('--localization',type=Path,default=DEFAULT_LOCALIZATION);p.add_argument('--output',type=Path,default=DEFAULT_OUTPUT)
    p.add_argument('--check',action='store_true');a=p.parse_args()
    try:
        for name,data in build_outputs(a.database,a.localization).items():
            path=a.output/name
            if a.check:
                if not path.is_file() or path.read_bytes()!=data:raise ValueError('Stale '+name)
            else:a.output.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    except (ValueError,KeyError,OSError,sqlite3.Error) as e:print(e,file=sys.stderr);return 1
    print('Town defense source metadata checked' if a.check else 'Town defense source metadata generated; no cohort available')
    return 0


if __name__=='__main__':raise SystemExit(main())
