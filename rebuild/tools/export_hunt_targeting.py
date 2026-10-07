#!/usr/bin/env python3
"""Recover the hunt's exact layer25 query capsule and ordered defence identity offline."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
import export_reference_actor as actor
from export_reference_assets import pointer
from export_reference_hitboxes import skeleton_data, bind_points
from export_hare_history_center import identity
from export_hare_perception import cfloat
from export_attack_playback import source_definition
from export_starter_outfit import body, uncomment

ROOT=Path(__file__).resolve().parents[2]
DEFENCE=-1768499269866288629
COLLIDER=-2101344497280277659
TRANSFORM=5682115552912406982
VIEW=1752844016538949710
FILES=['Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs','Assets/Scripts/Attack/PEDefenceTrigger.cs',
       'Assets/Scripts/Attack/PECapsuleTrigger.cs','Assets/Scripts/Force/ForceSetting.cs',
       'Assets/PeLauncher/SingleGame.cs','Assets/PeLauncher/PeGameLoader.cs',
       'Assets/Resources/ForceSetting/ForceSettings_Story.xml']
CONTRACTS=(
    (FILES[0], 'ordered_target_selection', r'void\s+ChangeAttackTarget\s*\(', 'a69dfd976f1ffdde5318e509609973d80de4b777196ac9c3c49c91e14b7a3485'),
    (FILES[1], 'ordered_defence_priority', r'public\s+bool\s+GetClosest\s*\(', '3cd453e3a0dd66f44e61315811d5105577b58de4de14afb37dfbe123a45f7a42'),
    (FILES[2], 'capsule_closest_point', r'public\s+bool\s+GetClosestPos\s*\(', 'abf9a92946122c695c34efae775b57c44604d38b0692cdd8c0dbea178b41faee'),
    (FILES[3], 'force_conflict', r'public\s+bool\s+Conflict\s*\(', '55af9a5bc9bac2b5e06ab96488891664778470fe54071a08f92a613d0f6db7a3'),
    (FILES[3], 'directional_force_alliance', r'public\s+bool\s+AllyForce\s*\(', 'faf51e862d183d95e635029cfc1307ed8c07992afbdc6e255ad8d802a1daf41a'),
    (FILES[4], 'adventure_load_chain', r'void\s+LoadAdventure\s*\(', '3041e1ac11d199e52fee1a6e726e1ce0449080b239045eea89ece0a2a1e4842b'),
    (FILES[5], 'scenario_fresh_restore_force_binding', r'abstract\s+class\s+LoadScenario\s*:\s*ModuleLoader\b', '786464822a52f2b4c8f3a7f1846997919a1744a1278e8920e712a0499fbc480e'),
    (FILES[5], 'adventure_scenario_dispatch', r'class\s+LoadRandomStory\s*:\s*LoadScenario\b', 'f69175857bac58c03a9c946714b88cc1f122d1a757485a51cd8d6549cccaaddc'),
)

def validate_sources(root):
    for path,label,pattern,expected in CONTRACTS:
        text=uncomment((root/path).read_text(encoding='utf-8-sig',errors='replace'))
        if hashlib.sha256(body(text,pattern).encode()).hexdigest()!=expected:
            raise ValueError(label+' source contract changed')

def qualify_player_query(action):
    if (action['m_AttackHeight'],action['m_AttackChecRange'],action['m_LockMaxAngle'],action['m_AttackLayer']['m_Bits'])!=(1.5,3.,115.,33554432):
        raise ValueError('Player query changed')

def qualify_forces(xml):
    players={int(p.attrib['ID']):int(p.attrib['Force']) for p in xml.findall('./Player/PlayerDesc')}
    forces={int(p.attrib['ID']):p for p in xml.findall('./Force/ForceDesc')}
    if players.get(1)!=1 or players.get(4)!=2 or 2 in [int(x) for x in forces[1].findtext('Ally').split(',')] or forces[1].findtext('EnemyConflict')!='true':
        raise ValueError('Known player/hare force conflict changed')

def qualify(objects,bones,binds):
    kind,d=objects[DEFENCE]
    if kind!='MonoBehaviour' or objects[pointer(d['m_Script'])][1].get('m_ClassName')!='PEDefenceTrigger':raise ValueError('Defence binding changed')
    go=pointer(d['m_GameObject']);g=objects[go][1]
    if g['m_Layer']!=25 or not g['m_IsActive'] or not d['m_Enabled']:raise ValueError('Defence activation/layer changed')
    _,t=objects[TRANSFORM];_,root=objects[VIEW]
    if pointer(t['m_GameObject'])!=go or pointer(t['m_Father'])!=VIEW or pointer(root['m_Father'])!=0 or not identity(t) or not identity(root):raise ValueError('Query hierarchy changed')
    if not objects[pointer(root['m_GameObject'])][1]['m_IsActive']:raise ValueError('Inactive root')
    colliders=[i for i,(k,v) in objects.items() if k.endswith('Collider') and pointer(v['m_GameObject'])==go]
    if colliders!=[COLLIDER]:raise ValueError('Expected one query collider')
    kind,c=objects[COLLIDER]
    actor.finite([c['m_Radius'],c['m_Height']])
    if kind!='CapsuleCollider' or not c['m_Enabled'] or not c['m_IsTrigger'] or c['m_Direction']!=2 or c['m_Center']!=dict(x=0.,y=.5,z=0.) or abs(c['m_Radius']-1.2)>1e-6 or abs(c['m_Height']-3.07)>1e-6:raise ValueError('Query capsule changed')
    center=pointer(d['centerBone']);parts=[]
    for p in d['defenceParts']:
        bone=pointer(p['capsule']['trans']);joint=bones.index(bone) if bone else 0xffffffff
        parts.append(dict(name=p['name'],joint=joint,center=bone==center,bind_points=bind_points(p['capsule'],binds[joint]) if bone else [[0.]*3 for _ in range(4)]))
    if len(parts)!=15 or sum(p['center'] for p in parts)!=1 or not parts[1]['center'] or parts[1]['name']!='Body1':raise ValueError('Defence centre/order changed')
    return dict(component=DEFENCE,collider=COLLIDER,transform=TRANSFORM,view_transform=VIEW,layer=25,
        radius=c['m_Radius'],height=c['m_Height'],center=[c['m_Center'][k] for k in 'xyz'],axis=c['m_Direction'],parts=parts)

def recover(game,assetdir,root=ROOT):
    import UnityPy
    validate_sources(root)
    bundle=game/actor.BUNDLE
    env=UnityPy.load(str(bundle))
    objects={o.path_id:(o.type.name,o.read_typetree()) for o in env.objects if o.type.name in ('MonoBehaviour','MonoScript','Transform','GameObject','CapsuleCollider','SphereCollider','BoxCollider','MeshCollider','SkinnedMeshRenderer','Mesh')}
    data=(assetdir/'hare.peac').read_bytes();nj,fingerprint,binds=skeleton_data(data)
    manifest=json.loads((assetdir/'hare-manifest.json').read_text())
    if manifest['source_sha256']!=hashlib.sha256(bundle.read_bytes()).hexdigest() or manifest['files']['hare.peac']['sha256']!=hashlib.sha256(data).hexdigest():raise ValueError('Actor provenance changed')
    renderer=objects[actor.RENDERER_ID][1];bones=[pointer(x) for x in renderer['m_Bones']]
    if bones!=manifest['joint_transform_path_ids'] or len(bones)!=nj:raise ValueError('Actor joint order changed')
    mesh=objects[pointer(renderer['m_Mesh'])][1]
    source_binds=[[[b[f'e{r}{c}'] for c in range(4)] for r in range(4)] for b in mesh['m_BindPose']]
    if len(source_binds)!=nj or any(struct.pack('<12f',*actor.matrix_values(a))!=struct.pack('<12f',*actor.matrix_values(b)) for a,b in zip(binds,source_binds)):raise ValueError('Actor inverse binds changed')
    result=qualify(objects,bones,binds)
    definition,player_sources=source_definition(root)
    qualify_player_query(definition['action'])
    xml=ET.fromstring((root/FILES[-1]).read_bytes().decode('gb18030'))
    qualify_forces(xml)
    result.update(schema='pe-hunt-targeting-1',source=actor.BUNDLE,source_sha256=manifest['source_sha256'],
        skeleton_hash=f'{fingerprint:016x}',player_id=1,hare_player_id=4,force_conflict=True,
        player_query=dict(radius=3.,height=1.5,angle=115.,mask=33554432),
        player_query_sources=player_sources,
        source_contracts=[dict(path=p,label=label,method_sha256=digest) for p,label,_,digest in CONTRACTS],
        sources=[dict(path=p,sha256=hashlib.sha256((root/p).read_bytes()).hexdigest()) for p in FILES],
        limitations=['Stable encounter slot order replaces unobserved Physics.OverlapSphere ordering.',
        'Inclusive analytic sphere/capsule overlap replaces PhysX broadphase; no movement capsule substitution.',
        'Present living pinned cohort only; no ragdolls, reputation/aggro inference, LOS, full aim IK or target tracking.',
        'Player query is source EntityPlayer prefab; existing installed equivalence proof is separate.'])
    return result

def header(r):
    vector=lambda v:'{'+','.join(cfloat(struct.unpack('<f',struct.pack('<f',x))[0]) for x in v)+'}'
    lines=['/* Generated by export_hunt_targeting.py; do not edit. */','#ifndef PE_HUNT_TARGETING_SOURCE_H','#define PE_HUNT_TARGETING_SOURCE_H','#include <stdint.h>',
      '#define PE_HUNT_TARGET_LAYER 25u','#define PE_HUNT_TARGET_PLAYER_ID 4','#define PE_HUNT_TARGET_PARTS 15u',
      '#define PE_HUNT_TARGET_SKELETON UINT64_C(0x'+r['skeleton_hash']+')',
      'static const float pe_hunt_target_center[3]='+vector(r['center'])+';',
      'static const float pe_hunt_target_radius='+cfloat(r['radius'])+';',
      'static const float pe_hunt_target_half_segment='+cfloat(max(0,r['height']*.5-r['radius']))+';',
      'typedef struct {const char *name;uint32_t joint;int center;float points[4][3];} PeHuntTargetPartSource;',
      'static const PeHuntTargetPartSource pe_hunt_target_parts[PE_HUNT_TARGET_PARTS]={']
    for p in r['parts']:lines.append('{'+json.dumps(p['name'])+','+str(p['joint'])+'u,'+str(int(p['center']))+',{'+','.join(vector(v) for v in p['bind_points'])+'}},')
    return '\n'.join(lines+['};','#endif',''])

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--game',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers');ap.add_argument('--actor-dir',type=Path,default=actor.DEFAULT_OUTPUT);ap.add_argument('--output',type=Path,default=ROOT/'rebuild/generated');ap.add_argument('--check',action='store_true');a=ap.parse_args()
    r=recover(a.game,a.actor_dir);outputs={'pe_hunt_targeting_source.h':header(r),'pe_hunt_targeting_source.json':json.dumps(r,indent=2,sort_keys=True)+'\n'}
    if a.output.resolve()==a.game.resolve() or a.game.resolve() in a.output.resolve().parents:raise ValueError('Output inside game')
    for name,text in outputs.items():
        p=a.output/name
        if a.check:
            if p.read_text()!=text:raise ValueError('Stale '+name)
        else:a.output.mkdir(parents=True,exist_ok=True);p.write_text(text)
    print('Hunt targeting source '+('checked' if a.check else 'exported'))
if __name__=='__main__':main()
