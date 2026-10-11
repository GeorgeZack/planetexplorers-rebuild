#!/usr/bin/env python3
"""Attach recovered safetyaxe1527 or woodaxe26 to source mountMain over exact baked body clocks.
Standard library. Fresh output only; no runtime, graphics or attack activation.
"""
from read_reference_adventure import source_label
import argparse
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import analyze_reference_bake as bake
import analyze_reference_pose as pose
from export_baked_knife import read_json,source_file,clip_headers,check_body
from export_reference_assets import IDENTITY,trs
from export_reference_knife_collision import obj_parts


AXES = {1527: ('safety-axe', 'safety_axe'), 26: ('wood-axe', 'wood_ax')}


def identity(visual):
    spec=AXES.get(visual['item_id'])
    if not spec or visual['format']!='PE-source-'+spec[0]+'-1' or visual['resource']!='Prefab/Item/Equip/Weapon/Ax/'+spec[1]:
        raise ValueError('Expected exact recovered safetyaxe1527 or woodaxe26')
    return spec[0]


def axe_rig(player,visual,mesh):
    basename=identity(visual)
    attachment=visual['source_equipment']['m_HandChangeAttr']
    if attachment['m_PutOnBone']!='mountMain':raise ValueError('Axe drawn mount changed')
    root=visual['source_root'];pose.close_matrix(trs(root['m_LocalPosition'],root['m_LocalRotation'],root['m_LocalScale']),IDENTITY,'axe source root')
    chains=[]
    for prefab in visual['source_mounts']:
        for chain in prefab['mount_chains']:
            if chain[0]['name']==player['sex'].capitalize()+'Model' and chain[-1]['name']=='mountMain':
                names=[node['name'] for node in chain]
                if names.count('Model')!=1:raise ValueError('Ambiguous source model root')
                chains.append((chain,'/'.join(names[names.index('Model')+1:])))
    if len(chains)!=1:raise ValueError('Unique source mount required')
    chain,mount_path=chains[0];materials={m['obj_symbol']:m for m in visual['materials']}
    rig=dict(schema=1,sex=player['sex'],avatar=copy.deepcopy(player['avatar']),parts=[],materials=[])
    for symbol,(rows,indices) in obj_parts(mesh.decode('utf-8')).items():
        material=copy.deepcopy(materials[symbol]);material['asset']='source-'+basename;rig['materials'].append(material)
        skin={**{f'weight[{i}]':float(i==0) for i in range(4)},**{f'boneIndex[{i}]':0 for i in range(4)}}
        rig['parts'].append(dict(resource=visual['resource'],bone_names=['mountMain'],inverse_binds=[IDENTITY],
            positions=[r[0] for r in rows],normals=[r[1] for r in rows],uv=[r[2] for r in rows],skin=[dict(skin) for _ in rows],
            triangles=[[indices[i:i+3] for i in range(0,len(indices),3)]],materials=[['source-'+basename,material['path_id']]]))
    if not rig['parts']:raise ValueError('Axe geometry empty')
    return rig,mount_path,chain[-1]


def export(bake_path,player_path,assets,body,output):
    inputs=[p.resolve() for p in (bake_path,player_path,assets/'manifest.json',body)];output=output.resolve()
    if output.exists() or any(output==p or output in p.parents for p in inputs):raise ValueError('Fresh separate output required')
    player=read_json(player_path);visual=read_json(assets/'manifest.json');basename=identity(visual);mesh=source_file(assets,visual,basename+'.obj','.obj')
    rig,mount_path,mount=axe_rig(player,visual,mesh);report,samples=bake.analyze(bake_path,rig,retain=True)
    mapping=report['rig_mapping']['bones'].get('mountMain')
    if not mapping or mapping['relative_path']!=mount_path:raise ValueError('Bake/source axe mount path differs')
    node=report['skeleton']['nodes'][mapping['index']]
    pose.close_matrix(pose.matrix(node['initialLocal'],'baked mount local'),trs(mount['position'],mount['rotation'],mount['scale']),'source mount local')
    textures={m['diffuse_file']:source_file(assets,visual,m['diffuse_file'],'.png') for m in rig['materials']}
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.baked-axe-',dir=output.parent) as temp:
        staging=Path(temp);source_dir=staging/'sources';source_dir.mkdir()
        for name,data in textures.items():(source_dir/name).write_bytes(data)
        generated=[]
        for clip in report['clips']:
            directory=staging/clip['name'];directory.mkdir();result=bake.export_clip(directory,rig,source_dir/'axe-rig.json',samples[clip['name']],report,clip);generated.append((directory,result))
        combined=staging/'all';result=bake.combine_clips(combined,generated)
        expected=clip_headers((combined/result['parts'][0]['file']).read_bytes());body_sources=check_body(body,expected)
        for part in result['parts']:
            old=part['file'];part['file']=old.replace('player-part-',basename+'-part-');(combined/old).rename(combined/part['file'])
        (combined/'recording.peas').write_text('PEAS1\n'+''.join(p['file']+' '+p['texture']+'\n' for p in result['parts']),encoding='ascii')
        result.update(item_id=visual['item_id'],attachment='mountMain',attachment_mode='drawn',source_mount_path=mount_path,source_mount=mount,
            sources=[dict(path=source_label(p),sha256=pose.sha256(p)) for p in inputs],body_actors=body_sources,
            coordinate_space='Exact source baked mountMain matrix times original axe prefab-local geometry; identity rigid bind',
            collision_authorized=False,activation_observed=False,
            limitations=['Fixed drawn mount only; no AxPutOn/AxPutOff transition.',
                         'Character clip baking omits controller transitions and procedural IK.',
                         'No axe combat/cutting collision or progress inferred from visual pose.'])
        (combined/'recording-manifest.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n');combined.rename(output)
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('bake',type=Path);p.add_argument('--rig',type=Path,required=True);p.add_argument('--visual-assets',type=Path,required=True);p.add_argument('--body-scene',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    result=export(a.bake,a.rig,a.visual_assets,a.body_scene,a.output)
    print(f'Exported axe{result["item_id"]}: {len(result["parts"])} parts/{len(result["clips"])} exact body clips to {a.output}')
if __name__=='__main__':main()
