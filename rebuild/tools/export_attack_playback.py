#!/usr/bin/env python3
"""Recover the player's serialized contact-responsive attack playback rule offline.

Optional UnityPy is required. --game additionally checks the installed modern
ResourceManager binding and exact runtime payload; no engine/game is started.
AnimationCurve.Evaluate is native internal-call. Separate pinned-binary
inspection and isolated arithmetic replay qualify the supported interpolation;
this exporter itself validates serialized data and does not execute that replay.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
from export_content import ROOT, describe_source
from export_starter_outfit import body, uncomment
from export_player_collider_center import read_prefab, pointer
from export_capsule_history import UNITY_SHA256

PREFAB = 'Assets/Scripts/PeEntity/Resources/EntityPlayer.prefab'
ACTION = 'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs'
EQUIP = 'Assets/Scripts/PeEntity/Cmpt/Motion/Motion_Equip.cs'
TRIGGER = 'Assets/Scripts/Attack/PEAttackTrigger.cs'
CREATOR = 'Assets/PeWorld/PeEntityCreator.cs'
PLAYER = 'Assets/Scripts/PeEntity/Cmpt/MainPlayerCmpt.cs'
SETTINGS = 'Assets/Scripts/Lilj/SystemSettingData.cs'
SOURCE_FILES = (PREFAB, ACTION, EQUIP, EQUIP+'.meta', TRIGGER, CREATOR, PLAYER, SETTINGS)
CONTRACTS = (
    (ACTION, 'playback_formula', r'void\s+UpdateAnimSpeed\s*\(', 'e339d530cf759f8bf7f7fe0dad928cb88582aeb8ee89a92d0a66601908182fa4'),
    (ACTION, 'contact_stamp', r'void\s+OnHitTrigger\s*\(', 'da53e74104d39211fca308fe15bb5022f4d880f480f34ed7d2baea2e4780fa33'),
    (ACTION, 'action_update_order', r'public\s+override\s+bool\s+Update\s*\(', '6c73f8cf99cf79294275cce6db44335f4e4aebf98dee562a1d004f1d3e36da44'),
    (TRIGGER, 'trigger_before_skill', r'void\s+OnHit\s*\(PEDefenceTrigger', 'f20b6c9f183872850aecbe2c92680b805581ffab366ee03d94da557ce842757d'),
    (CREATOR, 'player_creation', r'public\s+PeEntity\s+CreatePlayer\s*\(', '428c45f261426f1b68146920dc107c17786f10ba7c0d44af7b8543e2bc3c9adc'),
    (PLAYER, 'input_attack_direction', r'void\s+UpdateOtherAction\s*\(', 'bf1c093b9cefe61d53060fa3d97b81c96b0f13318a4fc22de95b53a9ccadebac'),
    (ACTION, 'initial_attack_direction', r'public\s+override\s+void\s+DoAction\s*\(', '1b1d3545ed3b0b5ddbfe1df63091df0320374522c251f48db9234190633a9715'),
    (ACTION, 'continued_attack_direction', r'public\s+bool\s+CheckContinueAttack\s*\(', '258d19a41fdcc51b293b8e6e400beecfc24f8cb7792b597237394626761fa271'),
    (ACTION, 'attack_facing_update', r'bool\s+UpdateSkillState\s*\(', '97f3cd0ce820e913238f98a22fddff1adb11d042295c3132ab9c33d00a864c4b'),
)
# Exact IEEE754 source-prefab values; tangentMode is editor metadata, absent
# from installed runtime keys. Stored slopes remain authoritative.
EXPECTED_KEYS = (
    (0., 1., -7.586142539978027, -7.586142539978027, 0),
    (.09999999403953552, .009999999776482582, 0., -0., 17),
    (.25, .009999999776482582, 0., 6.207302093505859, 5),
    (.75, 1.2999999523162842, 1.1442314386367798, 1.1442314386367798, 0),
    (.9995110630989075, .9999892115592957, 0., 0., 0),
)
KEY_FIELDS = ('time', 'value', 'inSlope', 'outSlope', 'tangentMode')


def validate_sources(root):
    # Legacy MainPlayer comments contain non-UTF8 bytes; contracted methods
    # use ASCII tokens. Provenance continues to hash the exact original bytes.
    texts = {p: uncomment((root/p).read_text(encoding='utf-8-sig',errors='replace')) for p in (ACTION, EQUIP, TRIGGER, CREATOR, PLAYER, SETTINGS)}
    for path, label, pattern, expected in CONTRACTS:
        if hashlib.sha256(body(texts[path], pattern).encode()).hexdigest() != expected:
            raise ValueError(label+' source contract changed')
    compact = {p: re.sub(r'\s+', '', t) for p,t in texts.items()}
    for path, fragments in {
        EQUIP: ('[SerializeField]Action_SwordAttackm_SwordAttack;', 'm_SwordAttack.m_UseStamina=isMainPlayer;', 'm_MotionMgr.AddAction(m_SwordAttack);'),
        CREATOR: ('publicconststringPlayerPrefabPath="EntityPlayer";',),
        ACTION: ('floatanimScaleStartTime;',),
        SETTINGS: ('publicboolAttackWhithMouseDir=false;',),
    }.items():
        if any(s not in compact[path] for s in fragments):
            raise ValueError('Player action binding changed: '+path)
    if len(re.findall(r'animScaleStartTime\s*=', texts[ACTION])) != 1:
        raise ValueError('Playback timestamp gained a reset/assignment')
    return [describe_source(root/p) for p in SOURCE_FILES]


def qualify(objects, guid):
    matches = [(pid,o['data']) for pid,o in objects.items() if o['kind']=='MonoBehaviour' and o.get('script_guid')==guid]
    if len(matches)!=1: raise ValueError('Unique source Motion_Equip required')
    pid, component = matches[0]
    if pid!=11400022 or component['m_Enabled']!=1 or pointer(component['m_GameObject'])!=100000:
        raise ValueError('Player equipment component binding changed')
    root = objects[100000]
    if root['kind']!='GameObject' or root['data']['m_Name']!='EntityPlayer' or not root['data']['m_IsActive']:
        raise ValueError('Player root binding changed')
    action = component['m_SwordAttack']; curve = action['animScaleCurve']
    keys = [tuple(k[f] for f in KEY_FIELDS) for k in curve['m_Curve']]
    if len(keys)!=5 or any(struct.pack('<4fi',*a)!=struct.pack('<4fi',*b) for a,b in zip(keys,EXPECTED_KEYS)):
        raise ValueError('Player contact playback keys/tangents changed')
    if curve['m_PreInfinity']!=2 or curve['m_PostInfinity']!=2 or action['animScaleTime']!=struct.unpack('<f',struct.pack('<f',.65))[0]:
        raise ValueError('Player playback duration/infinity changed')
    if action['m_PauseActions']!=[] or action['m_MoveAttackSpeed']!=4.5 or action['m_SprintAttackSpeed']!=7 or action['m_RotateAcc']!=60:
        raise ValueError('Supported player attack selection changed')
    return dict(component_path_id=pid, game_object_path_id=100000, script_guid=guid,
                action_field='m_SwordAttack', duration_seconds=action['animScaleTime'],
                curve=curve, serialized_use_stamina=action['m_UseStamina'], action=action)


def source_definition(root=ROOT):
    provenance = validate_sources(root)
    match = re.search(r'^guid: ([a-f0-9]{32})$', (root/(EQUIP+'.meta')).read_text(), re.M)
    if not match: raise ValueError('Missing Motion_Equip GUID')
    return qualify(read_prefab(root/PREFAB), match[1]), provenance


def verify_installed(game, definition):
    from export_reference_player import Assets
    unity=game/'PE_Client_Data/Managed/UnityEngine.dll'
    if hashlib.sha256(unity.read_bytes()).hexdigest()!=UNITY_SHA256:
        raise ValueError('Unqualified UnityEngine API; inspect AnimationCurve again')
    assets=Assets(game/'PE_Client_Data')
    manager,=[o for o in assets.file('mainData').objects.values() if o.type.name=='ResourceManager']
    roots=[assets.resolve(manager,p) for name,p in manager.read_typetree()['m_Container'] if name=='entityplayer']
    root,=[o for o in roots if o.type.name=='GameObject']
    components=[]
    for o in assets.components(root,'MonoBehaviour'):
        h=o.read_typetree(check_read=False)
        script=assets.resolve(o,h['m_Script']).read_typetree()
        if script['m_ClassName']=='Motion_Equip' and script['m_Namespace']=='Pathea':components.append(o)
    component,=components; h=component.read_typetree(check_read=False)
    if root.path_id!=19445 or component.path_id!=326125 or not h['m_Enabled']:
        raise ValueError('Installed player equipment binding changed')
    raw=component.get_raw_data()
    # Header32, preceding two serialized action pause arrays empty; sword starts40.
    # Runtime Unity5 curve keys store four floats; editor tangentMode is stripped.
    a=definition['action']
    expected=struct.pack('<3I3f3I3fI',0,0,0,a['m_MoveAttackSpeed'],a['m_SprintAttackSpeed'],a['m_RotateAcc'],
                         a['m_UseStamina'],a['m_AttackLayer']['m_Bits'],a['firstPersonAttack'],
                         a['m_AttackHeight'],a['m_LockMaxAngle'],a['m_AttackChecRange'],5)
    expected+=b''.join(struct.pack('<4f',*(k[f] for f in KEY_FIELDS[:4])) for k in a['animScaleCurve']['m_Curve'])
    expected+=struct.pack('<2I2f',2,2,a['animScaleTime'],a['animLerpF'])
    if len(raw)!=540 or raw[32:180]!=expected:
        raise ValueError('Installed serialized sword action differs from source')
    paths=[game/'PE_Client_Data'/root.assets_file.name, game/'PE_Client_Data/mainData', unity]
    return dict(resource='entityplayer', root_path_id=root.path_id, component_path_id=component.path_id,
                asset=component.assets_file.name, verified_payload_range=[32,180],
                payload_sha256=hashlib.sha256(raw[32:180]).hexdigest(),
                inputs=[dict(file=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in paths],
                limitation='Read-only serialized binding validation; AnimationCurve.Evaluate was not executed. Separate pinned native arithmetic evidence is linked in generated metadata.')


def f(value):
    s=format(value,'.9g')
    return (s if any(x in s for x in '.eE') else s+'.0')+'f'


def build_outputs(root=ROOT):
    d, provenance=source_definition(root)
    keys=d['curve']['m_Curve']
    manifest=dict(schema=1, source_files=provenance, binding={k:v for k,v in d.items() if k!='action'},
        source_method_guards={label:digest for _,label,_,digest in CONTRACTS},
        engine_api=dict(unity_engine_sha256=UNITY_SHA256,
            method='UnityEngine.AnimationCurve.Evaluate(single)', implementation_flags=4096,
            managed_body=False, evaluated=False),
        interpolation_evidence=dict(audit='docs/audit/UNITY5_CURVE_EVALUATOR.md',
            result='docs/reference/unity5-curve-arithmetic.json',
            tool='rebuild/tools/qualify_unity5_curve_arithmetic.py',
            binary_sha256='1eb5ad6ff19dddb960a71453603ed7c67403403186c9dfeadc798a6021c4d2a6',
            compared_points=4109, exact_float_matches=4109,
            scope='Separate isolated replay of call-free native selector/coefficient/Horner instructions on Linux x86-64; static-disassembly clamp dispatch. No initialized engine or gameplay execution.'),
        facing=dict(rotate_acceleration=d['action']['m_RotateAcc'],class_default_rotate_acceleration=3,
            mouse_direction_default=False,attack_direction='normalized accepted movement; zero falls back to body forward',
            no_target_rotation='Quaternion.Lerp(current, LookRotation(attack direction), rotate acceleration * Time.deltaTime)',
            active_target_rotation='Immediate horizontal LookRotation of frozen stage target direction',
            qualification='C# branches and serialized source/optional installed binding; no native Quaternion.Lerp execution.'),
        rule=dict(low_stamina_factor=.9,minimum_base_speed=.5,maximum_base_speed=1.5,
            elapsed='float(Time.time - animScaleStartTime)', curve_parameter='elapsed / duration_seconds',
            response_active='elapsed < duration_seconds', clamp_before_curve=True, clamp_after_curve=False,
            resets_on='eligible non-self defence-trigger contact before skill CollisionCheck',
            action_start_resets=False, combo_resets=False, action_end_resets=False,
            initial_timestamp=0, clock='Unity process Time.time; not authored clip time'),
        qualification=dict(serialized_keys='Source prefab exact key values, tangents and raw infinity codes recovered.',
            installed_verification='Use --game to require modern ResourceManager runtime payload agreement.',
            interpolation='Separate pinned native-binary disassembly and isolated arithmetic replay qualify finite unweighted cubic Hermite operation order and start-inclusive/end-exclusive segment selection; this exporter does not run that qualification.',
            engine_evaluate='Installed UnityEngine.AnimationCurve.Evaluate has internal-call implementation flag0x1000; no managed body.',
            infinity='Separate raw-deserializer and evaluator disassembly establishes serialized infinity2 clamps. It must not be cast to public WrapMode2, which means loop.',
            tail='At normalized time>=0.999511063, clamp to stored last value0.999989212. Source action independently uses base speed once elapsed>=duration. Tail dispatch is statically qualified, not executed by the arithmetic replay.',
            startup='Timestamp default0 permits response during first duration of process even without contact; native epoch ownership must be explicit.'))
    header='''/* Generated by export_attack_playback.py; do not edit. */
#ifndef PE_ATTACK_PLAYBACK_SOURCE_H
#define PE_ATTACK_PLAYBACK_SOURCE_H
#define PE_ATTACK_PLAYBACK_KEY_COUNT 5u
typedef struct {float time,value,in_slope,out_slope;int tangent_mode;} PeAttackPlaybackKey;
typedef struct {float duration_seconds;int pre_infinity,post_infinity;
    float low_stamina_factor,minimum_base_speed,maximum_base_speed;} PeAttackPlaybackSource;
static const PeAttackPlaybackKey pe_attack_playback_keys[PE_ATTACK_PLAYBACK_KEY_COUNT]={
'''
    header+=''.join('    {'+','.join(f(k[field]) for field in KEY_FIELDS[:4])+','+str(k['tangentMode'])+'},\n' for k in keys)
    header+='};\nstatic const PeAttackPlaybackSource pe_attack_playback_source={'+f(d['duration_seconds'])+',2,2,.9f,.5f,1.5f};\n'
    header+='#define PE_SWORD_ROTATE_ACCELERATION '+f(d['action']['m_RotateAcc'])+'\n#endif\n'
    return {'pe_attack_playback_source.h':header.encode(), 'pe_attack_playback_source.json':(json.dumps(manifest,indent=2)+'\n').encode()}


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true')
    p.add_argument('--output',type=Path,default=ROOT/'rebuild/generated');p.add_argument('--game',type=Path)
    p.add_argument('--installed-report',type=Path)
    a=p.parse_args(argv)
    try:
        outputs=build_outputs()
        if a.installed_report and not a.game:raise ValueError('--installed-report requires --game')
        if a.game:
            report=verify_installed(a.game,source_definition()[0])
            if a.installed_report:
                if a.installed_report.exists():raise ValueError('Fresh installed report path required')
                a.installed_report.parent.mkdir(parents=True,exist_ok=True)
                a.installed_report.write_text(json.dumps(report,indent=2)+'\n')
        for name,data in outputs.items():
            path=a.output/name
            if a.check:
                if not path.is_file() or path.read_bytes()!=data:raise ValueError('Stale generated '+name)
            else:a.output.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        print('Player attack playback metadata '+('checked' if a.check else 'generated'))
        return 0
    except (OSError,ValueError,KeyError,ImportError) as error:
        print(error,file=sys.stderr);return 1


if __name__=='__main__':raise SystemExit(main())
