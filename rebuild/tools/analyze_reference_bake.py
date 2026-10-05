#!/usr/bin/env python3
"""Strictly validate closed isolated-Animator bake JSONL and export native parts.

Standard library only. Does not launch Unity, evaluate clips or invent poses.
"""
import argparse
import json
import math
from pathlib import Path
import shutil
import struct
import tempfile
import zlib

import analyze_reference_pose as pose
from export_reference_assets import IDENTITY, multiply
from export_reference_actor import inverse_affine


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def mapping_for(rig, skeleton):
    if skeleton['gender'].lower() != rig['sex'] or skeleton['avatar'] != rig['avatar']['m_Name']:
        pose.fail('Bake gender/Avatar differs from rig')
    avatar = rig['avatar']['m_Avatar']; paths = dict(rig['avatar']['m_TOS'])
    hashes = dict(zip(avatar['m_SkeletonNameIDArray'], avatar['m_AvatarSkeleton']['data']['m_ID']))
    mapping = {}; missing = []; explicit = {}
    for part in rig['parts']:
        if 'bone_paths' not in part: continue
        if len(part['bone_paths']) != len(part['bone_names']): pose.fail('Explicit skin path count differs')
        for name, path in zip(part['bone_names'], part['bone_paths']):
            if not isinstance(path,str) or not path or len(path)>8192: pose.fail('Invalid explicit skin bone path')
            if name in explicit and explicit[name]!=path: pose.fail('Conflicting explicit skin bone paths')
            explicit[name]=path
    for part in rig['parts']:
        for name in part['bone_names']:
            if name in mapping or name in missing: continue
            key = hashes.get(zlib.crc32(name.encode())); expected = explicit.get(name,paths.get(key))
            candidates = [i for i, node in enumerate(skeleton['nodes']) if node['path'] == expected] if expected is not None else []
            if expected is None:
                candidates = [i for i, node in enumerate(skeleton['nodes']) if node['path'].rsplit('/',1)[-1] == name]
            if len(candidates) > 1: pose.fail('Ambiguous bake bone: '+name)
            if not candidates: missing.append(name)
            else: mapping[name] = {'index': candidates[0], 'relative_path': skeleton['nodes'][candidates[0]]['path']}
    return {'bones': mapping, 'missing_bones': missing, 'avatar': skeleton['avatar']}


def analyze(path, rig, retain=False):
    if not path.is_file() or path.stat().st_size > pose.MAX_BYTES: pose.fail('Missing/oversized bake')
    if rig['schema'] != 1 or not 1 <= len(rig['parts']) <= 64 or not 1 <= len({name for part in rig['parts'] for name in part['bone_names']}) <= 256:
        pose.fail('Invalid source rig schema/part/bone count')
    header = skeleton = mapping = active = None
    complete = False; total = 0; clips = []; samples = []; kept = {}; maximum = 0.; last_frame = -1; last_real = -1.; used_clone_ids=set()
    with path.open('rb') as stream:
        while line := stream.readline(pose.MAX_LINE+1):
            if len(line) > pose.MAX_LINE or stream.tell() > pose.MAX_BYTES or not line.endswith(b'\n'): pose.fail('Oversized/truncated bake line')
            row = pose.decode(line)
            if not isinstance(row, dict): pose.fail('Bake record must be an object')
            kind = row['type']
            if complete: pose.fail('Data after bake end')
            if header is None:
                if kind != 'header' or row['schema'] != 'pe-animator-bake-1' or row['matrixOrder'] != 'row-major' or row['coordinateSpace'] != 'isolated-animator-model-local':
                    pose.fail('Unsupported bake header')
                if any(row[k] is not False for k in ('fireEvents','controllerAttached','applyRootMotion')): pose.fail('Bake isolation flags differ')
                if row.get('sampleCountPolicy') not in (None,'ceil-f64-product-terminal-length'):pose.fail('Unknown sample count policy')
                if row.get('cleanupPolicy') not in (None,'stop-disable-dispose','retain-inactive-per-clip-until-process-exit'):pose.fail('Unknown lifecycle policy')
                if row.get('cleanupPolicy')=='retain-inactive-per-clip-until-process-exit' and row.get('maxRetainedGraphs')!=10:pose.fail('Invalid retained graph bound')
                if not 1 <= pose.integer(row['sampleRate'],'rate') <= 60 or not 1 <= pose.integer(row['maxSamples'],'sample bound') <= 3600 or not 1 <= pose.integer(row['maxBytes'],'byte bound') <= pose.MAX_BYTES:
                    pose.fail('Invalid bake bounds')
                header = row; continue
            if kind == 'skeleton':
                if skeleton is not None or total: pose.fail('Duplicate/late skeleton')
                nodes = row['nodes']
                if not 2 <= len(nodes) <= 512: pose.fail('Invalid skeleton count')
                source_ids = set(); clone_ids = set(); paths = set()
                for i, node in enumerate(nodes):
                    relative = node['path']; parent = pose.integer(node['parent'],'parent')
                    if not isinstance(relative,str) or len(relative)>8192 or relative in paths: pose.fail('Invalid/duplicate relative path')
                    if i == 0:
                        if parent != -1 or relative != '': pose.fail('Invalid root node')
                    elif not 0 <= parent < i or not relative or relative.rpartition('/')[0] != nodes[parent]['path']:
                        pose.fail('Invalid node ancestry/path')
                    for key, seen in (('sourceId',source_ids),('cloneId',clone_ids)):
                        identity = pose.integer(node[key],key)
                        if not identity or identity in seen: pose.fail('Null/duplicate transform identity')
                        seen.add(identity)
                    paths.add(relative); inverse_affine(pose.matrix(node['initialLocal'],'initial local'))
                if not 1 <= pose.integer(row['plannedSamples'],'planned') <= header['maxSamples']: pose.fail('Invalid planned sample count')
                for value in row['humanBones'].values():
                    if not 0 <= pose.integer(value,'human bone') < len(nodes): pose.fail('Unmapped human bone')
                skeleton = row; mapping = mapping_for(rig,row); continue
            if skeleton is None: pose.fail('Missing skeleton')
            if kind == 'clip':
                if active is not None or len(clips) >= 10 or row['index'] != len(clips): pose.fail('Invalid clip order/count')
                if not isinstance(row['name'],str) or not row['name'].isascii() or not row['name'].isalnum() or len(row['name'])>31 or any(c['name']==row['name'] for c in clips): pose.fail('Invalid/duplicate clip name')
                length = pose.finite_number(row['length'],'clip length')
                if not 0 < length <= 10 or not 0 < pose.finite_number(row['sourceFrameRate'],'source rate') <= 120: pose.fail('Invalid clip timing')
                # Legacy Mono may retain extra precision for the float product
                # on its evaluation stack. Both valid rounding paths differ by
                # at most one sample at a float-near-grid endpoint. Record which
                # occurred; reject every other count, never relax pose times.
                product=f32(length)*header['sampleRate']
                rounded_count=math.ceil(f32(product))+1
                extended_count=math.ceil(product)+1
                count=pose.integer(row['samples'],'clip samples')
                valid_counts=(extended_count,) if header.get('sampleCountPolicy') else (rounded_count,extended_count)
                if count not in valid_counts:pose.fail('Clip sample count differs from source float timeline')
                pose.boolean(row['isLooping'],'clip loop')
                if header.get('cleanupPolicy')=='retain-inactive-per-clip-until-process-exit':
                    ids=row['cloneIds']
                    if len(ids)!=len(skeleton['nodes']) or len(set(ids))!=len(ids) or any(not pose.integer(i,'clip clone id') for i in ids):pose.fail('Invalid clip clone identities')
                    if used_clone_ids.intersection(ids):pose.fail('Retained clone reused across clips')
                    if not clips and ids!=[n['cloneId'] for n in skeleton['nodes']]:pose.fail('First clip/skeleton clone identity mismatch')
                    used_clone_ids.update(ids)
                source_id=pose.integer(row['sourcePathId'],'clip source')
                if not -(1<<63)<=source_id<(1<<63) or not source_id or not pose.integer(row['runtimeId'],'runtime clip'): pose.fail('Invalid clip identity')
                if len(row['events'])>512: pose.fail('Event bound exceeded')
                for event in row['events']:
                    if not 0 <= pose.finite_number(event['time'],'event time') <= length+.001 or not isinstance(event['function'],str) or not isinstance(event['stringParameter'],str): pose.fail('Invalid clip event')
                active = dict(row)
                active['sample_count_arithmetic']='both agree' if rounded_count==extended_count else ('extended product precision' if count==extended_count else 'float32-rounded product')
                active['runtime_length_f32']=f32(length)
                samples = []; continue
            if kind == 'pose':
                if active is None or row['clip'] != active['index'] or row['sample'] != len(samples) or len(samples) >= active['samples']: pose.fail('Out-of-order pose')
                frame = pose.integer(row['frame'],'Unity frame',0); real = pose.finite_number(row['realtime'],'realtime')
                if frame <= last_frame or real <= last_real: pose.fail('Nonmonotonic runtime observation')
                last_frame, last_real = frame, real
                at = pose.finite_number(row['authoredTime'],'authored time'); expected = min(len(samples)/header['sampleRate'],active['runtime_length_f32'])
                if header.get('sampleCountPolicy') and len(samples)==active['samples']-1:expected=active['runtime_length_f32']
                if abs(at-expected)>1e-6: pose.fail('Authored sample time mismatch')
                local = [pose.matrix(m,'local') for m in row['local']]; world = [pose.matrix(m,'world') for m in row['world']]
                if len(local)!=len(skeleton['nodes']) or len(world)!=len(local): pose.fail('Pose node count mismatch')
                root = pose.matrix(row['rootWorld'],'root world'); maximum=max(maximum,pose.close_matrix(root,IDENTITY,'identity root'))
                for i, node in enumerate(skeleton['nodes']):
                    inverse_affine(world[i]); inverse_affine(local[i])
                    composed = local[i] if i==0 else multiply(world[node['parent']],local[i])
                    maximum=max(maximum,pose.close_matrix(world[i],composed,'parent/world'))
                maximum=max(maximum,pose.close_matrix(world[0],root,'root duplicate'))
                for key in ('animatorRootPosition','deltaPosition'): pose.vector(row[key],3,key)
                for key in ('animatorRootRotation','deltaRotation'):
                    q=pose.vector(row[key],4,key)
                    if abs(sum(v*v for v in q)-1)>1e-3: pose.fail('Invalid diagnostic root quaternion')
                inverse = inverse_affine(root)
                values={name:multiply(inverse,world[data['index']]) for name,data in mapping['bones'].items()}
                samples.append({'sample':row['sample'],'frame':frame,'realtime':at,'bones':values,
                                'root_diagnostics':{k:row[k] for k in ('animatorRootPosition','animatorRootRotation','deltaPosition','deltaRotation')}})
                total+=1
                if total>header['maxSamples']: pose.fail('Total sample bound exceeded')
                continue
            if kind == 'clipEnd':
                if active is None or row['clip'] != active['index'] or row['samples'] != len(samples) or len(samples) != active['samples']: pose.fail('Incomplete clip')
                if not samples: pose.fail('Empty clip')
                changes={name:max(pose.matrix_error(sample['bones'][name],samples[0]['bones'][name]) for sample in samples) for name in mapping['bones']}
                positions={name:max(math.sqrt(sum((sample['bones'][name][axis][3]-samples[0]['bones'][name][axis][3])**2 for axis in range(3))) for sample in samples) for name in mapping['bones']}
                active.update(maximum_bone_matrix_change=max(changes.values(),default=0),
                              changing_bones=sum(v>1e-5 for v in changes.values()),
                              maximum_bone_position_change_m=max(positions.values(),default=0),
                              bone_matrix_change=changes,bone_position_change_m=positions)
                if mapping['bones']:
                    active['bone_position_bounds']={key:[fn(sample['bones'][name][axis][3] for sample in samples for name in mapping['bones']) for axis in range(3)] for key,fn in [('min',min),('max',max)]}
                    active['first_bone_positions']={name:[samples[0]['bones'][name][axis][3] for axis in range(3)] for name in mapping['bones']}
                clips.append(active)
                if retain: kept[active['name']]=samples
                active=None; samples=[];continue
            if kind == 'end':
                if active is not None or row['reason'] != 'complete' or row['samples']!=total or total!=skeleton['plannedSamples'] or not clips: pose.fail('Bake is incomplete or failed')
                complete=True;continue
            pose.fail('Unknown bake record')
    if not complete: pose.fail('Closed successful bake end record required')
    return {'schema':1,'header':header,'skeleton':skeleton,'rig_mapping':mapping,'complete':True,'samples':total,'clips':clips,
            'maximum_hierarchy_matrix_error':maximum,'general_retargeting_validated':False,
            'root_motion_validated':False,'runtime_motion_detected':any(c['changing_bones'] for c in clips),
            'process_teardown_validated':False,'lifetime_note':'A completed output does not establish safe graph cleanup or process exit; verify externally.'}, kept


def export_clip(directory, rig, rig_path, samples, report, clip):
    # Reuse only the bounded multipart skin/texture writer. Its internal time
    # key is named realtime; these explicit samples carry authored clip time.
    result=pose.export_recording(directory,rig,rig_path,samples,report,report['header']['sampleRate'],list(range(len(rig['parts']))))
    for part in result['parts']:
        path=directory/part['file']; data=bytearray(path.read_bytes()); _,version,nv,ni,nj,nc=struct.unpack_from('<4s5I',data)
        if version!=2 or nc!=1: pose.fail('Unexpected writer layout')
        offset=24+nv*56+ni*4+nj*48
        struct.pack_into('<32s',data,offset,clip['name'].encode('ascii'))
        struct.pack_into('<I',data,offset+44,int(clip['isLooping']))
        path.write_bytes(data)
    result.update(clip=clip['name'],loop=clip['isLooping'],source_clip=clip,
                  coordinate_space='inverse(isolated Animator root) * isolated boneWorld * source inverseBind',
                  timebase='requested source authored seconds; paused isolated Unity AnimationClipPlayable',
                  interpolation='sampled authored poses; native affine palette interpolation between samples',
                  limitations=['Source clip baked by installed Unity, not a native humanoid solver.',
                               'No original Animator controller, gameplay scripts, procedural IK or transition blending.',
                               'Initial hierarchy copied from live local TRS and rebound; unanimated extras require validation.',
                               'Root motion diagnostic only; no cumulative attack/locomotion displacement claimed.',
                               'Source starter/default meshes do not reproduce runtime morphs and shader state.'])
    result.pop('first_capture_sample',None);result.pop('last_capture_sample',None)
    (directory/'recording-manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


def combine_clips(directory, sources):
    """Combine validated writer outputs, sharing identical skin geometry once."""
    if not 1 <= len(sources) <= 10: pose.fail('Invalid combined clip count')
    first_dir, first = sources[0]
    directory.mkdir()
    total_bytes = 0
    for part_index, part in enumerate(first['parts']):
        prefix = None; payloads = []
        for source_dir, manifest in sources:
            if len(manifest['parts']) != len(first['parts']): pose.fail('Combined part count differs')
            other = manifest['parts'][part_index]
            if other['file'] != part['file'] or other['texture_sha256'] != part['texture_sha256']: pose.fail('Combined part material differs')
            data=(source_dir/other['file']).read_bytes()
            magic,version,nv,ni,nj,nc=struct.unpack_from('<4s5I',data)
            if magic!=b'PEAC' or version!=2 or nc!=1: pose.fail('Combined source is not a single v2 clip')
            offset=24+nv*56+ni*4+nj*48
            if prefix is None: prefix=bytearray(data[:offset])
            elif bytes(prefix)!=data[:offset]: pose.fail('Combined source geometry/inverse binds differ')
            payloads.append(data[offset:])
        struct.pack_into('<I',prefix,20,len(sources))
        data=bytes(prefix)+b''.join(payloads);total_bytes+=len(data)
        if total_bytes>pose.MAX_BYTES:pose.fail('Combined actor byte budget exceeded')
        (directory/part['file']).write_bytes(data)
        shutil.copyfile(first_dir/part['texture'],directory/part['texture'])
    shutil.copyfile(first_dir/'recording.peas',directory/'recording.peas')
    manifest={k:v for k,v in first.items() if k not in ('clip','fps','frames','duration','loop','source_clip')}
    manifest['clips']=[{k:m[k] for k in ('clip','fps','frames','duration','loop','source_clip')} for _,m in sources]
    manifest['geometry_shared']=True
    (directory/'recording-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('bake',type=Path);p.add_argument('--rig',type=Path,required=True)
    p.add_argument('--report',type=Path,required=True);p.add_argument('--export-dir',type=Path);p.add_argument('--clip',action='append')
    a=p.parse_args()
    try:
        bake=a.bake.expanduser().resolve();rig_path=a.rig.expanduser().resolve();report_path=a.report.expanduser().resolve()
        if report_path in (bake,rig_path,bake.with_suffix('.launch.json')):pose.fail('Report cannot replace inputs')
        if rig_path.stat().st_size>64*1024*1024:pose.fail('Rig too large')
        rig=pose.decode(rig_path.read_text());report,samples=analyze(bake,rig,retain=a.export_dir is not None)
        report['sources']=[{'path':str(f),'sha256':pose.sha256(f)} for f in (bake,rig_path,Path(__file__).resolve())]
        launch_path=bake.with_suffix('.launch.json')
        report['launch_provenance']=None
        if launch_path.is_file():
            if launch_path.stat().st_size>1024*1024:pose.fail('Launch provenance too large')
            launch=pose.decode(launch_path.read_text())
            if launch['schema']!='pe-animator-bake-launch-1' or Path(launch['capture']).resolve()!=bake:pose.fail('Launch provenance capture mismatch')
            plan=launch['plan']
            if plan['avatar']!=report['skeleton']['avatar'] or plan['gender']!=report['skeleton']['gender'] or plan['sample_rate']!=report['header']['sampleRate'] or len(plan['clips'])!=len(report['clips']):pose.fail('Launch/bake identity mismatch')
            for expected,actual in zip(plan['clips'],report['clips']):
                if expected['name']!=actual['name'] or expected['path_id']!=actual['sourcePathId'] or expected['loop']!=actual['isLooping'] or abs(expected['length']-actual['length'])>.0001 or abs(expected['frame_rate']-actual['sourceFrameRate'])>.001:pose.fail('Launch/bake clip identity mismatch')
            report['sources'].append({'path':str(launch_path),'sha256':pose.sha256(launch_path)})
            report['launch_provenance']=launch
        if a.export_dir:
            out=a.export_dir.expanduser().resolve()
            if any(out==f or out in f.parents for f in (bake,rig_path)):pose.fail('Output cannot contain source inputs')
            selected=a.clip or [c['name'] for c in report['clips']]
            if len(set(selected))!=len(selected) or any(n not in samples for n in selected):pose.fail('Invalid selected clip')
            out.parent.mkdir(parents=True,exist_ok=True)
            with tempfile.TemporaryDirectory(prefix='.bake-',dir=out.parent) as temp:
                staging=Path(temp);generated=[]
                for clip in report['clips']:
                    if clip['name'] not in selected:continue
                    directory=staging/clip['name'];directory.mkdir()
                    manifest=export_clip(directory,rig,rig_path,samples[clip['name']],report,clip)
                    manifest['sources']=report['sources'];(directory/'recording-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
                    generated.append((directory,manifest))
                combine_clips(staging/'all',generated)
                out.mkdir(parents=True,exist_ok=True)
                for directory in staging.iterdir():
                    dest=out/directory.name;dest.mkdir(exist_ok=True)
                    for f in directory.iterdir():f.replace(dest/f.name)
            report['export_dir']=str(out)
        report_path.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.NamedTemporaryFile('w',dir=report_path.parent,delete=False) as s:
            temp=Path(s.name);json.dump(report,s,indent=2);s.write('\n')
        temp.replace(report_path)
        print(f'Validated {report["samples"]} samples / {len(report["clips"])} clips; motion detected={report["runtime_motion_detected"]}; missing bones={len(report["rig_mapping"]["missing_bones"])}')
    except (OSError,ValueError,KeyError,TypeError,IndexError,OverflowError,struct.error) as e:p.exit(1,f'Bake analysis: {e}\n')


if __name__=='__main__':main()
