#!/usr/bin/env python3
"""Compare a bounded humanoid joint-rotation candidate with existing Unity bakes.

Offline research only: no engine initialization, runtime asset installation or
claim of complete retargeting. The C helper must be built separately. Source
root/goal solving, limb twist distribution and layered animation are NOT modeled.
"""
from read_reference_adventure import source_label
import argparse
import ctypes
import hashlib
import json
import math
import struct
from pathlib import Path

from analyze_reference_bake import analyze as validate_bake
from export_reference_assets import trs
from export_reference_player import Assets, packed_channels


def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def vector(value, keys):
    result = [value[k] for k in keys]
    if any(type(x) not in (int, float) or not math.isfinite(x) for x in result):
        raise ValueError('Nonfinite vector')
    return result


def product(a, b):
    x, y, z, w = a
    X, Y, Z, W = b
    return [w*X+x*W+y*Z-z*Y, w*Y-x*Z+y*W+z*X,
            w*Z+x*Y-y*X+z*W, w*W-x*X-y*Y-z*Z]


def normalized(q):
    n = math.sqrt(sum(x*x for x in q))
    if not math.isfinite(n) or n < 1e-12:
        raise ValueError('Invalid rotation magnitude')
    return [x/n for x in q]


def sample_channels(channels, time):
    """Dense frame-major interpolation; constants retain their binding identity.

    This is a diagnostic phase sampler, not qualified native curve interpolation.
    Source normalized muscle values may exceed [-1,1] and are never clamped.
    """
    if not math.isfinite(time) or time < 0:
        raise ValueError('Invalid authored time')
    count = channels['frame_count']
    rate = channels['sample_rate']
    begin = channels['begin_time']
    if type(count) is not int or count < 1 or not math.isfinite(rate) or rate <= 0 or not math.isfinite(begin):
        raise ValueError('Invalid dense clock')
    phase = min(max((time-begin)*rate, 0), count-1)
    i = int(phase)
    j = min(i+1, count-1)
    result = {}
    for b in channels['bindings']:
        if b['kind'] != 'humanoid_scalar_unretargeted':
            continue
        attr = b['binding']['attribute']
        if type(attr) is not int or not 7 <= attr <= 133 or attr in result or b['width'] != 1:
            raise ValueError('Unsupported/duplicate humanoid scalar binding')
        values = b['samples']
        if len(values) not in (1, count) or any(len(v) != 1 or not math.isfinite(v[0]) for v in values):
            raise ValueError('Invalid scalar samples')
        result[attr] = values[0][0] if len(values) == 1 else values[i][0]*(1-(phase-i))+values[j][0]*(phase-i)
    if set(result) != set(range(7, 134)):
        raise ValueError('Require exact root/goals/92-muscle channel closure')
    return result


def bone_mapping(rig, skeleton, traits):
    """Join exact paths; do not conflate trait/human/avatar/capture indices."""
    av = rig['avatar']['m_Avatar']
    human = av['m_Human']['data']
    if human['m_HasTDoF'] or len(traits['muscles']) != 92 or len(traits['bones']) != 54:
        raise ValueError('Unsupported humanoid schema')
    paths = dict(rig['avatar']['m_TOS'])
    ids = av['m_AvatarSkeleton']['data']['m_ID']
    avatar_by_path = {paths[x]: i for i, x in enumerate(ids) if x in paths}
    if len(avatar_by_path) != len(ids):
        raise ValueError('Missing/duplicate Avatar paths')
    names = [b['name'].replace(' ', '') for b in traits['bones']]
    if len(set(names)) != len(names):
        raise ValueError('Ambiguous human trait names')
    node_paths = [n['path'] for n in skeleton['nodes']]
    if len(set(node_paths)) != len(node_paths) or any(
            type(i) is not int or not 0 <= i < len(node_paths)
            for i in skeleton['humanBones'].values()):
        raise ValueError('Invalid/duplicate captured bone mapping')
    mapping, omitted = [], []
    for trait_index, (name, trait) in enumerate(zip(names, traits['bones'])):
        muscles = trait['muscleIndices']
        if len(muscles) != 3 or any(type(m) is not int or not -1 <= m < 92 for m in muscles):
            raise ValueError('Invalid bone muscle indices')
        if name not in skeleton['humanBones']:
            omitted.append({'bone': name, 'reason': 'not bound in captured Avatar'})
            continue
        node = skeleton['humanBones'][name]
        path = skeleton['nodes'][node]['path']
        if path not in avatar_by_path:
            raise ValueError('Captured human path absent from Avatar')
        avatar_index = avatar_by_path[path]
        human_index = av['m_HumanSkeletonReverseIndexArray'][avatar_index]
        if (type(human_index) is not int or not 0 <= human_index < len(av['m_HumanSkeletonIndexArray']) or
                av['m_HumanSkeletonIndexArray'][human_index] != avatar_index):
            raise ValueError('Human/Avatar index mapping mismatch')
        axes_index = human['m_Skeleton']['data']['m_Node'][human_index]['m_AxesId']
        if all(m < 0 for m in muscles):
            omitted.append({'bone': name, 'reason': 'root/body transform is not a joint muscle'})
            continue
        if axes_index < 0:
            omitted.append({'bone': name, 'reason': 'no serialized axes'})
            continue
        axes = human['m_Skeleton']['data']['m_AxesArray'][axes_index]
        if axes['m_Type'] != 1:
            raise ValueError('Unqualified axes type')
        scale = vector(av['m_DefaultPose']['data']['m_X'][avatar_index]['s'], 'xyz')
        if any(abs(x) < 1e-8 for x in scale):
            raise ValueError('Singular default scale')
        mapping.append({'bone': name, 'path': path, 'trait_index': trait_index,
                        'capture_index': node, 'avatar_index': avatar_index,
                        'human_index': human_index, 'axes_index': axes_index,
                        'muscles': muscles, 'axes': axes, 'scale': scale})
    return mapping, omitted


class RotationMath:
    def __init__(self, path):
        self.library = ctypes.CDLL(str(Path(path).resolve()))
        self.call = self.library.pe_human_rotation
        fp = ctypes.POINTER(ctypes.c_float)
        self.call.argtypes = [fp, fp, fp, fp, fp]
        self.call.restype = ctypes.c_bool

    def __call__(self, values, axes):
        triple = ctypes.c_float*3
        out = (ctypes.c_float*4)()
        if not self.call(triple(*values), triple(*vector(axes['m_Limit']['m_Min'], 'xyz')),
                         triple(*vector(axes['m_Limit']['m_Max'], 'xyz')),
                         triple(*vector(axes['m_Sgn'], 'xyz')), out):
            raise ValueError('C rotation helper rejected source axes/muscles')
        return list(out)


def candidate_matrix(binding, scalar, rotation):
    values = [scalar[42+m] if m >= 0 else 0 for m in binding['muscles']]
    axes = binding['axes']
    post = vector(axes['m_PostQ'], 'xyzw')
    q = product(product(vector(axes['m_PreQ'], 'xyzw'), rotation(values, axes)),
                [-post[0], -post[1], -post[2], post[3]])
    return trs(dict(x=0, y=0, z=0), dict(zip('xyzw', normalized(q))),
               dict(zip('xyz', binding['scale'])))


def loop_muscle_delta(source):
    """Bounded loop-pose candidate, backed by serialized deltas and bake comparison.

    No root/goal correction or mirrored/cycle-offset branch is modeled here.
    """
    if type(source['m_LoopBlend']) is not bool or type(source['m_LoopTime']) is not bool:
        raise ValueError('Invalid loop blend flag')
    if not source['m_LoopBlend'] or not source['m_LoopTime']:
        return None
    if (source['m_Mirror'] is not False or
            source['m_CycleOffset'] != 0 or source['m_StartTime'] != 0):
        raise ValueError('Unqualified loop correction branch')
    duration = source['m_StopTime']
    if type(duration) not in (int, float) or not math.isfinite(duration) or duration <= 0:
        raise ValueError('Invalid loop duration')
    pose = source['m_DeltaPose']
    parts = (pose['m_DoFArray'], pose['m_LeftHandPose']['m_DoFArray'], pose['m_RightHandPose']['m_DoFArray'])
    if [len(p) for p in parts] != [52, 20, 20]:
        raise ValueError('Invalid loop muscle delta closure')
    delta = [v for part in parts for v in part]
    if any(type(v) not in (int, float) or not math.isfinite(v) for v in delta):
        raise ValueError('Invalid loop muscle delta')
    return {'duration': duration, 'delta': delta,
            'formula': 'f32(muscle + f32(clamped_forward_phase * serialized delta))',
            'endpoint_adapter': 'corrected terminal keys; native integer loop phases wrap to start'}


def compare(clip, samples, mapping, rotation, loop=None):
    totals = {b['bone']: {'max_local_basis_error': 0., 'sum_local_basis_error': 0.,
                         'worst_authored_time': 0., 'samples': 0} for b in mapping}
    for row in samples:
        scalar = sample_channels(clip, row['authoredTime'])
        if loop is not None:
            def f32(x): return struct.unpack('<f', struct.pack('<f', x))[0]
            phase = f32(min(max(row['authoredTime']/loop['duration'], 0), 1))
            for i, delta in enumerate(loop['delta']):
                scalar[42+i] = f32(f32(scalar[42+i])+f32(phase*delta))
        for b in mapping:
            m = candidate_matrix(b, scalar, rotation)
            expected = row['local'][b['capture_index']]
            error = max(abs(m[r][c]-expected[r*4+c]) for r in range(3) for c in range(3))
            t = totals[b['bone']]
            if error > t['max_local_basis_error']:
                t['max_local_basis_error'] = error
                t['worst_authored_time'] = row['authoredTime']
            t['sum_local_basis_error'] += error
            t['samples'] += 1
    for value in totals.values():
        value['mean_local_basis_error'] = value.pop('sum_local_basis_error')/value['samples']
    return totals


def validate_launch(path, capture, validated, asset):
    launch = json.loads(path.read_text())
    if launch['schema'] != 'pe-animator-bake-launch-1' or Path(launch['capture']).resolve() != capture.resolve():
        raise ValueError('Launch provenance capture mismatch')
    plan = launch['plan']
    if (plan['avatar'] != validated['skeleton']['avatar'] or
            plan['gender'] != validated['skeleton']['gender'] or
            plan['sample_rate'] != validated['header']['sampleRate'] or
            len(plan['clips']) != len(validated['clips']) or
            plan['asset_sha256'] != sha(asset)):
        raise ValueError('Launch/source identity mismatch')
    for expected, actual in zip(plan['clips'], validated['clips']):
        times = (expected['length'], actual['length'], expected['frame_rate'], actual['sourceFrameRate'])
        if any(type(v) not in (int, float) or not math.isfinite(v) or v <= 0 for v in times):
            raise ValueError('Invalid launch/bake clip clock')
        if (expected['name'] != actual['name'] or expected['path_id'] != actual['sourcePathId'] or
                expected['loop'] != actual['isLooping'] or abs(expected['length']-actual['length']) > .0001 or
                abs(expected['frame_rate']-actual['sourceFrameRate']) > .001):
            raise ValueError('Launch/bake clip identity mismatch')


def validate_rotation_proof(proof, library):
    comparison = proof['comparison']
    if (proof['schema'] != 'pe-unity5-human-rotation-1' or
            proof['binary_sha256'] != '1eb5ad6ff19dddb960a71453603ed7c67403403186c9dfeadc798a6021c4d2a6' or
            comparison['bitwise_mismatches'] != 0 or comparison['maximum_component_error'] != 0 or
            comparison['native_rsqrt'] is not True or
            comparison['library_sha256'] != sha(library) or
            comparison['c_source_sha256'] != sha(Path(__file__).with_name('human_rotation_math.c'))):
        raise ValueError('Qualified rotation helper/proof mismatch')


def run(args):
    launch_path = args.capture.with_suffix('.launch.json')
    asset_path = args.game/'PE_Client_Data/sharedassets1.assets'
    inputs = [args.rig, args.traits, args.capture, launch_path, args.library, args.rotation_proof,
              asset_path, Path(__file__), Path(__file__).with_name('human_rotation_math.c'),
              Path(__file__).with_name('human_rotation_math.h'),
              Path(__file__).with_name('export_reference_player.py'),
              Path(__file__).with_name('analyze_reference_bake.py')]
    provenance = [{'path': source_label(p), 'sha256': sha(p)} for p in inputs]
    rig = json.loads(args.rig.read_text())
    traits_document = json.loads(args.traits.read_text())
    traits = traits_document.get('human_traits', traits_document)
    validated, _ = validate_bake(args.capture, rig)
    validate_launch(launch_path, args.capture, validated, asset_path)
    validate_rotation_proof(json.loads(args.rotation_proof.read_text()), args.library)
    skeleton = validated['skeleton']
    mapping, omitted = bone_mapping(rig, skeleton, traits)
    poses = {}
    with args.capture.open() as stream:
        for line in stream:
            row = json.loads(line)
            if row['type'] == 'pose':
                poses.setdefault(row['clip'], []).append(row)
    rotation = RotationMath(args.library)
    assets = Assets(args.game/'PE_Client_Data')
    file = assets.file('sharedassets1.assets')
    avatar = file.objects[rig['avatar_id']]
    if (avatar.type.name != 'Avatar' or
            json.dumps(avatar.read_typetree(), sort_keys=True, allow_nan=False) !=
            json.dumps(rig['avatar'], sort_keys=True, allow_nan=False)):
        raise ValueError('Rig Avatar differs from pinned installed source')
    results = []
    for entry in validated['clips']:
        obj = file.objects[entry['sourcePathId']]
        if obj.type.name != 'AnimationClip':
            raise ValueError('Captured clip identity is not an AnimationClip')
        tree = obj.read_typetree()
        decoded = packed_channels(tree, rig['avatar'])
        if decoded['name'] != entry['name'] or abs(decoded['stop_time']-entry['length']) > 1e-4:
            raise ValueError('Captured/source clip identity mismatch')
        samples = poses[entry['index']]
        loop = loop_muscle_delta(tree['m_MuscleClip'])
        rows = compare(decoded, samples, mapping, rotation, loop)
        results.append({'name': entry['name'], 'source_path_id': entry['sourcePathId'],
                        'source_object_sha256': hashlib.sha256(obj.get_raw_data()).hexdigest(),
                        'samples': len(samples), 'bones': rows,
                        'loop_muscle_correction': loop,
                        'uncorrected_bones': compare(decoded, samples, mapping, rotation) if loop else None,
                        'maximum_local_basis_error': max(v['max_local_basis_error'] for v in rows.values())})
    if any(sha(p) != source['sha256'] for p, source in zip(inputs, provenance)):
        raise ValueError('Comparison inputs changed during evaluation')
    return {'schema': 'pe-humanoid-rotation-probe-1', 'samples': validated['samples'],
            'candidate': 'preQ * GetZYRoll(muscles, axes) * inverse(postQ), with Avatar signed local scale',
            'numeric_scope': 'local 3x3 basis only; translation/world pose/root/goals/limb solve not modeled',
            'general_retargeting_validated': False, 'shield_composition_validated': False,
            'mapping': [{k: v for k, v in b.items() if k != 'axes'} for b in mapping],
            'omitted': omitted, 'clips': results,
            'inputs': provenance}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('rig', 'traits', 'capture', 'library', 'game', 'output', 'rotation-proof'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError('Require a fresh output path; never replace inputs or prior evidence')
    result = run(args)
    with args.output.open('x') as output:
        output.write(json.dumps(result, indent=2)+'\n')
    print(f"Compared {result['samples']} captured samples; full retargeting remains unqualified")


if __name__ == '__main__':
    main()
