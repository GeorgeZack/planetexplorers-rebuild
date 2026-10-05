#!/usr/bin/env python3
"""Export knife33 rigidly attached to a validated isolated bake's waist or hand mount.

Standard library only. Requires a closed bake, recovered player rig, knife
visual/collision manifests and a matching multipart body scene. Writes a fresh
knife-only PEAS scene with the body's exact ordered PEAC2 clip clocks. No Unity
launch, hand transition, attack activation or damage is inferred. Drawn mode
exports source capsule geometry, which still requires host event/permission gates.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import tempfile

import analyze_reference_bake as bake
import analyze_reference_pose as pose
from export_reference_assets import IDENTITY, trs, transform_point
from export_reference_hitboxes import local_points, skeleton_data, HEADER, RECORD
from export_reference_knife_collision import obj_parts


def read_json(path, limit=64*1024*1024):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > limit:
        raise ValueError('Missing/oversized/symlink JSON input')
    value = pose.decode(path.read_text())
    if not isinstance(value, dict): raise ValueError('JSON object required')
    return value


def source_file(directory, manifest, name, suffix):
    if not pose.safe_basename(name, suffix): raise ValueError('Unsafe knife source basename')
    path = directory/name
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 64*1024*1024:
        raise ValueError('Missing/oversized knife source')
    data = path.read_bytes(); expected = manifest['files'][name]
    if len(data) != expected['bytes'] or hashlib.sha256(data).hexdigest() != expected['sha256']:
        raise ValueError('Knife visual source hash mismatch')
    return data


def knife_rig(player, collision, visual, mesh, attachment="stowed"):
    if attachment not in ("stowed", "drawn"): raise ValueError("Unknown knife attachment")
    mount_name = "mountMain" if attachment == "drawn" else "mountWaist"
    field = "m_PutOnBone" if attachment == "drawn" else "m_PutOffBone"
    source = collision['source']; definition = visual['assets']['training-knife']
    placement = definition['placement_provenance']
    if source['item_id'] != 33 or placement['item_id'] != 33:
        raise ValueError('Expected source knife33')
    if source['attachment'][field] != mount_name or placement['attachment'][field] != mount_name:
        raise ValueError('Knife source mount changed')
    root = placement['root_override']
    pose.close_matrix(trs(root['m_LocalPosition'], root['m_LocalRotation'], root['m_LocalScale']), IDENTITY, 'knife prefab root identity')
    sex = player['sex']; chains = []
    for prefab in source['source_mounts']:
        for chain in prefab['mount_chains']:
            if chain[0]['name'] == sex.capitalize()+'Model' and chain[-1]['name'] == mount_name:
                names = [node['name'] for node in chain]
                if names.count('Model') != 1: raise ValueError('Ambiguous model root')
                chains.append((chain, '/'.join(names[names.index('Model')+1:])))
    if len(chains) != 1: raise ValueError('Unique source model mount required')
    chain, mount_path = chains[0]
    materials = {m['obj_symbol']: m for m in definition['materials']}
    rig = dict(schema=1, sex=sex, avatar=copy.deepcopy(player['avatar']), parts=[], materials=[])
    for symbol, (rows, indices) in obj_parts(mesh.decode('utf-8')).items():
        material = copy.deepcopy(materials[symbol]); material['asset'] = 'source-knife'
        rig['materials'].append(material)
        skin = {**{f'weight[{i}]': float(i == 0) for i in range(4)}, **{f'boneIndex[{i}]': 0 for i in range(4)}}
        rig['parts'].append(dict(resource='Prefab/Item/Equip/Weapon/Sword/knife', bone_names=[mount_name],
            inverse_binds=[IDENTITY], positions=[r[0] for r in rows], normals=[r[1] for r in rows], uv=[r[2] for r in rows],
            skin=[dict(skin) for _ in rows], triangles=[[indices[i:i+3] for i in range(0, len(indices), 3)]],
            materials=[['source-knife', material['path_id']]]))
    return rig, mount_path, chain[-1]


def drawn_hitbox(actor, source, placement):
    """One source capsule in rigid knife-root bind space; no active flag."""
    capsule = source['capsule']; transform = source['source_transform_fields']
    if capsule['trans']['m_FileID'] or capsule['trans']['m_PathID'] != source['source_transform']:
        raise ValueError('Capsule source transform mismatch')
    if transform not in placement['prefab_transforms']:
        raise ValueError('Blade transform differs from source visual prefab')
    root = placement['root_override']
    if transform['m_Father']['m_FileID'] or not any(p['m_FileID'] == 0 and p['m_PathID'] == source['source_transform'] for p in root['m_Children']):
        raise ValueError('Blade must be a direct child of source knife root')
    if source['attack_form'] != 0 or source['attack_layer_mask'] != 34154496 or capsule['axis'] != 2:
        raise ValueError('Unexpected source knife trigger metadata')
    if abs(capsule['radius']-.3)>1e-6 or abs(capsule['heigh']-1.5)>1e-6 or any(abs(capsule['offset'][k]-v)>1e-6 for k,v in zip('xyz',(0,0,-.1))):
        raise ValueError('Unexpected source knife capsule geometry')
    points = local_points(capsule)
    if points != source['local_points']: raise ValueError('Capsule cached local points differ')
    local = trs(transform['m_LocalPosition'],transform['m_LocalRotation'],transform['m_LocalScale'])
    points = [transform_point(local,p) for p in points]
    joints, fingerprint, _ = skeleton_data(actor)
    if joints != 1: raise ValueError('Drawn knife must have one rigid joint')
    binary = HEADER.pack(b'PEHB',1,1,1,fingerprint,0,0)
    binary += RECORD.pack(b'SingleHandSword',1,1,0,source['attack_form'],0,1.,*(v for p in points for v in p))
    return binary,dict(skeleton_hash=f'{fingerprint:016x}',kind='attack',group='right',joint=0,bind_points=points,
        source_attack_layer_mask=source['attack_layer_mask'],attachment='mountMain',activation_observed=False,
        note='Geometry only. Host must authorize drawn attachment, action window, skill guide and contact permissions; material0/scale1 are unused attack placeholders.')


def clip_headers(data):
    if len(data) < 24: raise ValueError('Truncated body actor')
    magic, version, nv, ni, nj, count = struct.unpack_from('<4s5I', data)
    if magic != b'PEAC' or version != 2 or not 1 <= count <= 32 or not 1 <= nj <= 256 or not 1 <= nv <= 1000000 or not 1 <= ni <= 3000000:
        raise ValueError('Expected bounded PEAC2 body actor')
    offset = 24+nv*56+ni*4+nj*48; headers = []
    for _ in range(count):
        if offset+48 > len(data): raise ValueError('Truncated body clip')
        raw = data[offset:offset+48]; frames = struct.unpack_from('<I', raw, 36)[0]
        if not 2 <= frames <= 20000: raise ValueError('Invalid body frame count')
        headers.append(raw); offset += 48+frames*nj*48
    if offset != len(data): raise ValueError('Truncated/trailing body actor')
    return headers


def check_body(path, expected):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 65536: raise ValueError('Invalid body scene')
    lines = path.read_text('ascii').splitlines()
    if not lines or lines[0] != 'PEAS1' or not 1 <= len(lines)-1 <= 64: raise ValueError('Invalid body scene records')
    actors = set(); total = 0; sources = []
    for line in lines[1:]:
        fields = line.split(' ')
        if len(fields) != 2 or not pose.safe_basename(fields[0], '.peac') or not pose.safe_basename(fields[1], '.png'):
            raise ValueError('Invalid body scene basename')
        if fields[0] in actors: raise ValueError('Duplicate body actor')
        actors.add(fields[0]); actor = path.parent/fields[0]
        if actor.is_symlink() or not actor.is_file() or actor.stat().st_size > pose.MAX_BYTES: raise ValueError('Invalid body actor file')
        data = actor.read_bytes(); total += len(data)
        if total > 512*1024*1024: raise ValueError('Body byte bound exceeded')
        if clip_headers(data) != expected: raise ValueError('Body and stowed knife ordered clip clocks differ')
        sources.append(dict(path=str(actor), sha256=hashlib.sha256(data).hexdigest()))
    return sources


def export(bake_path, player_path, collision_path, assets, body, output, attachment="stowed"):
    paths = [p.resolve() for p in (bake_path, player_path, collision_path, assets/'training-knife-manifest.json', body)]
    output = output.resolve()
    if output.exists() or any(output == p or output in p.parents for p in paths):
        raise ValueError('Output must be a fresh directory separate from inputs')
    player = read_json(player_path); collision = read_json(collision_path); visual = read_json(paths[3], 8*1024*1024)
    mesh = source_file(assets, visual, 'training-knife.obj', '.obj')
    rig, mount_path, mount = knife_rig(player, collision, visual, mesh, attachment)
    report, samples = bake.analyze(bake_path, rig, retain=True)
    mount_name = 'mountMain' if attachment == 'drawn' else 'mountWaist'
    mapping = report['rig_mapping']['bones'].get(mount_name)
    if not mapping or mapping['relative_path'] != mount_path: raise ValueError('Bake/source mount path differs')
    node = report['skeleton']['nodes'][mapping['index']]
    pose.close_matrix(pose.matrix(node['initialLocal'], 'baked mount local'), trs(mount['position'], mount['rotation'], mount['scale']), 'source mount local')
    textures = {m['diffuse_file']: source_file(assets, visual, m['diffuse_file'], '.png') for m in rig['materials']}
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.baked-knife-', dir=output.parent) as temporary:
        staging = Path(temporary); source_dir = staging/'sources'; source_dir.mkdir()
        for name, data in textures.items(): (source_dir/name).write_bytes(data)
        generated = []
        for clip in report['clips']:
            directory = staging/clip['name']; directory.mkdir()
            result = bake.export_clip(directory, rig, source_dir/'knife-rig.json', samples[clip['name']], report, clip)
            generated.append((directory, result))
        combined = staging/'all'; result = bake.combine_clips(combined, generated)
        expected = clip_headers((combined/result['parts'][0]['file']).read_bytes())
        body_sources = check_body(body, expected)
        for part in result['parts']:
            old = part['file']; part['file'] = old.replace('player-part-', f'knife-{attachment}-part-')
            (combined/old).rename(combined/part['file'])
        (combined/'recording.peas').write_text('PEAS1\n'+''.join(p['file']+' '+p['texture']+'\n' for p in result['parts']), encoding='ascii')
        result.update(attachment=mount_name, attachment_mode=attachment, source_mount_path=mount_path, source_mount=mount,
            coordinate_space=f'baked Animator-model-local {mount_name} world matrix * prefab-local knife OBJ; rigid identity bind',
            sources=[dict(path=str(p), sha256=pose.sha256(p)) for p in paths], body_actors=body_sources,
            collision_authorized=False, activation_observed=False,
            limitations=['Fixed selected mount only; no animated draw/put-away transition.',
                         'Pure baked source clips exclude controller transitions and procedural IK.',
                         'Source diffuse textures retained; original material shaders are not reproduced.'])
        if attachment == 'drawn':
            first = result['parts'][0]['file']
            binary, geometry = drawn_hitbox((combined/first).read_bytes(), collision['source'], visual['assets']['training-knife']['placement_provenance'])
            geometry['actor'] = first; geometry['file'] = 'knife-drawn.pehb'
            (combined/geometry['file']).write_bytes(binary); result['hitbox'] = geometry
        (combined/'recording-manifest.json').write_text(json.dumps(result, indent=2)+'\n')
        combined.rename(output)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--attachment', choices=('stowed','drawn'), default='stowed')
    parser.add_argument('bake', type=Path); parser.add_argument('--rig', type=Path, required=True)
    parser.add_argument('--knife-collision', type=Path, required=True); parser.add_argument('--visual-assets', type=Path, required=True)
    parser.add_argument('--body-scene', type=Path, required=True); parser.add_argument('--output', type=Path, required=True)
    a = parser.parse_args()
    try:
        result = export(a.bake, a.rig, a.knife_collision, a.visual_assets, a.body_scene, a.output, a.attachment)
        print(f'Exported {len(result["parts"])} {a.attachment} knife parts / {len(result["clips"])} matching clips: {a.output}/recording.peas')
    except (OSError, ValueError, KeyError, TypeError, IndexError, struct.error, OverflowError) as error:
        parser.exit(1, f'Baked knife export failed: {error}\n')
if __name__ == '__main__': main()
