#!/usr/bin/env python3
"""Attach shared recovered Torch10 geometry to exact sampled held/stowed mounts.

Uses a closed isolated locomotion bake; does not launch Unity or infer sword
attacks, draw transitions, procedural IK or the Sword movement controller.
Shared mesh/material recovery is owned by export_torch_assets.py.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import tempfile

import analyze_reference_bake as bake
import analyze_reference_pose as pose
from export_content import ROOT
from export_reference_assets import IDENTITY, trs
from export_reference_knife_collision import obj_parts
from export_baked_knife import read_json, clip_headers, check_body
from inventory_reference_bake import SWORD_CLIPS


def read_shared_assets(assets):
    manifest=read_json(assets/'torch.json',8*1024*1024)
    source=manifest['source'];prefab=source['prefab']
    expected='Assets/Resources/Prefab/Item/Equip/Other/torch.prefab'
    if source['item_id']!=10 or prefab['path']!=expected or pose.sha256(ROOT/expected)!=prefab['sha256']:
        raise ValueError('Shared Torch source identity/hash mismatch')
    files={}
    for name,digest in manifest['files'].items():
        if not isinstance(name,str) or Path(name).name!=name:raise ValueError('Unsafe shared Torch basename')
        path=assets/name
        if path.is_symlink() or not path.is_file() or path.stat().st_size>64*1024*1024:
            raise ValueError('Missing/oversized shared Torch file')
        data=path.read_bytes()
        if hashlib.sha256(data).hexdigest()!=digest:raise ValueError('Shared Torch asset hash mismatch')
        files[name]=data
    visual=json.loads(files['torch-visual.json'])
    if any(m['diffuse_file'] not in files for m in visual['materials']):raise ValueError('Unhashed Torch texture')
    return visual,files['torch.obj']


def source_mount(UnityPy, sex, mount_name):
    path=ROOT/f'Assets/Resources/Prefab/PlayerPrefab/{sex.capitalize()}Model.prefab'
    env=UnityPy.load(str(path))
    names={o.path_id:o.read_typetree()['m_Name'] for o in env.objects if o.type.name=='GameObject'}
    transforms={o.path_id:o.read_typetree() for o in env.objects if o.type.name=='Transform'}
    chains=[]
    for identity,row in transforms.items():
        if names[row['m_GameObject']['m_PathID']]!=mount_name:continue
        chain=[];seen=set()
        while identity:
            if identity in seen or identity not in transforms:raise ValueError('Invalid source mount ancestry')
            seen.add(identity);node=transforms[identity]
            chain.append(dict(name=names[node['m_GameObject']['m_PathID']],position=node['m_LocalPosition'],
                              rotation=node['m_LocalRotation'],scale=node['m_LocalScale']))
            if node['m_Father']['m_FileID']:raise ValueError('External source mount ancestry')
            identity=node['m_Father']['m_PathID']
        chain.reverse();parts=[n['name'] for n in chain]
        if parts.count('Model')==1:chains.append((chain,'/'.join(parts[parts.index('Model')+1:])))
    if len(chains)!=1:raise ValueError('Unique source Animator mount required')
    return chains[0],path


def torch_rig(player, visual, mesh, mount_name):
    if mount_name not in ('mountMain','mountBack'):raise ValueError('Invalid Torch attachment')
    materials={m['obj_symbol']:m for m in visual['materials']}
    rig=dict(schema=1,sex=player['sex'],avatar=copy.deepcopy(player['avatar']),parts=[],materials=[])
    for symbol,(rows,indices) in obj_parts(mesh.decode('utf-8')).items():
        material=copy.deepcopy(materials[symbol]);material['asset']='source-torch';rig['materials'].append(material)
        skin={**{f'weight[{i}]':float(i==0) for i in range(4)},**{f'boneIndex[{i}]':0 for i in range(4)}}
        rig['parts'].append(dict(resource='Prefab/Item/Equip/Other/torch',bone_names=[mount_name],inverse_binds=[IDENTITY],
            positions=[r[0] for r in rows],normals=[r[1] for r in rows],uv=[r[2] for r in rows],skin=[dict(skin) for _ in rows],
            triangles=[[indices[i:i+3] for i in range(0,len(indices),3)]],materials=[['source-torch',material['path_id']]]))
    if not rig['parts']:raise ValueError('Empty Torch geometry')
    return rig


def check_style(report, style):
    if style not in ('normal','sword'):raise ValueError('Invalid Torch pose style')
    if style=='sword':
        if report['skeleton']['gender']!='Female':raise ValueError('Sword Torch body export currently requires the verified female source group')
        actual={c['name']:c['sourcePathId'] for c in report['clips']}
        if len(report['clips'])!=len(SWORD_CLIPS) or actual!=SWORD_CLIPS:
            raise ValueError('Sword Torch requires the complete exact source locomotion group')


def export(bake_path,player_path,assets,body,output,attachment,style='normal'):
    import UnityPy
    if attachment not in ('held','stowed'):raise ValueError('Invalid Torch attachment mode')
    output=output.resolve();assets=assets.resolve()
    paths=[p.resolve() for p in (bake_path,player_path,assets/'torch-visual.json',assets/'torch.obj',body)]
    if output.exists() or any(output==p or output in p.parents for p in paths):raise ValueError('Output must be a fresh separate directory')
    player=read_json(player_path);visual,mesh=read_shared_assets(assets)
    paths.extend([assets/'torch.json',ROOT/'Assets/Resources/Prefab/Item/Equip/Other/torch.prefab'])
    mount_name='mountMain' if attachment=='held' else 'mountBack'
    rig=torch_rig(player,visual,mesh,mount_name)
    (chain,mount_path),model_path=source_mount(UnityPy,player['sex'],mount_name);paths.append(model_path)
    report,samples=bake.analyze(bake_path,rig,retain=True)
    check_style(report,style)
    mapping=report['rig_mapping']['bones'].get(mount_name)
    if not mapping or mapping['relative_path']!=mount_path:raise ValueError('Source/bake Torch mount differs')
    node=report['skeleton']['nodes'][mapping['index']];mount=chain[-1]
    pose.close_matrix(pose.matrix(node['initialLocal'],'baked Torch mount'),trs(mount['position'],mount['rotation'],mount['scale']),'source Torch mount')
    textures={}
    for material in rig['materials']:
        name=material['diffuse_file']
        if not pose.safe_basename(name,'.png'):raise ValueError('Invalid Torch texture basename')
        path=assets/name
        if path.is_symlink() or not path.is_file() or path.stat().st_size>64*1024*1024:raise ValueError('Invalid Torch texture')
        textures[name]=path.read_bytes();paths.append(path)
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.baked-torch-',dir=output.parent) as temporary:
        staging=Path(temporary);source_dir=staging/'sources';source_dir.mkdir()
        for name,data in textures.items():(source_dir/name).write_bytes(data)
        generated=[]
        for clip in report['clips']:
            directory=staging/clip['name'];directory.mkdir()
            result=bake.export_clip(directory,rig,source_dir/'torch-rig.json',samples[clip['name']],report,clip)
            generated.append((directory,result))
        combined=staging/'all';result=bake.combine_clips(combined,generated)
        expected=clip_headers((combined/result['parts'][0]['file']).read_bytes());body_sources=check_body(body,expected)
        for part in result['parts']:
            old=part['file'];part['file']=old.replace('player-part-',f'torch-{attachment}-part-');(combined/old).rename(combined/part['file'])
        (combined/'recording.peas').write_text('PEAS1\n'+''.join(p['file']+' '+p['texture']+'\n' for p in result['parts']),encoding='ascii')
        result.update(item_id=10,attachment=mount_name,attachment_mode=attachment,source_mount_path=mount_path,source_mount=mount,
            sources=[dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in paths],body_actors=body_sources,
            coordinate_space=f'baked Animator-model-local {mount_name} matrix * unrotated prefab-local Torch OBJ; rigid identity bind',
            limitations=['Exact sampled Normal locomotion mounts; Sword movement style/controller layering remains unported.',
                'Draw/stow selects attachments immediately; SwordPutOn/SwordPutOff transitions and combat are not reconstructed.',
                'No Unity particle simulation in the actor; host separately renders the source point light and bounded flame effect.'])
        if style=='sword':
            result['pose_style']='Sword'
            result['limitations'][0]='Exact sampled Sword locomotion leaves; controller blending, layer masks and procedural IK remain unported.'
        (combined/'recording-manifest.json').write_text(json.dumps(result,indent=2)+'\n');combined.rename(output)
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('bake',type=Path);p.add_argument('--rig',type=Path,required=True)
    p.add_argument('--visual-assets',type=Path,required=True);p.add_argument('--body-scene',type=Path,required=True)
    p.add_argument('--style',choices=('normal','sword'),default='normal',help='sword requires a complete fresh female Sword bake and matching body; does not install runtime assets')
    p.add_argument('--attachment',choices=('held','stowed'),required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    try:
        result=export(a.bake,a.rig,a.visual_assets,a.body_scene,a.output,a.attachment,a.style)
        print(f'Torch10 {a.attachment}: {len(result["parts"])} parts, {len(result["clips"])} matching clips')
    except (OSError,ValueError,KeyError,TypeError,IndexError) as error:p.exit(1,f'Torch bake export failed: {error}\n')

if __name__=='__main__':main()
