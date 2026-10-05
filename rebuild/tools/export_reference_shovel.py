#!/usr/bin/env python3
"""Recover source wood shovel12 visual geometry, materials and mount metadata.
Read-only source/installed assets. Optional UnityPy1.25.4, no graphics/runtime.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import tempfile
from export_reference_assets import trs, transform_point, transform_normal, floats, determinant, property_map, material_symbols
from export_reference_player import Assets
from export_training_gathering_assets import checked_mesh, check_same_transform
ROOT=Path(__file__).resolve().parents[2]
PREFAB=ROOT/'Assets/Resources/Prefab/Item/Equip/Weapon/Spade/wood_spade.prefab'
RESOURCE='Prefab/Item/Equip/Weapon/Spade/wood_spade'


def prefab_mounts(UnityPy,path):
    """Retain actual animated Main/Back mount ancestry, excluding ragdoll later."""
    env=UnityPy.load(str(path))
    names={o.path_id:o.read_typetree()['m_Name'] for o in env.objects if o.type.name=='GameObject'}
    transforms={o.path_id:o.read_typetree() for o in env.objects if o.type.name=='Transform'}
    chains=[]
    for identity,node in transforms.items():
        if names[node['m_GameObject']['m_PathID']] not in ('mountMain','mountBack'):continue
        chain=[];seen=set();cursor=identity
        while cursor:
            if cursor in seen or cursor not in transforms:raise ValueError('Invalid source mount hierarchy')
            seen.add(cursor);t=transforms[cursor]
            chain.append(dict(path_id=cursor,name=names[t['m_GameObject']['m_PathID']],position=t['m_LocalPosition'],rotation=t['m_LocalRotation'],scale=t['m_LocalScale']))
            cursor=t['m_Father']['m_PathID']
        chains.append(list(reversed(chain)))
    return dict(path=str(path.relative_to(ROOT)),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),mount_chains=chains)


def recover(game,output):
    import UnityPy
    from UnityPy.helpers.MeshHelper import MeshHandler
    db=sqlite3.connect((ROOT/'Assets/Resources/DataBase/localData.bytes').as_uri()+'?mode=ro',uri=True)
    row=db.execute('SELECT _modelPath,_position,_bindNode FROM PrototypeItem WHERE CAST(id AS INTEGER)=12').fetchone();db.close()
    if row!=(RESOURCE,'528','5'):raise ValueError('Wood shovel12 mapping changed')
    objects={o.path_id:o for o in UnityPy.load(str(PREFAB)).objects}
    root=objects[400000].read_typetree();child=objects[434150].read_typetree();equipment=objects[11483140].read_typetree()
    assets=Assets(game/'PE_Client_Data');manager=next(o for o in assets.file('mainData').objects.values() if o.type.name=='ResourceManager')
    refs=[assets.resolve(manager,p) for n,p in manager.read_typetree()['m_Container'] if n==RESOURCE.lower()]
    go,=[o for o in refs if o.type.name=='GameObject'];installed_root=go.path_id
    t,=assets.components(go,'Transform');check_same_transform(root,t.read_typetree())
    visual=[]
    for ptr in t.read_typetree()['m_Children']:
        ct=assets.resolve(t,ptr);cg=assets.resolve(ct,ct.read_typetree()['m_GameObject'])
        if assets.components(cg,'MeshFilter'):visual.append((cg,ct))
    (go,t),=visual;check_same_transform(child,t.read_typetree())
    filt,=assets.components(go,'MeshFilter');renderer,=assets.components(go,'MeshRenderer')
    mesh_obj=assets.resolve(filt,filt.read_typetree()['m_Mesh']);mesh=MeshHandler(mesh_obj.read());mesh.process()
    vertices=list(mesh.m_Vertices);normals=list(mesh.m_Normals);uvs=list(mesh.m_UV0);faces=list(mesh.get_triangles())
    checked_mesh(vertices,normals,uvs,faces)
    refs=renderer.read_typetree()['m_Materials']
    if len(refs)!=len(faces):raise ValueError('Shovel material count differs')
    materials=[assets.resolve(renderer,p) for p in refs];symbols=material_symbols([m.path_id for m in materials]);rows=[]
    mtl=['# Source material slots; diffuse-only preview conversion\n']
    for material in materials:
        d=material.read_typetree();saved=d['m_SavedProperties'];color=property_map(saved['m_Colors']).get('_Color',dict(r=1,g=1,b=1,a=1));main=property_map(saved['m_TexEnvs']).get('_MainTex')
        symbol=symbols[material.path_id];mtl += ['newmtl '+symbol+'\n','Kd '+floats([color[a] for a in 'rgb'])+'\n','d '+floats([color['a']])+'\n','illum 1\n']
        row=dict(path_id=material.path_id,obj_symbol=symbol,name=d['m_Name'],properties=saved)
        if not main or not main['m_Texture']['m_PathID']:raise ValueError('Shovel diffuse texture missing')
        texture=assets.resolve(material,main['m_Texture']);image=texture.read().image
        if image.width*image.height>16777216:raise ValueError('Shovel texture oversized')
        filename='wood-shovel-'+symbol+'.png';image.save(output/filename);mtl.append('map_Kd '+filename+'\n\n');row['diffuse_file']=filename;rows.append(row)
    (output/'wood-shovel.mtl').write_text(''.join(mtl))
    matrix=trs(child['m_LocalPosition'],child['m_LocalRotation'],child['m_LocalScale'])
    lines=['# Baked Unity XYZ meters; original vertex/normal coordinate convention\n','mtllib wood-shovel.mtl\n']
    lines += ['v '+floats(transform_point(matrix,v))+'\n' for v in vertices]
    lines += ['vt '+floats(v[:2])+'\n' for v in uvs]
    lines += ['vn '+floats(transform_normal(matrix,v))+'\n' for v in normals]
    for material,submesh in zip(materials,faces):
        lines.append('usemtl '+symbols[material.path_id]+'\n')
        for face in submesh:
            order=list(reversed(face)) if determinant(matrix)<0 else face
            lines.append('f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in order)+'\n')
    (output/'wood-shovel.obj').write_text(''.join(lines))
    mounts=[prefab_mounts(UnityPy,ROOT/f'Assets/Resources/Prefab/PlayerPrefab/{sex}Model.prefab') for sex in ('Male','Female')]
    for entry in mounts:entry['mount_chains']=[c for c in entry['mount_chains'] if any(t['name']=='Model' for t in c)]
    files={p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in output.iterdir()}
    return dict(format='PE-source-wood-shovel-1',item_id=12,resource=RESOURCE,equipment_slot_mask=528,bind_node=5,
        coordinates='Shovel prefab-root local Unity XYZ; authored visual child transform baked once',
        source_prefab=str(PREFAB.relative_to(ROOT)),source_sha256=hashlib.sha256(PREFAB.read_bytes()).hexdigest(),
        source_root=root,source_visual_transform=child,source_equipment=equipment,source_mounts=mounts,
        installed_root=installed_root,installed_mesh=mesh_obj.path_id,installed_mesh_file=mesh_obj.assets_file.name,
        installed_mesh_sha256=hashlib.sha256(mesh_obj.get_raw_data()).hexdigest(),source_installed_transforms_match=True,
        vertices=len(vertices),triangles=sum(map(len,faces)),materials=rows,files=files,
        limitations=['Diffuse source geometry only; mount must come from real source character palette.',
                     'No shovel digging activation, terrain progress or draw transition is inferred.'])


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers');p.add_argument('--output',type=Path,default=ROOT/'rebuild/build/reference-assets/wood-shovel');a=p.parse_args()
    output=a.output.expanduser().resolve();game=a.game_dir.expanduser().resolve()
    if output.exists() or output==game or game in output.parents:raise ValueError('Fresh output outside original installation required')
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.wood-shovel-',dir=output.parent) as temp:
        staging=Path(temp);report=recover(game,staging);(staging/'manifest.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n');staging.rename(output)
    print(f'Exported original woodshovel12: {report["vertices"]} vertices/{report["triangles"]} triangles to {output}')
if __name__=='__main__':main()
