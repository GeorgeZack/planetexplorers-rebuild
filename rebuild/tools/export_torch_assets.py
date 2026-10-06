#!/usr/bin/env python3
"""Recover Torch10 visuals and current-source deployment/light metadata.

UnityPy is optional tooling, never a runtime dependency. Installed legacy bundle
provides the static mesh and original textures; current Unity source prefab and
scripts define gameplay. Inputs are read-only. Runtime outputs stay ignored.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import tempfile
from export_reference_assets import recover,pointer,multiply,trs,transform_point,property_map
ROOT=Path(__file__).resolve().parents[2]
PREFAB=ROOT/'Assets/Resources/Prefab/Item/Equip/Other/torch.prefab'
BUNDLE=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers/AssetBundles/Item/torch.unity3d'
OUTPUT=ROOT/'rebuild/build/reference-assets/torch'
GENERATED=ROOT/'rebuild/generated'
SCRIPTS={
 'drag':'Assets/Scripts/Items/DrawItem/ItemDragging/ItemDraggingTorch.cs',
 'drag_base':'Assets/Scripts/Items/DrawItem/ItemDragging/ItemDraggingBase.cs',
 'construct':'Assets/Scripts/Items/DrawItem/DragItemNonLogic/ItemScript_Torch.cs',
 'pickup':'Assets/Scripts/Items/DrawItem/DragItemMousePick/DragItemMousePickItemList.cs',
 'range':'Assets/Scripts/MousePicker/MousePickable.cs',
 'burn':'Assets/Scripts/Lilj/torch.cs'}
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def tree_guid(external):
    raw=bytes(external.guid).hex()
    return ''.join(raw[i+1]+raw[i] for i in range(0,32,2))
def source():
    import UnityPy
    env=UnityPy.load(str(PREFAB));file=next(iter(env.files.values()))
    objects={o.path_id:o for o in env.objects};trees={i:o.read_typetree() for i,o in objects.items() if o.type.name in ('GameObject','Transform','MonoBehaviour','CapsuleCollider','Light','MeshFilter','ParticleSystem','ParticleSystemRenderer')}
    root=trees[400010]
    if root['m_LocalPosition']!={'x':0.,'y':0.,'z':0.} or root['m_LocalRotation']!={'x':0.,'y':0.,'z':0.,'w':1.} or root['m_LocalScale']!={'x':1.,'y':1.,'z':1.}:raise ValueError('Torch source local root changed')
    drag,script,pick=trees[11458228],trees[11489594],trees[11402462]
    if drag['DraggingDistance']!=10 or pointer(drag['itemBounds']) or pick['operateDistance']!=4 or script['m_items']!=[{'protoId':1001,'count':1}] or script['m_Colliders']:raise ValueError('Torch placement/pickup contract changed')
    expected={11402462:'Assets/Scripts/Items/DrawItem/DragItemMousePick/DragItemMousePickItemList.cs',11458228:SCRIPTS['drag'],11489594:SCRIPTS['construct']}
    for identity,path in expected.items():
        guid=tree_guid(file.externals[trees[identity]['m_Script']['m_FileID']-1]);meta=(ROOT/(path+'.meta')).read_text()
        if 'guid: '+guid not in meta:raise ValueError('Torch script identity changed')
    colliders=[]
    for identity in (13664468,13665548):
        c=trees[identity]
        if not c['m_IsTrigger'] or not c['m_Enabled'] or c['m_Direction']!=2 or abs(c['m_Radius']-.08)>1e-6 or abs(c['m_Height']-.7)>1e-6 or c['m_Center']['x'] or c['m_Center']['y'] or abs(c['m_Center']['z']-.2)>1e-6:raise ValueError('Torch query capsule changed')
        colliders.append(dict(component=identity,center=[c['m_Center'][k] for k in 'xyz'],radius=c['m_Radius'],height=c['m_Height'],axis=2,trigger=True))
    transforms={i:t for i,t in trees.items() if objects[i].type.name=='Transform'};by_owner={pointer(t['m_GameObject']):i for i,t in transforms.items()}
    def world(identity):
        t=transforms[identity];local=trs(t['m_LocalPosition'],t['m_LocalRotation'],t['m_LocalScale']);parent=pointer(t['m_Father'])
        return multiply(world(parent),local) if parent else local
    light=trees[10800000];light_pos=transform_point(world(by_owner[pointer(light['m_GameObject'])]),[0,0,0])
    if light['m_Type']!=2 or light['m_Range']!=20 or abs(light['m_Intensity']-1.2)>1e-6:raise ValueError('Torch point light changed')
    db=ROOT/'Assets/Resources/DataBase/localData.bytes'
    with sqlite3.connect(db.resolve().as_uri()+'?mode=ro',uri=True) as connection:
        row=connection.execute('SELECT _modelPath,stacking_num,_durability FROM PrototypeItem WHERE CAST(id AS INTEGER)=10').fetchone()
    if row!=('Prefab/Item/Equip/Other/torch','1','1500'):raise ValueError('Torch item identity changed')
    physics_path=ROOT/'ProjectSettings/DynamicsManager.asset'
    physics=next(o.read_typetree() for o in UnityPy.load(str(physics_path)).objects)
    return dict(schema=1,item_id=10,prefab=dict(path=str(PREFAB.relative_to(ROOT)),sha256=sha(PREFAB)),
        database=dict(path=str(db.relative_to(ROOT)),sha256=sha(db)),
        scripts={name:dict(path=path,sha256=sha(ROOT/path)) for name,path in SCRIPTS.items()},
        placement=dict(max_distance=10,ray_distance=100,rotation_degrees=[-90,0,0],item_bounds=False,physical_blocker=False),
        pickup=dict(max_bounds_distance=4,root_coarse_axes_distance=5,reward_item=1001,reward_count=1,destroys_original_instance=True),
        query_capsules=colliders,light=dict(position=light_pos,color=[light['m_Color'][k] for k in 'rgba'],range=light['m_Range'],intensity=light['m_Intensity']),
        burn=dict(fuel_timer=False,water_extinguish_depth=.5),
        physics=dict(path=str(physics_path.relative_to(ROOT)),sha256=sha(physics_path),gravity=physics['m_Gravity']),
        flame_position=transform_point(world(400006),[0,0,0]),
        particle_systems={str(i):t for i,t in trees.items() if objects[i].type.name in ('ParticleSystem','ParticleSystemRenderer')},
        limitation='Particle curves are source metadata, not a complete Unity particle-system simulation. Old bundle gameplay is not used.')
def generated_outputs(data):
    l=data['light'];capsule=data['query_capsules'][0]
    def f(value):
        v=format(value,'.9g');return v+('f' if '.' in v or 'e' in v else '.0f')
    lines=['/* Generated by tools/export_torch_assets.py; do not edit. */','#ifndef PE_TORCH_SOURCE_H','#define PE_TORCH_SOURCE_H','#define PE_TORCH_ITEM 10','#define PE_TORCH_GET_ITEM 1001','#define PE_TORCH_GET_COUNT 1','#define PE_TORCH_PLACE_DISTANCE 10.0f','#define PE_TORCH_QUERY_DISTANCE 4.0f','#define PE_TORCH_RAY_DISTANCE 100.0f']
    for axis,value in zip('XYZ',l['position']):lines.append(f'#define PE_TORCH_LIGHT_POSITION_{axis} {f(value)}')
    for axis,value in zip('XYZ',data['flame_position']):lines.append(f'#define PE_TORCH_FLAME_POSITION_{axis} {f(value)}')
    lines.append(f'#define PE_TORCH_PARTICLE_GRAVITY_Y {f(data["physics"]["gravity"]["y"])}')
    for channel,value in zip('RGBA',l['color']):lines.append(f'#define PE_TORCH_LIGHT_{channel} {f(value)}')
    lines += [f'#define PE_TORCH_LIGHT_RANGE {f(l["range"])}',f'#define PE_TORCH_LIGHT_INTENSITY {f(l["intensity"])}',f'#define PE_TORCH_CAPSULE_RADIUS {f(capsule["radius"])}',f'#define PE_TORCH_CAPSULE_HEIGHT {f(capsule["height"])}',f'#define PE_TORCH_CAPSULE_CENTER_Z {f(capsule["center"][2])}']
    flame=data['particle_systems']['19800004'];initial=flame['InitialModule']
    scalars={'LIFETIME':initial['startLifetime']['scalar'],'SPEED':initial['startSpeed']['scalar'],
        'SIZE':initial['startSize']['scalar'],'GRAVITY_MODIFIER':initial['gravityModifier'],
        'RATE':flame['EmissionModule']['rate']['scalar'],'RADIUS':flame['ShapeModule']['radius']}
    for name,value in scalars.items():lines.append(f'#define PE_TORCH_FLAME_{name} {f(value)}')
    lines.append(f'#define PE_TORCH_FLAME_MAX_PARTICLES {initial["maxNumParticles"]}')
    color=initial['startColor']['maxColor']['rgba']
    lines.append('static const unsigned char pe_torch_flame_color[4]={'+','.join(str((color>>shift)&255) for shift in (0,8,16,24))+'};')
    size=flame['SizeModule']['curve']['maxCurve']['m_Curve']
    if len(size)!=2 or any(row['inSlope'] or row['outSlope'] for row in size):raise ValueError('Unsupported source flame size curve')
    lines.append('static const float pe_torch_flame_size[2][2]={'+','.join('{'+f(row['time'])+','+f(row['value'])+'}' for row in size)+'};')
    alpha=flame['ColorModule']['gradient']['maxGradient']
    lines.append('static const float pe_torch_flame_alpha[][2]={'+','.join('{'+f(alpha['atime'+str(i)]/65535)+','+f((alpha['key'+str(i)]['rgba']>>24)/255)+'}' for i in range(alpha['m_NumAlphaKeys']))+'};')
    lines+=['#endif','']
    return {'pe_torch_source.h':'\n'.join(lines).encode(),'pe_torch_source.json':(json.dumps(data,indent=2)+'\n').encode()}
def export_assets(bundle,output,data):
    import UnityPy
    output.mkdir(parents=True,exist_ok=True);visual=recover(bundle,'torch',output,True)
    if len(visual['parts'])!=1 or visual['parts'][0]['collision'] or visual['parts'][0]['matrix']!=[[1.,0.,0.,0.],[0.,1.,0.,0.],[0.,0.,1.,0.],[0.,0.,0.,1.]]:raise ValueError('Torch visual no longer raw item-local mesh')
    (output/'torch-visual.json').write_text(json.dumps(visual,indent=2)+'\n')
    env=UnityPy.load(str(bundle));objects={o.path_id:o for o in env.objects};effects={}
    for o in objects.values():
        if o.type.name!='Material':continue
        material=o.read_typetree();name=material['m_Name']
        if name not in ('fx_flame01','fx_rubble02','fx_smoke02'):continue
        saved=material['m_SavedProperties'];main=property_map(saved.get('m_TexEnvs',[])).get('_MainTex');texture=objects[pointer(main['m_Texture'])] if main else None
        if not texture or texture.type.name!='Texture2D':raise ValueError('Missing original torch particle texture')
        filename=name+'.png';texture.read().image.save(output/filename)
        effects[name]=dict(texture=filename,material_path_id=o.path_id,texture_path_id=texture.path_id,properties=saved)
    if len(effects)!=3:raise ValueError('Incomplete original particle texture set')
    manifest=dict(schema=1,source=data,visual=visual,particle_materials=effects,files={p.name:sha(p) for p in sorted(output.iterdir()) if p.is_file() and p.name!='torch.json'})
    (output/'torch.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bundle',type=Path,default=BUNDLE);p.add_argument('--output',type=Path,default=OUTPUT);p.add_argument('--generated',type=Path,default=GENERATED);p.add_argument('--source-only',action='store_true');p.add_argument('--check',action='store_true');a=p.parse_args()
    data=source();outputs=generated_outputs(data)
    for name,content in outputs.items():
        path=a.generated/name
        if a.check:
            if not path.is_file() or path.read_bytes()!=content:raise ValueError('Stale '+str(path))
        else:path.write_bytes(content)
    if not a.source_only:
        if a.check:
            with tempfile.TemporaryDirectory(prefix='pe-torch-') as temp:
                manifest=export_assets(a.bundle,Path(temp),data)
                for name in list(manifest['files'])+['torch.json']:
                    if (Path(temp)/name).read_bytes()!=(a.output/name).read_bytes():raise ValueError('Stale torch asset '+name)
        else:export_assets(a.bundle,a.output,data)
    print('Torch source/assets checked' if a.check else 'Torch source/assets exported')
if __name__=='__main__':main()
