#!/usr/bin/env python3
"""Recover source safety axe1527 or wood axe26 visual geometry, materials and mount metadata.
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
from inventory_player_animation import prefab_mounts
ROOT=Path(__file__).resolve().parents[2]
PREFAB=ROOT/'Assets/Resources/Prefab/Item/Equip/Weapon/Ax/safety_axe.prefab'
RESOURCE='Prefab/Item/Equip/Weapon/Ax/safety_axe'


AXES = {1527: ("safety-axe", "safety_axe", 450780, 487518, 11497314),
        26: ("wood-axe", "wood_ax", 400000, 450868, 11484812)}


def recover(game,output,item_id=1527):
    if item_id not in AXES:raise ValueError("Unsupported source axe")
    basename,resource_name,root_id,visual_id,equipment_id=AXES[item_id]
    resource="Prefab/Item/Equip/Weapon/Ax/"+resource_name
    prefab=ROOT/("Assets/Resources/"+resource+".prefab")
    import UnityPy
    from UnityPy.helpers.MeshHelper import MeshHandler
    db=sqlite3.connect((ROOT/'Assets/Resources/DataBase/localData.bytes').as_uri()+'?mode=ro',uri=True)
    row=db.execute('SELECT _modelPath,_position,_bindNode FROM PrototypeItem WHERE CAST(id AS INTEGER)=?',(item_id,)).fetchone();db.close()
    if row!=(resource,'16','5'):raise ValueError('Source axe item mapping changed')
    objects={o.path_id:o for o in UnityPy.load(str(prefab)).objects}
    root=objects[root_id].read_typetree();child=objects[visual_id].read_typetree();equipment=objects[equipment_id].read_typetree()
    assets=Assets(game/'PE_Client_Data');manager=next(o for o in assets.file('mainData').objects.values() if o.type.name=='ResourceManager')
    refs=[assets.resolve(manager,p) for n,p in manager.read_typetree()['m_Container'] if n==resource.lower()]
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
    if len(refs)!=len(faces):raise ValueError('Axe material count differs')
    materials=[assets.resolve(renderer,p) for p in refs];symbols=material_symbols([m.path_id for m in materials]);rows=[]
    mtl=['# Source material slots; diffuse-only preview conversion\n']
    for material in materials:
        d=material.read_typetree();saved=d['m_SavedProperties'];color=property_map(saved['m_Colors']).get('_Color',dict(r=1,g=1,b=1,a=1));main=property_map(saved['m_TexEnvs']).get('_MainTex')
        symbol=symbols[material.path_id];mtl += ['newmtl '+symbol+'\n','Kd '+floats([color[a] for a in 'rgb'])+'\n','d '+floats([color['a']])+'\n','illum 1\n']
        row=dict(path_id=material.path_id,obj_symbol=symbol,name=d['m_Name'],properties=saved)
        if not main or not main['m_Texture']['m_PathID']:raise ValueError('Axe diffuse texture missing')
        texture=assets.resolve(material,main['m_Texture']);image=texture.read().image
        if image.width*image.height>16777216:raise ValueError('Axe texture oversized')
        filename=basename+'-'+symbol+'.png';image.save(output/filename);mtl.append('map_Kd '+filename+'\n\n');row['diffuse_file']=filename;rows.append(row)
    (output/(basename+'.mtl')).write_text(''.join(mtl))
    matrix=trs(child['m_LocalPosition'],child['m_LocalRotation'],child['m_LocalScale'])
    lines=['# Baked Unity XYZ meters; original vertex/normal coordinate convention\n','mtllib '+basename+'.mtl\n']
    lines += ['v '+floats(transform_point(matrix,v))+'\n' for v in vertices]
    lines += ['vt '+floats(v[:2])+'\n' for v in uvs]
    lines += ['vn '+floats(transform_normal(matrix,v))+'\n' for v in normals]
    for material,submesh in zip(materials,faces):
        lines.append('usemtl '+symbols[material.path_id]+'\n')
        for face in submesh:
            order=list(reversed(face)) if determinant(matrix)<0 else face
            lines.append('f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in order)+'\n')
    (output/(basename+'.obj')).write_text(''.join(lines))
    mounts=[prefab_mounts(UnityPy,ROOT/f'Assets/Resources/Prefab/PlayerPrefab/{sex}Model.prefab') for sex in ('Male','Female')]
    for entry in mounts:entry['mount_chains']=[c for c in entry['mount_chains'] if any(t['name']=='Model' for t in c)]
    files={p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in output.iterdir()}
    return dict(format='PE-source-'+basename+'-1',item_id=item_id,resource=resource,equipment_slot_mask=16,bind_node=5,
        coordinates='Axe prefab-root local Unity XYZ; authored visual child rotation baked once',
        source_prefab=str(prefab.relative_to(ROOT)),source_sha256=hashlib.sha256(prefab.read_bytes()).hexdigest(),
        source_root=root,source_visual_transform=child,source_equipment=equipment,source_mounts=mounts,
        installed_root=installed_root,installed_mesh=mesh_obj.path_id,installed_mesh_file=mesh_obj.assets_file.name,
        installed_mesh_sha256=hashlib.sha256(mesh_obj.get_raw_data()).hexdigest(),source_installed_transforms_match=True,
        vertices=len(vertices),triangles=sum(map(len,faces)),materials=rows,files=files,
        limitations=['Diffuse source geometry only; mount must come from real source character palette.',
                     'No axe hit activation, attack damage, tree progress, or draw transition is inferred.'])


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers');p.add_argument('--item-id',type=int,choices=sorted(AXES),default=1527);p.add_argument('--output',type=Path);a=p.parse_args()
    output=(a.output or ROOT/"rebuild/build/reference-assets"/AXES[a.item_id][0]).expanduser().resolve();game=a.game_dir.expanduser().resolve()
    if output.exists() or output==game or game in output.parents:raise ValueError('Fresh output outside original installation required')
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.safety-axe-',dir=output.parent) as temp:
        staging=Path(temp);report=recover(game,staging,a.item_id);(staging/'manifest.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n');staging.rename(output)
    print(f'Exported original axe{a.item_id}: {report["vertices"]} vertices/{report["triangles"]} triangles to {output}')
if __name__=='__main__':main()
