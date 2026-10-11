#!/usr/bin/env python3
"""Recover knife33 geometry and optional closed pose-capture capsule samples.
No graphics, input, process injection, damage or inferred attack activation.
Requires UnityPy1.25.4 for source/installed prefab reads; geometry tests stdlib.
"""
from read_reference_adventure import source_label
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sqlite3
import struct
import tempfile

from export_reference_assets import multiply, transform_point
from export_reference_actor import inverse_affine, matrix_values
from export_reference_hitboxes import local_points, skeleton_data, HEADER, RECORD
from inventory_player_animation import prefab_mounts
from analyze_reference_pose import decode, validate_frame, matrix, sample_matrices, MAX_BYTES, MAX_LINE, MAX_FRAMES

ROOT=Path(__file__).resolve().parents[2]
PREFAB=ROOT/'Assets/Resources/Prefab/Item/Equip/Weapon/Sword/knife.prefab'
BLADE=re.compile(r'/(mountMain|mountWaist)/(knife(?:\(Clone\))?)/SingleHandSword$')


def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def basis(matrix_value,uniform=False):
    columns=[[matrix_value[r][c] for r in range(3)] for c in range(3)]
    lengths=[math.sqrt(sum(x*x for x in c)) for c in columns]
    if any(not math.isfinite(x) or x<1e-6 or x>1000 for x in lengths):raise ValueError('Unsupported transform scale')
    shear=max(abs(sum(columns[a][k]*columns[b][k] for k in range(3))/(lengths[a]*lengths[b])) for a,b in ((0,1),(0,2),(1,2)))
    if shear>.0001:raise ValueError('Sheared transform cannot supply exact scalar capsule radius')
    if uniform and max(lengths)-min(lengths)>.0001*max(lengths):raise ValueError('Nonuniform model root would turn a sphere into an ellipsoid')
    return lengths,shear


def posed_capsule(capsule,root_world,weapon_world):
    """Preserve source world radius; convert only under a uniform model root."""
    root_scale,root_shear=basis(root_world,True);weapon_scale,weapon_shear=basis(weapon_world)
    points=local_points(capsule);local=multiply(inverse_affine(root_world),weapon_world)
    world_points=[transform_point(weapon_world,p) for p in points[:2]]
    model_points=[transform_point(local,p) for p in points[:2]]
    radius=capsule['radius']*weapon_scale[0]
    values=[x for p in world_points+model_points for x in p]+[radius]
    if any(not math.isfinite(x) or abs(x)>1e6 for x in values):raise ValueError('Capsule output bound exceeded')
    return dict(model_a=model_points[0],model_b=model_points[1],model_radius=radius/root_scale[0],
                world_a=world_points[0],world_b=world_points[1],world_radius=radius,
                weapon_model_matrix=local,maximum_basis_shear=max(root_shear,weapon_shear))


def source_definition():
    import UnityPy
    database=ROOT/'Assets/Resources/DataBase/localData.bytes'
    with sqlite3.connect(database.as_uri()+'?mode=ro',uri=True) as db:
        row=db.execute('SELECT _modelPath FROM PrototypeItem WHERE id="33"').fetchone()
    if row!=('Prefab/Item/Equip/Weapon/Sword/knife',):raise ValueError('Knife item33 mapping changed')
    objects={o.path_id:o for o in UnityPy.load(str(PREFAB)).objects}
    trigger=objects[11422180].read_typetree();equipment=objects[11433344].read_typetree()
    if len(trigger['attackParts'])!=1:raise ValueError('Expected one knife capsule')
    part=trigger['attackParts'][0];capsule=part['capsule'];local_points(capsule)
    transform=objects[capsule['trans']['m_PathID']].read_typetree()
    owner=objects[transform['m_GameObject']['m_PathID']].read_typetree()['m_Name']
    if owner!='SingleHandSword':raise ValueError('Unexpected source knife attachment')
    box=objects[6583062].read_typetree()
    tags=next(iter(UnityPy.load(str(ROOT/'ProjectSettings/TagManager.asset')).objects)).read_typetree()['layers']
    sources=[PREFAB,database,ROOT/'Assets/Scripts/Attack/PECapsuleTrigger.cs',ROOT/'Assets/Scripts/Attack/PEAttackTrigger.cs',
             ROOT/'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Sword.cs',
             ROOT/'Assets/Scripts/ZhouXun/Voxel Creation/Scripts/WhiteCat/PE-VC/BoneCollector.cs']
    mounts=[prefab_mounts(UnityPy,ROOT/f'Assets/Resources/Prefab/PlayerPrefab/{sex}Model.prefab') for sex in ('Male','Female')]
    for mount in mounts:mount['mount_chains']=[c for c in mount['mount_chains'] if any(t['name']=='Model' for t in c)]
    return dict(item_id=33,source_component=11422180,source_transform=capsule['trans']['m_PathID'],attack_form=part['attackForm'],
                capsule=capsule,local_points=local_points(capsule),source_transform_fields=transform,
                broadphase_box=box,attack_layer_mask=trigger['attackLayer']['m_Bits'],
                attack_layers=[dict(index=i,name=tags[i]) for i in range(32) if trigger['attackLayer']['m_Bits']&(1<<i)],
                attachment=equipment['m_HandChangeAttr'],source_mounts=mounts,
                sources=[dict(path=str(p.relative_to(ROOT)),sha256=digest(p)) for p in sources])


def installed_definition(game,source):
    from export_reference_player import Assets
    assets=Assets(game/'PE_Client_Data')
    manager=next(o for o in assets.file('mainData').objects.values() if o.type.name=='ResourceManager')
    candidates=[assets.resolve(manager,p) for n,p in manager.read_typetree()['m_Container'] if n=='prefab/item/equip/weapon/sword/knife']
    root,=[o for o in candidates if o.type.name=='GameObject'];blade=assets.child(root,'SingleHandSword')
    transform,=assets.components(blade,'Transform');found=[]
    for component in assets.components(blade,'MonoBehaviour'):
        header=component.read_typetree(check_read=False)
        if assets.resolve(component,header['m_Script']).read_typetree()['m_ClassName']=='PEAttackTrigger':found.append(component)
    trigger,=found;raw=trigger.get_raw_data()
    # Unity5.2 player MonoBehaviour base is32B, then mask/count/one part.
    # Full byte count and source field values checked; no editor TestHit fields.
    if len(raw)!=80:raise ValueError('Unexpected installed knife trigger layout')
    mask,count,form,file_id,bone,radius,height,x,y,z,axis=struct.unpack_from('<IIiiqff3fi',raw,32)
    c=dict(radius=radius,heigh=height,offset=dict(x=x,y=y,z=z),axis=axis)
    box,=assets.components(blade,'BoxCollider');box=box.read_typetree()
    if count!=1 or file_id or bone!=transform.path_id or form!=source['attack_form'] or mask!=source['attack_layer_mask'] or local_points(c)!=source['local_points']:
        raise ValueError('Installed knife differs from source capsule')
    for k in ('m_Size','m_Center','m_IsTrigger','m_Enabled'):
        if box[k]!=source['broadphase_box'][k]:raise ValueError('Installed broadphase differs from source')
    return dict(root_id=root.path_id,trigger_id=trigger.path_id,transform_id=transform.path_id,
                raw_trigger_sha256=hashlib.sha256(raw).hexdigest(),capsule_matches=True,box_matches=True,
                transform=transform.read_typetree(),asset=str(game/'PE_Client_Data/resources.assets'))


def capture_samples(path,capsule):
    if not path.is_file() or path.stat().st_size>MAX_BYTES:raise ValueError('Missing/oversized closed capture')
    header=None;ended=False;count=0;previous=None;frames=[];identity=None;gender=None;counts={'mountMain':0,'mountWaist':0}
    with path.open('rb') as stream:
        while line:=stream.readline(MAX_LINE+1):
            if len(line)>MAX_LINE or stream.tell()>MAX_BYTES or not line.endswith(b'\n'):raise ValueError('Capture line/byte bound or truncation')
            row=decode(line);kind=row['type']
            if ended:raise ValueError('Data after capture end')
            if header is None:
                if kind!='header' or row['schema']!=1 or row['phase']!='WaitForEndOfFrame' or row['matrixOrder']!='row-major':raise ValueError('Unsupported capture header')
                if not 1<=row['maxFrames']<=MAX_FRAMES or not 1<=row['maxBytes']<=MAX_BYTES:raise ValueError('Invalid capture bounds')
                header=row;continue
            if kind=='humanTraits':
                if count:raise ValueError('Late human traits')
                continue
            if kind=='end':
                if row['frames']!=count:raise ValueError('Capture end count mismatch')
                ended=True;continue
            if kind!='frame':raise ValueError('Unknown capture record')
            records,world,_=validate_frame(row)
            if row['sample']!=count or count>=header['maxFrames']:raise ValueError('Invalid sample sequence')
            if previous and (row['frame']<=previous['frame'] or row['realtime']<=previous['realtime'] or row['time']<previous['time']):raise ValueError('Nonmonotonic capture clocks')
            previous=row;count+=1
            root=row['modelRoot'];matches=[(t,BLADE.search(t['path'])) for t in row['transforms'] if t['path'].startswith(root['path']+'/') and BLADE.search(t['path'])]
            if len(matches)!=1:raise ValueError('Expected exactly one captured knife/SingleHandSword')
            weapon,match=matches[0];parent=records[weapon['parentId']];mount=records[parent['parentId']]
            current=(root['id'],weapon['id'],parent['id'])
            if identity is not None and identity!=current:raise ValueError('Captured weapon/model identity changed')
            identity=current
            if gender is not None and gender!=row['gender']:raise ValueError('Capture gender changed')
            gender=row['gender'];attachment=match[1]
            if mount['path'].rsplit('/',1)[-1]!=attachment:raise ValueError('Knife parent chain disagrees')
            counts[attachment]+=1
            posed=posed_capsule(capsule,world[root['id']],world[weapon['id']])
            frames.append(dict(sample=row['sample'],frame=row['frame'],time=row['time'],realtime=row['realtime'],
                               weapon_transform_id=weapon['id'],weapon_path=weapon['path'],attachment=attachment,
                               transform_active_in_hierarchy=weapon['activeInHierarchy'],attack_active=None,
                               model_root_world=world[root['id']],mount_local=matrix(mount['local'],'mount local'),
                               knife_model_matrix=multiply(inverse_affine(world[root['id']]),world[parent['id']]),
                               **posed))
    if not ended or not count:raise ValueError('Recording requires completed nonempty capture')
    return dict(path=source_label(path),sha256=digest(path),gender=gender,frame_count=count,attachment_counts=counts,
                realtime_duration=frames[-1]['realtime']-frames[0]['realtime'],
                attack_activation_observed=False,collision_authorized=False,frames=frames)


def obj_parts(text):
    """Strict bounded triangulated v/vt/vn subset of the reference OBJ exporter."""
    vertices=[];normals=[];uvs=[];parts={};material=None
    for line in text.splitlines():
        words=line.split()
        if not words or words[0].startswith('#'):continue
        kind=words[0]
        if kind in ('v','vn','vt'):
            size=2 if kind=='vt' else 3
            if len(words)!=size+1:raise ValueError('Invalid knife OBJ vector')
            values=[float(x) for x in words[1:]]
            if any(not math.isfinite(x) or abs(x)>10000 for x in values):raise ValueError('Invalid knife OBJ numeric range')
            target=vertices if kind=='v' else normals if kind=='vn' else uvs
            if len(target)>=100000:raise ValueError('Knife OBJ vertex bound exceeded')
            if kind=='vn' and sum(x*x for x in values)<1e-12:raise ValueError('Zero knife normal')
            target.append(values)
        elif kind=='usemtl':
            if len(words)!=2 or not re.fullmatch(r'[A-Za-z0-9_-]+',words[1]):raise ValueError('Invalid knife material')
            material=words[1];parts.setdefault(material,[])
        elif kind=='f':
            if len(words)!=4 or material is None:raise ValueError('Knife faces must be material-assigned triangles')
            if sum(len(f) for f in parts.values())>=100000:raise ValueError('Knife triangle bound exceeded')
            triangle=[]
            for word in words[1:]:
                if not re.fullmatch(r'[1-9][0-9]*/[1-9][0-9]*/[1-9][0-9]*',word):raise ValueError('Invalid knife OBJ index')
                indices=[int(x)-1 for x in word.split('/')]
                if any(i>=len(a) for i,a in zip(indices,(vertices,uvs,normals))):raise ValueError('Knife OBJ index out of range')
                triangle.append(tuple(indices))
            parts[material].append(triangle)
        elif kind not in ('mtllib','o','g'):raise ValueError('Unsupported knife OBJ statement')
    if not vertices or not parts or len(parts)>8:raise ValueError('Missing/oversized knife mesh')
    result={}
    for material,triangles in parts.items():
        unique={};rows=[];indices=[]
        for triangle in triangles:
            for key in triangle:
                if key not in unique:
                    unique[key]=len(rows);v,uv,n=key;rows.append((vertices[v],normals[n],uvs[uv]))
                indices.append(unique[key])
        if not indices:raise ValueError('Empty knife material')
        result[material]=(rows,indices)
    return result


def rigid_peac(rows,indices,palettes,fps,duration):
    binary=bytearray(struct.pack('<4s5I',b'PEAC',2,len(rows),len(indices),1,1))
    for position,normal,uv in rows:binary.extend(struct.pack('<12f4H',*position,*normal,*uv,1.,0.,0.,0.,0,0,0,0))
    binary.extend(struct.pack('<'+'I'*len(indices),*indices))
    binary.extend(struct.pack('<12f',1,0,0,0,0,1,0,0,0,0,1,0))
    binary.extend(struct.pack('<32sfIfI',b'captured',fps,len(palettes),duration,0))
    for palette in palettes:
        inverse_affine(palette);binary.extend(struct.pack('<12f',*matrix_values(palette)))
    return binary


def knife_hitbox(binary,capsule,selected):
    """One source blade capsule, tied to the rigid actor's exact bind geometry."""
    transforms=[multiply(inverse_affine(f['knife_model_matrix']),f['weapon_model_matrix']) for f in selected]
    local=transforms[0]
    if any(abs(t[r][c]-local[r][c])>1e-5 for t in transforms for r in range(4) for c in range(4)):
        raise ValueError('Blade moves relative to knife root; one rigid joint cannot encode it')
    points=[transform_point(local,p) for p in local_points(capsule)]
    joints,fingerprint,_=skeleton_data(binary)
    if joints!=1:raise ValueError('Knife hitbox requires the rigid one-joint actor')
    data=HEADER.pack(b'PEHB',1,1,joints,fingerprint,0,0)
    data+=RECORD.pack(b'SingleHandSword',1,1,0,0,0,1.,*(x for p in points for x in p))
    return data,dict(file='knife-recording-0.pehb',skeleton_hash=f'{fingerprint:016x}',kind='attack',group='right',joint=0,
                     bind_points=points,activation_observed=False,source_damage_scale=None,
                     note='Damage scale 1/material 0 are unused attack-record placeholders; source attack form 0.')


def clip_bytes(data):
    joints,_,_=skeleton_data(data)
    _,_,vertices,indices,_,clips=struct.unpack_from('<4s5I',data)
    offset=24+56*vertices+4*indices+48*joints
    if clips!=1 or len(data)<offset+48:raise ValueError('Combined scene requires one complete clip')
    raw=data[offset:offset+48];name,fps,frames,duration,loop=struct.unpack('<32sfIfI',raw)
    if not name.startswith(b'captured\0') or not math.isfinite(fps) or fps<=0 or not math.isfinite(duration) or duration<=0 or not frames or loop or len(data)!=offset+48+frames*joints*48:
        raise ValueError('Invalid captured clip structure')
    return data[4:8]+raw # Version is part of timing semantics, not skeleton identity.


def combine_body(output,body,expected_clip,report):
    """Copy a bounded snapshot into staging; never modify the body scene."""
    if body.is_symlink() or not body.is_file() or body.stat().st_size>65536:raise ValueError('Invalid body scene')
    lines=body.read_text('ascii').splitlines()
    if not lines or lines[0]!='PEAS1' or not 1<=len(lines)-1<=30:raise ValueError('Invalid body scene parts')
    snapshots={};actors=set();rows=[];total=0
    for line in lines[1:]:
        fields=line.split(' ')
        if len(fields)!=2:raise ValueError('Invalid body scene row')
        for name,suffix in zip(fields,('.peac','.png')):
            if not re.fullmatch(r'[A-Za-z0-9_-][A-Za-z0-9_.-]*',name) or '..' in name or not name.endswith(suffix):raise ValueError('Invalid body asset basename')
            if (output/name).exists():raise ValueError('Body/knife filename collision')
            path=body.parent/name
            if name in snapshots:continue
            if path.is_symlink() or not path.is_file() or path.stat().st_size>256*1024*1024:raise ValueError('Missing/oversized body asset')
            data=path.read_bytes();total+=len(data)
            if total>512*1024*1024:raise ValueError('Combined body byte budget exceeded')
            if suffix=='.peac' and clip_bytes(data)!=expected_clip:raise ValueError('Body and knife clip clocks differ')
            if suffix=='.png' and not data.startswith(b'\x89PNG\r\n\x1a\n'):raise ValueError('Invalid body PNG signature')
            snapshots[name]=data
        if fields[0] in actors:raise ValueError('Duplicate body actor')
        actors.add(fields[0]);rows.append(line)
    for name,data in snapshots.items():(output/name).write_bytes(data)
    rows += [p['file']+' '+p['texture'] for p in report['parts']]
    (output/'recording.peas').write_text('PEAS1\n'+'\n'.join(rows)+'\n')
    report['combined_scene']=dict(file='recording.peas',body_source=str(body),parts=len(rows),
                                files={name:hashlib.sha256(data).hexdigest() for name,data in snapshots.items()})


def export_visual(output,assets,recording,first,last,fps,capsule=None,body_scene=None):
    if not math.isfinite(fps) or not 1<=fps<=240 or first<0 or (last is not None and last<first):raise ValueError('Invalid visual sample interval/FPS')
    selected=[f for f in recording['frames'] if f['sample']>=first and (last is None or f['sample']<=last)]
    if len(selected)<2:raise ValueError('Visual recording requires at least two selected samples')
    if any(not f['transform_active_in_hierarchy'] for f in selected):raise ValueError('Knife visibility changes are not encoded in PEAC')
    times=[f['realtime']-selected[0]['realtime'] for f in selected]
    fps=struct.unpack('<f',struct.pack('<f',fps))[0]
    duration=struct.unpack('<f',struct.pack('<f',times[-1]))[0];frames=math.ceil(duration*fps)+1
    if not 0<duration<=120 or frames>20000:raise ValueError('Visual duration/frame bound exceeded')
    samples=[dict(bones={'knife':f['knife_model_matrix']}) for f in selected]
    palettes=[sample_matrices(samples,times,times[-1] if f==frames-1 else min(f/fps,times[-1]),['knife'])[0] for f in range(frames)]
    manifest_path=assets/'training-knife-manifest.json'
    if manifest_path.stat().st_size>8*1024*1024:raise ValueError('Oversized knife visual manifest')
    manifest=decode(manifest_path.read_text());source=manifest['assets']['training-knife']
    materials={m['obj_symbol']:m for m in source['materials']}
    def checked_file(name,suffix):
        if not re.fullmatch(r'[A-Za-z0-9_-][A-Za-z0-9_.-]*',name) or '..' in name or not name.endswith(suffix):raise ValueError('Invalid knife visual filename')
        path=assets/name;expected=manifest['files'][name]
        if not path.is_file() or path.stat().st_size>64*1024*1024:raise ValueError('Missing/oversized knife visual file')
        data=path.read_bytes()
        if len(data)!=expected['bytes'] or hashlib.sha256(data).hexdigest()!=expected['sha256']:raise ValueError('Knife visual source hash mismatch')
        return data
    mesh=checked_file('training-knife.obj','.obj');parts=obj_parts(mesh.decode('utf-8'));outputs=[]
    for index,(symbol,(rows,indices)) in enumerate(parts.items()):
        material=materials[symbol];texture=material['diffuse_file'];png=checked_file(texture,'.png')
        filename=f'knife-recording-{index}.peac';binary=rigid_peac(rows,indices,palettes,fps,duration)
        if len(binary)>64*1024*1024:raise ValueError('Knife part byte bound exceeded')
        (output/filename).write_bytes(binary);(output/texture).write_bytes(png)
        if index==0:
            expected_clip=clip_bytes(binary)
            if capsule is not None:
                hitbox,hitbox_report=knife_hitbox(binary,capsule,selected)
                (output/'knife-recording-0.pehb').write_bytes(hitbox)
        outputs.append(dict(file=filename,texture=texture,vertices=len(rows),indices=len(indices),source_material=symbol,sha256=hashlib.sha256(binary).hexdigest()))
    (output/'knife-recording.peas').write_text('PEAS1\n'+''.join(p['file']+' '+p['texture']+'\n' for p in outputs))
    report=dict(schema=1,peac_version=2,terminal_at_duration=True,clip='captured',fps=fps,frames=frames,duration=duration,loop=False,parts=outputs,
                first_capture_sample=selected[0]['sample'],last_capture_sample=selected[-1]['sample'],capture_sha256=recording['sha256'],
                source_visual_manifest=str(manifest_path),source_visual_manifest_sha256=digest(manifest_path),
                coordinate_space='inverse(captured modelRoot world) * captured knife root world; OBJ already prefab-local',
                activation_observed=False,interpolation='Same realtime/resampled affine palette clock as analyze_reference_pose.py',
                limitations=['Rigid recorded visual only; no active trigger, contact or damage inference.','Original diffuse textures; original shader not reproduced.'])
    if capsule is not None:report['hitbox']=hitbox_report
    if body_scene is not None:combine_body(output,body_scene,expected_clip,report)
    (output/'knife-recording-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,help='Completed observational JSONL; never a live partial capture')
    parser.add_argument('--installed-game',type=Path,help='Optional read-only installed capsule comparison')
    parser.add_argument('--output',type=Path,default=ROOT/'rebuild/build/reference-assets/knife-collision.json')
    parser.add_argument('--scene-dir',type=Path,help='Separate directory for optional rigid visual PEAC/material parts')
    parser.add_argument('--body-scene',type=Path,help='Copy this PEAS into a fresh scene directory; all clip metadata must match exactly')
    parser.add_argument('--visual-assets',type=Path,default=ROOT/'rebuild/build/reference-assets')
    parser.add_argument('--first-sample',type=int,default=0);parser.add_argument('--last-sample',type=int)
    parser.add_argument('--fps',type=float,default=30.)
    args=parser.parse_args()
    try:
        source=source_definition();report=dict(schema=1,kind='knife-collision-audit',source=source,recording=None,
            limitations=['Geometry only: probe does not observe PEAttackTrigger.active or Collider.enabled.',
                         'No inferred hit activation, damage, target filtering, sweep or mission progression.',
                         'World local-X radius derived from orthogonal recorded transform; sheared transforms rejected.',
                         'Scalar model-space capsule requires uniform model-root scale.',
                         'Recorded samples are not a general humanoid animation solution.'])
        output=args.output.expanduser().resolve()
        if args.installed_game:
            game=args.installed_game.expanduser().resolve()
            if output==game or game in output.parents:raise ValueError('Output must be outside installation')
            report['installed']=installed_definition(game,source)
        if args.capture:
            capture=args.capture.expanduser().resolve()
            if output==capture:raise ValueError('Output cannot overwrite capture')
            report['recording']=capture_samples(capture,source['capsule'])
        if args.scene_dir:
            if report['recording'] is None:raise ValueError('Rigid visual requires a capture')
            scene=args.scene_dir.expanduser().resolve();assets=args.visual_assets.expanduser().resolve()
            if scene==assets or scene in assets.parents or scene==capture or scene in capture.parents:raise ValueError('Visual output must be separate from source assets/capture')
            if args.installed_game and (scene==game or game in scene.parents):raise ValueError('Visual output must be outside installation')
            body=args.body_scene.expanduser().resolve() if args.body_scene else None
            if body is not None and scene.exists():raise ValueError('Combined scene output must be a fresh directory')
            scene.parent.mkdir(parents=True,exist_ok=True)
            with tempfile.TemporaryDirectory(dir=scene.parent,prefix='.knife-recording-') as temporary:
                staging=Path(temporary);report['visual_recording']=export_visual(staging,assets,report['recording'],args.first_sample,args.last_sample,args.fps,source['capsule'],body)
                scene.mkdir(parents=True,exist_ok=True)
                for file in sorted(staging.iterdir()):file.replace(scene/file.name)
        elif args.body_scene:raise ValueError('--body-scene requires --scene-dir')
        payload=json.dumps(report,sort_keys=True,indent=2)+'\n'
        if len(payload.encode())>64*1024*1024:raise ValueError('Output exceeds64MiB')
        output.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.NamedTemporaryFile('w',dir=output.parent,prefix='.knife-collision-',delete=False) as stream:stream.write(payload);temporary=Path(stream.name)
        temporary.replace(output)
        print('Exported knife capsule'+(f' with {report["recording"]["frame_count"]} recorded poses' if report['recording'] else '')+'; activation unknown')
    except (OSError,ValueError,KeyError,TypeError,IndexError,struct.error,OverflowError) as error:parser.exit(1,f'Knife collision export failed: {error}\n')
if __name__=='__main__':main()
