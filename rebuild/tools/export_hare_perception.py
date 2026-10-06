#!/usr/bin/env python3
"""Export installed hare head sensors bound to the recovered PEAC skeleton.

Optional UnityPy1.25.4; original assets are read-only. Does not launch Unity or
sample a live process. --check verifies source hashes, mounts and generated files.
"""
import argparse
import hashlib
import importlib.metadata
import json
import math
from pathlib import Path
import struct

import export_reference_actor as actor
from export_reference_assets import pointer, multiply, transform_point
from export_reference_hitboxes import skeleton_data

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'rebuild/generated'
SOURCES = [ROOT/'Assets/Scripts/Assist'/name for name in ('PEVision.cs','PEHearing.cs','PEPerception.cs')]


def conformal_error(matrix):
    actor.finite([matrix[r][c] for r in range(3) for c in range(4)])
    columns = [[matrix[r][c] for r in range(3)] for c in range(3)]
    lengths = [math.sqrt(sum(x*x for x in column)) for column in columns]
    if min(lengths) < 1e-9:
        raise ValueError('Degenerate sensor bone transform')
    errors = [abs(lengths[i]/lengths[0]-1) for i in range(1,3)]
    errors += [abs(sum(a*b for a,b in zip(columns[i],columns[j]))/(lengths[i]*lengths[j])) for i in range(3) for j in range(i)]
    return max(errors)


def palette_error(data, joint, inverse_bind):
    _,version,nv,ni,nj,nc = struct.unpack_from('<4s5I',data)
    offset = 24+nv*56+ni*4+nj*48
    to_mesh = actor.inverse_affine(inverse_bind)
    maximum = 0.
    for _ in range(nc):
        if offset+48 > len(data):raise ValueError('Truncated actor clip')
        frames = struct.unpack_from('<I',data,offset+36)[0]
        offset += 48
        if not 1 <= frames <= 20000 or offset+frames*nj*48>len(data):raise ValueError('Invalid actor frames')
        for frame in range(frames):
            values = struct.unpack_from('<12f',data,offset+(frame*nj+joint)*48)
            palette = [list(values[i:i+4]) for i in range(0,12,4)]+[[0.,0.,0.,1.]]
            bone = multiply(palette,to_mesh)
            maximum = max(maximum,conformal_error(bone))
        offset += frames*nj*48
    if offset != len(data):raise ValueError('Unexpected actor payload')
    if maximum > 1e-4:raise ValueError('Nonuniform/sheared sampled head transforms: TransformDirection cannot use affine axis point')
    return maximum


def recover(bundle, actor_directory):
    import UnityPy
    if importlib.metadata.version('UnityPy') != '1.25.4':raise ValueError('Expected UnityPy1.25.4')
    if not bundle.is_file() or bundle.stat().st_size>64*1024*1024:raise ValueError('Missing/oversized bundle')
    source_hash = hashlib.sha256(bundle.read_bytes()).hexdigest()
    manifest_path = actor_directory/'hare-manifest.json'; peac = actor_directory/'hare.peac'
    if manifest_path.stat().st_size>8*1024*1024 or peac.stat().st_size>256*1024*1024:raise ValueError('Oversized actor input')
    manifest = json.loads(manifest_path.read_text());data=peac.read_bytes()
    if manifest['source_sha256']!=source_hash or manifest['renderer_path_id']!=actor.RENDERER_ID:raise ValueError('Actor/source provenance mismatch')
    if manifest['files']['hare.peac']['sha256']!=hashlib.sha256(data).hexdigest():raise ValueError('Actor file/manifest hash mismatch')
    nj,fingerprint,binds=skeleton_data(data)
    objects={obj.path_id:obj for obj in UnityPy.load(str(bundle)).objects}
    renderer=objects[actor.RENDERER_ID].read_typetree();bones=[pointer(p) for p in renderer['m_Bones']]
    if bones!=manifest['joint_transform_path_ids'] or len(bones)!=nj or len(set(bones))!=nj:raise ValueError('Joint order mismatch')
    mesh=objects[pointer(renderer['m_Mesh'])].read_typetree()
    source_binds=[[[b[f'e{r}{c}'] for c in range(4)] for r in range(4)] for b in mesh['m_BindPose']]
    if len(source_binds)!=nj or any(struct.pack('<12f',*actor.matrix_values(a))!=struct.pack('<12f',*actor.matrix_values(b)) for a,b in zip(binds,source_binds)):raise ValueError('Inverse-bind mismatch')
    sensors={}
    for pid,obj in objects.items():
        if obj.type.name!='MonoBehaviour':continue
        tree=obj.read_typetree();script=objects.get(pointer(tree['m_Script']))
        kind=script.read_typetree().get('m_ClassName') if script else None
        if kind not in ('PEVision','PEHearing'):continue
        if kind in sensors:raise ValueError('Multiple source sensors need an explicit multi-sensor export')
        go_id=pointer(tree['m_GameObject']);go=objects[go_id].read_typetree()
        transform_ids=[p for p,o in objects.items() if o.type.name=='Transform' and pointer(o.read_typetree()['m_GameObject'])==go_id]
        if len(transform_ids)!=1 or transform_ids[0] not in bones:raise ValueError('Sensor is not a unique skin bone')
        transform_id=transform_ids[0];joint=bones.index(transform_id)
        node=next(n for n in manifest['hierarchy'] if n['path_id']==transform_id)
        ancestor=transform_id;names=[];active=True
        for _ in range(256):
            transform=objects[ancestor].read_typetree();owner=objects[pointer(transform['m_GameObject'])].read_typetree()
            active=active and bool(owner['m_IsActive'])
            if ancestor==actor.MODEL_ROOT_ID:break
            names.append(owner['m_Name']);ancestor=pointer(transform['m_Father'])
            if not ancestor:raise ValueError('Sensor is outside model root')
        else:raise ValueError('Sensor hierarchy cycle/depth limit')
        if node['path']!='/'.join(reversed(names)):raise ValueError('Sensor hierarchy path mismatch')
        radius=float(tree['radius'])
        if not math.isfinite(radius) or not 0<=radius<=1000:raise ValueError('Invalid radius')
        to_mesh=actor.inverse_affine(binds[joint])
        entry=dict(component=pid,game_object=go_id,owner=go['m_Name'],transform=transform_id,joint=joint,path=node['path'],
                   enabled=bool(tree['m_Enabled']),active=active,radius=radius,
                   bind_origin=transform_point(to_mesh,[0.,0.,0.]))
        if kind=='PEVision':
            axis=[float(tree['axis'][k]) for k in 'xyz'];actor.finite(axis);angle=float(tree['angle'])
            if not 0<angle<=180 or math.sqrt(sum(x*x for x in axis))<1e-9:raise ValueError('Invalid vision cone')
            entry.update(angle=angle,axis=axis,bind_axis_point=transform_point(to_mesh,axis),sampled_conformal_error=palette_error(data,joint,binds[joint]))
        else:entry['ignore_block']=bool(tree['isIgnoreBlock'])
        sensors[kind]=entry
    if set(sensors)!={'PEVision','PEHearing'}:raise ValueError('Missing source sensors')
    if sensors['PEVision']['transform']!=sensors['PEHearing']['transform']:raise ValueError('Different mounts require separate runtime poses')
    return dict(schema=1,source=actor.BUNDLE,source_sha256=source_hash,actor_sha256=hashlib.sha256(data).hexdigest(),
        manifest_sha256=hashlib.sha256(manifest_path.read_bytes()).hexdigest(),skeleton_hash=f'{fingerprint:016x}',joints=nj,
        sensors=sensors,block_layers=[11,12,13,14,16,21],sources=[dict(path=str(p.relative_to(ROOT)),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in SOURCES],
        limitations=['Installed serialized sensor settings; live values may be changed by source scripts/buffs.',
        'TransformDirection ignores scale. Export rejects nonconformal sampled head matrices; normalize posed axis-point minus origin.',
        'Between samples the existing native affine palette interpolation remains an approximation to Unity rotation interpolation.',
        'Sensor radius is an unscaled world distance. Outer root uniform scale applies to sensor origin, not radius.',
        'This export does not add a perception ray, target permission, hostile target memory or combat behavior.'])


def cfloat(value):
    text=format(value,'.9g')
    return text+('f' if '.' in text or 'e' in text else '.0f')


def render_header(report):
    v=report['sensors']['PEVision'];h=report['sensors']['PEHearing']
    vector=lambda values:'{'+','.join(cfloat(x) for x in values)+'}'
    return '\n'.join([
        '/* Generated by tools/export_hare_perception.py; do not edit. */',
        '#ifndef PE_HARE_PERCEPTION_SOURCE_H','#define PE_HARE_PERCEPTION_SOURCE_H','#include <stdint.h>',
        f'#define PE_HARE_SENSOR_JOINT {v["joint"]}u',f'#define PE_HARE_SENSOR_SKELETON_HASH UINT64_C(0x{report["skeleton_hash"]})',
        f'#define PE_HARE_SENSOR_VISION_COMPONENT INT64_C({v["component"]})',f'#define PE_HARE_SENSOR_HEARING_COMPONENT INT64_C({h["component"]})',
        f'#define PE_HARE_SENSOR_TRANSFORM INT64_C({v["transform"]})',
        f'#define PE_HARE_SENSOR_VISION_ENABLED {int(v["enabled"] and v["active"])}',f'#define PE_HARE_SENSOR_HEARING_ENABLED {int(h["enabled"] and h["active"])}',
        f'#define PE_HARE_SENSOR_VISION_RADIUS {cfloat(v["radius"])}',f'#define PE_HARE_SENSOR_VISION_ANGLE {cfloat(v["angle"])}',
        f'#define PE_HARE_SENSOR_HEARING_RADIUS {cfloat(h["radius"])}',f'#define PE_HARE_SENSOR_HEARING_IGNORE_BLOCK {int(h["ignore_block"])}',
        f'static const float pe_hare_sensor_bind_origin[3]={vector(v["bind_origin"])};',
        f'static const float pe_hare_sensor_bind_axis_point[3]={vector(v["bind_axis_point"])};',
        f'static const char pe_hare_sensor_path[]={json.dumps(v["path"])};', '#endif',''])


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    p.add_argument('--actor-dir',type=Path,default=actor.DEFAULT_OUTPUT);p.add_argument('--output',type=Path,default=OUTPUT);p.add_argument('--check',action='store_true');args=p.parse_args(argv)
    try:
        report=recover(args.game_dir/actor.BUNDLE,args.actor_dir)
        outputs={'pe_hare_perception_source.h':render_header(report),'pe_hare_perception_source.json':json.dumps(report,indent=2,sort_keys=True)+'\n'}
        if args.check:
            for name,text in outputs.items():
                if not (args.output/name).is_file() or (args.output/name).read_text()!=text:raise ValueError(f'Stale generated {name}')
        else:
            game=args.game_dir.resolve();out=args.output.resolve()
            if out==game or game in out.parents:raise ValueError('Output must be outside original installation')
            args.output.mkdir(parents=True,exist_ok=True)
            for name,text in outputs.items():(args.output/name).write_text(text)
        print(('Verified' if args.check else 'Exported')+' hare perception mount: joint'+str(report['sensors']['PEVision']['joint']))
    except (ImportError,importlib.metadata.PackageNotFoundError):p.exit(1,'Use the isolated UnityPy1.25.4 environment.\n')
    except (OSError,ValueError,KeyError,TypeError,struct.error,StopIteration) as e:p.exit(1,f'Hare perception export failed: {e}\n')

if __name__=='__main__':main()
