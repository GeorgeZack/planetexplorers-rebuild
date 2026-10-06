#!/usr/bin/env python3
"""Export source player stamina/comfort buffs without launching or changing Unity."""
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

ADDITIONAL_SOURCES = (
    'Assets/Scripts/SkillSystem/SkAttribs.cs',
    'Assets/Scripts/SkillSystem/SkAttribsModifier.cs',
    'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs',
    'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_HumanMove.cs',
    'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_DigTerrain.cs',
    'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Fell.cs',
)
SOURCE_FILES = shared.SOURCE_FILES + ADDITIONAL_SOURCES
BUFFS = {
    30200052: (65509, '1', 'exp,1,if(target.expFunc.InCoolingForConsumingStamina(target.sums[6])){target.raws[3]+=target.sums[5]*target.sums[4];}'),
    30200049: (65506, '5', 'exp,1,target.raws[8]+=target.sums[10]*target.sums[9]*5'),
    30200175: (65509, '1', 'exp,6,target.buffPostAdd=(1-target.raws[8]/target.sums[7])*2'),
}
PLAYER_FIELDS = {
    'stamina_max': 'StaminaMax', 'initial_stamina': 'Stamina', 'recovery_speed': 'StaminaRecoverSpeed',
    'reduction_percent': 'StaminaReducePercent', 'base_delay_seconds': 'StaminaRecoverDelay',
    'comfort_max': 'ComfortMax', 'initial_comfort': 'Comfort', 'comfort_recovery_speed': 'ComfortRecoverSpeed',
    'comfort_reduction_percent': 'ComfortReducePercent',
}


def validate_sources(root):
    # Reuse the exact same initial-player binding, frame/buff scheduler, remount
    # and death contracts as the passive-HP exporter; no second lifecycle policy.
    shared.validate_sources(root)
    sources = {p: uncomment((root / p).read_text(encoding='utf-8-sig', errors='replace')) for p in SOURCE_FILES}
    funcs = sources['Assets/Scripts/SkillSystem/SkExpFuncSet.cs']
    shared.exact(body(funcs, r'public\s+bool\s+InCoolingForConsumingStamina\s*\('),
        'return (Time.time > SkRuntimeInfo.Current.Target._lastestTimeOfConsumingStamina + Mathf.Max(cooltime, 0.1f));',
        'Strict stamina consumption cooldown')
    entity = sources['Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs']
    enum = body(entity, r'public\s+enum\s+AttribType')
    if not enum.startswith('HpMax=0,Hp,StaminaMax,Stamina,StaminaReducePercent,StaminaRecoverSpeed,StaminaRecoverDelay,ComfortMax,Comfort,ComfortReducePercent,ComfortRecoverSpeed,'):
        raise ValueError('Stamina/comfort attribute indices changed')
    events = body(entity, r'void\s+CheckAttrEvent\s*\(')
    stamina_case = events.split('caseAttribType.Stamina:', 1)[1].split('break;', 1)[0]
    shared.exact(stamina_case, '''if(oldVal!=newVal){DispatchStaminaEvent(newVal-oldVal);
        if(oldVal>newVal)_lastestTimeOfConsumingStamina=Time.time;}''', 'Consumption timestamp change listener')
    if 'AttribInspectoscope.Instance.CheckAttrib(this,attType,newVal);' not in events:
        raise ValueError('Post-sum attribute clamping notification changed')
    pairs = body(entity, r'void\s+InitPair\s*\(')
    for current, maximum in [('Stamina', 'StaminaMax'), ('Comfort', 'ComfortMax')]:
        binding = f'm_AttribPairs[(int)AttribType.{current}]=m_AttribPairs[(int)AttribType.{maximum}]=newAttribPair(AttribType.{current},AttribType.{maximum});'
        if binding not in pairs:
            raise ValueError(current + ' current/max pairing changed')
    pair = body(entity.split('public class AttribPair', 1)[1], r'public\s+void\s+CheckAttrib\s*\(')
    shared.exact(pair, '''if(type==m_Current){
        if(value<0||value>entity.GetAttribute(m_Max))entity.SetAttribute(m_Current,Mathf.Clamp(value,0,entity.GetAttribute(m_Max)));
        }else if(type==m_Max){float currentValue=entity.GetAttribute(m_Current);
        if(currentValue>value)entity.SetAttribute(m_Current,value);}''', 'Post-sum current/max clamp')
    if 'publicfloat_lastestTimeOfConsumingStamina=0;' not in shared.compact(sources['Assets/Scripts/SkillSystem/SkEntityHelper.cs']):
        raise ValueError('Initial consumption timestamp changed')
    attribs = sources[ADDITIONAL_SOURCES[0]]
    shared.exact(body(attribs, r'public\s+void\s+ExecBuffs\s*\('), '''
        int n=_buffs.Count;if(n<=0)return;_tmpBuffs.Clear();_tmpBuffs.AddRange(_buffs);int idx;
        for(int i=n-1;i>=0;i--){idx=_buffs[i]==_tmpBuffs[i]?i:_buffs.IndexOf(_tmpBuffs[i]);
        if(idx>=0){if(!_tmpBuffs[i].Exec(_parentAttribs)&&_tmpBuffs[i].OnDiscard(_parentAttribs)){_buffs.RemoveAt(idx);}}}
    ''', 'Reverse source buff execution')
    shared.exact(body(attribs, r'public\s+void\s+Update\s*\(SkBeModified\s+beModified\)'), '''
        _pack.ExecBuffs();int nAttribs=_raws.Count;
        for(int i=0;i<nAttribs;i++){if(_dirties[i]){
        buffMul=1.0f;buffPreAdd=0.0f;buffPostAdd=0.0f;_pack.ExecTmpBuffs(i);
        _sums[i]=buffMul*(_raws[i]+buffPreAdd)+buffPostAdd;
        SkEntity entity=GetModCaster(i);int casterId=0;if(entity!=null){casterId=entity.GetId();}
        _dirties[i]=false;beModified.indexList.Add(i);beModified.valueList.Add(_sums[i]);beModified.casterIdList.Add(casterId);}}
    ''', 'Ascending dirty attribute summation')
    shared.exact(body(attribs, r'private\s+void\s+RawSetter\s*\('),
        'NumList r=(NumList)_raws;r.Set(idx,v);_dirties[idx]=true;', 'Deferred raw clamping')
    shared.exact(body(attribs, r'private\s+void\s+SumSetter\s*\('), '''
        NumList s=(NumList)_sums;float oldValue=s.Get(idx);s.Set(idx,v);OnAlterNumAttribs(idx,oldValue,v);
    ''', 'Summation notifications')
    shared.exact(body(attribs, r'public\s+void\s+Deserialize\s*\('), '''
        DisableNumAttribsEvent();int nAttribs=_raws.Count;for(int i=0;i<nAttribs;i++){_sums[i]=_raws[i]=r.ReadSingle();}
        _dirties.Clear();EnableNumAttribsEvent();''', 'Loaded raw and summed attributes')
    modifiers = sources[ADDITIONAL_SOURCES[1]]
    shared.exact(body(modifiers, r'public\s+SkAttribsOpEXP\s*\('), '''
        string[] args=strArgs.Split(new char[]{','},2);_idx=Convert.ToInt32(args[0]);
        _bTmpOp=args[1].Contains("buff");SkInst.s_ExpCompiler.AddExpString(this,args[1]);''', 'Temporary expression classification')
    modifier_class = modifiers.split('public class SkAttribsModifier', 1)[1]
    shared.exact(body(modifier_class, r'public\s+void\s+Exec\s*\('), '''
        int n=_ops.Count;for(int i=0;i<n;i++){_ops[i].Exec(dst,paraCaster,paraTarget,para);}ReqExecTmp(dst);
    ''', 'Periodic pulse dirties delayed temporary expression')
    shared.exact(body(modifier_class, r'public\s+void\s+ReqExecTmp\s*\('), '''
        int n=_tmpOpsIdxs.Count;for(int i=0;i<n;i++){dst.modflags[_tmpOpsIdxs[i]]=true;}
    ''', 'Temporary modifier dirty indices')
    shared.exact(body(modifier_class, r'public\s+void\s+TryExecTmp\s*\('), '''
        int n=_tmpOpsIdxs.Count;for(int i=0;i<n;i++){
        if(_tmpOpsIdxs[i]==idxToMod){for(int j=0;j<times;j++){
        _tmpOps[i].Exec(dst,paraCaster,paraTarget,para);SkAttribsOpMAT op=_tmpOps[i] as SkAttribsOpMAT;
        if(op!=null){dst.buffMul+=op._mul;dst.buffPreAdd+=op._preAdd;dst.buffPostAdd+=op._postAdd;}}}}
    ''', 'Indexed repeated temporary summation')
    for path in ADDITIONAL_SOURCES[2:]:
        if 'skillCmpt._lastestTimeOfConsumingStamina=Time.time;' not in shared.compact(sources[path]):
            raise ValueError('Missing active action stamina lock: ' + path)
    sword_speed = body(sources[ADDITIONAL_SOURCES[2]], r'void\s+UpdateAnimSpeed\s*\(')
    if not sword_speed.endswith('skillCmpt._lastestTimeOfConsumingStamina=Time.time;}'):
        raise ValueError('Sword action consumption timestamp cadence changed')
    return [describe_source(root / p) for p in SOURCE_FILES]


def build_outputs(database=DEFAULT_DATABASE, source_root=ROOT):
    provenance = validate_sources(source_root)
    with closing(connect_readonly(database)) as db:
        players, _ = numeric_rows(db, 'initProp', 'id'); buffs, _ = numeric_rows(db, 'skBuff', '_id')
        first = db.execute('SELECT id FROM initProp ORDER BY rowid LIMIT 1').fetchone()
        if first is None or first['id'] != '1':
            raise ValueError('First player initProp row changed')
        player = dict(players[1])
        expect(player, {'InitBuffList': shared.INITIAL_BUFFS, 'StaminaMax': 100, 'Stamina': 100,
            'StaminaRecoverSpeed': 40, 'StaminaReducePercent': 1, 'StaminaRecoverDelay': 0,
            'ComfortMax': 500, 'Comfort': 500, 'ComfortRecoverSpeed': '-.1388', 'ComfortReducePercent': 1}, 'Player stamina/comfort')
        rows = {}
        for identity, (kind, interval, expression) in BUFFS.items():
            row = dict(buffs[identity])
            expect(row, {'_type': kind, '_priority': 0, '_stackLimit': 1, '_timeActive': -1,
                '_timeInterval': interval, '_eff': 0, '_effBeg': 0, '_effEnd': 0, '_mods': expression}, f'Stamina/comfort buff{identity}')
            rows[identity] = row
    rule = {name: float(player[field]) for name, field in PLAYER_FIELDS.items()}
    rule.update(player_init_id=1, stamina_buff_id=30200052, comfort_buff_id=30200049, delay_buff_id=30200175,
        minimum_delay_seconds=.1, comfort_delay_scale=2.0,
        stamina_interval_seconds=float(rows[30200052]['_timeInterval']),
        comfort_interval_seconds=float(rows[30200049]['_timeInterval']),
        delay_interval_seconds=float(rows[30200175]['_timeInterval']), lifetime_seconds=-1.0)
    header = '''/* Generated by export_stamina_recovery.py; do not edit. */
#ifndef PE_STAMINA_RECOVERY_SOURCE_H
#define PE_STAMINA_RECOVERY_SOURCE_H
typedef struct {
    int player_init_id, stamina_buff_id, comfort_buff_id, delay_buff_id;
    float stamina_max, initial_stamina, recovery_speed, reduction_percent;
    float base_delay_seconds, minimum_delay_seconds;
    float comfort_max, initial_comfort, comfort_recovery_speed, comfort_reduction_percent, comfort_delay_scale;
    double stamina_interval_seconds, comfort_interval_seconds, delay_interval_seconds, lifetime_seconds;
} PeStaminaRecoverySource;
static const PeStaminaRecoverySource pe_stamina_recovery_source = {
    %(player_init_id)d, %(stamina_buff_id)d, %(comfort_buff_id)d, %(delay_buff_id)d,
    %(stamina_max).1ff, %(initial_stamina).1ff, %(recovery_speed).1ff, %(reduction_percent).1ff,
    %(base_delay_seconds).1ff, %(minimum_delay_seconds).1ff,
    %(comfort_max).1ff, %(initial_comfort).1ff, %(comfort_recovery_speed).4ff, %(comfort_reduction_percent).1ff, %(comfort_delay_scale).1ff,
    %(stamina_interval_seconds).1f, %(comfort_interval_seconds).1f, %(delay_interval_seconds).1f, %(lifetime_seconds).1f
};
#endif
''' % rule
    manifest = dict(schema=1, source=describe_source(database), source_files=provenance, rule=rule,
        initial_player={k: player[k] for k in ('id', 'InitBuffList', *PLAYER_FIELDS.values())},
        source_buffs=[rows[i] for i in BUFFS],
        semantics=dict(shared_lifecycle='pe_passive_recovery_source owns the eight-frame attribute gate and delayed .5 initialization check; source guards are shared.',
            relevant_execution_order=[30200175, 30200052, 30200049],
            stamina='Each admitted evaluation adds current summed recovery_speed * reduction_percent; strict Time.time > last_consumption + max(previous summed delay,.1f).',
            comfort='Each comfort evaluation adds summed recovery_speed * reduction_percent *5 to raw comfort; current/max clamp occurs during later summation.',
            delay='Temporary expression keyed6; its scheduled pulse dirties6. Later ascending summation6 uses new raw comfort8 before comfort8 clamps. Assignment is idempotent across repeated temporary evaluations.',
            raw_summed='Recovery reads the previous summed delay. Raw writes dirty their own attribute, not every dependent attribute. Comfort changes do not by themselves immediately recalculate delay.',
            load='Raw values serialized; deserialize initially assigns sums=raw and clears dirties. Delay source raw starts0; buff phase is transient.',
            consumption='Decreased summed stamina stamps Time.time; Sword, jump/step and dig/fell active updates also refresh the timestamp.',
            scope='Exact starter player three-buff subset only; no general nutrition, food buffs, other clothes, revival, UI or runtime save policy is generated.'),
    )
    return {'pe_stamina_recovery_source.h': header.encode(),
            'pe_stamina_recovery_source.json': (json.dumps(manifest, indent=2, ensure_ascii=False) + '\n').encode()}


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
        print('Stamina recovery metadata ' + ('checked' if a.check else 'generated'))
        return 0
    except (OSError, ValueError, KeyError, IndexError, sqlite3.Error) as error:
        print(error, file=sys.stderr); return 1


if __name__ == '__main__':
    raise SystemExit(main())
