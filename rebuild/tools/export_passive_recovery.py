#!/usr/bin/env python3
"""Export the guarded original player passive-HP rule from read-only sources."""
import argparse
from contextlib import closing
import json
from pathlib import Path
import re
import sqlite3
import sys

from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, numeric_rows
from export_starter_outfit import body, uncomment, expect

SOURCE_FILES = (
    'Assets/Scripts/SkillSystem/SkBuffs.cs',
    'Assets/Scripts/SkillSystem/SkExpFuncSet.cs',
    'Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs',
    'Assets/Scripts/SkillSystem/SkEntityHelper.cs',
    'Assets/Scripts/SkillSystem/SkEntityAttribsUpdater.cs',
    'Assets/PeWorld/PeEntityCreator.cs',
    'Assets/Scripts/SkillSystem/SkAliveEntity.cs',
)
INITIAL_BUFFS = '30200049,30200050,30200051,30200052,30200053,30200046,30200149,30200150,30200151,30200152,30200175'
EXPRESSION = 'exp,1,if(target.expFunc.InCoolingForGettingHurt(4)&&target.raws[1]/target.sums[0]<0.33f){target.raws[1]+=target.sums[0]*0.01f;}'


def compact(text):
    return re.sub(r'\s+', '', text)


def exact(actual, expected, label):
    if actual != compact(expected):
        raise ValueError(label + ' source contract changed')


def validate_sources(root):
    texts = [uncomment((root / p).read_text(encoding='utf-8-sig', errors='replace')) for p in SOURCE_FILES]
    buff_source = compact(texts[0])
    for fragment in ('buff._lifeTime=Convert.ToSingle(reader.GetString(reader.GetOrdinal("_timeActive")));',
                     'buff._interval=Convert.ToSingle(reader.GetString(reader.GetOrdinal("_timeInterval")));'):
        if fragment not in buff_source:
            raise ValueError('Buff table clock binding changed')
    exact(body(texts[0], r'public\s+bool\s+Exec\s*\(ISkAttribs\s+dst\)'), '''
        bool ret=true; SkRuntimeInfo.Current=this;
        if(_n==0){
            if(_buff._effBeg!=null) _buff._effBeg.Apply(Target,this);
            if(_buff._eff!=null) _buff._eff.Apply(Target,this);
            if(_buff._mods!=null) _buff._mods.Exec(dst,_paraCaster,_paraTarget,_para as ISkAttribsModPara);
            _n++;
        }
        float elapse=Time.time-_startTime;
        if(_buff._interval>PETools.PEMath.Epsilon && elapse>_n*_buff._interval){
            if(_buff._eff!=null) _buff._eff.Apply(Target,this);
            if(_buff._mods!=null) _buff._mods.Exec(dst,_paraCaster,_paraTarget,_para as ISkAttribsModPara);
            _n++;
        }
        if(_buff._lifeTime>-PETools.PEMath.Epsilon && elapse>_buff._lifeTime){ret=false;}
        SkRuntimeInfo.Current=null; return ret;
    ''', 'Buff evaluation order/clock')
    create = body(texts[0], r'public\s+static\s+SkBuffInst\s+Create\s*\(')
    if 'inst._startTime=Time.time;' not in create or 'internalint_n=0;' not in compact(texts[0]):
        raise ValueError('Buff initial clock contract changed')
    exact(body(texts[0], r'public\s+static\s+SkBuffInst\s+Mount\s*\('), '''
        if(inst==null)return null;
        int id=inst._buff._id;int type=inst._buff._type;
        int prior=inst._buff._priority;int maxStack=inst._buff._stackLimit;
        List<SkBuffInst> stackBuffs=buffPak._buffs.FindAll((SkBuffInst it)=>it._buff._id==id);
        if(stackBuffs.Count==0){
            if(buffPak._buffs.Exists((SkBuffInst it)=>(it._buff._type==type&&it._buff._priority>prior)))return null;
            Unmount(buffPak,(SkBuffInst it)=>(it._buff._type==type&&it._buff._priority<prior));
        }else if(stackBuffs.Count>=maxStack){Unmount(buffPak,stackBuffs[0]);}
        buffPak._buffs.Add(inst);return inst;
    ''', 'Buff remount/priority lifecycle')
    exact(body(texts[1], r'public\s+bool\s+InCoolingForGettingHurt\s*\('),
          'return (Time.time >= SkRuntimeInfo.Current.Target._lastestTimeOfGettingHurt + cooltime);', 'Hurt cooldown')
    for name in ('GetHurt', 'TryGetHurt'):
        hurt = body(texts[1], r'public\s+void\s+' + name + r'\s*\(')
        # The timestamp is written on the admitted damage path even when dmg=0;
        # only TryGetHurt owns a CanDamage branch. Guard the complete local
        # timestamp block, not just the presence of an assignment token.
        if 'floatcurTime=Time.time;' not in hurt or 'if(target!=null)target._lastestTimeOfGettingHurt=curTime;' not in hurt:
            raise ValueError(name + ' hurt timestamp contract changed')
        if hurt.count('target._lastestTimeOfGettingHurt=') != 1:
            raise ValueError(name + ' ambiguous hurt timestamp')
        between = hurt.split('floatcurTime=Time.time;', 1)[1].split('SkInstinst=', 1)[0]
        exact(between, '''if(caster!=null)caster._lastestTimeOfHurtingSb=curTime;
                         if(target!=null)target._lastestTimeOfGettingHurt=curTime;''', name + ' timestamp block')
    enum = body(texts[2], r'public\s+enum\s+AttribType')
    if not enum.startswith('HpMax=0,Hp,'):
        raise ValueError('HP attribute index contract changed')
    exact(body(texts[2], r'public\s+virtual\s+void\s+InitEntity\s*\('), '''
        if(parent!=null&&useParentMasks!=null){Init(OnNumAttribs,OnPutInPak,parent,useParentMasks);}
        else if(parent!=null||useParentMasks!=null){Debug.LogError("Invalid Arguments to init skill entity");}
        else{Init(OnNumAttribs,OnPutInPak,AttribsCnt);}
        if(null!=m_Attrs){foreach(Attr attr in m_Attrs){SetAttribute((int)(attr.m_Type),attr.m_Value);}}
        if(null!=m_InitBuffList)foreach(int buffID in m_InitBuffList)
            MountBuff(this,buffID,new System.Collections.Generic.List<int>(),new System.Collections.Generic.List<float>());
    ''', 'Initial entity buff mount')
    exact(body(texts[2], r'protected\s+void\s+SetAutoBuffActive\s*\('), '''
        if(null==m_InitBuffList)return;
        if(active){foreach(int buffID in m_InitBuffList)
            MountBuff(this,buffID,new System.Collections.Generic.List<int>(),new System.Collections.Generic.List<float>());}
        else{foreach(int buffID in m_InitBuffList)CancelBuffById(buffID);}
    ''', 'Automatic buff activation')
    exact(body(texts[2], r'protected\s+virtual\s+void\s+CheckInitAttr\s*\('), '''
        if(GetAttribute(AttribType.Hp)<=0)OnDeath(null);else SetAutoBuffActive(true);
    ''', 'Alive initialization')
    exact(body(texts[2], r'void\s+OnDeath\s*\('), '''
        DispatchDeathEvent(caster);SetAutoBuffActive(false);isDead=true;
    ''', 'Death disables initial buffs')
    if 'publicfloat_lastestTimeOfGettingHurt=0;' not in compact(texts[3]):
        raise ValueError('Initial hurt timestamp changed')
    if 'constintUpdateFrameInterval=8;' not in compact(texts[4]):
        raise ValueError('Source attribute update frame interval changed')
    exact(body(texts[4], r'public\s+override\s+void\s+Update\s*\('), '''
        if(Time.frameCount<_prevFrmCnt+UpdateFrameInterval)return;
        int n=_reqEntities.Count;
        for(int i=n-1;i>=0;i--){
            if(_reqEntities[i]==null||_reqEntities[i].Equals(null)){_reqEntities.RemoveAt(i);}
            else{_reqEntities[i].UpdateAttribs();}
        }
        _prevFrmCnt=Time.frameCount;
    ''', 'Source attribute update cadence')
    player = texts[5].split('public static class PlayerProtoDb', 1)[1]
    player = player.split('public class RandomNpcDb', 1)[0]
    if '[DbField("InitBuffList")]publicint[]initBuff;' not in compact(player):
        raise ValueError('Player initial buff binding changed')
    exact(body(player, r'public\s+static\s+Item\s+Get\s*\(\s*\)'),
          'return items.Count>0?items[0]:null;', 'Active player prototype selection')
    if 'ReadFullTable("initprop")' not in compact(body(player, r'public\s+static\s+void\s+Load\s*\(')):
        raise ValueError('Player initialization table changed')
    restore = compact(texts[6]).split('caseEEntityProto.Player:', 1)[1].split('break;', 1)[0]
    exact(restore, '''PlayerProtoDb.Item playerPDB=PlayerProtoDb.Get();
        if(null!=playerPDB)m_InitBuffList=playerPDB.initBuff;''', 'Loaded player initial buffs')
    exact(body(texts[6], r'void\s+Start\s*\('), '''
        if(!m_Init)InitSkEntity();Entity.AddMsgListener(this);
        m_CollisionEntities=new Dictionary<Collider,List<Collider>>();
        m_View=Entity.viewCmpt;m_Trans=Entity.peTrans;m_Animator=Entity.animCmpt;
        m_Beat=Entity.GetCmpt<Motion_Beat>();m_MotionMgr=Entity.motionMgr;
        m_MotionEquip=Entity.GetCmpt<Motion_Equip>();m_Live=Entity.GetCmpt<Motion_Live>();
        onHpChange+=OnHpChange;onSkillEvent+=OnTargetSkill;onTranslate+=OnTranslate;
        Invoke("CheckInitAttr",0.5f);
    ''', 'Deferred initial attribute check')
    return [describe_source(root / p) for p in SOURCE_FILES]


def build_outputs(database=DEFAULT_DATABASE, source_root=ROOT):
    provenance = validate_sources(source_root)
    with closing(connect_readonly(database)) as db:
        players, _ = numeric_rows(db, 'initProp', 'id')
        buffs, _ = numeric_rows(db, 'skBuff', '_id')
        first = db.execute('SELECT id FROM initProp ORDER BY rowid LIMIT 1').fetchone()
        if first is None or first['id'] != '1':
            raise ValueError('PlayerProtoDb.Get no longer selects initProp1')
        player = dict(players[1]); buff = dict(buffs[30200046])
        expect(player, {'InitBuffList': INITIAL_BUFFS, 'HpMax': 500, 'Hp': 500}, 'Initial player recovery')
        expect(buff, {'_type': 65300, '_priority': 0, '_stackLimit': 1, '_timeActive': -1,
                      '_timeInterval': 1, '_eff': 0, '_effBeg': 0, '_effEnd': 0, '_mods': EXPRESSION}, 'Passive recovery buff')
    expression = re.fullmatch(r'exp,1,if\(target\.expFunc\.InCoolingForGettingHurt\(([0-9.]+)\)'
                             r'&&target\.raws\[1\]/target\.sums\[0\]<([0-9.]+)f\)'
                             r'\{target\.raws\[1\]\+=target\.sums\[0\]\*([0-9.]+)f;\}', buff['_mods'])
    if expression is None:
        raise ValueError('Unsupported passive recovery expression')
    cooldown, threshold, heal = map(float, expression.groups())
    rule = dict(player_init_id=int(player['id']), buff_id=int(buff['_id']), interval_seconds=float(buff['_timeInterval']),
                cooldown_seconds=cooldown, heal_fraction=heal, threshold_fraction=threshold, lifetime_seconds=float(buff['_timeActive']),
                initialization_check_delay_seconds=.5, attribute_frame_interval=8)
    manifest = dict(schema=1, source=describe_source(database), source_files=provenance, rule=rule,
        source_buff=buff, initial_player={k: player[k] for k in ('id', 'HpMax', 'Hp', 'InitBuffList')},
        scheduler=dict(initial_evaluation=True, interval_comparison='strict greater than',
                       max_overdue_ticks_per_evaluation=1, tick_before_expiration=True,
                       source_attribute_frame_interval=8, clock='Time.time',
                       cooldown_comparison='greater than or equal', initial_last_hurt=0,
                       hp_arithmetic='source float ratio < .33f; add max_hp * .01f; no .33 cap',
                       death='PESkEntity.OnDeath cancels initial buffs',
                       deferred_initial_check='Start invokes CheckInitAttr after .5s; living activation mounts initial buffs and replaces any existing same-ID stack-limit1 instance. Fresh/load first-mount distinction depends on initial prefab/creation order; native Invoke/update ordering remains an adapter.',
                       initialization='PlayerProtoDb.Get selects first initProp row; loaded player restores its initial buff list'),
        scope='Passive HP rule only. Source cadence is provenance, not a native frame-rate or save migration policy. No stamina, general buffs, revival or runtime behavior is generated.')
    header = '''/* Generated by export_passive_recovery.py; do not edit. */
#ifndef PE_PASSIVE_RECOVERY_SOURCE_H
#define PE_PASSIVE_RECOVERY_SOURCE_H
typedef struct {
    int player_init_id, buff_id;
    double interval_seconds, cooldown_seconds;
    float heal_fraction, threshold_fraction;
    double lifetime_seconds;
    double initialization_check_delay_seconds;
    unsigned attribute_frame_interval;
} PePassiveRecoverySource;
static const PePassiveRecoverySource pe_passive_recovery_source = {
    %(player_init_id)d, %(buff_id)d, %(interval_seconds).1f, %(cooldown_seconds).1f, %(heal_fraction).2ff, %(threshold_fraction).2ff, %(lifetime_seconds).1f, %(initialization_check_delay_seconds).1f, %(attribute_frame_interval)d
};
#endif
''' % rule
    return {'pe_passive_recovery_source.h': header.encode(),
            'pe_passive_recovery_source.json': (json.dumps(manifest, indent=2, ensure_ascii=False) + '\n').encode()}


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
                a.output.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
        print('Passive recovery metadata ' + ('checked' if a.check else 'generated'))
        return 0
    except (OSError, ValueError, KeyError, IndexError, sqlite3.Error) as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
