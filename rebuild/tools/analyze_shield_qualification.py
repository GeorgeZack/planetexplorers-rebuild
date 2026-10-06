#!/usr/bin/env python3
"""Validate bounded clone repeatability; report errors, never infer pose parity.

Exact stored numeric equality across repeated evaluations is required before
preparing an independent phase grid. This is not approval to export poses.
The successful three-frame smoke analyzer remains unchanged.
"""
import argparse
import itertools
import json
import math
from pathlib import Path

from analyze_reference_pose import decode
from analyze_shield_composition import compare_frame, finite, matrix, multiply, weights
from inventory_shield_layers import sha

MAX_BYTES = 16 * 1024 * 1024
MAX_LINE = 1024 * 1024
READBACK_LIMITS = {'normalized_time': 1e-4, 'clip_weight': 1e-5,
                   'layer_weight': 1e-5, 'parameter': 1e-6}
ORDER = ((0, 0, 0), (0, 1, 0), (0, 2, 0), (0, 0, 1),
         (1, 2, 0), (1, 0, 0), (1, 1, 0), (1, 2, 1))
IDENTITY = [1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def integer(value, minimum=0, maximum=2**31-1):
    require(type(value) is int and minimum <= value <= maximum, 'Invalid bounded integer')
    return value


def boolean(value):
    require(type(value) is bool, 'Invalid boolean')
    return value


def affine(value):
    result = matrix(value)
    require(max(abs(a-b) for a, b in zip(result[12:], [0, 0, 0, 1])) <= 1e-5,
            'Non-affine matrix')
    return result


def hierarchy(bones, fields):
    require(isinstance(bones, list) and 2 <= len(bones) <= 512, 'Hierarchy bound')
    result = {}
    for bone in bones:
        path = bone['path']
        require(type(path) is str and len(path) <= 2048 and '\x00' not in path, 'Invalid hierarchy path')
        require(path == '' or all(p not in ('.', '..', '') for p in path.split('/')), 'Invalid hierarchy path')
        require(path not in result, 'Duplicate hierarchy path')
        if path:
            parent = path.rpartition('/')[0]
            require(parent in result, 'Missing/out-of-order hierarchy parent')
        else:
            require(not result, 'Root must be first')
        if 'parent' in bone:
            require(bone['parent'] == (path.rpartition('/')[0] if path else None), 'Hierarchy parent differs')
        result[path] = {field: affine(bone[field]) for field in fields}
    require('' in result, 'Missing hierarchy root')
    return result


def differences(first, second, fields=('local', 'model')):
    require(set(first) == set(second), 'Hierarchy path set differs')
    result = {}
    for field in fields:
        worst_matrix = {'error': 0., 'path': '', 'component': 0}
        worst_position = {'error_metres': 0., 'path': ''}
        for path in first:
            a, b = first[path][field], second[path][field]
            errors = [abs(x-y) for x, y in zip(a, b)]
            component = max(range(16), key=errors.__getitem__)
            if errors[component] > worst_matrix['error']:
                worst_matrix = {'error': errors[component], 'path': path, 'component': component}
            distance = math.sqrt(sum((a[i]-b[i])**2 for i in (3, 7, 11)))
            if distance > worst_position['error_metres']:
                worst_position = {'error_metres': distance, 'path': path}
        result[field] = {'max_matrix': worst_matrix, 'max_position': worst_position}
    return result


def digest_file(path, expected):
    require(type(expected) is str and len(expected) == 64 and
            all(c in '0123456789abcdef' for c in expected), 'Invalid provenance digest')
    require(path.is_file() and sha(path) == expected, 'Provenance mismatch: '+str(path))
    return expected


def provenance(plan, plan_path, header):
    require(header['planSha256'] == sha(plan_path), 'Plan provenance mismatch')
    checked = {}
    for key in ('capture', 'source_plan', 'rig', 'baseline_plan', 'baseline_capture'):
        path = Path(plan[key])
        require(path.is_absolute(), 'Absolute provenance path required')
        checked[key] = digest_file(path, plan[key+'_sha256'])
    folder = Path(plan['bundle_dir'])
    require(folder.is_absolute(), 'Absolute bundle directory required')
    manifest_path = folder/'manifest.json'
    checked['manifest'] = digest_file(manifest_path, plan['controller_manifest_sha256'])
    manifest = decode(manifest_path.read_bytes())
    require(manifest.get('schema') == 'pe-shield-controller-clone-1', 'Wrong controller manifest')
    require(Path(manifest['bundle']).name == manifest['bundle'], 'Invalid bundle basename')
    checked['bundle'] = digest_file(folder/manifest['bundle'], plan['bundle_sha256'])
    require(header['bundleSha256'] == checked['bundle'] == manifest['bundle_sha256'], 'Bundle identity differs')
    checked['source_asset'] = digest_file(Path(manifest['source_asset']), plan['source_asset_sha256'])
    require(manifest['source_asset_sha256'] == checked['source_asset'], 'Source asset differs')
    require(boolean(manifest['ik_disabled']) == boolean(plan['ik_disabled']), 'IK variant differs')
    validated = manifest['validated']
    require(validated['callback_objects'] == 0 and validated['clip_events'] == 0 and
            validated['external_dependencies'] == 0 and validated['animation_clips'] == 452,
            'Controller isolation manifest differs')
    return checked


def validate_frame(actual, expected):
    """Malformed data raises; well-formed mismatching evaluation stays diagnostic."""
    integer(actual['frame']);integer(actual['sourceSample'])
    for key, expected_key in (('requestedBaseTime', 'base_normalized_time'),
                              ('requestedBlockTime', 'block_normalized_time')):
        require(abs(finite(actual[key])-finite(expected['evidence'][expected_key])) <= 1e-4,
                'Requested phase differs from plan')
    require(isinstance(actual['layers'], list) and len(actual['layers']) == len(expected['layers']),
            'Layer count differs')
    for layer_index, (a, e) in enumerate(zip(actual['layers'], expected['layers'])):
        require(a['name'] == e['name'], 'Layer ordering differs')
        boolean(a['inTransition'])
        integer(a['state'], -2**31, 2**32-1)
        integer(a['nextState'], -2**31, 2**32-1)
        require(0 <= finite(a['weight']) <= 1.00001, 'Invalid layer weight')
        if a['normalizedTime'] is None:
            require(layer_index not in (0, 3) and not a['inTransition'] and a['weight'] == 0 and
                    a['clips'] == [] and a.get('normalizedTimeAbsentReason') == 'inactive-clipless-layer',
                    'Unexplained absent current clock')
        else:
            finite(a['normalizedTime'])
            require(a.get('normalizedTimeAbsentReason') is None, 'Finite current clock has absent reason')
        if not a['inTransition']:
            require(a['nextNormalizedTime'] is None and a.get('nextTimeAbsentReason') == 'not-transitioning',
                    'Nontransition next clock sentinel differs')
        else:
            finite(a['nextNormalizedTime'])
            require(a.get('nextTimeAbsentReason') is None, 'Transition next clock has absent reason')
        for key in ('clips', 'nextClips'):
            require(isinstance(a[key], list) and len(a[key]) <= 452, 'Clip count bound')
            for clip in a[key]:
                require(type(clip['name']) is str and 0 < len(clip['name']) <= 1024, 'Invalid clip name')
            weights(a[key])
    require(len({p['name'] for p in expected['parameters']}) == len(expected['parameters']),
            'Duplicate expected parameter')
    for p in expected['parameters']:
        value = actual['parameters'].get(p['name'])
        if p['type'] == 4:
            boolean(value)
        elif p['type'] == 3:
            integer(value, -2**31, 2**31-1)
        else:
            require(p['type'] == 1, 'Unsupported replay parameter type');finite(value)
    result = compare_frame(actual, expected)
    # compare_frame aggregates named weights; distinct clip lengths must also
    # match, including zero-weight residual entries if present in readback.
    length_matches = True
    for index in (0, 3):
        known = {}
        for clip in expected['layers'][index]['currentClips']:
            known.setdefault(clip['name'], []).append(finite(clip['length']))
        for clip in actual['layers'][index]['clips']:
            if clip['weight'] > 0:
                length_matches &= any(abs(clip['length']-n) <= 1e-5 for n in known.get(clip['name'], []))
    result['authored_clip_lengths_match'] = length_matches
    result['state_clock_weight_readback_matches'] &= length_matches
    return result, hierarchy(actual['bones'], ('local', 'model'))


def analyze(capture, plan_path):
    require(capture.stat().st_size <= MAX_BYTES and plan_path.stat().st_size <= MAX_BYTES, 'Byte bound')
    capture_hash, plan_hash = sha(capture), sha(plan_path)
    plan = decode(plan_path.read_bytes())
    require(plan.get('schema') == 'pe-shield-qualification-plan-1' and plan.get('phase') == 'repeatability', 'Wrong plan phase/schema')
    require(len(plan['frames']) == 3 and plan['max_frames'] == 8 and plan['expected_clones'] == 2, 'Plan bound differs')
    require([(r['clone'], r['config'], r['repetition']) for r in plan['sequence']] == list(ORDER), 'Plan sequence differs')
    rows = []
    with capture.open('rb') as stream:
        while line := stream.readline(MAX_LINE+1):
            require(len(line) <= MAX_LINE and len(rows) < 100, 'Line/row bound')
            row = decode(line);require(isinstance(row, dict), 'Object row required');rows.append(row)
    diagnostics = [r for r in rows if r.get('type') == 'diagnostic']
    require(len(diagnostics) <= 40 and all(type(r.get('message')) is str and len(r['message']) <= 4096 for r in diagnostics), 'Diagnostic bound')
    rows = [r for r in rows if r.get('type') != 'diagnostic']
    require(len(rows) == 14 and rows[0].get('type') == 'header' and rows[-1].get('type') == 'end', 'Require exact closed repeatability stream')
    header, end = rows[0], rows[-1]
    require(header.get('schema') == 'pe-shield-qualification-1' and header.get('phase') == 'repeatability', 'Wrong capture phase/schema')
    require(header['maxSamples'] == 8 and header['maxClones'] == 2 and header['maxBytes'] == MAX_BYTES and header['maxSeconds'] == 60,
            'Capture bounds differ')
    require(header['cleanup'] == 'retain-inactive-until-exit' and header['postCleanupFrames'] == 120,
            'Retention/cleanup contract differs')
    require(end['reason'] == 'complete' and end['frames'] == 8 and end.get('error') is None, 'Capture incomplete/failed')
    require(plan['variant'] in ('noik', 'sourceik') and header['variant'] == plan['variant'] and
            plan['ik_disabled'] == (plan['variant'] == 'noik'), 'Variant differs')
    for replay in plan['sequence']:
        expected = plan['frames'][replay['config']]['evidence']
        for key in ('base_normalized_time', 'block_normalized_time'):
            require(finite(replay[key]) == finite(expected[key]), 'Plan replay phase differs')
    checked = provenance(plan, plan_path, header)
    clones, poses, reports = {}, [], []
    last_frame = -1
    for row in rows[1:-1]:
        kind = row.get('type')
        if kind == 'preEvaluation':
            index = integer(row['clone'], 0, 1)
            require(index == len(clones) and index not in clones and len(poses) == index*4, 'Initial clone order/identity differs')
            require(boolean(row['beforeAnimatorCreated']), 'Initial hierarchy captured after Animator creation')
            require(row['avatar'] == 'FemaleAvatar' and row['sourceController'] == 'FemaleAnimator2', 'Source Avatar/controller differs')
            integer(row['avatarId'], -2**31, 2**31-1)
            affine(row['sourceRootWorld'])
            require(all(b.get('components') == ['UnityEngine.Transform'] for b in row['clone_bones']), 'Impure pre-evaluation clone')
            source = hierarchy(row['source_bones'], ('local', 'model'))
            copied = hierarchy(row['clone_bones'], ('local', 'model'))
            require(set(source) == set(copied), 'Copied source hierarchy differs')
            original = {path: {'local': b['local'] if path else IDENTITY} for path, b in source.items()}
            initial_diff = differences(copied, original, ('local',))
            require(copied['']['model'] == IDENTITY, 'Clone initial root model differs')
            clones[index] = {'initial': copied, 'source': source, 'avatar_id': row['avatarId'],
                             'initial_difference': initial_diff}
        elif kind == 'cloneReady':
            index = integer(row['clone'], 0, 1)
            require(index in clones and 'ready' not in clones[index] and not any(p['clone'] == index for p in poses),
                    'Clone readiness order differs')
            require(integer(row['bones'], 2, 512) == len(clones[index]['initial']), 'Clone hierarchy count differs')
            require(row['layers'] == len(plan['frames'][0]['layers']), 'Clone layer count differs')
            require(row['sourceAvatar'] == 'FemaleAvatar' and row['sourceController'] == 'FemaleAnimator2', 'Clone source identity differs')
            pure = row['purity']
            require(integer(pure['behaviours']) == 0 and not boolean(pure['fire_events']) and
                    not boolean(pure['apply_root_motion']) and finite(pure['animator_speed']) == 0 and
                    pure['non_transform_components'] == ['UnityEngine.Animator'], 'Clone purity contract differs')
            clones[index]['ready'] = row
        elif kind == 'frame':
            number = len(poses)
            require(number < 8 and integer(row['sample'], 0, 7) == number, 'Sample order differs')
            expected_order = ORDER[number]
            require(tuple(integer(row[k], 0, 2) for k in ('clone', 'config', 'repetition')) == expected_order,
                    'Replay order/clone/config/repetition differs')
            index, config, repetition = expected_order
            require(index in clones and 'ready' in clones[index], 'Frame precedes initial hierarchy')
            require(integer(row['frame']) > last_frame, 'Frame clock must advance')
            last_frame = row['frame']
            comparison, bones = validate_frame(row, plan['frames'][config])
            require(set(bones) == set(clones[index]['initial']), 'Evaluated hierarchy differs')
            poses.append({'clone': index, 'config': config, 'repetition': repetition, 'sample': number, 'bones': bones,
                          'evaluation': {'parameters': row['parameters'], 'animatorSpeed': row['animatorSpeed'],
                                         'active_layers': [row['layers'][0], row['layers'][3]],
                                         'all_layer_weights': [l['weight'] for l in row['layers']]}})
            comparison.update(clone=index, config=config, repetition=repetition, sample=number)
            reports.append(comparison)
        else:
            raise ValueError('Unexpected stream record: '+str(kind))
    require(len(clones) == 2 and len(poses) == 8 and all('ready' in c for c in clones.values()), 'Incomplete clones/frames')
    require(set(clones[0]['initial']) == set(clones[1]['initial']), 'Fresh clone hierarchy differs')
    require(clones[0]['avatar_id'] == clones[1]['avatar_id'], 'Shared source Avatar identity differs')
    pairs = []
    for a, b in itertools.combinations(poses, 2):
        if a['config'] != b['config']:
            continue
        pairs.append({'config': a['config'], 'samples': [a['sample'], b['sample']],
                      'clones': [a['clone'], b['clone']],
                      'kind': 'same_clone_revisit' if a['clone'] == b['clone'] else 'fresh_clone_order',
                      'exact_evaluation_readback': a['evaluation'] == b['evaluation'],
                      'errors': differences(a['bones'], b['bones'])})
    worst_matrix = max(p['errors'][space]['max_matrix']['error'] for p in pairs for space in ('local', 'model'))
    worst_position = max(p['errors'][space]['max_position']['error_metres'] for p in pairs for space in ('local', 'model'))
    initial_max = max(c['initial_difference']['local']['max_matrix']['error'] for c in clones.values())
    readback = all(r['state_clock_weight_readback_matches'] for r in reports)
    repeated_readback = all(p['exact_evaluation_readback'] for p in pairs)
    qualified = readback and repeated_readback and initial_max == 0 and worst_matrix == 0 and worst_position == 0
    require(sha(capture) == capture_hash and sha(plan_path) == plan_hash, 'Capture/plan changed during analysis')
    return {'schema': 'pe-shield-qualification-report-1', 'phase': 'repeatability',
            'capture': str(capture.resolve()), 'capture_sha256': capture_hash,
            'plan': str(plan_path.resolve()), 'plan_sha256': plan_hash, 'provenance': checked,
            'ik_disabled': plan['ik_disabled'], 'diagnostics': diagnostics, 'frames': reports,
            'initial_between_clones': differences(clones[0]['initial'], clones[1]['initial']),
            'initial_model_copy_diagnostics': [dict(clone=i, errors=differences(c['initial'], c['source'], ('model',)))
                                               for i, c in clones.items()],
            'clone_initial_copy': [dict(clone=i, errors=c['initial_difference']) for i, c in clones.items()],
            'initial_copy_max_matrix_error': initial_max, 'readback_limits': READBACK_LIMITS,
            'readback_matches': readback, 'comparisons': pairs,
            'repeat_max_matrix_error': worst_matrix, 'repeat_max_position_error_metres': worst_position,
            'exact_numeric_repeatability': worst_matrix == 0, 'exact_repeated_readback': repeated_readback,
            'repeatability_policy': 'exact stored numeric equality; no tolerance relaxation',
            'repeatability_qualified': qualified, 'grid_preparation_allowed': qualified,
            'runtime_pose_export_ready': False,
            'limits': ['Exact stored numeric equality governs clone repeatability; source/live residuals never relax this gate.',
                       'Source includes live IK; source-to-clone residuals are diagnostics, not pose parity.',
                       'All skeleton paths are compared in local and root-relative model space, including initial pose copying.',
                       'Repeated independent layer phases do not qualify transitions, phase interpolation, live combat or meshes.',
                       'Post-cleanup survival and ordinary quit require separate operator evidence.']}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--plan', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        require(not args.report.exists() and args.report.resolve() not in (args.capture.resolve(), args.plan.resolve()), 'Fresh report path required')
        result = analyze(args.capture.resolve(), args.plan.resolve())
        args.report.parent.mkdir(parents=True, exist_ok=True)
        with args.report.open('x') as out:
            json.dump(result, out, indent=2, allow_nan=False);out.write('\n')
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.error(str(error))
    print(json.dumps({k: result[k] for k in ('readback_matches', 'repeat_max_matrix_error', 'grid_preparation_allowed', 'runtime_pose_export_ready')}))


if __name__ == '__main__':
    main()
