#!/usr/bin/env python3
"""Recover the original hare mesh and sampled idle/run/melee/walk skin palettes without Unity.

Optional dependency: UnityPy==1.25.4. Outputs hare.peac, hare-diffuse.png and
hare-manifest.json only; original bundles and other recovered assets stay intact.
PEAC v1 is little endian: 24-byte header (magic/version/vertex/index/joint/clip
counts), 56-byte vertices (position3/normal3/uv2/weights4 f32 + joints4 u16),
u32 indices, joint inverse-bind affine matrices (12 f32, row-major), then clips:
name32/fpsf32/frame_countu32/durationf32/loopu32 followed by frame-major joint
skin palettes (12 f32 each). Palettes ALREADY include inverse binds. Preserve
Unity XYZ. Playback loops on source duration, not dense frame count / fps.
"""
import argparse
import hashlib
import importlib.metadata
import json
import math
from pathlib import Path
import struct
import tempfile
import zlib

from export_reference_assets import IDENTITY, multiply, pointer, property_map, trs, transform_point

DEFAULT_OUTPUT = Path(__file__).resolve().parents[1] / 'build/reference-assets'
BUNDLE = 'AssetBundles/Monster/m_lepus_hare.unity3d'
RENDERER_ID = 6629980832213904343
MODEL_ROOT_ID = -8365773678112902782
MELEE = [
    ('attack0', 'attack0_handhitleftfront', 30100003),
    ('attack1', 'attack1_handhitrightfront', 30100004),
    ('attack2', 'attack2_handhitfront', 30100008),
    ('attack3', 'attack3_normalattack0', 30100046),
    ('attack4', 'attack4_normalattack1', 30100047),
]
CLIPS = [('idle', 'normal_leisureidle'), ('run', 'move_run')] + [(name, clip) for name, clip, _ in MELEE] + [('walk', 'move_walk')]
CONTROLLER_ID = 4131577587453613724


def finite(values):
    if not all(math.isfinite(value) for value in values):
        raise ValueError('Non-finite actor data')


def inverse_affine(matrix):
    a, b, c = matrix[0][:3]; d, e, f = matrix[1][:3]; g, h, i = matrix[2][:3]
    det = a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g)
    if not math.isfinite(det) or abs(det) < 1e-12:
        raise ValueError('Singular actor transform')
    result = [[(e*i-f*h)/det, (c*h-b*i)/det, (b*f-c*e)/det, 0.],
              [(f*g-d*i)/det, (a*i-c*g)/det, (c*d-a*f)/det, 0.],
              [(d*h-e*g)/det, (b*g-a*h)/det, (a*e-b*d)/det, 0.],
              [0., 0., 0., 1.]]
    for row in range(3):
        result[row][3] = -sum(result[row][column]*matrix[column][3] for column in range(3))
    return result


def matrix_values(matrix):
    values = [value for row in matrix[:3] for value in row]
    finite(values)
    return values


def local_matrix(position, rotation, scale):
    finite([*position, *rotation, *scale])
    magnitude = math.sqrt(sum(value*value for value in rotation))
    if abs(magnitude-1) > .001:
        raise ValueError('Animation quaternion is not unit length')
    return trs(dict(zip('xyz', position)), dict(zip('xyzw', [v/magnitude for v in rotation])),
               dict(zip('xyz', scale)))


def binding_channels(bindings, paths, dense_count, constant_count):
    """Unity generic transform binding order partitions dense then constant scalars."""
    result = []
    offset = 0
    seen = set()
    for binding in bindings:
        attribute = binding['attribute']
        if (binding['classID'] != 4 or binding['customType'] != 0 or binding['isPPtrCurve'] or
                pointer(binding['script']) != 0 or attribute not in (1, 2, 3)):
            raise ValueError('Unsupported non-TRS animation binding')
        if binding['path'] not in paths:
            raise ValueError('Unresolved animation transform path hash')
        node = paths[binding['path']]
        if (node, attribute) in seen:
            raise ValueError('Duplicate animation transform binding')
        seen.add((node, attribute))
        count = 4 if attribute == 2 else 3
        if offset < dense_count < offset+count:
            raise ValueError('Transform binding straddles dense/constant channel boundary')
        result.append((node, attribute, offset, count))
        offset += count
    if offset != dense_count+constant_count:
        raise ValueError('Animation channel count does not match bindings')
    return result


def hierarchy_world(nodes, local):
    """Nodes are parent-first, making cycles and missing parents explicit errors."""
    worlds = {}
    for node in nodes:
        parent = node['parent']
        matrix = local[node['path_id']]
        if parent:
            if parent not in worlds:
                raise ValueError('Actor hierarchy is not parent-first')
            matrix = multiply(worlds[parent], matrix)
        worlds[node['path_id']] = matrix
    return worlds


def palette_for(nodes, local, bones, inverse_binds, model_root):
    worlds = hierarchy_world(nodes, local)
    root_inverse = inverse_affine(worlds[model_root])
    return [multiply(multiply(root_inverse, worlds[bone]), bind)
            for bone, bind in zip(bones, inverse_binds)]


def skin_position(position, weights, joints, palette):
    result = [0., 0., 0.]
    for weight, joint in zip(weights, joints):
        transformed = transform_point(palette[joint], position)
        for axis in range(3):
            result[axis] += weight*transformed[axis]
    return result


def melee_provenance(objects):
    """Resolve actual controller states/behaviours and DB skills; no inferred hit times."""
    import sqlite3
    root = Path(__file__).resolve().parents[2]
    database = root/'Assets/Resources/DataBase/localData.bytes'
    controller = objects[CONTROLLER_ID].read_typetree()
    strings = dict(controller['m_TOS'])
    states = {strings[state['data']['m_FullPathID']]: state['data']
              for machine in controller['m_Controller']['m_StateMachineArray']
              for state in machine['data']['m_StateConstantArray']}
    description = controller['m_StateMachineBehaviourVectorDescription']
    ranges = {key['m_StateID']: value for key, value in description['m_StateMachineBehaviourRanges']
              if key['m_LayerIndex'] == 0}
    reports = {}
    with sqlite3.connect(database.as_uri()+'?mode=ro', uri=True) as connection:
        connection.row_factory = sqlite3.Row
        for name, source_name, skill_id in MELEE:
            state_name = 'Base Layer.Attack.'+name
            state = states[state_name]
            leaves = [node['data'] for blend in state['m_BlendTreeConstantArray']
                      for node in blend['data']['m_NodeArray'] if not node['data']['m_ChildIndices']]
            if len(leaves) != 1 or leaves[0]['m_ClipID'] >= len(controller['m_AnimationClips']):
                raise ValueError('Expected single direct motion in hare attack state')
            clip_id = pointer(controller['m_AnimationClips'][leaves[0]['m_ClipID']])
            clip = objects[clip_id].read_typetree()
            if clip['m_Name'] != source_name or state['m_Loop'] or state['m_Speed'] <= 0:
                raise ValueError('Hare attack controller mapping changed')
            skill = connection.execute('SELECT * FROM skMain WHERE CAST(_id AS INTEGER)=?', (skill_id,)).fetchone()
            effect = connection.execute('SELECT * FROM skEffect WHERE CAST(_id AS INTEGER)=?',
                                        (int(skill['_effPrepare']),)).fetchone()
            if effect['_anms'] != name:
                raise ValueError('Hare skill no longer maps to expected attack state')
            span = ranges[state['m_FullPathID']]
            selected = description['m_StateMachineBehaviourIndices'][span['m_StartIndex']:span['m_StartIndex']+span['m_Count']]
            behaviours = []
            windows = []
            duration = clip['m_MuscleClip']['m_StopTime']-clip['m_MuscleClip']['m_StartTime']
            for index in selected:
                behaviour_id = pointer(controller['m_StateMachineBehaviours'][index])
                tree = objects[behaviour_id].read_typetree()
                script = objects[pointer(tree['m_Script'])].read_typetree()
                kind = script['m_ClassName']
                behaviours.append({'path_id': behaviour_id, 'class': kind, 'fields': tree})
                if kind not in ('PEAnimatorAttackTrigger', 'PEAnimatorAttackTriggerEX'):
                    continue
                attacks = tree['m_Attacks'] if kind.endswith('EX') else [tree]
                for attack in attacks:
                    start, end = attack['startTime'], attack['endTime']
                    if not 0 <= start < end <= 1 or not attack['bones']:
                        raise ValueError('Invalid hare melee trigger window')
                    windows.append({'behaviour_path_id': behaviour_id, 'class': kind,
                                    'start_normalized': start, 'end_normalized': end,
                                    'start_seconds_at_state_speed': start*duration/state['m_Speed'],
                                    'end_seconds_at_state_speed': end*duration/state['m_Speed'],
                                    'start_inclusive': True, 'end_inclusive': not kind.endswith('EX'),
                                    'bones': attack['bones'],
                                    'end_event': 'MonsterEndAttack' if kind.endswith('EX') else None})
            if not windows:
                raise ValueError('Melee state has no recovered collision windows')
            reports[name] = {'controller_path_id': CONTROLLER_ID, 'state': state_name,
                             'state_full_path_id': state['m_FullPathID'], 'clip_path_id': clip_id,
                             'state_speed': state['m_Speed'], 'skill': dict(skill), 'prepare_effect': dict(effect),
                             'clip_animation_events': clip['m_Events'], 'state_behaviours': behaviours,
                             'attack_windows': windows}
    paths = [database, root/'Assets/Scripts/Assist/AnimationStateInfo/PEAnimatorAttackTrigger.cs',
             root/'Assets/Scripts/Assist/AnimationStateInfo/PEAnimatorAttackTriggerEX.cs',
             root/'Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs']
    sources = [{'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()} for path in paths]
    return reports, {'sources': sources, 'controller_path_id': CONTROLLER_ID,
                     'missing_hurt_death_clips': True,
                     'death_evidence': 'BiologyViewCmpt.OnDeath calls ActivateRagdoll(null, false); no hurt/death clip or state is present in this hare controller.',
                     'controller_state_names': sorted(states)}


def recover(bundle, output):
    import UnityPy
    from UnityPy.helpers.MeshHelper import MeshHandler
    if not bundle.is_file() or bundle.stat().st_size > 64*1024*1024:
        raise ValueError('Missing or oversized hare source bundle')
    environment = UnityPy.load(str(bundle))
    objects = {obj.path_id: obj for obj in environment.objects}
    transforms = {pid: obj.read_typetree() for pid, obj in objects.items() if obj.type.name == 'Transform'}
    game_objects = {pid: obj.read_typetree() for pid, obj in objects.items() if obj.type.name == 'GameObject'}
    renderer = objects[RENDERER_ID].read_typetree()
    if objects[RENDERER_ID].type.name != 'SkinnedMeshRenderer' or not renderer['m_Enabled']:
        raise ValueError('Expected enabled hare model renderer')
    if game_objects[pointer(transforms[MODEL_ROOT_ID]['m_GameObject'])]['m_Name'] != 'lepus_hare_model':
        raise ValueError('Hare imported model root changed')
    owner = pointer(renderer['m_GameObject'])
    by_object = {pointer(tree['m_GameObject']): pid for pid, tree in transforms.items()}
    renderer_transform = by_object[owner]
    nodes = []
    paths = {}
    visited = set()

    def visit(pid, path, parent):
        if pid in visited or len(visited) >= 1024:
            raise ValueError('Cyclic or oversized actor hierarchy')
        visited.add(pid)
        tree = transforms[pid]
        owner = pointer(tree['m_GameObject'])
        if not game_objects[owner]['m_IsActive']:
            raise ValueError('Inactive transform in selected model hierarchy')
        crc = zlib.crc32(path.encode('utf-8'))
        if crc in paths:
            raise ValueError('Transform path hash collision')
        paths[crc] = pid
        nodes.append({'path_id': pid, 'parent': parent, 'path': path, 'crc32': crc,
                      'position': [tree['m_LocalPosition'][k] for k in 'xyz'],
                      'rotation': [tree['m_LocalRotation'][k] for k in 'xyzw'],
                      'scale': [tree['m_LocalScale'][k] for k in 'xyz']})
        for child in sorted(pointer(child) for child in tree['m_Children']):
            name = game_objects[pointer(transforms[child]['m_GameObject'])]['m_Name']
            visit(child, path+'/'+name if path else name, pid)

    # All exported poses are relative to this imported model root; outer game
    # placement is a native runtime concern, not a vertex transform to bake twice.
    visit(MODEL_ROOT_ID, '', 0)
    if renderer_transform not in {node['path_id'] for node in nodes}:
        raise ValueError('Renderer is outside selected model hierarchy')
    bones = [pointer(bone) for bone in renderer['m_Bones']]
    if len(bones) != len(set(bones)) or any(bone not in paths.values() for bone in bones):
        raise ValueError('Invalid skin bone references')
    mesh_id = pointer(renderer['m_Mesh'])
    mesh_data = objects[mesh_id].read()
    tree = objects[mesh_id].read_typetree()
    mesh = MeshHandler(mesh_data); mesh.process()
    positions, normals, uvs = mesh.m_Vertices, mesh.m_Normals, mesh.m_UV0
    skin = tree['m_Skin']
    if not (len(positions) == len(normals) == len(uvs) == len(skin)):
        raise ValueError('Mesh vertex channel counts disagree')
    inverse_binds = [[[bind[f'e{row}{column}'] for column in range(4)] for row in range(4)]
                     for bind in tree['m_BindPose']]
    if len(bones) != len(inverse_binds) or not 1 <= len(bones) <= 256:
        raise ValueError('Skin joint and inverse-bind counts disagree')
    rest_identity_error = 0.
    for bind in inverse_binds:
        finite(matrix_values(bind))
        if bind[3] != [0., 0., 0., 1.]:
            raise ValueError('Non-affine inverse bind')
        reconstructed = multiply(inverse_affine(bind), bind)
        rest_identity_error = max(rest_identity_error, max(abs(reconstructed[r][c]-IDENTITY[r][c])
                                                          for r in range(4) for c in range(4)))
    vertices = []
    for position, normal, uv, weights in zip(positions, normals, uvs, skin):
        values = [weights[f'weight[{i}]'] for i in range(4)]
        joints = [weights[f'boneIndex[{i}]'] for i in range(4)]
        finite([*position[:3], *normal[:3], *uv[:2], *values])
        if any(v < 0 or v > 1 for v in values) or abs(sum(values)-1) > .0001:
            raise ValueError('Invalid skin weight sum')
        if any(joint < 0 or joint >= len(bones) for joint in joints):
            raise ValueError('Out-of-range skin joint')
        vertices.append((position[:3], normal[:3], uv[:2], values, joints))
    submeshes = list(mesh.get_triangles())
    if len(submeshes) != 1 or len(renderer['m_Materials']) != 1:
        raise ValueError('Expected single-material hare mesh')
    indices = [index for triangle in submeshes[0] for index in triangle]
    if any(index < 0 or index >= len(vertices) for index in indices):
        raise ValueError('Out-of-range actor index')
    material_id = pointer(renderer['m_Materials'][0])
    material = objects[material_id].read_typetree()
    properties = material['m_SavedProperties']
    texture_properties = property_map(properties['m_TexEnvs'])['_MainTex']
    if texture_properties['m_Scale'] != {'x': 1., 'y': 1.} or texture_properties['m_Offset'] != {'x': 0., 'y': 0.}:
        raise ValueError('Unexpected diffuse UV transform')
    texture_id = pointer(texture_properties['m_Texture'])
    image = objects[texture_id].read().image
    image.save(output/'hare-diffuse.png')
    tint = property_map(properties['m_Colors']).get('_Color', dict(r=1., g=1., b=1., a=1.))
    original = {node['path_id']: local_matrix(node['position'], node['rotation'], node['scale']) for node in nodes}
    authored_palette = palette_for(nodes, original, bones, inverse_binds, MODEL_ROOT_ID)
    authored_bounds = bounds(skin_position(v[0], v[3], v[4], authored_palette) for v in vertices)
    clip_objects = {obj.read_typetree()['m_Name']: obj for obj in objects.values() if obj.type.name == 'AnimationClip'}
    combat, combat_evidence = melee_provenance(objects)
    exported = []
    reports = []
    for name, source_name in CLIPS:
        obj = clip_objects[source_name]
        clip = obj.read_typetree(); muscle = clip['m_MuscleClip']; data = muscle['m_Clip']['data']
        dense = data['m_DenseClip']; constant = data['m_ConstantClip']['data']
        if data['m_StreamedClip']['curveCount'] != 0 or dense['m_BeginTime'] != 0 or muscle['m_StartTime'] != 0:
            raise ValueError('Unsupported streamed curves or nonzero animation start')
        channels, frames, fps = dense['m_CurveCount'], dense['m_FrameCount'], dense['m_SampleRate']
        duration = muscle['m_StopTime']-muscle['m_StartTime']
        if not 1 <= frames <= 20000 or not 1 <= fps <= 240 or not 0 < duration <= (frames-1)/fps+.00001:
            raise ValueError('Invalid dense animation timing')
        samples = dense['m_SampleArray']
        if len(samples) != frames*channels:
            raise ValueError('Invalid dense sample count')
        finite([*samples, *constant])
        bindings = binding_channels(clip['m_ClipBindingConstant']['genericBindings'], paths, channels, len(constant))
        palettes = []
        animated_bounds = None
        first_positions = None
        max_vertex_displacement = 0.
        max_loop_gap = 0.
        # The last dense frame can be a guard sample beyond source duration.
        loop_frame = min(frames-1, round(duration*fps))
        for frame in range(frames):
            scalars = samples[frame*channels:(frame+1)*channels]+constant
            components = {node['path_id']: {1: node['position'], 2: node['rotation'], 3: node['scale']} for node in nodes}
            for node, attribute, offset, count in bindings:
                components[node][attribute] = scalars[offset:offset+count]
            local = {node: local_matrix(values[1], values[2], values[3]) for node, values in components.items()}
            palette = palette_for(nodes, local, bones, inverse_binds, MODEL_ROOT_ID)
            for matrix in palette:
                inverse_affine(matrix)  # Normal transformation must remain invertible.
            palettes.append(palette)
            posed = [skin_position(v[0], v[3], v[4], palette) for v in vertices]
            animated_bounds = union_bounds(animated_bounds, bounds(posed))
            if first_positions is None:
                first_positions = posed
            else:
                displacement = max(math.dist(a, b) for a, b in zip(first_positions, posed))
                max_vertex_displacement = max(max_vertex_displacement, displacement)
                if frame == loop_frame:
                    max_loop_gap = displacement
        if max_vertex_displacement < .0001:
            raise ValueError('Recovered animation has no vertex motion')
        exported.append((name, fps, frames, duration, int(muscle['m_LoopTime']), palettes))
        reports.append({'name': name, 'source_name': source_name, 'path_id': obj.path_id, 'fps': fps,
                        'frame_count': frames, 'duration': duration, 'loop': muscle['m_LoopTime'],
                        'dense_channels': channels, 'constant_channels': len(constant),
                        'binding_count': len(bindings), 'bounds': animated_bounds,
                        'animation_events': clip['m_Events'], 'combat': combat.get(name),
                        'max_vertex_displacement_from_frame0': max_vertex_displacement,
                        'loop_endpoint_max_vertex_gap': max_loop_gap,
                        'source_root_motion_flags': {key: value for key, value in muscle.items()
                                                     if key.startswith('m_KeepOriginal') or key == 'm_AverageSpeed'}})
    with (output/'hare.peac').open('wb') as stream:
        stream.write(struct.pack('<4s5I', b'PEAC', 1, len(vertices), len(indices), len(bones), len(exported)))
        for position, normal, uv, weights, joints in vertices:
            stream.write(struct.pack('<12f4H', *position, *normal, *uv, *weights, *joints))
        stream.write(struct.pack(f'<{len(indices)}I', *indices))
        for bind in inverse_binds:
            stream.write(struct.pack('<12f', *matrix_values(bind)))
        for name, fps, frames, duration, loop, palettes in exported:
            stream.write(struct.pack('<32sfIfI', name.encode('ascii'), fps, frames, duration, loop))
            for palette in palettes:
                for matrix in palette:
                    stream.write(struct.pack('<12f', *matrix_values(matrix)))
    source_hash = hashlib.sha256(bundle.read_bytes()).hexdigest()
    manifest = {'format_version': 1, 'binary_format': 'PEAC1', 'unitypy': '1.25.4',
                'source': str(bundle), 'source_sha256': source_hash, 'source_bytes': bundle.stat().st_size,
                'unity_versions': sorted({asset.unity_version for asset in environment.assets}),
                'coordinates': 'Original Unity XYZ meters; matrices row-major affine; native placement external',
                'palette_formula': 'inverse(model_root_world) * animated_bone_world * mesh_inverse_bind',
                'model_root_transform': MODEL_ROOT_ID, 'renderer_path_id': RENDERER_ID,
                'mesh_path_id': mesh_id, 'material_path_id': material_id, 'texture_path_id': texture_id,
                'diffuse_file': 'hare-diffuse.png', 'diffuse_tint': tint, 'material_properties': properties,
                'vertices': len(vertices), 'indices': len(indices), 'joints': len(bones),
                'combat_provenance': combat_evidence,
                'joint_transform_path_ids': bones, 'hierarchy': nodes, 'clips': reports,
                'authored_hierarchy_pose_bounds': authored_bounds,
                'inverse_bind_reconstruction_max_error': rest_identity_error,
                'limitations': ['Idle/run/walk and five melee clips; no Animator transitions, AI or ragdoll. Recovered attack-window metadata is not an implemented collision/hit system.',
                                'Diffuse only; source shaders and normal maps not reproduced.',
                                'Authored hierarchy is already posed, not necessarily the inverse-bind rest pose.',
                                'Dense samples are preserved; native in-between matrix interpolation approximates quaternion interpolation.',
                                'Root motion extraction is not implemented; selected clips have zero source average speed.'],
                'files': {p.name: {'bytes': p.stat().st_size, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                          for p in sorted(output.iterdir())}}
    (output/'hare-manifest.json').write_text(json.dumps(manifest, sort_keys=True, indent=2)+'\n')
    return manifest


def bounds(points):
    values = list(points)
    if not values:
        raise ValueError('Empty mesh')
    return [[min(point[axis] for point in values) for axis in range(3)],
            [max(point[axis] for point in values) for axis in range(3)]]


def union_bounds(a, b):
    if a is None:
        return b
    return [[min(a[0][i], b[0][i]) for i in range(3)], [max(a[1][i], b[1][i]) for i in range(3)]]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args(argv)
    try:
        if importlib.metadata.version('UnityPy') != '1.25.4':
            raise ValueError('Expected UnityPy==1.25.4 in an isolated environment')
        game = args.game_dir.expanduser().resolve(); output = args.output.expanduser().resolve()
        if output == game or game in output.parents:
            raise ValueError('Output must be outside the original installation')
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.pe-actor-export-', dir=output.parent) as temporary:
            staging = Path(temporary)
            manifest = recover(game/BUNDLE, staging)
            output.mkdir(exist_ok=True)
            for path in sorted(staging.iterdir(), key=lambda p: p.name == 'hare-manifest.json'):
                path.replace(output/path.name)
        print(f'Exported hare: {manifest["vertices"]} vertices, {manifest["joints"]} joints, idle/run/melee/walk to {output}')
    except (ImportError, importlib.metadata.PackageNotFoundError):
        parser.exit(1, 'Install UnityPy==1.25.4 in an isolated virtual environment first.\n')
    except (OSError, ValueError, KeyError, IndexError, struct.error) as error:
        parser.exit(1, f'Actor export failed: {error}\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
