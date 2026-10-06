#!/usr/bin/env python3
"""Recover modern sword1/shield34 visuals and existing qualified pose attachments.

Fresh staging only: no installation, Unity launch or fabricated Sword/block pose.
Requires UnityPy1.25.4. Uses ResourceManager, never legacy Item bundles.
"""
import argparse
import copy
import hashlib
import importlib.metadata
import json
import math
from pathlib import Path
import struct
import tempfile
import zlib

import analyze_reference_bake as bake
import analyze_reference_pose as pose
import export_starter_sword_shield as metadata
from export_baked_knife import read_json, clip_headers, check_body
from export_baked_torch import source_mount
from export_directional_player import NAMES
from export_reference_assets import IDENTITY, multiply, trs, transform_point, transform_normal, determinant, property_map, floats
from export_reference_hitboxes import local_points, skeleton_data, HEADER, RECORD
from export_reference_knife_collision import obj_parts
from export_reference_player import Assets
from inventory_reference_bake import CLIPS, SWORD_CLIPS, SWORD_BACKWARD_CLIPS

ROOT=metadata.ROOT
GROUPS={'player-wood-sword-drawn':('sword','mountMain'),
        'player-wood-sword-stowed':('sword','mountBack'),
        'player-wood-shield':('shield','Bone L Forearm')}
RESOURCES={'sword':'prefab/item/equip/weapon/sword/wood_sword',
           'shield':'prefab/item/equip/armor/shield/wood_shield'}


def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def fresh_output(output,inputs):
    if output.is_symlink() or output.exists():raise ValueError('Output must be a fresh directory')
    out=output.resolve()
    for p in inputs:
        p=p.resolve()
        if out==p or out in p.parents or p.is_dir() and p in out.parents:
            raise ValueError('Output must be separate from source inputs')
    return out


def local(tree):return trs(tree['m_LocalPosition'],tree['m_LocalRotation'],tree['m_LocalScale'])


def modern_root(assets,manager,resource,classname):
    matches=[assets.resolve(manager,p) for n,p in manager.read_typetree()['m_Container'] if n==resource]
    roots=[o for o in matches if o.type.name=='GameObject']
    if len(roots)!=1:raise ValueError('Unique modern ResourceManager prefab required')
    root=roots[0];classes=[]
    for component in assets.components(root,'MonoBehaviour'):
        header=component.read_typetree(check_read=False)
        classes.append(assets.resolve(component,header['m_Script']).read_typetree()['m_ClassName'])
    if classes!=[classname]:raise ValueError('Legacy or unexpected equipped component')
    return root


def validate_material(tree):
    saved=tree['m_SavedProperties'];color=property_map(saved.get('m_Colors',[])).get('_Color',dict(r=1.,g=1.,b=1.,a=1.))
    if any(color[k]!=1 for k in 'rgba'):raise ValueError('Nonwhite material tint requires explicit recovery')
    main=property_map(saved.get('m_TexEnvs',[])).get('_MainTex')
    if not main or not main['m_Texture']['m_PathID']:raise ValueError('Source diffuse texture required')
    if main['m_Scale']!={'x':1.,'y':1.} or main['m_Offset']!={'x':0.,'y':0.}:
        raise ValueError('Nonidentity material UV transform requires explicit recovery')
    return main


def visual_assets(game,output,source):
    """Only exact modern prefab-root local rigid geometry, diffuse material slots."""
    from UnityPy.helpers.MeshHelper import MeshHandler
    assets=Assets(game/'PE_Client_Data')
    manager,=[o for o in assets.file('mainData').objects.values() if o.type.name=='ResourceManager']
    streams=set();visuals={}
    for key,classname in [('sword','PeSword'),('shield','PESheild')]:
        root=modern_root(assets,manager,RESOURCES[key],classname)
        roottr,=assets.components(root,'Transform');root_tree=roottr.read_typetree()
        pose.close_matrix(local(root_tree),IDENTITY,'modern prefab root')
        model=assets.child(root,'wood_sword_Model' if key=='sword' else 'wood_shield')
        tr,=assets.components(model,'Transform');tree=tr.read_typetree()
        expected=IDENTITY if key=='sword' else local(source['prefabs']['shield_mesh_transform'])
        pose.close_matrix(local(tree),expected,'source/installed mesh transform')
        if tree['m_Father']!={'m_FileID':0,'m_PathID':roottr.path_id}:raise ValueError('Modern mesh parent changed')
        filters=assets.components(model,'MeshFilter');renderers=assets.components(model,'MeshRenderer')
        if len(filters)!=1 or len(renderers)!=1:raise ValueError('Single rigid modern mesh required')
        renderer=renderers[0];rend=renderer.read_typetree()
        if not rend['m_Enabled'] or assets.components(model,'SkinnedMeshRenderer'):raise ValueError('Unexpected modern renderer')
        mesh_obj=assets.resolve(filters[0],filters[0].read_typetree()['m_Mesh'])
        mesh=MeshHandler(mesh_obj.read());mesh.process();submeshes=list(mesh.get_triangles())
        if not 1<=len(mesh.m_Vertices)<=100000 or len(mesh.m_Normals)!=len(mesh.m_Vertices) or len(mesh.m_UV0)!=len(mesh.m_Vertices):
            raise ValueError('Incomplete/oversized rigid mesh channels')
        if not 1<=len(submeshes)<=8 or len(submeshes)!=len(rend['m_Materials']):raise ValueError('Material/submesh mapping changed')
        matrix=multiply(local(root_tree),local(tree));positions=[transform_point(matrix,v) for v in mesh.m_Vertices]
        normals=[transform_normal(matrix,n) for n in mesh.m_Normals]
        if any(not math.isfinite(v) or abs(v)>10000 for row in positions+normals+mesh.m_UV0 for v in row):raise ValueError('Invalid rigid mesh numeric range')
        materials=[]
        for slot,ref in enumerate(rend['m_Materials']):
            material=assets.resolve(renderer,ref);data=material.read_typetree();main=validate_material(data)
            texture=assets.resolve(material,main['m_Texture']);texture_tree=texture.read_typetree()
            stream=texture_tree.get('m_StreamData',{})
            if stream.get('path'):
                path=assets.data/Path(stream['path']).name
                if not path.is_file():raise ValueError('Missing source texture stream')
                streams.add(path)
            filename=f'wood-{key}-texture-{texture.path_id}.png';texture.read().image.save(output/filename)
            materials.append(dict(path_id=material.path_id,obj_symbol=f'{key}_material_{slot}',name=data['m_Name'],
                                  diffuse_file=filename,texture_path_id=texture.path_id,properties=data['m_SavedProperties']))
        lines=['# Modern ResourceManager prefab-local Unity XYZ; no axis reflection\n']
        lines+=['v '+floats(v)+'\n' for v in positions]
        lines+=['vt '+floats(v[:2])+'\n' for v in mesh.m_UV0]
        lines+=['vn '+floats(v)+'\n' for v in normals]
        triangles=0
        for index,faces in enumerate(submeshes):
            lines.append('usemtl '+materials[index]['obj_symbol']+'\n')
            for face in faces:
                if len(face)!=3 or any(not 0<=i<len(positions) for i in face):raise ValueError('Invalid source triangle')
                order=tuple(reversed(face)) if determinant(matrix)<0 else face
                lines.append('f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in order)+'\n');triangles+=1
        if not 1<=triangles<=100000:raise ValueError('Empty/oversized source mesh')
        obj='wood-'+key+'.obj';(output/obj).write_text(''.join(lines))
        # Run the strict consumer on the newly generated OBJ before publishing.
        if set(obj_parts((output/obj).read_text()))!={m['obj_symbol'] for m in materials}:raise ValueError('OBJ material mapping differs')
        visuals[key]=dict(item_id=1 if key=='sword' else 34,resource=RESOURCES[key],component_class=classname,
                          root_id=root.path_id,root_transform=roottr.path_id,mesh_id=mesh_obj.path_id,
                          renderer_id=renderer.path_id,mesh_transform=tree,root_fields=root_tree,
                          vertices=len(positions),triangles=triangles,materials=materials,obj=obj,
                          model_matrix=matrix,bounds=[[min(v[a] for v in positions) for a in range(3)],
                                                      [max(v[a] for v in positions) for a in range(3)]])
    # Verify source/installed capsule and no shield collider in addition to the visuals.
    metadata.verify_installed(game,source['prefabs'])
    paths=sorted({assets.data/n for n in assets.files}|streams)
    manifest=dict(schema='pe-starter-pair-visuals-1',items=visuals,
                  sources=[dict(path=str(p),sha256=sha(p)) for p in paths],
                  source_metadata_sha256=hashlib.sha256((json.dumps(source,ensure_ascii=False,indent=2)+'\n').encode()).hexdigest(),
                  files={p.name:sha(p) for p in sorted(output.iterdir()) if p.is_file()},
                  limitations=['Diffuse textures and exact material slots only; original Unity shader/specular behavior not reproduced.',
                               'Modern rigid prefab visuals only, not legacy Item bundle gameplay.'])
    (output/'visual-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');return manifest


def rigid_rig(player,visual,mesh,mount_name,mount_path):
    materials={m['obj_symbol']:m for m in visual['materials']};rig=dict(schema=1,sex=player['sex'],avatar=copy.deepcopy(player['avatar']),parts=[],materials=[])
    for symbol,(rows,indices) in obj_parts(mesh).items():
        material=copy.deepcopy(materials[symbol]);material['asset']='modern-starter-pair';rig['materials'].append(material)
        skin={**{f'weight[{i}]':float(i==0) for i in range(4)},**{f'boneIndex[{i}]':0 for i in range(4)}}
        rig['parts'].append(dict(resource=visual['resource'],bone_names=[mount_name],bone_paths=[mount_path],inverse_binds=[IDENTITY],
                                positions=[r[0] for r in rows],normals=[r[1] for r in rows],uv=[r[2] for r in rows],
                                skin=[dict(skin) for _ in rows],triangles=[[indices[i:i+3] for i in range(0,len(indices),3)]],
                                materials=[['modern-starter-pair',material['path_id']]]))
    return rig


def validate_capture(report,launch,path,expected):
    if report['skeleton']['gender']!='Female' or report['skeleton']['avatar']!='FemaleAvatar':raise ValueError('Qualified female rig required')
    if tuple(c['name'] for c in report['clips'])!=tuple(expected):raise ValueError('Unexpected qualified clip set/order')
    if any(c['sourcePathId']!=CLIPS[c['name']] for c in report['clips']):raise ValueError('Source clip identity differs')
    if launch['schema']!='pe-animator-bake-launch-1' or Path(launch['capture']).resolve()!=path.resolve():raise ValueError('Capture launch provenance differs')
    plan=launch['plan']
    if plan['avatar']!=report['skeleton']['avatar'] or plan['gender']!=report['skeleton']['gender'] or plan['sample_rate']!=report['header']['sampleRate'] or len(plan['clips'])!=len(report['clips']):raise ValueError('Launch plan identity differs')
    for want,actual in zip(plan['clips'],report['clips']):
        if want['name']!=actual['name'] or want['path_id']!=actual['sourcePathId'] or want['loop']!=actual['isLooping'] or abs(want['length']-actual['length'])>.0001 or abs(want['frame_rate']-actual['sourceFrameRate'])>.001:
            raise ValueError('Launch/capture authored clock differs')


def validate_mount(report,mount_name,chain,path,player):
    mapping=report['rig_mapping']['bones'].get(mount_name)
    if not mapping or mapping['relative_path']!=path:raise ValueError('Source/capture mount ancestry differs')
    node=report['skeleton']['nodes'][mapping['index']]
    captured=pose.matrix(node['initialLocal'],'initial mount')
    bind=trs(chain[-1]['position'],chain[-1]['rotation'],chain[-1]['scale'])
    if mount_name=='Bone L Forearm':
        # This is a real animated skin joint, not an unanimated mount marker.
        # Its arbitrary live initial pose must NOT be replaced by prefab bind TRS.
        avatar=player['avatar'];a=avatar['m_Avatar'];hashes=dict(zip(a['m_SkeletonNameIDArray'],a['m_AvatarSkeleton']['data']['m_ID']))
        identity=hashes.get(zlib.crc32(mount_name.encode()));avatar_path=dict(avatar['m_TOS']).get(identity)
        if avatar_path!=path or not any(mount_name in part['bone_names'] for part in player['parts']):
            raise ValueError('Shield forearm is not the exact source Avatar skin joint')
        return dict(kind='animated-avatar-skin-joint',initial_vs_prefab_bind_matrix_error=pose.matrix_error(captured,bind),
                    interpretation='Initial live animation pose may differ from prefab bind; every exported palette uses the captured animated forearm.')
    pose.close_matrix(captured,bind,'source unanimated mount marker')
    return dict(kind='unanimated-mount-marker',initial_vs_prefab_bind_matrix_error=pose.matrix_error(captured,bind))


def sword_hitbox(actor,source):
    capsule=source['prefabs']['capsule'];points=local_points(capsule)
    if capsule['trans']!={'m_FileID':0,'m_PathID':400004} or capsule['axis']!=2 or capsule['radius']!=metadata.f32(.2) or capsule['heigh']!=metadata.f32(1.8):raise ValueError('Modern sword capsule identity differs')
    joints,fingerprint,binds=skeleton_data(actor)
    if joints!=1 or binds!=[IDENTITY]:raise ValueError('Rigid sword requires one identity bind')
    binary=HEADER.pack(b'PEHB',1,1,1,fingerprint,0,0)+RECORD.pack(b'SingleHandSword',1,1,0,0,0,1.,*(v for p in points for v in p))
    return binary,dict(file='wood-sword-drawn.pehb',skeleton_hash=f'{fingerprint:016x}',kind='attack',group='right',joint=0,
                       bind_points=points,source_attack_layer_mask=34154496,activation_observed=False,
                       note='Source geometry only; equipment/action/skill/permission gates remain mandatory.')


def file_hashes(root):
    result={}
    for p in sorted(root.rglob('*')):
        if p.is_symlink():raise ValueError('Symlink in stage')
        if p.is_file():result[str(p.relative_to(root))]=sha(p)
    return result


def validate_stage(stage,body,expected_names):
    body_headers=None
    for group in GROUPS:
        manifest=read_json(stage/group/'recording-manifest.json');first=stage/group/manifest['parts'][0]['file']
        headers=clip_headers(first.read_bytes())
        if tuple(h[:32].rstrip(b'\0').decode() for h in headers)!=tuple(expected_names):raise ValueError('Attachment clip names/order differ')
        check_body(stage/group/'recording.peas',headers);check_body(body,headers)
        if body_headers is not None and headers!=body_headers:raise ValueError('Attachment group clocks differ')
        body_headers=headers
        if group=='player-wood-sword-drawn':
            data=(stage/group/'wood-sword-drawn.pehb').read_bytes();_,_,_,_,fingerprint,_,_=HEADER.unpack_from(data)
            if fingerprint!=skeleton_data(first.read_bytes())[1]:raise ValueError('Sword hitbox fingerprint differs')
        elif list((stage/group).glob('*.pehb')):raise ValueError('No stowed/shield hitbox is authorized')
    return body_headers


def export(game,capture,running,rig_path,body,output,pose_set='normal'):
    if importlib.metadata.version('UnityPy')!='1.25.4':raise ValueError('Require UnityPy1.25.4')
    import UnityPy
    inputs=[game,capture,rig_path,body,ROOT/'Assets',ROOT/'rebuild/generated']
    if running:inputs.append(running)
    output=fresh_output(output,inputs);player=read_json(rig_path)
    if player['sex']!='female' or player['avatar']['m_Name']!='FemaleAvatar':raise ValueError('Only qualified female starter rig supported')
    data=metadata.build_outputs();source=json.loads(data['pe_starter_sword_shield_source.json'])
    paths=[capture,capture.with_suffix('.launch.json'),rig_path,body,ROOT/'rebuild/generated/pe_starter_sword_shield_source.json']
    if paths[-1].read_bytes()!=data['pe_starter_sword_shield_source.json']:raise ValueError('Stale source metadata')
    if running:paths.extend([running,running.with_suffix('.launch.json')])
    if pose_set not in ('normal','sword','sword-backward') or pose_set!='normal' and running:raise ValueError('Invalid pose set/running combination')
    base_names=NAMES if pose_set=='normal' else tuple(SWORD_CLIPS) if pose_set=='sword' else tuple(SWORD_BACKWARD_CLIPS)
    before={p:sha(p) for p in paths};expected=base_names+(('RunSwordAttack',) if running else ())
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.starter-pair-',dir=output.parent) as temporary:
        stage=Path(temporary);visual_dir=stage/'visual';visual_dir.mkdir();visual=visual_assets(game,visual_dir,source)
        rigs={};mounts={};union=dict(schema=1,sex=player['sex'],avatar=player['avatar'],parts=[])
        for group,(kind,mount_name) in GROUPS.items():
            (chain,path),prefab=source_mount(UnityPy,'female',mount_name);mounts[group]=(chain,path,prefab)
            rigs[group]=rigid_rig(player,visual['items'][kind],(visual_dir/visual['items'][kind]['obj']).read_text(),mount_name,path)
            union['parts'].extend(rigs[group]['parts'])
        reports=[];mount_validation={group:[] for group in GROUPS}
        for path,names in [(capture,base_names)]+([(running,('RunSwordAttack',))] if running else []):
            report,samples=bake.analyze(path,union,retain=True)
            validate_capture(report,read_json(path.with_suffix('.launch.json'),1024*1024),path,names)
            for group,(_,mount_name) in GROUPS.items():
                chain,mount_path,_=mounts[group]
                mount_validation[group].append(validate_mount(report,mount_name,chain,mount_path,player))
            reports.append((report,samples))
        for group,(kind,mount_name) in GROUPS.items():
            rig=rigs[group];generated=[];scratch=stage/('temporary-'+group);scratch.mkdir()
            for report,samples in reports:
                for clip in report['clips']:
                    directory=scratch/clip['name'];directory.mkdir()
                    result=bake.export_clip(directory,rig,visual_dir/'source-rig.json',samples[clip['name']],report,clip)
                    generated.append((directory,result))
            destination=stage/group;result=bake.combine_clips(destination,generated)
            for part in result['parts']:
                old=part['file'];part['file']=old.replace('player-part-',group+'-part-');(destination/old).rename(destination/part['file'])
            (destination/'recording.peas').write_text('PEAS1\n'+''.join(p['file']+' '+p['texture']+'\n' for p in result['parts']))
            chain,path,prefab=mounts[group]
            result.update(item_id=1 if kind=='sword' else 34,attachment=mount_name,source_mount_path=path,source_mount=chain[-1],
                          source_prefab=dict(path=str(prefab.relative_to(ROOT)),sha256=sha(prefab)),
                          mount_validation=mount_validation[group],
                          pose_scope=('Qualified existing Normal/Attack6-9 leaves' if pose_set=='normal' else f'Qualified{len(base_names)} Sword locomotion leaves')+(' plus running408' if running else ''),
                          collision_authorized=False,activation_observed=False,
                          limitations=[('Normal locomotion adapter, not recovered Sword locomotion.' if pose_set=='normal' else 'Exact Sword locomotion leaves; full controller blending is not reconstructed.'),
                                       'Shield follows unblocked source forearm poses; no masked block pose or mitigation enabled.',
                                       'Fixed mounts; no animated draw/stow transitions, original IK or shader equivalence.',
                                       'Sampled408 pose only; no root-motion or running sword action qualification.'])
            if group=='player-wood-sword-drawn':
                actor=destination/result['parts'][0]['file'];binary,hitbox=sword_hitbox(actor.read_bytes(),source)
                if len(result['parts'])!=1:raise ValueError('Sword capsule needs unique rigid actor')
                (destination/hitbox['file']).write_bytes(binary);hitbox['actor']=actor.name;result['hitbox']=hitbox
            result['body_actors']=check_body(body,clip_headers((destination/result['parts'][0]['file']).read_bytes()))
            (destination/'recording-manifest.json').write_text(json.dumps(result,indent=2)+'\n')
            import shutil
            shutil.rmtree(scratch) # Only this exporter-created temporary intermediate directory.
        validate_stage(stage,body,expected)
        if before!={p:sha(p) for p in paths}:raise ValueError('Source input changed during export')
        for original in visual['sources']:
            if sha(Path(original['path']))!=original['sha256']:raise ValueError('Installed source changed during export')
        manifest=dict(schema='pe-starter-pair-attachments-1',groups=list(GROUPS),clips=list(expected),installed=False,
                      source_metadata_sha256=sha(paths[4]),sources=[dict(path=str(p),sha256=digest) for p,digest in before.items()],
                      files=file_hashes(stage),pose_set=pose_set,normal_locomotion_adapter=pose_set=='normal',shield_block_pose_recovered=False,
                      limitations=['No replacement body/assets installed, no gameplay/equipment enabled.',
                                   'Masked shield layer and draw/stow transitions are not reconstructed.'])
        (stage/'starter-pair-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        stage.rename(output)
    return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--game',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--capture',type=Path,required=True);p.add_argument('--running-capture',type=Path)
    p.add_argument('--pose-set',choices=('normal','sword','sword-backward'),default='normal',help='Explicit complete source leaf group; Sword requires its separate matching body')
    p.add_argument('--rig',type=Path,default=ROOT/'rebuild/build/reference-player-starter/player-female-starter-rig.json')
    p.add_argument('--body-scene',type=Path,default=ROOT/'rebuild/build/reference-assets/player/recording.peas')
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    try:
        paths=[x.expanduser().resolve() for x in (a.game,a.capture,a.rig,a.body_scene)]
        result=export(paths[0],paths[1],a.running_capture.expanduser().resolve() if a.running_capture else None,paths[2],paths[3],a.output.expanduser(),a.pose_set)
        print(f'Staged3 modern attachments, {len(result["clips"])} exact body clocks; no installation or block poses: {a.output}')
    except (OSError,ValueError,KeyError,TypeError,IndexError,ImportError,struct.error) as error:p.exit(1,str(error)+'\n')


if __name__=='__main__':main()
