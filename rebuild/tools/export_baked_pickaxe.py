#!/usr/bin/env python3
"""Attach recovered woodpickaxe19 to source mountMain/mountBack over exact baked body clocks.
Standard library. Fresh output only; no runtime, graphics or attack activation.
"""
from read_reference_adventure import source_label
import argparse
import copy
import json
from pathlib import Path
import tempfile
import analyze_reference_bake as bake
import analyze_reference_pose as pose
from export_baked_knife import read_json,source_file,clip_headers,check_body
from export_reference_assets import IDENTITY,trs
from export_reference_knife_collision import obj_parts


def pickaxe_rig(player,visual,mesh,mode="drawn"):
    if visual['format']!='PE-source-wood-pickaxe-1' or visual['item_id']!=19 or visual['resource']!='Prefab/Item/Equip/Weapon/Hoe/wood_hoe':
        raise ValueError('Expected recovered woodpickaxe19')
    attachment=visual['source_equipment']['m_HandChangeAttr']
    if mode not in ('drawn','stowed'):raise ValueError('Unsupported pickaxe mount mode')
    mount_name='mountMain' if mode=='drawn' else 'mountBack'
    if attachment['m_PutOnBone']!='mountMain' or attachment['m_PutOffBone']!='mountBack':raise ValueError('Pickaxe mounts changed')
    root=visual['source_root'];pose.close_matrix(trs(root['m_LocalPosition'],root['m_LocalRotation'],root['m_LocalScale']),IDENTITY,'pickaxe source root')
    chains=[]
    for prefab in visual['source_mounts']:
        for chain in prefab['mount_chains']:
            if chain[0]['name']==player['sex'].capitalize()+'Model' and chain[-1]['name']==mount_name:
                names=[node['name'] for node in chain]
                if names.count('Model')!=1:raise ValueError('Ambiguous source model root')
                chains.append((chain,'/'.join(names[names.index('Model')+1:])))
    if len(chains)!=1:raise ValueError('Unique source mount required')
    chain,mount_path=chains[0];materials={m['obj_symbol']:m for m in visual['materials']}
    rig=dict(schema=1,sex=player['sex'],avatar=copy.deepcopy(player['avatar']),parts=[],materials=[])
    for symbol,(rows,indices) in obj_parts(mesh.decode('utf-8')).items():
        material=copy.deepcopy(materials[symbol]);material['asset']='source-wood-pickaxe';rig['materials'].append(material)
        skin={**{f'weight[{i}]':float(i==0) for i in range(4)},**{f'boneIndex[{i}]':0 for i in range(4)}}
        rig['parts'].append(dict(resource=visual['resource'],bone_names=[mount_name],inverse_binds=[IDENTITY],
            positions=[r[0] for r in rows],normals=[r[1] for r in rows],uv=[r[2] for r in rows],skin=[dict(skin) for _ in rows],
            triangles=[[indices[i:i+3] for i in range(0,len(indices),3)]],materials=[['source-wood-pickaxe',material['path_id']]]))
    if not rig['parts']:raise ValueError('Pickaxe geometry empty')
    return rig,mount_path,chain[-1],mount_name


def export(bake_path,player_path,assets,body,output,mode="drawn"):
    inputs=[p.resolve() for p in (bake_path,player_path,assets/'manifest.json',body)];output=output.resolve()
    if output.exists() or any(output==p or output in p.parents for p in inputs):raise ValueError('Fresh separate output required')
    player=read_json(player_path);visual=read_json(assets/'manifest.json');mesh=source_file(assets,visual,'wood-pickaxe.obj','.obj')
    rig,mount_path,mount,mount_name=pickaxe_rig(player,visual,mesh,mode);report,samples=bake.analyze(bake_path,rig,retain=True)
    mapping=report['rig_mapping']['bones'].get(mount_name)
    if not mapping or mapping['relative_path']!=mount_path:raise ValueError('Bake/source pickaxe mount path differs')
    node=report['skeleton']['nodes'][mapping['index']]
    pose.close_matrix(pose.matrix(node['initialLocal'],'baked mount local'),trs(mount['position'],mount['rotation'],mount['scale']),'source mount local')
    textures={m['diffuse_file']:source_file(assets,visual,m['diffuse_file'],'.png') for m in rig['materials']}
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.baked-pickaxe-',dir=output.parent) as temp:
        staging=Path(temp);source_dir=staging/'sources';source_dir.mkdir()
        for name,data in textures.items():(source_dir/name).write_bytes(data)
        generated=[]
        for clip in report['clips']:
            directory=staging/clip['name'];directory.mkdir();result=bake.export_clip(directory,rig,source_dir/'pickaxe-rig.json',samples[clip['name']],report,clip);generated.append((directory,result))
        combined=staging/'all';result=bake.combine_clips(combined,generated)
        expected=clip_headers((combined/result['parts'][0]['file']).read_bytes());body_sources=check_body(body,expected)
        for part in result['parts']:
            old=part['file'];part['file']=old.replace('player-part-','wood-pickaxe-part-');(combined/old).rename(combined/part['file'])
        (combined/'recording.peas').write_text('PEAS1\n'+''.join(p['file']+' '+p['texture']+'\n' for p in result['parts']),encoding='ascii')
        result.update(item_id=19,attachment=mount_name,attachment_mode=mode,source_mount_path=mount_path,source_mount=mount,
            sources=[dict(path=source_label(p),sha256=pose.sha256(p)) for p in inputs],body_actors=body_sources,
            coordinate_space='Exact source baked '+mount_name+' matrix times original wood-pickaxe prefab-local geometry; identity rigid bind',
            collision_authorized=False,activation_observed=False,
            limitations=['Fixed selected mount only; no MinePutOn/MinePutOff transition.',
                         'Character clip baking omits controller transitions and procedural IK.',
                         'No pickaxe digging collision or terrain progress inferred from visual pose.'])
        (combined/'recording-manifest.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n');combined.rename(output)
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('bake',type=Path);p.add_argument('--rig',type=Path,required=True);p.add_argument('--visual-assets',type=Path,required=True);p.add_argument('--body-scene',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--mode',choices=('drawn','stowed'),default='drawn');a=p.parse_args()
    result=export(a.bake,a.rig,a.visual_assets,a.body_scene,a.output,a.mode)
    print(f'Exported woodpickaxe19: {len(result["parts"])} parts/{len(result["clips"])} exact body clips to {a.output}')
if __name__=='__main__':main()
