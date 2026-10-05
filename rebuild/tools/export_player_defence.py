#!/usr/bin/env python3
"""Recover source female defence capsules and a compatible ten-clip PEAC.

Uses optional UnityPy 1.25.4 only to read the repository prefab. The auxiliary
actor preserves the first rendered part's actual geometry, adding source joints
needed by the legs. Never modifies the existing scene or original installation.
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

import analyze_reference_bake as bake
import analyze_reference_pose as pose
import export_reference_hitboxes as hb
from export_reference_assets import pointer, transform_point
from export_reference_actor import matrix_values, multiply

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_RIG = ROOT/'rebuild/build/reference-player-starter/player-female-starter-rig.json'
DEFAULT_ACTORS = ROOT/'rebuild/build/animator-bake/player-female/all'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def source_capsules(path):
    import UnityPy
    if not path.is_file() or path.stat().st_size > 16*1024*1024:
        raise ValueError('Missing or oversized source prefab')
    env=UnityPy.load(str(path)); objects={o.path_id:o for o in env.objects}
    transforms={pid:o.read_typetree() for pid,o in objects.items() if o.type.name=='Transform'}
    candidates=[]
    for pid,obj in objects.items():
        if obj.type.name!='MonoBehaviour':continue
        tree=obj.read_typetree()
        if 'defenceParts' in tree:candidates.append((pid,tree))
    if len(candidates)!=1:raise ValueError('Expected exactly one source defence trigger')
    component,tree=candidates[0]
    if not tree['m_Enabled'] or not 1<=len(tree['defenceParts'])<=64:
        raise ValueError('Disabled or invalid source defence trigger')
    root=pointer(tree['modelRoot']); entries=[]
    for index,part in enumerate(tree['defenceParts']):
        capsule=part['capsule'];hb.local_points(capsule)
        bone=pointer(capsule['trans'])
        if not bone:raise ValueError('Player defence part has no source bone')
        cursor=bone;chain=[];seen=set()
        while cursor!=root:
            if cursor in seen or cursor not in transforms:raise ValueError('Defence bone outside model hierarchy')
            seen.add(cursor);node=transforms[cursor]
            chain.append(objects[pointer(node['m_GameObject'])].read_typetree()['m_Name'])
            cursor=pointer(node['m_Father'])
        if not chain:raise ValueError('Defence capsule attached to model root')
        name=part['name'];damage=part['damageScale'] or 1.
        if not isinstance(name,str) or not name.isascii() or not 0<len(name)<32 or not math.isfinite(damage) or damage<=0:
            raise ValueError('Invalid defence name or multiplier')
        entries.append(dict(name=name,kind=0,group=0,enabled=True,
                            form=part['defenceType'],material=part['defenceMaterial'],damage_scale=damage,
                            source_component=component,source_part=index,source_joint_transform=bone,
                            source_bone_name=chain[0],source_bone_path='/'.join(reversed(chain)),source_capsule=capsule))
    if len({e['name'] for e in entries})!=len(entries):raise ValueError('Duplicate source defence names')
    owner=objects[pointer(tree['m_GameObject'])].read_typetree()
    return entries,dict(component=component,owner=owner['m_Name'],layer=owner['m_Layer'],
                       model_root=root,center_bone=pointer(tree['centerBone']))


def extend_part(rig, entries, mapping):
    """Keep original skin indices; append only independently sourced missing bones."""
    extended=copy.deepcopy(rig);part=extended['parts'][0]
    source={}
    for item in rig['parts']:
        for i,name in enumerate(item['bone_names']):
            source.setdefault(name,(item,i))
    original=len(part['bone_names']); additions=[]
    for entry in entries:
        name=entry['source_bone_name']
        if name not in mapping['bones'] or mapping['bones'][name]['relative_path']!=entry['source_bone_path']:
            raise ValueError('Source capsule path does not match captured Avatar bone: '+name)
        if name not in source:raise ValueError('No source inverse bind for defence bone: '+name)
        if name not in part['bone_names']:
            src,i=source[name]
            part['bone_names'].append(name);part['bone_ids'].append(src['bone_ids'][i]);part['inverse_binds'].append(src['inverse_binds'][i])
            additions.append(dict(name=name,source_resource=src['resource'],source_mesh=src['mesh_id'],source_joint=i))
        entry['joint']=part['bone_names'].index(name)
        entry['bind_points']=hb.bind_points(entry['source_capsule'],part['inverse_binds'][entry['joint']])
    if len(part['bone_names'])>256:raise ValueError('Defence skeleton too large')
    extended['parts']=[part]
    return extended,dict(original_joints=original,added_joints=additions)


def validate_palettes(data, entries, names, samples):
    """Compare serialized native palettes with direct sampled bone-local capsules."""
    nj,_,_=hb.skeleton_data(data);_,_,nv,ni,_,nc=struct.unpack_from('<4s5I',data)
    at=24+nv*56+ni*4+nj*48;clips=[];maximum=0.
    for _ in range(nc):
        raw,fps,count,duration,loop=struct.unpack_from('<32sfIfI',data,at);at+=48
        name=raw.rstrip(b'\0').decode('ascii');source=samples[name];times=[s['realtime'] for s in source]
        if count!=math.ceil(duration*fps)+1:raise ValueError('Unexpected native terminal frame count')
        clip_error=0.
        for frame in range(count):
            t=times[-1] if frame==count-1 else min(frame/fps,times[-1])
            world=pose.sample_matrices(source,times,t,names)
            for entry in entries:
                values=struct.unpack_from('<12f',data,at+(frame*nj+entry['joint'])*48)
                palette=[list(values[r:r+4]) for r in range(0,12,4)]+[[0.,0.,0.,1.]]
                # Quantize bind points exactly like PEHB, independently of source points.
                local=hb.local_points(entry['source_capsule'])
                points=[[bake.f32(x) for x in p] for p in entry['bind_points']]
                actual=[transform_point(palette,p) for p in points]
                expected=[transform_point(world[entry['joint']],p) for p in local]
                error=max(math.dist(a,b) for a,b in zip(actual,expected));clip_error=max(clip_error,error)
                if error>1e-5 or not 0<math.dist(actual[2],actual[3])<10:
                    raise ValueError('Serialized player capsule disagrees with sampled bone')
            # Every matrix also has to be finite and nonsingular, including unused torso joints.
            for j in range(nj):
                values=struct.unpack_from('<12f',data,at+(frame*nj+j)*48)
                bake.inverse_affine([list(values[r:r+4]) for r in range(0,12,4)]+[[0.,0.,0.,1.]])
        at+=count*nj*48;maximum=max(maximum,clip_error)
        clips.append(dict(name=name,frames=count,duration=duration,fps=fps,loop=bool(loop),maximum_point_error_m=clip_error))
    if at!=len(data):raise ValueError('Trailing or truncated native actor data')
    return clips,maximum


def recover(prefab,rig_path,capture,actor_dir,output):
    if not rig_path.is_file() or rig_path.stat().st_size>64*1024*1024:
        raise ValueError('Missing or oversized player rig')
    rig=json.loads(rig_path.read_text());report,samples=bake.analyze(capture,rig,True)
    if rig['sex']!='female' or rig['appearance']!='starter':raise ValueError('Expected recovered female starter rig')
    entries,trigger=source_capsules(prefab)
    if len(entries)!=16 or trigger['component']!=11488762 or trigger['model_root']!=400170:
        raise ValueError('Unexpected female defence prefab identity/count')
    extended,extension=extend_part(rig,entries,report['rig_mapping'])
    manifests=[]
    for clip in report['clips']:
        directory=output/clip['name'];directory.mkdir()
        manifest=bake.export_clip(directory,extended,rig_path,samples[clip['name']],report,clip)
        manifests.append((directory,manifest))
    combined=output/'all';bake.combine_clips(combined,manifests)
    manifest=json.loads((combined/'recording-manifest.json').read_text())
    first=manifest['parts'][0]['file'];data=(combined/first).read_bytes()
    # Only the first material is needed to anchor an auxiliary collision actor.
    original_path=actor_dir/first
    if not original_path.is_file() or not 24<=original_path.stat().st_size<=256*1024*1024:
        raise ValueError('Missing or oversized original first actor')
    original=original_path.read_bytes()
    _,_,nv,ni,_,_=struct.unpack_from('<4s5I',data)
    _,version,onv,oni,_,_=struct.unpack_from('<4s5I',original)
    geometry_end=24+nv*56+ni*4
    if version!=2 or (nv,ni)!=(onv,oni) or data[24:geometry_end]!=original[24:geometry_end]:
        raise ValueError('Existing first actor geometry differs from recovered source')
    nj,fingerprint,_=hb.skeleton_data(data)
    clips,error=validate_palettes(data,entries,extended['parts'][0]['bone_names'],samples)
    binary=hb.HEADER.pack(b'PEHB',1,len(entries),nj,fingerprint,0,0)
    for e in entries:
        binary+=hb.RECORD.pack(e['name'].encode(),0,0,e['joint'],e['form'],e['material'],e['damage_scale'],*(x for p in e['bind_points'] for x in p))
    (output/'player-defence.peac').write_bytes(data);(output/'player-defence.pehb').write_bytes(binary)
    provenance=[prefab,rig_path,capture,actor_dir/first,ROOT/'Assets/Scripts/Attack/PECapsuleTrigger.cs',ROOT/'Assets/Scripts/Attack/PEDefenceTrigger.cs']
    result=dict(schema=1,format='PEHB1',actor_format='PEAC2',sex='female',source_trigger=trigger,entries=entries,
                defence_records=len(entries),enabled_defence=len(entries),joints=nj,joint_names=extended['parts'][0]['bone_names'],
                extension=extension,skeleton_hash=f'{fingerprint:016x}',clips=clips,maximum_point_error_m=error,
                sources=[dict(path=str(p),sha256=sha(p)) for p in provenance],
                files={name:dict(bytes=(output/name).stat().st_size,sha256=sha(output/name)) for name in ['player-defence.peac','player-defence.pehb']},
                limitations=['Capsule settings are from the repository FemaleModel prefab; installed runtime defence component values were not captured.',
                             'Auxiliary actor reuses actual torso geometry with six added source leg/foot joints; it is not the original first-part skeleton.',
                             'All capsule bones use source-baked poses; no controller blending, procedural IK or root-motion reconstruction.',
                             'Instantaneous geometry only: target eligibility, damage permission, swept history and damage application are separate.',
                             'Player movement/collision body and rendered clothing are not defence capsules.'])
    (output/'player-defence.json').write_text(json.dumps(result,sort_keys=True,indent=2)+'\n')
    return result


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture',type=Path);p.add_argument('--rig',type=Path,default=DEFAULT_RIG)
    p.add_argument('--actor-dir',type=Path,default=DEFAULT_ACTORS)
    p.add_argument('--prefab',type=Path,default=ROOT/'Assets/Resources/Prefab/PlayerPrefab/FemaleModel.prefab')
    p.add_argument('--output',type=Path,default=ROOT/'rebuild/build/player-defence')
    a=p.parse_args(argv)
    try:
        if importlib.metadata.version('UnityPy')!='1.25.4':raise ValueError('Expected UnityPy1.25.4')
        output=a.output.expanduser().resolve()
        if not output.is_relative_to(ROOT/'rebuild/build'):raise ValueError('Keep recovered output within rebuild/build')
        output.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.player-defence-',dir=output.parent) as tmp:
            staging=Path(tmp);result=recover(a.prefab.resolve(),a.rig.resolve(),a.capture.expanduser().resolve(),a.actor_dir.resolve(),staging)
            output.mkdir(exist_ok=True)
            for name in ('player-defence.peac','player-defence.pehb','player-defence.json'):(staging/name).replace(output/name)
        print(f'Exported {result["enabled_defence"]} defence capsules, {result["joints"]} joints, {len(result["clips"])} clips; max point error {result["maximum_point_error_m"]:.3g}m')
    except (ImportError,importlib.metadata.PackageNotFoundError):p.exit(1,'Use optional UnityPy1.25.4.\n')
    except (ValueError,KeyError,TypeError,OSError,struct.error) as e:p.exit(1,f'Player defence export: {e}\n')

if __name__=='__main__':main()
