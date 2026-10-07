#!/usr/bin/env python3
"""Recover selected wild VETreeProtos meshes directly; no Unity/game execution."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import tempfile

from export_content import ROOT
from export_reference_player import Assets
from export_reference_assets import floats, property_map
from export_training_gathering_assets import checked_mesh

PROTOTYPES=(15,37,40,146)
NAMES=('tree15_grasslandC','tree08_grassland','tree09_grassland','tree16_grassland')
PREFAB=ROOT/'Assets/PeLauncher/Resources/VETreeProtos.prefab'


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def rawsha(obj):return hashlib.sha256(obj.get_raw_data()).hexdigest()
def guid(owner,ref):
    raw=owner.assets_file.externals[ref['m_FileID']-1].guid.hex()
    return ''.join(raw[i+1]+raw[i] for i in range(0,32,2))


def recover(game,output):
    import UnityPy
    from UnityPy.helpers.MeshHelper import MeshHandler
    from UnityPy.helpers.TypeTreeNode import TypeTreeNode
    source,=[o for o in UnityPy.load(str(PREFAB)).objects if o.type.name=='MonoBehaviour']
    st=source.read_typetree()
    if len(st['m_treePrototypeList'])!=512:raise ValueError('Source tree prototype count changed')
    a=Assets(game/'PE_Client_Data')
    manager,=[o for o in a.file('mainData').objects.values() if o.type.name=='ResourceManager']
    root,=[a.resolve(manager,p) for n,p in manager.read_typetree()['m_Container'] if n=='vetreeprotos']
    v,=a.components(root,'MonoBehaviour')
    header=v.read_typetree(check_read=False)
    if a.resolve(v,header['m_Script']).read_typetree()['m_ClassName']!='VoxelEditor':raise ValueError('Installed tree owner changed')
    node=TypeTreeNode.from_list([{k:value for k,value in n.items() if k!='m_Children'} for n in v._get_typetree_node().to_dict_list()])
    fields=('m_voxelTerrain','m_treePrototypeList','m_treePrototypeBendfactor','m_treePrototypeSizes','m_treePrototypeName')
    node.m_Children=list(node.m_Children)+[n for n in source._get_typetree_node().m_Children if n.m_Name in fields]
    vt=v.read_typetree(nodes=node) # Require complete installed byte coverage.
    if len(vt['m_treePrototypeList'])!=512:raise ValueError('Installed tree prototype count changed')
    rows=[];sources={str(PREFAB.relative_to(ROOT)):sha(PREFAB)}
    for proto,name in zip(PROTOTYPES,NAMES):
        print('Recovering wild prototype '+str(proto)+' '+name,flush=True)
        path=ROOT/'Assets/Prefabs/tree-Ambient-Occlusion'/(name+'.prefab')
        meta=path.with_suffix(path.suffix+'.meta')
        if 'guid: '+guid(source,st['m_treePrototypeList'][proto]) not in meta.read_text():raise ValueError('Source prototype GUID changed')
        so={o.path_id:o for o in UnityPy.load(str(path)).objects}
        sg=so[100000]
        if sg.read_typetree()['m_Name']!=name or any(o.type.name.endswith('Collider') for o in so.values()):raise ValueError('Wild source prefab changed')
        go=a.resolve(v,vt['m_treePrototypeList'][proto])
        if go.read_typetree()['m_Name']!=name:raise ValueError('Installed prototype order/name differs')
        mf,=a.components(go,'MeshFilter');mr,=a.components(go,'MeshRenderer')
        mo=a.resolve(mf,mf.read_typetree()['m_Mesh']);mt=mo.read_typetree()
        if mt['m_Name']!=name:raise ValueError('Installed mesh name differs')
        mh=MeshHandler(mo.read());mh.process()
        vertices=list(mh.m_Vertices);normals=list(mh.m_Normals);uvs=list(mh.m_UV0);submeshes=list(mh.get_triangles())
        checked_mesh(vertices,normals,uvs,submeshes)
        refs=mr.read_typetree()['m_Materials']
        if len(refs)!=1 or len(submeshes)!=1:raise ValueError('Wild plant material slots changed')
        mat=a.resolve(mr,refs[0]);mat_tree=mat.read_typetree();props=mat_tree['m_SavedProperties']
        colors=property_map(props['m_Colors']);scalar=property_map(props['m_Floats']);env=property_map(props['m_TexEnvs'])['_MainTex']
        tint=colors['_Color'];cutoff=scalar['_Cutoff']
        if not 0<=cutoff<=1 or env['m_Scale']!={'x':1.0,'y':1.0} or env['m_Offset']!={'x':0.0,'y':0.0}:raise ValueError('Wild plant material transform changed')
        texture=a.resolve(mat,env['m_Texture']);image=texture.read().image
        if image.width*image.height>16777216:raise ValueError('Oversize vegetation texture')
        texname='grassland-tree-a.png';texpath=output/texname
        if not texpath.exists():image.save(texpath)
        model='plant-'+str(proto)
        (output/(model+'.mtl')).write_text('# Source material slots; diffuse-only preview conversion\nnewmtl WildPlant\nKd '+floats([tint[k] for k in 'rgb'])+'\nd '+floats([tint['a']])+'\nillum 1\nmap_Kd '+texname+'\n')
        lines=['# Baked Unity XYZ meters; original vertex/normal coordinate convention\n','mtllib '+model+'.mtl\n']
        lines+=['v '+floats(v[:3])+'\n' for v in vertices]
        lines+=['vt '+floats(uv[:2])+'\n' for uv in uvs]
        lines+=['vn '+floats(n[:3])+'\n' for n in normals]
        lines+=['usemtl WildPlant\n']
        lines+=['f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in face)+'\n' for face in submeshes[0]]
        (output/(model+'.obj')).write_text(''.join(lines))
        shader=a.resolve(mat,mat_tree['m_Shader']);shader_tree=shader.read_typetree()
        shader_script=shader_tree.get('m_Script','');shader_script=shader_script.decode(errors='replace') if isinstance(shader_script,bytes) else shader_script
        sources[str(path.relative_to(ROOT))]=sha(path);sources[str(meta.relative_to(ROOT))]=sha(meta)
        aabb=mt['m_LocalAABB'];minimum=[aabb['m_Center'][k]-aabb['m_Extent'][k] for k in 'xyz'];maximum=[aabb['m_Center'][k]+aabb['m_Extent'][k] for k in 'xyz']
        rows.append(dict(prototype_id=proto,resource_id=proto+1000,name=name,model=model,vertices=len(vertices),triangles=sum(map(len,submeshes)),minimum=minimum,maximum=maximum,
                         source_mesh_guid=guid(so[3300000],so[3300000].read_typetree()['m_Mesh']),source_mesh_path_id=so[3300000].read_typetree()['m_Mesh']['m_PathID'],
                         installed_game_object=go.path_id,installed_mesh=mo.path_id,installed_file=mo.assets_file.name,installed_mesh_sha256=rawsha(mo),
                         installed_material=mat.path_id,installed_material_sha256=rawsha(mat),installed_texture=texture.path_id,installed_texture_sha256=rawsha(texture),
                         installed_shader_sha256=rawsha(shader),shader=shader_tree['m_Name'],shader_cull_statements=re.findall(r'(?im)^\s*Cull\s+([^\r\n]+)',shader_script),
                         cutoff=cutoff,tint=[tint[k] for k in 'rgba'],source_properties=props,source_transform=so[400000].read_typetree(),
                         obj_sha256=sha(output/(model+'.obj')),mtl_sha256=sha(output/(model+'.mtl'))))
    return dict(format_version=1,source_inputs=sources,prototype_count=512,props=rows,texture_sha256=sha(output/'grassland-tree-a.png'),
                limitations=['Direct wild prototype geometry/material recovery; no tutorial objects or scripts.',
                             'Unit scale and root position/rotation ignored; fixed native fixtures, not terrain random placement parity.',
                             'Diffuse and alpha cutoff only; wind, AO, emission and source tree shader lighting remain adapters.'])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--output',type=Path,default=ROOT/'rebuild/build/reference-assets/sandbox-vegetation')
    a=p.parse_args();out=a.output.resolve();game=a.game_dir.resolve()
    from export_vegetation_source import validate_sources,validate_assets
    validate_sources()
    if out.exists() or out==game or game in out.parents:raise ValueError('Output must be new and outside installed game')
    out.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.wild-vegetation-',dir=out.parent) as temp:
        stage=Path(temp);report=recover(game,stage)
        (stage/'manifest.json').write_text(json.dumps(report,indent=2,sort_keys=True,allow_nan=False)+'\n')
        validate_assets(stage)
        stage.rename(out)
    print('Recovered four wild plant models to '+str(out))


if __name__=='__main__':main()
