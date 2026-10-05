#!/usr/bin/env python3
"""Recover source wood pickaxe19 visual geometry, materials and mount metadata.
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
from export_reference_shovel import prefab_mounts
ROOT=Path(__file__).resolve().parents[2]
PREFAB=ROOT/'Assets/Resources/Prefab/Item/Equip/Weapon/Hoe/wood_hoe.prefab'
RESOURCE='Prefab/Item/Equip/Weapon/Hoe/wood_hoe'



def validate_equipment(equipment):
    """Do not silently export a different tool with the same resource name."""
    expected = dict(m_SkillID=20110002, m_AnimName='Mine', m_AnimSpeed=1.0,
                    m_StaminaCost=4.0, m_AnimDownThreshold=[50.0,20.0], m_DigMask=27)
    if any(equipment.get(key) != value for key,value in expected.items()):
        raise ValueError('Wood pickaxe source action identity changed')
    hand = equipment.get('m_HandChangeAttr', {})
    for key,value in dict(m_PutOnBone='mountMain', m_PutOffBone='mountBack',
                          m_PutOnAnim='MinePutOn', m_PutOffAnim='MinePutOff').items():
        if hand.get(key) != value:
            raise ValueError('Wood pickaxe source mount identity changed')


def recover(game,output):
    import UnityPy
    from UnityPy.helpers.MeshHelper import MeshHandler
    db=sqlite3.connect((ROOT/'Assets/Resources/DataBase/localData.bytes').as_uri()+'?mode=ro',uri=True)
    row=db.execute('SELECT _modelPath,_position,_bindNode FROM PrototypeItem WHERE CAST(id AS INTEGER)=19').fetchone();db.close()
    if row!=(RESOURCE,'528','5'):raise ValueError('Wood pickaxe19 mapping changed')
    objects={o.path_id:o for o in UnityPy.load(str(PREFAB)).objects}
    root=objects[400000].read_typetree();child=objects[402602].read_typetree();equipment=objects[11466528].read_typetree()
    validate_equipment(equipment)
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
    mesh_obj=assets.resolve(filt,filt.read_typetree()['m_Mesh'])
    if mesh_obj.read_typetree()['m_Name']!='wood_hoe':raise ValueError('Unexpected installed pickaxe mesh')
    mesh=MeshHandler(mesh_obj.read());mesh.process()
    vertices=list(mesh.m_Vertices);normals=list(mesh.m_Normals);uvs=list(mesh.m_UV0);faces=list(mesh.get_triangles())
    checked_mesh(vertices,normals,uvs,faces)
    refs=renderer.read_typetree()['m_Materials']
    if len(refs)!=len(faces):raise ValueError('Pickaxe material count differs')
    materials=[assets.resolve(renderer,p) for p in refs];symbols=material_symbols([m.path_id for m in materials]);rows=[]
    mtl=['# Source material slots; diffuse-only preview conversion\n']
    for material in materials:
        d=material.read_typetree();saved=d['m_SavedProperties'];color=property_map(saved['m_Colors']).get('_Color',dict(r=1,g=1,b=1,a=1));main=property_map(saved['m_TexEnvs']).get('_MainTex')
        symbol=symbols[material.path_id];mtl += ['newmtl '+symbol+'\n','Kd '+floats([color[a] for a in 'rgb'])+'\n','d '+floats([color['a']])+'\n','illum 1\n']
        row=dict(path_id=material.path_id,obj_symbol=symbol,name=d['m_Name'],properties=saved)
        if not main or not main['m_Texture']['m_PathID']:raise ValueError('Pickaxe diffuse texture missing')
        texture=assets.resolve(material,main['m_Texture']);image=texture.read().image
        if image.width*image.height>16777216:raise ValueError('Pickaxe texture oversized')
        filename='wood-pickaxe-'+symbol+'.png';image.save(output/filename);mtl.append('map_Kd '+filename+'\n\n');row['diffuse_file']=filename;row['texture_path_id']=texture.path_id;row['texture_asset']=texture.assets_file.name;row['texture_sha256']=hashlib.sha256(texture.get_raw_data()).hexdigest();rows.append(row)
    (output/'wood-pickaxe.mtl').write_text(''.join(mtl))
    matrix=trs(child['m_LocalPosition'],child['m_LocalRotation'],child['m_LocalScale'])
    lines=['# Baked Unity XYZ meters; original vertex/normal coordinate convention\n','mtllib wood-pickaxe.mtl\n']
    lines += ['v '+floats(transform_point(matrix,v))+'\n' for v in vertices]
    lines += ['vt '+floats(v[:2])+'\n' for v in uvs]
    lines += ['vn '+floats(transform_normal(matrix,v))+'\n' for v in normals]
    for material,submesh in zip(materials,faces):
        lines.append('usemtl '+symbols[material.path_id]+'\n')
        for face in submesh:
            order=list(reversed(face)) if determinant(matrix)<0 else face
            lines.append('f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in order)+'\n')
    (output/'wood-pickaxe.obj').write_text(''.join(lines))
    mounts=[prefab_mounts(UnityPy,ROOT/f'Assets/Resources/Prefab/PlayerPrefab/{sex}Model.prefab') for sex in ('Male','Female')]
    for entry in mounts:entry['mount_chains']=[c for c in entry['mount_chains'] if any(t['name']=='Model' for t in c)]
    files={p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in output.iterdir()}
    return dict(format='PE-source-wood-pickaxe-1',item_id=19,resource=RESOURCE,equipment_slot_mask=528,bind_node=5,
        coordinates='Pickaxe prefab-root local Unity XYZ; authored visual child transform baked once',
        source_prefab=str(PREFAB.relative_to(ROOT)),source_sha256=hashlib.sha256(PREFAB.read_bytes()).hexdigest(),
        source_root=root,source_visual_transform=child,source_equipment=equipment,source_equipment_component=11466528,source_mounts=mounts,
        installed_root=installed_root,installed_mesh=mesh_obj.path_id,installed_mesh_file=mesh_obj.assets_file.name,
        installed_mesh_sha256=hashlib.sha256(mesh_obj.get_raw_data()).hexdigest(),source_installed_transforms_match=True,
        vertices=len(vertices),triangles=sum(map(len,faces)),materials=rows,files=files,
        limitations=['Diffuse source geometry only; mount must come from real source character palette.',
                     'No pickaxe digging activation, terrain progress or draw transition is inferred.'])


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers');p.add_argument('--output',type=Path,default=ROOT/'rebuild/build/reference-assets/wood-pickaxe');a=p.parse_args()
    output=a.output.expanduser().resolve();game=a.game_dir.expanduser().resolve()
    if output.exists() or output==game or game in output.parents:raise ValueError('Fresh output outside original installation required')
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.wood-pickaxe-',dir=output.parent) as temp:
        staging=Path(temp);report=recover(game,staging);(staging/'manifest.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n');staging.rename(output)
    print(f'Exported original woodpickaxe19: {report["vertices"]} vertices/{report["triangles"]} triangles to {output}')
if __name__=='__main__':main()
