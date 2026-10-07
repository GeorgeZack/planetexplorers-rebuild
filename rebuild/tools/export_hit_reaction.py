#!/usr/bin/env python3
"""Recover the female-player reaction decision contract, without starting Unity.

This is not a reaction-animation, force motor or action-admission exporter.
UnityPy reads only the two original prefabs. All source/database input is read-only.
"""
import argparse
from contextlib import closing
import hashlib
import json
from pathlib import Path
import re
import sys

from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, numeric_rows
from export_player_collider_center import read_prefab, pointer
from export_starter_outfit import body, uncomment, expect

MODEL = 'Assets/Resources/Prefab/PlayerPrefab/FemaleModel.prefab'
PLAYER = 'Assets/Scripts/PeEntity/Resources/EntityPlayer.prefab'
BEAT = 'Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Beat.cs'
PARAM = 'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Param/BeatParam.cs'
VIEW = 'Assets/Scripts/PeEntity/Cmpt/BiologyViewRoot.cs'
AVATAR = 'Assets/Scripts/PeEntity/Cmpt/AvatarCmpt.cs'
ALIVE = 'Assets/Scripts/SkillSystem/SkAliveEntity.cs'
EVENT = 'Assets/Scripts/SkillSystem/SkTriggerEvent.cs'
EFFECT = 'Assets/Scripts/SkillSystem/SkEffect.cs'
ACTION = 'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/PEAction.cs'
ATTR = 'Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs'
SOURCES = (MODEL, PLAYER, BEAT, BEAT+'.meta', PARAM, PARAM+'.meta', VIEW,
           VIEW+'.meta', AVATAR, ALIVE, EVENT, EFFECT, ACTION, ATTR)
METHODS = (
    (BEAT, r'void\s+ApplyMoveEffect\s*\(', 'f16ea238ae2415af3396ffa506fe629797df06e855d89f4052213644da1945e9'),
    (ALIVE, r'void\s+ApplyForce\s*\(', '65b5e91c219de72ee83d0ef64730886b6fc2bf7698525c529a61b1f6d332de63'),
    (ALIVE, r'public\s+override\s+void\s+ApplyRepelEff\s*\(', '302374ae2ad2e0017ddd612d2a065204b09eee467e71efa6d3a412bc12ac0a96'),
    (EVENT, r'public\s+bool\s+Exec\s*\(', 'f840c3e802520f521eeff8d2c030e5ba9fcd1fe0738df9a8b4e56a47a7d867f3'),
    (EFFECT, r'public\s+void\s+Apply\s*\(', '6561eb27d5429ea2598b5ce05451ed28e50cb0c8ff7a5b27d4171539c08520d5'),
)
FIELDS = ('ThresholdWhacked', 'ThresholdRepulsed', 'ThresholdWentfly', 'ThresholdKnocked')
CURVES = ('m_AngleThresholdScale', 'm_AngleForceScale')
VALUES = ((1., .9850000739097595, .9970000386238098), (1., .9900000691413879, 1.))
SHIELD_MODS = 'exp,1,target.raws[70]=250;#exp,1,target.raws[71]=300;#exp,1,target.raws[72]=9999;#exp,1,target.raws[73]=9999'


def guid(root, path):
    match = re.search(r'^guid: ([0-9a-f]{32})$', (root/(path+'.meta')).read_text(), re.M)
    if not match:
        raise ValueError('Missing source script GUID: '+path)
    return match[1]


def enum_values(text, pattern):
    result = {}; value = -1
    for item in body(text, pattern).split(','):
        if item:
            parts = item.split('='); value = int(parts[1]) if len(parts)==2 else value+1
            result[parts[0]] = value
    return result


def validate_sources(root):
    texts = {p: uncomment((root/p).read_text(encoding='utf-8-sig', errors='replace'))
             for p in SOURCES if p.endswith('.cs')}
    for path, pattern, digest in METHODS:
        if hashlib.sha256(body(texts[path], pattern).encode()).hexdigest()!=digest:
            raise ValueError('Reaction method changed: '+path+' '+pattern)
    compact = {p: re.sub(r'\s+', '', text) for p,text in texts.items()}
    for path, parts in {
        AVATAR: ('conststringFemaleModelPrefabPath="Prefab/PlayerPrefab/FemaleModel";',
                 'return(null!=info&&info.sex==PeSex.Male)?MaleModelPrefabPath:FemaleModelPrefabPath;'),
        BEAT: ('m_Param=viewRoot.beatParam;', 'm_MotionMgr.AddAction(m_Whacked);',
               'm_MotionMgr.AddAction(m_Repulsed);', 'm_MotionMgr.AddAction(m_Wentfly);'),
        VIEW: ('beatParam=PEUtil.GetCmpt<BeatParam>(transform);',),
    }.items():
        if any(part not in compact[path] for part in parts):
            raise ValueError('Reaction binding changed: '+path)
    actions=enum_values(texts[ACTION], r'public\s+enum\s+PEActionType')
    attrs=enum_values(texts[ATTR], r'public\s+enum\s+AttribType')
    if any(actions.get(k)!=v for k,v in {'None':0,'Whacked':5,'Repulsed':6,'Wentfly':7,'Knocked':8}.items()):
        raise ValueError('Reaction action IDs changed')
    if any(attrs.get(k)!=70+i for i,k in enumerate(FIELDS)):
        raise ValueError('Reaction threshold attribute IDs changed')
    return [describe_source(root/p) for p in SOURCES]


def one(objects, script_guid):
    matches=[(pid,obj['data']) for pid,obj in objects.items()
             if obj['kind']=='MonoBehaviour' and obj.get('script_guid')==script_guid]
    if len(matches)!=1 or matches[0][1]['m_Enabled']!=1:
        raise ValueError('Expected one enabled source component')
    return matches[0]


def qualify(model, player, guids):
    pid, param=one(model,guids['param']); vid, view=one(model,guids['view'])
    mid, motion=one(player,guids['beat'])
    if (pid,vid,mid)!=(11471154,11424092,11400038) or pointer(view['beatParam'])!=pid:
        raise ValueError('Female view/BeatParam binding changed')
    if pointer(motion['m_GameObject'])!=100000 or player[100000]['data']['m_Name']!='EntityPlayer' or not player[100000]['data']['m_IsActive']:
        raise ValueError('EntityPlayer Motion_Beat root changed')
    view_go=pointer(view['m_GameObject']); param_go=pointer(param['m_GameObject'])
    if model[view_go]['data']['m_Name']!='FemaleModel' or not model[view_go]['data']['m_IsActive']:
        raise ValueError('Female model root changed')
    transforms={pointer(o['data']['m_GameObject']):(i,o['data']) for i,o in model.items() if o['kind']=='Transform'}
    by_id={i:d for i,d in transforms.values()}; tid=transforms[param_go][0]; seen=set()
    while tid!=transforms[view_go][0]:
        if not tid or tid in seen or tid not in by_id:
            raise ValueError('BeatParam is outside female view hierarchy')
        seen.add(tid)
        if not model[pointer(by_id[tid]['m_GameObject'])]['data']['m_IsActive']:
            raise ValueError('Inactive BeatParam hierarchy')
        tid=pointer(by_id[tid]['m_Father'])
    curves={}
    for field, values in zip(CURVES,VALUES):
        curve=param[field]
        expected=[dict(time=t,value=v,inSlope=0.,outSlope=0.,tangentMode=0) for t,v in zip((0.,90.,180.),values)]
        if curve['m_Curve']!=expected or curve['m_PreInfinity']!=2 or curve['m_PostInfinity']!=2:
            raise ValueError('Unqualified female angle curve: '+field)
        curves[field]=curve
    if param['m_ThresholdScaleInAir']!=.5:
        raise ValueError('Female airborne threshold scale changed')
    return dict(model_component=pid, view_component=vid, entity_component=mid,
                curves=curves, air_scale=param['m_ThresholdScaleInAir'])


def read_rows(database):
    with closing(connect_readonly(database)) as db:
        init,_=numeric_rows(db,'initProp','id'); buffs,_=numeric_rows(db,'skBuff','_id')
        events,_=numeric_rows(db,'skTriggerEvent','_id'); effects,_=numeric_rows(db,'skEffect','_id')
        if db.execute('SELECT id FROM initProp ORDER BY rowid LIMIT 1').fetchone()['id']!='1':
            raise ValueError('Initial player row changed')
        player=dict(init[1]); shield=dict(buffs[30200177]); effect=dict(effects[57])
        expect(player,dict(zip(FIELDS,(250,400,750,1750))),'Player reaction thresholds')
        expect(shield,{'_mods':SHIELD_MODS,'_timeActive':-1,'_timeInterval':0},'Shield reaction buff')
        expect(effect,{'_spEff':'True','_id':57},'Hare force effect')
        trigger=[]
        for identity,force in ((10100,100),(10400,400),(10250,250)):
            row=dict(events[identity])
            expect(row,{'_cond':'colinfo','_force':force,'_effOnHitTarget':57},'Hare trigger')
            if not row['_modsTarget'].startswith('exp,29,target.raws[29]=-1;#exp,1,target.expFunc.TryGetHurt('):
                raise ValueError('Hare rigid-before-damage contract changed')
            trigger.append(row)
    return player,shield,effect,trigger


def f(value):
    text=format(value,'.9g')
    return (text if any(c in text for c in '.eE') else text+'.0')+'f'


def build_outputs(root=ROOT, database=DEFAULT_DATABASE):
    sources=validate_sources(root)
    binding=qualify(read_prefab(root/MODEL),read_prefab(root/PLAYER),
                    dict(param=guid(root,PARAM),view=guid(root,VIEW),beat=guid(root,BEAT)))
    player,shield,effect,triggers=read_rows(database)
    data=dict(schema=1,source=describe_source(database),source_files=sources,binding=binding,
              player_thresholds={k:player[k] for k in FIELDS},shield_buff=shield,
              effect=effect,triggers=triggers,
              method_guards=[dict(path=p,pattern=r,sha256=s) for p,r,s in METHODS],
              scope='Disconnected female-player decision only; no action admission, force movement, IK, ragdoll, Rigid mutation or shield input.',
              order=['caller resolves permission and Vector3.Angle', 'evaluate angle threshold curve',
                     'multiply force by angle force curve', 'multiply threshold scale by .5 only when InAir',
                     'strict ordered threshold comparisons', 'Knocked and Wentfly both request Wentfly'],
              interpolation='Finite unweighted Hermite arithmetic follows the separately qualified Unity5 SSE polynomial; no engine evaluated this new reaction selector.',
              threshold_owner='Caller supplies current summed attributes. Exported base and mounted shield-buff values are snapshots, not automatic buff lifecycle or cancellation.',
              hit_ik='Null and Repulsed call ApplyHitEffect, whose randomness and IK curves are outside this decision slice.')
    header='''/* Generated by export_hit_reaction.py; do not edit. */
#ifndef PE_HIT_REACTION_SOURCE_H
#define PE_HIT_REACTION_SOURCE_H
typedef struct {float time,value,in_slope,out_slope;} PeHitReactionCurveKey;
typedef struct {int trigger_id; float force;} PeHitReactionTrigger;
'''
    for name,field in [('threshold',CURVES[0]),('force',CURVES[1])]:
        header+='static const PeHitReactionCurveKey pe_hit_reaction_'+name+'_curve[3]={\n'
        header+=''.join('    {'+','.join(f(k[v]) for v in ('time','value','inSlope','outSlope'))+'},\n' for k in binding['curves'][field]['m_Curve'])
        header+='};\n'
    header+='static const float pe_hit_reaction_base_thresholds[4]={'+','.join(f(float(player[k])) for k in FIELDS)+'};\n'
    shield_values=[float(value) for value in re.findall(r'target\.raws\[7[0-3]\]=([0-9]+)',shield['_mods'])]
    header+='static const float pe_hit_reaction_shield_thresholds[4]={'+','.join(f(value) for value in shield_values)+'};\n'
    header+='static const float pe_hit_reaction_air_scale='+f(binding['air_scale'])+';\n'
    header+='static const PeHitReactionTrigger pe_hit_reaction_triggers[3]={'+','.join('{'+r['_id']+','+f(float(r['_force']))+'}' for r in triggers)+'};\n#endif\n'
    return {'pe_hit_reaction_source.h':header.encode(),'pe_hit_reaction_source.json':(json.dumps(data,indent=2)+'\n').encode()}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true');parser.add_argument('--output',type=Path,default=ROOT/'rebuild/generated')
    args=parser.parse_args(argv)
    try:
        outputs=build_outputs()
        if args.check:
            for name,data in outputs.items():
                if not (args.output/name).is_file() or (args.output/name).read_bytes()!=data:
                    raise ValueError('Stale or missing '+name)
        else:
            args.output.mkdir(parents=True,exist_ok=True)
            for name,data in outputs.items():(args.output/name).write_bytes(data)
    except (OSError,ValueError,KeyError,ImportError) as error:
        print(error,file=sys.stderr);return 1
    return 0


if __name__=='__main__':raise SystemExit(main())
