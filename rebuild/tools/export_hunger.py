#!/usr/bin/env python3
"""Export the player's source hunger drain, starvation condition and food gains without launching Unity."""
import argparse
from contextlib import closing
import json
from pathlib import Path
import re
import sqlite3
import sys

from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, numeric_rows
from export_starter_outfit import body, uncomment, expect
import export_passive_recovery as shared

ABNORMAL = 'Assets/Scripts/PeEntity/Cmpt/AbnormalCondition/'
ADDITIONAL_SOURCES = (
    ABNORMAL + 'PEAbnormalCondition.cs',
    ABNORMAL + 'PEAbnormalTrigger.cs',
    ABNORMAL + 'PEAbnormalHit.cs',
    ABNORMAL + 'PEAbnormalEff.cs',
    'Assets/Scripts/PeEntity/Cmpt/AbnormalConditionCmpt.cs',
    'Assets/Scripts/SkillSystem/SkAttribs.cs',
    'Assets/Scripts/SkillSystem/SkAttribsModifier.cs',
)
SOURCE_FILES = shared.SOURCE_FILES + tuple(p for p in ADDITIONAL_SOURCES if p not in shared.SOURCE_FILES)
DRAIN_BUFF = 30200050
DRAIN_EXPRESSION = 'exp,1,target.raws[16]+=target.sums[18]*target.sums[17]*5'
STARVATION_ID = 8
STARVATION_BUFF = 30200113
STARVATION_MODS = 'exp,4,target.buffPreAdd+=1;#exp,5,target.buffPreAdd-=10;#exp,6,target.buffPreAdd+=2;#exp,9,target.buffPreAdd+=1;#exp,13,target.buffPreAdd+=1'
POISON_TRIGGER = 489
POISON_BUFF = 30200105
# The foods the rebuild's catalog already carries (content_manifest/pe_inventory_source).
# Pinned (hunger gain, nutrient attribute, nutrient gain) per food.
FOODS = {384: (30, 47, 2), 385: (60, 47, 4), 386: (80, 46, 4), 387: (20, 48, 4), 388: (30, 48, 6),
         389: (60, 46, 3), 390: (80, 46, 4), 391: (20, 49, 2), 392: (40, 49, 4), 394: (30, 49, 3)}
FOOD_TRIGGER = re.compile(r'exp,16,target\.raws\[16\]\+=([0-9]+);#exp,(4[5-9]),target\.raws\[\2\]\+=([0-9]+);'
                          r'#exp,\2,if\(target\.raws\[\2\]>100\)\{target\.raws\[\2\]=100;\}')
NUTRIENTS = {45: 'Food_Cereals', 46: 'Food_Meat', 47: 'Food_Bean', 48: 'Food_Fruit', 49: 'Food_Energy'}


def validate_sources(root):
    # The drain shares the passive-HP initial-buff lifecycle and its guards.
    shared.validate_sources(root)
    s = {p: uncomment((root / p).read_text(encoding='utf-8-sig', errors='replace')) for p in SOURCE_FILES}
    enum = body(s['Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs'], r'public\s+enum\s+AttribType')
    if not enum.startswith('HpMax=0,Hp,StaminaMax,Stamina,StaminaReducePercent,StaminaRecoverSpeed,StaminaRecoverDelay,'
                           'ComfortMax,Comfort,ComfortReducePercent,ComfortRecoverSpeed,OxygenMax,Oxygen,OxygenReducePercent,'
                           'OxygenRecoverSpeed,HungerMax,Hunger,HungerReducePercent,HungerRecoverSpeed,'):
        raise ValueError('Hunger attribute indices changed')
    if 'Clean,CleanRecover,Food_Cereals,Food_Meat,Food_Bean,Food_Fruit,Food_Energy,' not in enum:
        raise ValueError('Nutrient attribute indices changed')
    if 'm_AttribPairs[(int)AttribType.Hunger]=m_AttribPairs[(int)AttribType.HungerMax]=newAttribPair(AttribType.Hunger,AttribType.HungerMax);' \
            not in body(s['Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs'], r'void\s+InitPair\s*\('):
        raise ValueError('Hunger current/max pairing changed')
    condition = s[ADDITIONAL_SOURCES[0]]
    shared.exact(body(condition.split('public class HitAttr', 1)[1], r'public\s+float\s+GetRate\s*\(\s*float'),
        'if(val<minThreshold||val>maxThreshold)return 0;return Mathf.Lerp(minRate,maxRate,(val-minThreshold)/(maxThreshold-minThreshold));',
        'Abnormal attribute rate')
    n = condition.split('public class PEAbnormal_N', 1)[1]
    shared.exact(body(n, r'public\s+void\s+Update\s*\('), '''
        if(m_bReqApplyEff){m_bReqApplyEff=false;ApplyEffect();}
        if(!hasEffect&&m_Data.updateByModel&&!m_Entity.biologyViewCmpt.hasView)return;
        if(hasEffect){bool triggerHit=m_RemoveTriggers.Count>0;for(int i=0;i<m_RemoveTriggers.Count;++i)triggerHit=triggerHit&&m_RemoveTriggers[i].Hit();
        if(triggerHit)CheckRemove();for(int i=0;i<m_RemoveHits.Count;++i)m_RemoveHits[i].Update();for(int i=0;i<m_Effs.Count;++i)m_Effs[i].Update();}
        else{bool triggerHit=m_Triggers.Count>0;for(int i=0;i<m_Triggers.Count;++i)triggerHit=triggerHit&&m_Triggers[i].Hit();
        if(triggerHit&&!m_AbnormalCmpt.Entity.IsDeath())CheckHit();for(int i=0;i<m_HitRates.Count;++i)m_HitRates[i].Update();}
        for(int i=0;i<m_Triggers.Count;++i)m_Triggers[i].Update();for(int i=0;i<m_RemoveTriggers.Count;++i)m_RemoveTriggers[i].Update();
    ''', 'Abnormal check-then-accumulate update')
    shared.exact(body(n, r'void\s+CheckHit\s*\('), '''
        float hitRate=1f;for(int i=0;i<m_HitRates.Count;++i){m_HitRates[i].preHit=hitRate>PETools.PEMath.Epsilon;hitRate*=m_HitRates[i].HitRate();}
        if(hitRate>PETools.PEMath.Epsilon&&UnityEngine.Random.value<hitRate){ApplyEffect();}''', 'Abnormal start roll')
    shared.exact(body(n, r'void\s+CheckRemove\s*\('), '''
        float hitRate=1f;for(int i=0;i<m_RemoveHits.Count;++i){m_RemoveHits[i].preHit=hitRate>PETools.PEMath.Epsilon;hitRate*=m_RemoveHits[i].HitRate();}
        if(hitRate>PETools.PEMath.Epsilon&&UnityEngine.Random.value<hitRate)ApplyEndEffect();''', 'Abnormal end roll')
    timer = s[ADDITIONAL_SOURCES[1]].split('class PEAT_Time', 1)[1]
    shared.exact(body(timer, r'public\s+override\s+bool\s+Hit'), 'if(m_ElapseTime>=interval){m_ElapseTime=0;return true;}return base.Hit();',
        'Abnormal interval trigger')
    shared.exact(body(timer, r'public\s+override\s+void\s+Update'), 'm_ElapseTime+=Time.deltaTime;', 'Abnormal frame-time accumulation')
    shared.exact(body(s[ADDITIONAL_SOURCES[2]].split('class PEAH_Attr', 1)[1], r'public\s+override\s+float\s+HitRate'),
        'float retRate=1f;for(int i=0;i<attrs.Length;++i)retRate*=attrs[i].GetRate(entity);return retRate;', 'Abnormal attribute hit')
    effect = s[ADDITIONAL_SOURCES[3]].split('class PEAE_Buff', 1)[1]
    for fragment in ('if(addBuff)SkEntity.MountBuff(entity,buffList[i],newList<int>(),newList<float>());',
                     'if(addBuff)entity.CancelBuffById(buffList[i]);'):
        if fragment not in shared.compact(effect):
            raise ValueError('Abnormal buff mount/cancel changed')
    shared.exact(body(s[ADDITIONAL_SOURCES[4]], r'public\s+override\s+void\s+OnUpdate'), '''
        if(null==m_AbnormalList)return;PEAbnormal_N abnormal;for(int i=0;i<m_AbnormalList.Length;i++){abnormal=m_AbnormalList[i];if(null!=abnormal)abnormal.Update();}''',
        'Per-frame abnormal update')
    if '_sums[i]=buffMul*(_raws[i]+buffPreAdd)+buffPostAdd;' not in shared.compact(s[ADDITIONAL_SOURCES[5]]):
        raise ValueError('Attribute summation changed')
    shared.exact(body(s[ADDITIONAL_SOURCES[6]].split('class SkAttribsOpMAD', 1)[1], r'public\s+void\s+Exec'),
        'float var=dstAttribs.raws[_idx];var=var*_mul+_add;dstAttribs.raws[_idx]=var;', 'MAD modifier')
    return [describe_source(root / p) for p in SOURCE_FILES]


def rows(db, table):
    return [dict(r) for r in db.execute(f'SELECT * FROM "{table}" ORDER BY rowid')]


def build_outputs(database=DEFAULT_DATABASE, source_root=ROOT):
    provenance = validate_sources(source_root)
    with closing(connect_readonly(database)) as db:
        players, _ = numeric_rows(db, 'initProp', 'id'); buffs, _ = numeric_rows(db, 'skBuff', '_id')
        first = db.execute('SELECT id FROM initProp ORDER BY rowid LIMIT 1').fetchone()
        if first is None or first['id'] != '1':
            raise ValueError('First player initProp row changed')
        player = dict(players[1])
        expect(player, {'InitBuffList': shared.INITIAL_BUFFS, 'HungerMax': 500, 'Hunger': 300,
            'HungerReducePercent': 1, 'HungerRecoverSpeed': '-.0694'}, 'Player hunger')
        drain = dict(buffs[DRAIN_BUFF])
        expect(drain, {'_type': 65507, '_priority': 0, '_stackLimit': 1, '_timeActive': -1, '_timeInterval': 5,
            '_eff': 0, '_effBeg': 0, '_effEnd': 0, '_mods': DRAIN_EXPRESSION}, 'Hunger drain buff')
        starve_buff = dict(buffs[STARVATION_BUFF])
        expect(starve_buff, {'_type': 40701, '_stackLimit': 1, '_timeActive': -1, '_timeInterval': 0,
            '_mods': STARVATION_MODS}, 'Starvation buff')
        poison = dict(buffs[POISON_BUFF])
        expect(poison, {'_type': 30201, '_stackLimit': 1, '_timeActive': 20, '_timeInterval': 1,
            '_mods': 'exp,1,if(target.raws[1]>3){target.raws[1]-=3;}'}, 'Food poisoning buff')
        abnormal = [r for r in rows(db, 'AbnormalType') if r['AbnormalId'] == str(STARVATION_ID)]
        if len(abnormal) != 1:
            raise ValueError('Starvation abnormal row changed')
        abnormal = abnormal[0]
        expect(abnormal, {'AbnormalTarget': 1, 'IsDeathRemove': 0, 'UpdateByModel': 1, 'Trigger_Time': 1,
            'Trigger_BuffAdd': 0, 'Trigger_ItemGet': 0, 'Trigger_Damage': 0, 'Trigger_IntoWater': 0,
            'Hit_MutexAbnormal': 0, 'Hit_PreAbnormal': 0, 'Hit_BuffList': 0, 'Hit_Attr': '16,0,50,1,1',
            'Hit_Damage': 0, 'Hit_Time': 0, 'Hit_AreaTime': 0, 'Hit_RainTime': 0, 'Hit_Rate': 0,
            'Eff_BuffAdd': STARVATION_BUFF, 'Eff_RemoveAbnormal': 0, 'Eff_Anim': 0, 'Eff_Camera': 0,
            'RT_Imm': 0, 'RT_Time': '.1', 'RT_BuffRemove': 0, 'RT_EffEnd': 0, 'RT_OutWater': 0,
            'RH_BuffRemove': 0, 'RH_Attr': '16,51,1000,1,1', 'RE_BuffRemove': 0, 'RE_BuffAdd': 0, 'RE_AbnormalAdd': 0}, 'Starvation')
        triggers = {r['_id']: r for r in rows(db, 'skTriggerEvent')}
        expect(triggers[str(POISON_TRIGGER)], {'_cond': 'True', '_modsTarget': f'rnd,1,1,{POISON_BUFF},0.01'}, 'Food poisoning trigger')
        skills = {r['_id']: r for r in rows(db, 'skMain')}
        items = {r['id']: r for r in rows(db, 'PrototypeItem')}
        foods = []
        for item_id in FOODS:
            item = items[str(item_id)]; skill = skills[item['skillId']]
            expect(skill, {'_coolingTime': 4, '_coolingTimeType': 12, '_coolingTimeShared': 4, '_pretimeOfMain': 0,
                '_timeOfMain': 0, '_postimeOfMain': 0, '_pretimeOfPrepare': 0, '_postimeOfPrepare': 0}, f'Food skill {item_id}')
            events = skill['_triggerEvents'].split(',')
            if len(events) != 2 or events[1] != str(POISON_TRIGGER):
                raise ValueError(f'Food {item_id} trigger list changed')
            event = triggers[events[0]]
            match = FOOD_TRIGGER.fullmatch(event['_modsTarget'])
            if event['_cond'] != 'True' or not match:
                raise ValueError(f'Food {item_id} hunger trigger changed')
            if (int(match.group(1)), int(match.group(2)), int(match.group(3))) != FOODS[item_id]:
                raise ValueError(f'Food {item_id} gains changed')
            foods.append(dict(item_id=item_id, name=item['_engName'], skill_id=int(item['skillId']), trigger_id=int(events[0]),
                hunger=float(match.group(1)), nutrient=int(match.group(2)), nutrient_name=NUTRIENTS[int(match.group(2))],
                nutrient_gain=float(match.group(3))))
    hunger_max, initial = float(player['HungerMax']), float(player['Hunger'])
    speed, percent = float(player['HungerRecoverSpeed']), float(player['HungerReducePercent'])
    rule = dict(player_init_id=1, drain_buff_id=DRAIN_BUFF, starvation_abnormal_id=STARVATION_ID, starvation_buff_id=STARVATION_BUFF,
        hunger_max=hunger_max, initial_hunger=initial, recovery_speed=speed, reduction_percent=percent, drain_scale=5.0,
        drain_interval_seconds=float(drain['_timeInterval']),
        start_interval_seconds=1.0, start_minimum=0.0, start_maximum=50.0, start_rate=1.0,
        end_interval_seconds=.1, end_minimum=51.0, end_maximum=1000.0, end_rate=1.0,
        food_cooldown_seconds=4.0, food_cooldown_group=12, nutrient_maximum=100.0,
        poison_buff_id=POISON_BUFF, poison_chance=.01, poison_seconds=20.0, poison_interval_seconds=1.0,
        poison_damage=3.0, poison_minimum_hp=3.0)
    # Summed-attribute pre-adds while starving (sum = raw + preAdd with the starter's unit multipliers).
    starvation = [dict(attribute=4, name='StaminaReducePercent', pre_add=1.0), dict(attribute=5, name='StaminaRecoverSpeed', pre_add=-10.0),
        dict(attribute=6, name='StaminaRecoverDelay', pre_add=2.0), dict(attribute=9, name='ComfortReducePercent', pre_add=1.0),
        dict(attribute=13, name='OxygenReducePercent', pre_add=1.0)]
    food_rows = ''.join('    {%d, %d, %.1ff, %d, %.1ff},\n' % (f['item_id'], f['skill_id'], f['hunger'], f['nutrient'], f['nutrient_gain']) for f in foods)
    header = '''/* Generated by export_hunger.py; do not edit. */
#ifndef PE_HUNGER_SOURCE_H
#define PE_HUNGER_SOURCE_H
typedef struct {
    int player_init_id, drain_buff_id, starvation_abnormal_id, starvation_buff_id;
    float hunger_max, initial_hunger, recovery_speed, reduction_percent, drain_scale;
    double drain_interval_seconds;
    float start_interval_seconds, start_minimum, start_maximum, start_rate;
    float end_interval_seconds, end_minimum, end_maximum, end_rate;
    float food_cooldown_seconds; int food_cooldown_group; float nutrient_maximum;
    float stamina_reduce_percent_add, stamina_recover_speed_add, stamina_recover_delay_add,
          comfort_reduce_percent_add, oxygen_reduce_percent_add;
} PeHungerSource;
static const PeHungerSource pe_hunger_source = {
    %(player_init_id)d, %(drain_buff_id)d, %(starvation_abnormal_id)d, %(starvation_buff_id)d,
    %(hunger_max).1ff, %(initial_hunger).1ff, %(recovery_speed).4ff, %(reduction_percent).1ff, %(drain_scale).1ff,
    %(drain_interval_seconds).1f,
    %(start_interval_seconds).1ff, %(start_minimum).1ff, %(start_maximum).1ff, %(start_rate).1ff,
    %(end_interval_seconds).1ff, %(end_minimum).1ff, %(end_maximum).1ff, %(end_rate).1ff,
    %(food_cooldown_seconds).1ff, %(food_cooldown_group)d, %(nutrient_maximum).1ff,
''' % rule + '    %.1ff, %.1ff, %.1ff, %.1ff, %.1ff\n};\n' % tuple(m['pre_add'] for m in starvation) + '''typedef struct { int item_id, skill_id; float hunger; int nutrient; float nutrient_gain; } PeHungerFoodSource;
#define PE_HUNGER_SOURCE_FOODS %d
static const PeHungerFoodSource pe_hunger_source_foods[PE_HUNGER_SOURCE_FOODS] = {
%s};
#endif
''' % (len(foods), food_rows)
    manifest = dict(schema=1, source=describe_source(database), source_files=provenance, rule=rule,
        initial_player={k: player[k] for k in ('id', 'InitBuffList', 'HungerMax', 'Hunger', 'HungerReducePercent', 'HungerRecoverSpeed')},
        drain_buff=drain, starvation_abnormal=abnormal, starvation_buff=starve_buff, starvation_pre_adds=starvation,
        poison_buff=poison, foods=foods,
        semantics=dict(
            drain='Initial buff 30200050 every 5 s on the shared eight-frame attribute pass: raw hunger += summed speed * summed percent * 5 (-0.347 with starter values), first execution at mount; clamps 0..HungerMax at summation.',
            starvation='AbnormalType 8: while inactive, a frame-time timer is checked before it accumulates; at >= 1 s it resets and, if alive, hunger in [0,50] starts the condition (rate Lerp(1,1)=1). While active, a 0.1 s timer and hunger in [51,1000] end it. Start mounts buff 30200113, end cancels it. Not removed on death. Active state and its buff are saved.',
            starvation_effect='Buff 30200113 pre-adds +1 StaminaReducePercent, -10 StaminaRecoverSpeed, +2 StaminaRecoverDelay, +1 ComfortReducePercent, +1 OxygenReducePercent.',
            food='Each supported food runs one MAD-free expression trigger: raw hunger += gain, its nutrient += gain capped at 100; skill cooldown 4 s, shared group 12. A second trigger has a 1% chance to mount 20 s food poisoning (-3 HP per second while HP > 3).',
            scope='Player hunger only. Nutrient decay buffs 30200149-152, food poisoning, the body-weight presentation, abnormal notices, oxygen and every other food stay outside this export.'),
    )
    return {'pe_hunger_source.h': header.encode(),
            'pe_hunger_source.json': (json.dumps(manifest, indent=2, ensure_ascii=False) + '\n').encode()}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--database', type=Path, default=DEFAULT_DATABASE)
    p.add_argument('--output', type=Path, default=ROOT / 'rebuild/generated')
    p.add_argument('--check', action='store_true')
    a = p.parse_args(argv)
    try:
        outputs = build_outputs(a.database)
        for name, data in outputs.items():
            path = a.output / name
            if a.check:
                if not path.is_file() or path.read_bytes() != data:
                    raise ValueError('Stale generated ' + name)
            else:
                a.output.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
        print('Hunger metadata ' + ('checked' if a.check else 'generated'))
        return 0
    except (OSError, ValueError, KeyError, IndexError, sqlite3.Error) as error:
        print(error, file=sys.stderr); return 1


if __name__ == '__main__':
    raise SystemExit(main())
