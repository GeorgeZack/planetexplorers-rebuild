#!/usr/bin/env python3
"""Guard four fixed native vegetation examples against original source metadata."""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import sys

from export_content import ROOT, DEFAULT_DATABASE, describe_source, numeric_rows, c_float
from export_starter_outfit import body, uncomment

PROTOTYPE_PREFAB='Assets/PeLauncher/Resources/VETreeProtos.prefab'
GATHER='Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Gather.cs'
RANDOM='Assets/Scripts/ZhouXun/Random SubTerrain/RSubTerrainMgr.cs'
NATURAL='Assets/Scripts/NaturalRes/NaturalRes.cs'
DIG='Assets/Scripts/BuildNDig/DigTerrainManager.cs'
SKILL='Assets/Scripts/SkillSystem/SkEntitySubTerrain.cs'
QUALIFIED=({'prototype_id': 15,
  'prefab': 'Assets/Prefabs/tree-Ambient-Occlusion/tree15_grasslandC.prefab',
  'model': 'plant-15',
  'triangles': 522,
  'minimum': [-0.37289829552173615, -0.11479967832565308, -0.3285984396934509],
  'maximum': [0.3728983849287033, 0.9400627017021179, 0.31728029251098633],
  'cutoff': 0.6499999761581421,
  'fixture_item_id': 238,
  'fixture_count': 2,
  'harvestable': True,
  'resource_contract': {'type': '10',
                        'durability': '255',
                        'production': '238,100;0.03,943,100;',
                        'fixed_num': '2',
                        'self_num': '0',
                        'gather_skill': '10007'},
  'source_mesh_guid': 'eafb7152fc8f2f64b87c4cba2f6d9984',
  'source_mesh_path_id': 4300000,
  'installed_game_object': 18563,
  'installed_mesh': 7004,
  'installed_file': 'resources.assets',
  'installed_mesh_sha256': '76dbe66854b3c4466ccee23eaf18413f79b567efdf3bdf36a4303a88105589ff',
  'installed_material_sha256': '4362d72a13e08c57eb24dd8ef68085bbbd46659e0ea3ab904c6999e359ca13bb',
  'installed_texture_sha256': 'b27e11db5e985917b3a637e62c0bdde18386ff50083ab29cc8efd899c8ec03d2',
  'installed_shader_sha256': '76e8edb160964b39a31faececde24f177cd9a075e2d3aa6c6350263dc00292ce',
  'obj_sha256': '3d1eb95d4eebbe563b1fc44ca8301a0f22248d27e3b472269bb0b0c10fb3e7f1',
  'mtl_sha256': 'c7e990ab5c185b8b4d1305dc444f623874002ff4410e47b8162c65ff9e14441c',
  'two_sided': True},
 {'prototype_id': 37,
  'prefab': 'Assets/Prefabs/tree-Ambient-Occlusion/tree08_grassland.prefab',
  'model': 'plant-37',
  'triangles': 165,
  'minimum': [-0.5677318572998047, -0.051777809858322144, -0.5666250288486481],
  'maximum': [0.5662540197372437, 1.0357666313648224, 0.5673609673976898],
  'cutoff': 0.6499999761581421,
  'fixture_item_id': 235,
  'fixture_count': 3,
  'harvestable': True,
  'resource_contract': {'type': '10',
                        'durability': '255',
                        'production': '235,70,1583,100;0.1,388,50,940,75,1583,100;',
                        'fixed_num': '3',
                        'self_num': '0',
                        'gather_skill': '10007'},
  'source_mesh_guid': 'c1444f8ad9217a44e881e4c2b2a94465',
  'source_mesh_path_id': 4300000,
  'installed_game_object': 18545,
  'installed_mesh': 6807,
  'installed_file': 'resources.assets',
  'installed_mesh_sha256': 'b7f94b4b291c1b6b746678f11fa7c5c20c30b87d6fb0e494725a1269a08d873b',
  'installed_material_sha256': '4362d72a13e08c57eb24dd8ef68085bbbd46659e0ea3ab904c6999e359ca13bb',
  'installed_texture_sha256': 'b27e11db5e985917b3a637e62c0bdde18386ff50083ab29cc8efd899c8ec03d2',
  'installed_shader_sha256': '76e8edb160964b39a31faececde24f177cd9a075e2d3aa6c6350263dc00292ce',
  'obj_sha256': 'd058cc4aaa9baf011695b87795e8bef95bf10f2809221ebb04bc312461959ad4',
  'mtl_sha256': 'c7e990ab5c185b8b4d1305dc444f623874002ff4410e47b8162c65ff9e14441c',
  'two_sided': True},
 {'prototype_id': 40,
  'prefab': 'Assets/Prefabs/tree-Ambient-Occlusion/tree09_grassland.prefab',
  'model': 'plant-40',
  'triangles': 647,
  'minimum': [-1.4945176839828491, -0.08550143241882324, -1.0266589522361755],
  'maximum': [1.3110183477401733, 1.4378069639205933, 0.9934425950050354],
  'cutoff': 0.6499999761581421,
  'fixture_item_id': 239,
  'fixture_count': 4,
  'harvestable': True,
  'resource_contract': {'type': '10',
                        'durability': '255',
                        'production': '239,70,1583,100;0.1,388,50,944,75,1583,100;',
                        'fixed_num': '4',
                        'self_num': '0',
                        'gather_skill': '10007'},
  'source_mesh_guid': 'd3c2ffbe4006edc42ab6bd7184373489',
  'source_mesh_path_id': 4300000,
  'installed_game_object': 18548,
  'installed_mesh': 6896,
  'installed_file': 'resources.assets',
  'installed_mesh_sha256': '1a903b2fa4a4120e3ef9ce71af437840b30dacae59a83273e44e957447a75e01',
  'installed_material_sha256': '4362d72a13e08c57eb24dd8ef68085bbbd46659e0ea3ab904c6999e359ca13bb',
  'installed_texture_sha256': 'b27e11db5e985917b3a637e62c0bdde18386ff50083ab29cc8efd899c8ec03d2',
  'installed_shader_sha256': '76e8edb160964b39a31faececde24f177cd9a075e2d3aa6c6350263dc00292ce',
  'obj_sha256': '7cb80e5e43c54437c3d9ae0771e71e9323c204e7f5208674bdcfb7ddf766178c',
  'mtl_sha256': 'c7e990ab5c185b8b4d1305dc444f623874002ff4410e47b8162c65ff9e14441c',
  'two_sided': True},
 {'prototype_id': 146,
  'prefab': 'Assets/Prefabs/tree-Ambient-Occlusion/tree16_grassland.prefab',
  'model': 'plant-146',
  'triangles': 2282,
  'minimum': [-0.34211355447769165, -0.20074651390314102, -0.018277212977409363],
  'maximum': [0.2889757752418518, 0.204245887696743, 0.24768169224262238],
  'cutoff': 0.6499999761581421,
  'fixture_item_id': 0,
  'fixture_count': 0,
  'harvestable': False,
  'resource_contract': {'type': '10',
                        'durability': '255',
                        'production': '1006,50,1583,100;;',
                        'fixed_num': '3',
                        'self_num': '0',
                        'gather_skill': '10007'},
  'source_mesh_guid': 'd3e592e6c701bfe418b6f3a85116de7c',
  'source_mesh_path_id': 4300000,
  'installed_game_object': 18566,
  'installed_mesh': 6897,
  'installed_file': 'resources.assets',
  'installed_mesh_sha256': '48b7e4e35e45ffa8784aa334c0f7147a30fb901c08bdda3286e2f0f62365168d',
  'installed_material_sha256': '4362d72a13e08c57eb24dd8ef68085bbbd46659e0ea3ab904c6999e359ca13bb',
  'installed_texture_sha256': 'b27e11db5e985917b3a637e62c0bdde18386ff50083ab29cc8efd899c8ec03d2',
  'installed_shader_sha256': '76e8edb160964b39a31faececde24f177cd9a075e2d3aa6c6350263dc00292ce',
  'obj_sha256': 'b0585d879e8bf6d0b7d073b323e9dfb692b48fc1487b1e4e14e00afee8575e21',
  'mtl_sha256': 'c7e990ab5c185b8b4d1305dc444f623874002ff4410e47b8162c65ff9e14441c',
  'two_sided': True})
INPUT_PINS={'Assets/PeLauncher/Resources/VETreeProtos.prefab': '0ba32b30fe8fc0807979125fdb34263aefaef43d3cac9f2d64c3fd5f7d9f585b',
 'Assets/Prefabs/tree-Ambient-Occlusion/tree15_grasslandC.prefab': 'bf47779449ef367ca79deb0ace4be11b0e634b241c5ce1460899048c9a143d0f',
 'Assets/Prefabs/tree-Ambient-Occlusion/tree08_grassland.prefab': 'e2a99107b676bf6aec1952cf5cd86e487067cc717118532280a4e4bfd5ed7f19',
 'Assets/Prefabs/tree-Ambient-Occlusion/tree09_grassland.prefab': '8a4b5fbf6c9e9c095cd7bcb4773d8ef66919b25df8ea4f0f4fdfdf7b6fa0ddc2',
 'Assets/Prefabs/tree-Ambient-Occlusion/tree16_grassland.prefab': '2a637cfdee93ad4c50557fe487ce255505c5b625190ad6cd3f1b90b861a2b6a4'}
GUARDS=(('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Gather.cs',
  'public\\s+class\\s+Action_Gather',
  '0db18c20cfe28c2d4bbcd4192bfe890dcb2b2f65d2785145af9aa13b6ec32fa6'),
 ('Assets/Scripts/ZhouXun/Random SubTerrain/RSubTerrainMgr.cs',
  'public\\s+static\\s+List<GlobalTreeInfo>\\s+Picking\\(Vector3',
  '5e89686ee9bbe22d985f6519439a372639059ab4b3ff8c096351dc15a15ec65c'),
 ('Assets/Scripts/ZhouXun/Random SubTerrain/RSubTerrainMgr.cs',
  'void\\s+Init\\s*\\(',
  '30dc7868e054edc04e85b79ac3b3d8be9723b46b366d1278e43aa5d091f1643f'),
 ('Assets/Scripts/NaturalRes/NaturalRes.cs',
  'public\\s+class\\s+NaturalRes',
  'e4cd22c2e0166bd4436ce97d9824b71653b1360deaf823c0c9989a3fcf88b43d'),
 ('Assets/Scripts/BuildNDig/DigTerrainManager.cs',
  'internal\\s+static\\s+Dictionary<int,\\s*int>\\s+GetTreeResouce\\s*\\(',
  '1e22be50258f41c3f4cec33d5c8686493d30bde72fa80898c7f1982f5e524bfe'),
 ('Assets/Scripts/BuildNDig/DigTerrainManager.cs',
  'internal\\s+static\\s+float\\s+Fell\\s*\\(',
  '0aadf0c7e84b51218bc784808a785eca42eb3a80546afba1da41d66bb6da2d12'),
 ('Assets/Scripts/SkillSystem/SkEntitySubTerrain.cs',
  'private\\s+void\\s+onAlterAttribs\\s*\\(',
  '73d51b8489ed1afb6517181e4b023eb97faeab9bebe72c98ff00c7e50e3cd05b'),
 ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/PEAction.cs',
  'public\\s+class\\s+ActionRelationData',
  '7782ec294c2a663dae0a123986a5e038aa82e832a42c0111ad3990f45c839c6b'),
 ('Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs',
  'public\\s+bool\\s+CanDoAction\\s*\\(',
  '00c7f04a757cc7dcfa09b31bbdb181076487a878006cff9109240b29a53227a8'),
 ('Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs',
  'public\\s+bool\\s+DoAction\\s*\\(',
  'f58a6d9b88dc13c74170eeb3c230d55da2a7258d8f45ed591d6bdddaba54bd85'),
 ('Assets/Scripts/PeEntity/Cmpt/Motion/MotionMgrCmpt.cs',
  'bool\\s+CheckDepend\\s*\\(',
  '6d799610a2c704a14f1036b9396ddc54d918d37817afeeb650248870ee564ffb'))
GATHER_RELATION={'ActionType': '32',
 'Name': 'Gather',
 'PauseAction': '40',
 'EndAction': '4,12,13,15,28,47,58',
 'EndImmediately': '1,2,3,14,30,31,42,45,49,50,60,61',
 'Ground': '0',
 'InAir': '0',
 'InWater': '0',
 'Rotate': '2',
 'Sprint': '0',
 'Jump': '0',
 'Repulsed': '2',
 'Wentfly': '2',
 'Knocked': '2',
 'Death': '2',
 'GetUp': '2',
 'Revive': '2',
 'Step': '2',
 'HoldEquip': '0',
 'SwordAttack': '2',
 'HoldShield': '0',
 'Halt': '2',
 'Fall': '2',
 'GunHold': '0',
 'GunReload': '2',
 'GunShoot': '2',
 'BowHold': '0',
 'BowShoot': '2',
 'BowReload': '2',
 'Sleep': '2',
 'Eat': '2',
 'HoldAimEquip': '0',
 'Dig': '2',
 'Fell': '2',
 'Gather': '0',
 'PickUpItem': '2',
 'GetOnVehicle': '0',
 'JetPack': '0',
 'Parachute': '0',
 'Glider': '2',
 'Climb': '0',
 'Build': '2',
 'HoldFlashlight': '0',
 'Stuned': '2',
 'Pump': '2',
 'Sit': '2',
 'Operation': '2',
 'Draw': '2',
 'Throw': '2',
 'Lie': '2',
 'GetOnTrain': '2',
 'Cutscene': '2',
 'Whacked': '2',
 'Cure': '2',
 'HeavyEquipment': '0',
 'DrillingDig': '2',
 'Hand': '2',
 'Leisure': '0',
 'RopeGunShoot': '2',
 'Abnormal': '2',
 'Ride': '2'}
TEXTURE_SHA256='861aad50f2de9d28daf5bb2c7de95e49173f8944d546d9550ea3d902020c090c'


def source_text(path):
    raw=path.read_bytes()
    return raw.decode('utf-16' if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig',errors='replace')


def validate_sources(root=ROOT):
    for rel,pin in INPUT_PINS.items():
        if hashlib.sha256((root/rel).read_bytes()).hexdigest()!=pin:
            raise ValueError('Vegetation source prefab changed: '+rel)
    texts={p:uncomment(source_text(root/p)) for p,_,_ in GUARDS}
    for path,pattern,expected in GUARDS:
        if hashlib.sha256(body(texts[path],pattern).encode()).hexdigest()!=expected:
            raise ValueError('Vegetation source contract changed: '+path+' '+pattern)
    return [root/p for p in dict.fromkeys(list(INPUT_PINS)+list(texts))]


def build_outputs(root=ROOT,database=DEFAULT_DATABASE):
    sources=validate_sources(root)
    if len(QUALIFIED)!=4 or not GUARDS or not INPUT_PINS:raise ValueError('Incomplete vegetation qualification')
    with sqlite3.connect(database.resolve().as_uri()+'?mode=ro',uri=True) as db:
        db.row_factory=sqlite3.Row
        resource,_=numeric_rows(db,'resource','ID')
        relation=dict(db.execute("SELECT * FROM ActionRelation WHERE ActionType='32'").fetchone())
        if relation!=GATHER_RELATION:raise ValueError('Vegetation gather action relation changed')
        records=[]
        for q in QUALIFIED:
            r=resource[q['prototype_id']+1000]
            expected=q['resource_contract']
            if any(str(r[k])!=str(v) for k,v in expected.items()):raise ValueError('Vegetation resource contract changed')
            records.append(dict(q,resource=dict(r)))
    gameplay=['/* Generated by export_vegetation_source.py; native fixed-example policy is explicit. */',
              '#ifndef PE_VEGETATION_SOURCE_GENERATED_H','#define PE_VEGETATION_SOURCE_GENERATED_H',
              '#define PE_VEGETATION_SOURCE_COUNT 4u',
              'static const PePlantSource pe_vegetation_sources[PE_VEGETATION_SOURCE_COUNT] = {']
    visual=['/* Generated by export_vegetation_source.py from qualified original mesh metadata. */',
            '#ifndef PE_VEGETATION_VISUAL_SOURCE_GENERATED_H','#define PE_VEGETATION_VISUAL_SOURCE_GENERATED_H','#include <stdint.h>',
            'typedef struct { uint32_t prototype_id; const char *model; unsigned triangles; float cutoff; float minimum[3],maximum[3]; } PeVegetationVisualSource;',
            '#define PE_VEGETATION_VISUAL_COUNT 4u',
            'static const PeVegetationVisualSource pe_vegetation_visual_sources[PE_VEGETATION_VISUAL_COUNT] = {']
    for q in records:
        lo='{'+','.join(c_float(v) for v in q['minimum'])+'}';hi='{'+','.join(c_float(v) for v in q['maximum'])+'}'
        gameplay.append('    {%du,%d,%du,%s,%s,%s},'%(q['prototype_id'],q['fixture_item_id'],q['fixture_count'],'true' if q['harvestable'] else 'false',lo,hi))
        visual.append('    {%du,"%s",%du,%s,%s,%s},'%(q['prototype_id'],q['model'],q['triangles'],c_float(q['cutoff']),lo,hi))
    gameplay+=['};','#endif',''];visual+=['};','#endif','']
    manifest=dict(format_version=1,sources=[describe_source(database)]+[describe_source(p) for p in sources],plants=records,action_relation=relation,
                  source_gather=dict(requested_range=3,effective_horizontal_range=2,range_inclusive=True,scan_floor_cell_radius=2,source_skill=20110003,type=10),
                  adapters=['Fixed manually selected fixtures, no random world generation.',
                            'Source production probabilities and optional seed/fallback drops are owned by pe_harvest; fixture_item_id/count describe primary metadata only.',
                            '146 is decorative only although original resource1146 is gatherable.',
                            'Unit native instance transform, immediate native gathering; no full source animation/skill/timing claim.'])
    return {'pe_vegetation_source.h':'\n'.join(gameplay).encode(),
            'pe_vegetation_visual_source.h':'\n'.join(visual).encode(),
            'pe_vegetation_source.json':(json.dumps(manifest,indent=2)+'\n').encode()}


def validate_assets(directory):
    report=json.loads((directory/'manifest.json').read_text())
    if report['prototype_count']!=512 or len(report['props'])!=len(QUALIFIED):
        raise ValueError('Vegetation asset prototype count changed')
    for q,p in zip(QUALIFIED,report['props']):
        for key in ('prototype_id','model','triangles','minimum','maximum','cutoff',
                    'source_mesh_guid','source_mesh_path_id','installed_mesh_sha256',
                    'installed_material_sha256','installed_texture_sha256','installed_shader_sha256',
                    'obj_sha256','mtl_sha256'):
            if q[key]!=p[key]:raise ValueError('Vegetation asset qualification changed: '+key)
        if not p['shader_cull_statements'] or any(v!='Off' for v in p['shader_cull_statements']):
            raise ValueError('Vegetation source culling changed')
        for extension in ('obj','mtl'):
            target=directory/(q['model']+'.'+extension)
            if hashlib.sha256(target.read_bytes()).hexdigest()!=q[extension+'_sha256']:
                raise ValueError('Vegetation asset contents changed: '+target.name)
    if hashlib.sha256((directory/'grassland-tree-a.png').read_bytes()).hexdigest()!=TEXTURE_SHA256:
        raise ValueError('Vegetation texture changed')
    return True


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--check',action='store_true');p.add_argument('--check-assets',type=Path);p.add_argument('--output',type=Path,default=ROOT/'rebuild/generated')
    a=p.parse_args(argv)
    try:
        if a.check_assets:validate_assets(a.check_assets)
        for name,data in build_outputs().items():
            path=a.output/name
            if a.check:
                if not path.is_file() or path.read_bytes()!=data:raise ValueError('Stale vegetation metadata '+name)
            else:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    except (OSError,ValueError,KeyError,sqlite3.Error) as e:
        print(e,file=sys.stderr);return 1
    print('Vegetation metadata checked' if a.check else 'Vegetation metadata generated');return 0


if __name__=='__main__':raise SystemExit(main())
