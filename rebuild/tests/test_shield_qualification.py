#!/usr/bin/env python3
"""Synthetic closed-stream regression; no engine or desktop access."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import analyze_shield_qualification as tool
import test_shield_composition as smoke_tests

I = tool.IDENTITY


def write(path, value):
    path.write_text(json.dumps(value, allow_nan=False)+'\n')


def fixture(folder):
    actual, expected = smoke_tests.ComparisonTests().fixture()
    expected['bones'] = [{'path': '', 'local': list(I), 'world': list(I)}, {'path': 'hips', 'local': list(I), 'world': list(I)}]
    expected['evidence'].update(base_normalized_time=expected['layers'][0]['current']['normalizedTime'],
                                block_normalized_time=expected['layers'][3]['current']['normalizedTime'])
    frames = []
    for config in range(3):
        f = copy.deepcopy(expected);f['sample'] = 10+config
        for index, key in ((0, 'base_normalized_time'), (3, 'block_normalized_time')):
            f['layers'][index]['current']['normalizedTime'] += config*.125
            f['evidence'][key] = f['layers'][index]['current']['normalizedTime']
        frames.append(f)
    for name in ('capture', 'source_plan', 'rig', 'asset', 'bundle', 'baseline_plan', 'baseline_capture'):
        (folder/name).write_bytes(('synthetic '+name).encode())
    manifest = {'schema': 'pe-shield-controller-clone-1', 'bundle': 'bundle', 'bundle_sha256': tool.sha(folder/'bundle'),
                'source_asset': str(folder/'asset'), 'source_asset_sha256': tool.sha(folder/'asset'), 'ik_disabled': True,
                'validated': {'callback_objects': 0, 'clip_events': 0, 'external_dependencies': 0, 'animation_clips': 452}}
    write(folder/'manifest.json', manifest)
    plan = {'schema': 'pe-shield-qualification-plan-1', 'phase': 'repeatability', 'variant': 'noik',
            'frames': frames, 'expected_clones': 2, 'max_frames': 8, 'ik_disabled': True,
            'bundle_dir': str(folder), 'bundle_sha256': tool.sha(folder/'bundle'),
            'source_asset_sha256': tool.sha(folder/'asset'), 'controller_manifest_sha256': tool.sha(folder/'manifest.json')}
    for key in ('capture', 'source_plan', 'rig', 'baseline_plan', 'baseline_capture'):
        plan[key] = str(folder/key);plan[key+'_sha256'] = tool.sha(folder/key)
    plan['sequence'] = [dict(clone=c, config=f, repetition=r,
        base_normalized_time=frames[f]['evidence']['base_normalized_time'],
        block_normalized_time=frames[f]['evidence']['block_normalized_time']) for c, f, r in tool.ORDER]
    path = folder/'plan.json';write(path, plan)
    rows = [{'type': 'header', 'schema': 'pe-shield-qualification-1', 'phase': 'repeatability', 'variant': 'noik',
             'planSha256': tool.sha(path), 'bundleSha256': plan['bundle_sha256'], 'maxSamples': 8, 'maxClones': 2,
             'maxSeconds': 60, 'maxBytes': tool.MAX_BYTES, 'cleanup': 'retain-inactive-until-exit', 'postCleanupFrames': 120}]
    for number, (clone, config, repetition) in enumerate(tool.ORDER):
        if number in (0, 4):
            bones = [{'path': '', 'parent': None, 'local': list(I), 'model': list(I)},
                     {'path': 'hips', 'parent': '', 'local': list(I), 'model': list(I)}]
            copies = copy.deepcopy(bones)
            for bone in copies:
                bone['components'] = ['UnityEngine.Transform']
            rows.append({'type': 'preEvaluation', 'clone': clone, 'avatar': 'FemaleAvatar', 'avatarId': 77,
                         'sourceController': 'FemaleAnimator2', 'sourceRootWorld': I, 'beforeAnimatorCreated': True,
                         'source_bones': copy.deepcopy(bones), 'clone_bones': copies})
            rows.append({'type': 'cloneReady', 'clone': clone, 'sourceAvatar': 'FemaleAvatar',
                         'sourceController': 'FemaleAnimator2', 'bones': 2, 'layers': len(actual['layers']),
                         'purity': {'behaviours': 0, 'fire_events': False, 'apply_root_motion': False,
                                    'animator_speed': 0., 'non_transform_components': ['UnityEngine.Animator']}})
        a = copy.deepcopy(actual);f = frames[config]
        a.update(type='frame', sample=number, clone=clone, config=config, repetition=repetition,
                 sourceSample=f['sample'], frame=100+number, requestedBaseTime=f['evidence']['base_normalized_time'],
                 requestedBlockTime=f['evidence']['block_normalized_time'])
        for index, layer in enumerate(a['layers']):
            layer['normalizedTime'] = f['layers'][index]['current']['normalizedTime']
            layer.update(nextState=0, nextNormalizedTime=None, nextTimeAbsentReason='not-transitioning',
                         normalizedTimeAbsentReason=None, nextClips=[])
        for bone in a['bones']:
            bone['local'] = list(I);bone['model'] = list(bone['model'])
        rows.append(a)
    rows.append({'type': 'end', 'reason': 'complete', 'frames': 8, 'error': None})
    return plan, rows


class QualificationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.plan, self.rows = fixture(self.folder)
        self.capture = self.folder/'evaluation.jsonl';self.plan_path = self.folder/'plan.json'

    def result(self):
        write(self.plan_path, self.plan);self.rows[0]['planSha256'] = tool.sha(self.plan_path)
        self.capture.write_text(''.join(json.dumps(r)+'\n' for r in self.rows))
        return tool.analyze(self.capture, self.plan_path)

    def frames(self):
        return [r for r in self.rows if r['type'] == 'frame']

    def test_exact_two_clone_revisits_enable_only_grid_preparation(self):
        result = self.result()
        self.assertTrue(result['readback_matches']);self.assertTrue(result['repeatability_qualified'])
        self.assertTrue(result['grid_preparation_allowed']);self.assertFalse(result['runtime_pose_export_ready'])
        self.assertEqual(len(result['comparisons']), 7)
        self.assertEqual({r['kind'] for r in result['comparisons']}, {'fresh_clone_order', 'same_clone_revisit'})
        self.assertEqual(result['repeat_max_matrix_error'], 0.)

    def test_tiny_order_dependence_cannot_be_blessed(self):
        self.frames()[3]['bones'][1]['local'][3] = 1e-12
        result = self.result();self.assertFalse(result['repeatability_qualified'])
        self.assertEqual(result['repeat_max_matrix_error'], 1e-12)
        self.assertTrue(result['readback_matches'])

    def test_tiny_readback_drift_blocks_grid_even_with_identical_poses(self):
        self.frames()[3]['layers'][0]['normalizedTime'] += 1e-6
        r = self.result();self.assertTrue(r['readback_matches'])
        self.assertTrue(r['exact_numeric_repeatability']);self.assertFalse(r['exact_repeated_readback'])
        self.assertFalse(r['repeatability_qualified'])

    def test_cross_clone_model_difference_quantified(self):
        self.frames()[5]['bones'][1]['model'][3] = .025
        result = self.result();self.assertAlmostEqual(result['repeat_max_position_error_metres'], .025)
        self.assertFalse(result['grid_preparation_allowed'])
        self.assertTrue(any(p['errors']['model']['max_position']['path'] == 'hips' for p in result['comparisons']))

    def test_source_residual_does_not_relax_repeatability(self):
        for f in self.frames():
            f['bones'][1]['model'][3] = .25
        result = self.result();self.assertTrue(result['repeatability_qualified'])
        self.assertEqual(result['frames'][0]['max_human_position_error_metres'], .25)
        self.assertFalse(result['runtime_pose_export_ready'])

    def test_source_root_is_normalized_before_initial_copy_comparison(self):
        pre = self.rows[1];pre['source_bones'][0]['local'] = list(I);pre['source_bones'][0]['local'][3] = 10
        self.assertTrue(self.result()['repeatability_qualified'])

    def test_initial_child_copy_difference_blocks_grid(self):
        self.rows[1]['clone_bones'][1]['local'][3] = .001
        r = self.result();self.assertEqual(r['initial_copy_max_matrix_error'], .001);self.assertFalse(r['grid_preparation_allowed'])

    def test_incomplete_error_duplicate_and_out_of_order_streams(self):
        baseline = copy.deepcopy(self.rows)
        for kind in ('missing-end', 'error', 'duplicate', 'order', 'frame-clock', 'pre-after-ready'):
            self.rows = copy.deepcopy(baseline)
            if kind == 'missing-end':self.rows.pop()
            elif kind == 'error':self.rows[-1]['reason'] = 'error'
            elif kind == 'duplicate':self.rows.insert(-1, copy.deepcopy(self.rows[3]))
            elif kind == 'order':self.frames()[1]['config'] = 2
            elif kind == 'frame-clock':self.frames()[1]['frame'] = self.frames()[0]['frame']
            else:self.rows[1], self.rows[2] = self.rows[2], self.rows[1]
            with self.subTest(kind=kind), self.assertRaises(ValueError):self.result()

    def test_requested_and_planned_phases_fail_closed(self):
        self.frames()[0]['requestedBlockTime'] += .2
        with self.assertRaisesRegex(ValueError, 'Requested phase'):self.result()
        self.frames()[0]['requestedBlockTime'] -= .2
        self.plan['sequence'][0]['base_normalized_time'] += .3
        with self.assertRaisesRegex(ValueError, 'Plan replay phase'):self.result()

    def test_actual_readback_mismatch_cannot_authorize_grid(self):
        baseline = copy.deepcopy(self.rows)
        for kind in ('clock', 'state', 'weight', 'length', 'parameter', 'transition', 'override'):
            self.rows = copy.deepcopy(baseline);f = self.frames()[0]
            if kind == 'clock':f['layers'][3]['normalizedTime'] += .02
            elif kind == 'state':f['layers'][0]['state'] = 10
            elif kind == 'weight':f['layers'][0]['clips'][0]['weight'] = .7
            elif kind == 'length':f['layers'][0]['clips'][0]['length'] += .1
            elif kind == 'parameter':f['parameters']['MoveStyle'] = 0.
            elif kind == 'transition':f['layers'][0].update(inTransition=True, nextNormalizedTime=.2, nextTimeAbsentReason=None)
            else:f['layers'][1].update(weight=1., clips=copy.deepcopy(f['layers'][0]['clips']))
            with self.subTest(kind=kind):
                result = self.result();self.assertFalse(result['readback_matches']);self.assertFalse(result['grid_preparation_allowed'])

    def test_malformed_matrix_hierarchy_parameters_and_purity(self):
        baseline = copy.deepcopy(self.rows)
        for kind in ('nan', 'affine', 'duplicate-path', 'parent', 'missing-human', 'bool-type', 'speed', 'events', 'component', 'late-pre', 'avatar'):
            self.rows = copy.deepcopy(baseline);f = self.frames()[0]
            if kind == 'nan':f['bones'][0]['model'][3] = float('nan')
            elif kind == 'affine':f['bones'][0]['local'][15] = 0
            elif kind == 'duplicate-path':f['bones'][1]['path'] = ''
            elif kind == 'parent':self.rows[1]['clone_bones'][1]['parent'] = 'missing'
            elif kind == 'missing-human':f['bones'][1]['path'] = 'other'
            elif kind == 'bool-type':f['parameters']['SheildBlock'] = 1
            elif kind == 'speed':f['animatorSpeed'] = 1.
            elif kind == 'events':self.rows[2]['purity']['fire_events'] = True
            elif kind == 'component':self.rows[1]['clone_bones'][1]['components'].append('UnityEngine.Collider')
            elif kind == 'late-pre':self.rows[1]['beforeAnimatorCreated'] = False
            else:self.rows[7]['avatarId'] = 78
            with self.subTest(kind=kind), self.assertRaises((ValueError, KeyError)):self.result()

    def test_provenance_changed_files_fail(self):
        for name in ('capture', 'source_plan', 'rig', 'asset', 'bundle', 'manifest.json', 'baseline_plan', 'baseline_capture'):
            path = self.folder/name;original = path.read_bytes();path.write_bytes(original+b'changed')
            with self.subTest(name=name), self.assertRaises(ValueError):self.result()
            path.write_bytes(original)

    def test_header_variant_and_bounds_fail(self):
        for key, value in (('maxSeconds', 61), ('maxBytes', tool.MAX_BYTES+1), ('maxClones', 3), ('variant', 'sourceik')):
            previous = self.rows[0][key];self.rows[0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):self.result()
            self.rows[0][key] = previous

    def test_diagnostics_bounded_and_preserved(self):
        self.rows.insert(1, {'type': 'diagnostic', 'message': 'load result'})
        self.assertEqual(len(self.result()['diagnostics']), 1)
        self.rows[1]['message'] = 'x'*4097
        with self.assertRaisesRegex(ValueError, 'Diagnostic bound'):self.result()

    def test_absent_clocks_match_successful_smoke_sentinel_policy(self):
        for frame in self.frames():
            frame['layers'][1].update(weight=0., normalizedTime=None, normalizedTimeAbsentReason='inactive-clipless-layer')
        self.assertTrue(self.result()['repeatability_qualified'])
        baseline = copy.deepcopy(self.rows)
        for kind in ('active-current', 'active-extra', 'reason', 'transition-next', 'finite-reason'):
            self.rows = copy.deepcopy(baseline);f = self.frames()[0]
            if kind == 'active-current':f['layers'][0]['normalizedTime'] = None
            elif kind == 'active-extra':f['layers'][1]['weight'] = 1.
            elif kind == 'reason':f['layers'][1]['normalizedTimeAbsentReason'] = 'unknown'
            elif kind == 'transition-next':f['layers'][0]['inTransition'] = True
            else:f['layers'][0]['normalizedTimeAbsentReason'] = 'inactive-clipless-layer'
            with self.subTest(kind=kind), self.assertRaises(ValueError):self.result()

    def test_grid_phase_has_no_accidental_runtime_support(self):
        self.plan['phase'] = 'grid'
        with self.assertRaisesRegex(ValueError, 'phase/schema'):self.result()


if __name__ == '__main__':
    unittest.main()
