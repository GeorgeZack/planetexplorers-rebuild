#!/usr/bin/env python3
"""Recover hare PECapsuleTrigger geometry bound to an existing PEAC skeleton.
Requires the optional UnityPy1.25.4 environment and export_reference_actor.py output.
Writes hare.pehb and hare-hitboxes.json only; never modifies source or actor files.
PEHB1: <4sIIIQII header (magic,version,count,joints,skeletonhash,reserved0,0),
then104B records: name32,kind/group/joint/form/material u32,damageScale f32,
12f32 bind points (endpointA,B,radiusOrigin,radiusXPoint). jointUINT_MAX disables.
"""
from read_reference_adventure import source_label
import argparse
from pe_actor_limits import PEAC_MAX_CLIPS
import hashlib
import importlib.metadata
import json
import math
from pathlib import Path
import struct
import tempfile

import export_reference_actor as actor
from export_reference_assets import pointer, multiply, transform_point

MAX_DISABLED = 0xffffffff
HEADER = struct.Struct('<4sIIIQII')
RECORD = struct.Struct('<32s5If12f')


def skeleton_data(data):
    if len(data) < 24 or len(data) > 256*1024*1024:
        raise ValueError('Missing/oversized PEAC')
    magic, version, nv, ni, nj, nc = struct.unpack_from('<4s5I', data)
    if magic != b'PEAC' or version not in (1,2) or not 1 <= nv <= 1000000 or not 1 <= ni <= 3000000 or ni % 3 or not 1 <= nj <= 256 or not 1 <= nc <= PEAC_MAX_CLIPS:
        raise ValueError('Invalid PEAC header')
    start = 24+nv*56+ni*4
    if start+nj*48 > len(data):
        raise ValueError('Truncated PEAC inverse binds')
    raw = data[start:start+nj*48]
    matrices = []
    for offset in range(0,len(raw),48):
        values = struct.unpack_from('<12f',raw,offset)
        actor.finite(values)
        matrix = [list(values[i:i+4]) for i in range(0,12,4)]+[[0.,0.,0.,1.]]
        actor.inverse_affine(matrix)
        matrices.append(matrix)
    fingerprint = 14695981039346656037
    for byte in data[8:20]+data[24:start+nj*48]:
        fingerprint = ((fingerprint ^ byte)*1099511628211)&((1<<64)-1)
    return nj, fingerprint, matrices


def local_points(capsule):
    radius,height = capsule['radius'],capsule['heigh']
    offset = [capsule['offset'][axis] for axis in 'xyz']
    actor.finite([radius,height,*offset])
    axis = capsule['axis']
    if not isinstance(axis,int) or not 0 <= axis < 6 or not 0 < radius <= 100 or not 0 < height <= 100:
        raise ValueError('Invalid source capsule')
    # ResetInfo uses these even when authored height<2r; do not silently clamp.
    direction = [0.,0.,0.];direction[axis % 3] = 1. if axis < 3 else -1.
    return [[offset[k]+radius*direction[k] for k in range(3)],
            [offset[k]+(height-radius)*direction[k] for k in range(3)],
            [0.,0.,0.],[radius,0.,0.]]


def bind_points(capsule, inverse_bind):
    to_mesh = actor.inverse_affine(inverse_bind)
    return [transform_point(to_mesh,point) for point in local_points(capsule)]


def recover(bundle, actor_directory, output):
    import UnityPy
    if not bundle.is_file() or bundle.stat().st_size > 64*1024*1024:
        raise ValueError('Missing/oversized hare bundle')
    source_hash = hashlib.sha256(bundle.read_bytes()).hexdigest()
    manifest_path = actor_directory/'hare-manifest.json'
    if not manifest_path.is_file() or manifest_path.stat().st_size > 8*1024*1024:
        raise ValueError('Export hare actor first')
    manifest = json.loads(manifest_path.read_text())
    if manifest['source_sha256'] != source_hash or manifest['renderer_path_id'] != actor.RENDERER_ID:
        raise ValueError('Actor/source provenance mismatch')
    peac = actor_directory/'hare.peac'
    if not peac.is_file() or peac.stat().st_size > 256*1024*1024:
        raise ValueError('Missing/oversized hare PEAC')
    nj, fingerprint, inverse_binds = skeleton_data(peac.read_bytes())
    environment = UnityPy.load(str(bundle));objects = {o.path_id:o for o in environment.objects}
    renderer = objects[actor.RENDERER_ID].read_typetree()
    bones = [pointer(p) for p in renderer['m_Bones']]
    if bones != manifest['joint_transform_path_ids'] or len(bones) != nj or len(set(bones)) != nj:
        raise ValueError('PEAC joint order does not match hare renderer')
    mesh = objects[pointer(renderer['m_Mesh'])].read_typetree()
    source_binds = [[[b[f'e{r}{c}'] for c in range(4)] for r in range(4)] for b in mesh['m_BindPose']]
    if len(source_binds) != nj or any(struct.pack('<12f',*actor.matrix_values(a)) != struct.pack('<12f',*actor.matrix_values(b)) for a,b in zip(source_binds,inverse_binds)):
        raise ValueError('PEAC inverse binds differ from source')
    transforms = {pid:o.read_typetree() for pid,o in objects.items() if o.type.name == 'Transform'}
    nodes = manifest['hierarchy'];local = {}
    for node in nodes:
        t = transforms[node['path_id']]
        if node['parent'] and pointer(t['m_Father']) != node['parent']:
            raise ValueError('Actor hierarchy parent mismatch')
        local[node['path_id']] = actor.local_matrix([t['m_LocalPosition'][k] for k in 'xyz'],[t['m_LocalRotation'][k] for k in 'xyzw'],[t['m_LocalScale'][k] for k in 'xyz'])
    worlds = actor.hierarchy_world(nodes,local);root_inverse = actor.inverse_affine(worlds[actor.MODEL_ROOT_ID])
    palettes = actor.palette_for(nodes,local,bones,inverse_binds,actor.MODEL_ROOT_ID)
    bone_indices = {pid:i for i,pid in enumerate(bones)};entries = [];unexported = [];max_error = 0.
    for pid,obj in sorted(objects.items()):
        if obj.type.name != 'MonoBehaviour':continue
        tree = obj.read_typetree();script = objects.get(pointer(tree['m_Script']))
        if not script:continue
        kind = script.read_typetree().get('m_ClassName')
        if kind not in ('PEDefenceTrigger','PEAttackTrigger'):continue
        owner = objects[pointer(tree['m_GameObject'])].read_typetree()['m_Name']
        if kind == 'PEDefenceTrigger':parts,category,group = tree['defenceParts'],0,0
        else:
            groups = {'Bip01 R Forearm':1,'Bip01 L Forearm':2,'Bip01':4}
            if owner not in groups:raise ValueError('Unexpected attack trigger owner')
            if owner == 'Bip01':
                unexported.append(dict(source_component=pid,owner=owner,parts=tree['attackParts'],reason='Pounce body transform is not a PEAC skin joint; requires a separate exported palette.'))
                continue
            parts,category,group = tree['attackParts'],1,groups[owner]
        for index,part in enumerate(parts):
            capsule = part['capsule'];bone = pointer(capsule['trans'])
            local_points(capsule)  # Validate even disabled records.
            name = part['name'] if category == 0 else owner+':'+str(index)
            if len(name.encode('ascii')) >= 32:raise ValueError('Hitbox name too long')
            enabled = bone != 0
            if enabled and bone not in bone_indices:raise ValueError(f'Capsule bone outside PEAC skeleton: {name} {bone}')
            joint = bone_indices[bone] if enabled else MAX_DISABLED
            points = bind_points(capsule,inverse_binds[joint]) if enabled else [[0.]*3 for _ in range(4)]
            if enabled:
                direct = multiply(root_inverse,worlds[bone])
                for original,recovered in zip(local_points(capsule),points):
                    expected = transform_point(direct,original)
                    actual = transform_point(palettes[joint],recovered)
                    max_error = max(max_error,math.dist(expected,actual))
                    if math.dist(expected,actual)>1e-5:raise ValueError('Bind-space point/hierarchy disagreement')
            entry = dict(name=name,kind=category,group=group,joint=joint,enabled=enabled,
                         form=part.get('defenceType',part.get('attackForm',0)),material=part.get('defenceMaterial',0),
                         damage_scale=(part.get('damageScale',1.) or 1.),bind_points=points,
                         source_component=pid,source_owner=owner,source_part=index,source_capsule=capsule,
                         source_layer_mask=tree.get('attackLayer'),source_joint_transform=bone)
            entries.append(entry)
    # Preserve source defence list order, then stable trigger/part order.
    entries.sort(key=lambda e:(e['kind'],e['source_component'],e['source_part']))
    if sum(e['kind']==0 for e in entries)!=15 or sum(e['kind']==1 for e in entries)!=4:
        raise ValueError('Unexpected hare capsule counts')
    binary = HEADER.pack(b'PEHB',1,len(entries),nj,fingerprint,0,0)
    for e in entries:
        binary += RECORD.pack(e['name'].encode(),e['kind'],e['group'],e['joint'],e['form'],e['material'],e['damage_scale'],*(x for p in e['bind_points'] for x in p))
    (output/'hare.pehb').write_bytes(binary)
    repo = Path(__file__).resolve().parents[2]
    sources = [repo/'Assets/Scripts/Attack'/name for name in ['PECapsuleTrigger.cs','PEDefenceTrigger.cs','PEAttackTrigger.cs']]
    report = dict(format='PEHB1',source=str(bundle),source_sha256=source_hash,skeleton_hash=f'{fingerprint:016x}',
                  fingerprint_rule='FNV1a64 of PEAC bytes8..19 and24..end-of-inversebinds (counts, vertices/weights/joints, indices, binds; excludes clips)',
                  joints=nj,records=len(entries),defence_records=15,enabled_defence=sum(e['kind']==0 and e['enabled'] for e in entries),
                  attack_records=4,entries=entries,unexported_triggers=unexported,hierarchy_reconstruction_max_error=max_error,
                  sources=[dict(path=source_label(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sources],
                  endpoint_rule='offset + radius*axis; offset + (height-radius)*axis',
                  radius_rule='distance(palette*inverse(bind)*zero,palette*inverse(bind)*(radius,0,0)); outer uniform actor scale applies',
                  limitations=['Three source defence parts have null transforms and remain disabled.',
                               'Pounce body trigger references a non-skin transform and is recorded in the manifest but not exported as a posed record.',
                               'Instantaneous geometry only; source capsule swept-history sampling is not recreated.',
                               'Clip activation, hit eligibility, collision layers, per-skill deduplication and damage are separate.',
                               'Between-frame palette interpolation follows native actor skinning, not Unity quaternion interpolation.'],
                  files={'hare.pehb':dict(bytes=len(binary),sha256=hashlib.sha256(binary).hexdigest())})
    (output/'hare-hitboxes.json').write_text(json.dumps(report,sort_keys=True,indent=2)+'\n')
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    parser.add_argument('--actor-dir',type=Path,default=actor.DEFAULT_OUTPUT)
    parser.add_argument('--output',type=Path,default=actor.DEFAULT_OUTPUT)
    args=parser.parse_args(argv)
    try:
        if importlib.metadata.version('UnityPy')!='1.25.4':raise ValueError('Expected UnityPy1.25.4')
        game=args.game_dir.expanduser().resolve();output=args.output.expanduser().resolve()
        if output==game or game in output.parents:raise ValueError('Output must be outside original installation')
        output.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.pe-hitbox-export-',dir=output.parent) as temporary:
            staging=Path(temporary);report=recover(game/actor.BUNDLE,args.actor_dir.expanduser().resolve(),staging)
            output.mkdir(exist_ok=True)
            for p in sorted(staging.iterdir()):p.replace(output/p.name)
        print(f'Exported hare hitboxes: {report["enabled_defence"]}/15 defence enabled, 4 forearm attack parts, skeleton {report["skeleton_hash"]}')
    except (ImportError,importlib.metadata.PackageNotFoundError):parser.exit(1,'Use the isolated UnityPy1.25.4 environment.\n')
    except (ValueError,KeyError,TypeError,OSError,struct.error) as error:parser.exit(1,f'Hitbox export failed: {error}\n')

if __name__=='__main__':main()
