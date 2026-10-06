#!/usr/bin/env python3
"""CPU-only channel identity and joint mapping regressions; no Unity process."""
import copy
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import probe_humanoid_rotation as probe


def channels():
    # Every attribute is distinct; array order deliberately disagrees with ID.
    return {'frame_count': 3, 'sample_rate': 2., 'begin_time': -.125,
            'bindings': [{'kind': 'humanoid_scalar_unretargeted',
                          'binding': {'attribute': a}, 'width': 1,
                          'samples': [[a + .25], [a + 1.25], [a + 2.25]]
                          if a % 2 else [[-a - .5]]}
                         for a in reversed(range(7, 134))]}


def mapping_fixture():
    q = dict(zip('xyzw', [0., 0., 0., 1.]))
    axes = {'m_Type': 1, 'm_PreQ': q.copy(), 'm_PostQ': q.copy(),
            'm_Sgn': dict(x=1., y=1., z=1.),
            'm_Limit': {'m_Min': dict(x=-1., y=-1., z=-1.),
                        'm_Max': dict(x=1., y=1., z=1.)}}
    # Trait0 -> capture2 -> Avatar1 -> human2 -> axes1; neither ordinal matches.
    # Trait1 -> capture0 -> Avatar2 -> human0 -> axes2.
    rig = {'avatar': {'m_TOS': [[11, 'root'], [22, 'root/arm'], [33, 'root/head']],
                     'm_Avatar': {
                         'm_AvatarSkeleton': {'data': {'m_ID': [11, 22, 33]}},
                         'm_HumanSkeletonIndexArray': [2, 0, 1],
                         'm_HumanSkeletonReverseIndexArray': [1, 2, 0],
                         'm_DefaultPose': {'data': {'m_X': [
                             {'s': dict(x=1., y=1., z=1.)},
                             {'s': dict(x=-2., y=3., z=.5)},
                             {'s': dict(x=1., y=1., z=1.)}]}},
                         'm_Human': {'data': {'m_HasTDoF': False,
                             'm_Skeleton': {'data': {
                                 'm_Node': [{'m_AxesId': 2}, {'m_AxesId': 0},
                                            {'m_AxesId': 1}],
                                 'm_AxesArray': [copy.deepcopy(axes) for _ in range(3)]}}}}}}}
    skeleton = {'nodes': [{'path': 'root/head'}, {'path': 'root'},
                          {'path': 'root/arm'}],
                'humanBones': {'LeftUpperArm': 2, 'Head': 0}}
    traits = {'muscles': list(range(92)), 'bones': [
        {'name': 'Left Upper Arm', 'muscleIndices': [38, -1, 36]},
        {'name': 'Head', 'muscleIndices': [11, 10, 9]}] + [
        {'name': 'Unbound%d' % i, 'muscleIndices': [-1, -1, -1]}
        for i in range(52)]}
    return rig, skeleton, traits


class ChannelTests(unittest.TestCase):
    def test_mixed_storage_identity_and_begin_time(self):
        source = channels()
        before = copy.deepcopy(source)
        result = probe.sample_channels(source, .125)  # phase .5, not .25
        self.assertEqual(set(result), set(range(7, 134)))
        for a in range(7, 134):
            self.assertEqual(result[a], a + .75 if a % 2 else -a - .5)
        self.assertEqual(source, before)

    def test_values_beyond_unit_range_remain_unclamped(self):
        source = channels()
        source['bindings'][0]['samples'] = [[-1.75], [2.25], [3.5]]
        self.assertEqual(probe.sample_channels(source, .875)[133], 3.5)
        self.assertEqual(probe.sample_channels(source, 0)[133], -.75)

    def test_phase_edges_and_constant_only_single_frame(self):
        source = channels()
        source['begin_time'] = .25
        self.assertEqual(probe.sample_channels(source, 0)[7], 7.25)
        self.assertEqual(probe.sample_channels(source, 100)[7], 9.25)
        source['frame_count'] = 1
        for b in source['bindings']:
            b['samples'] = [b['samples'][0]]
        self.assertEqual(probe.sample_channels(source, 100)[7], 7.25)

    def test_direct_transform_is_not_a_humanoid_scalar(self):
        source = channels()
        source['bindings'].append({'kind': 'direct_transform',
                                  'binding': {'attribute': 7}, 'width': 3,
                                  'samples': [[9., 8., 7.]]})
        self.assertEqual(probe.sample_channels(source, .125)[7], 7.75)

    def test_duplicate_missing_and_out_of_schema_attributes_rejected(self):
        for mutation in ('duplicate', 'missing', 'low', 'high', 'float', 'bool'):
            with self.subTest(mutation=mutation):
                source = channels()
                if mutation == 'duplicate':
                    source['bindings'].append(copy.deepcopy(source['bindings'][0]))
                elif mutation == 'missing':
                    source['bindings'].pop()
                else:
                    source['bindings'][0]['binding']['attribute'] = {
                        'low': 6, 'high': 134, 'float': 133., 'bool': True}[mutation]
                with self.assertRaises(ValueError):
                    probe.sample_channels(source, .1)

    def test_invalid_dense_clocks_rejected(self):
        for key, value in [('frame_count', 0), ('frame_count', True),
                           ('frame_count', 3.), ('sample_rate', 0.),
                           ('sample_rate', -2.), ('sample_rate', math.inf),
                           ('begin_time', math.nan)]:
            with self.subTest(key=key, value=value):
                source = channels(); source[key] = value
                with self.assertRaises(ValueError):
                    probe.sample_channels(source, .1)
        for time in (-.01, math.nan, math.inf, -math.inf):
            with self.subTest(time=time), self.assertRaises(ValueError):
                probe.sample_channels(channels(), time)

    def test_invalid_dense_and_constant_samples_rejected(self):
        for samples in ([], [[1.], [2.]], [[1., 2.]], [[math.nan]],
                        [[1.], [math.inf], [2.]], [[1.], [2.], [-math.inf]]):
            with self.subTest(samples=samples):
                source = channels(); source['bindings'][0]['samples'] = samples
                with self.assertRaises(ValueError):
                    probe.sample_channels(source, 0.)
        source = channels(); source['bindings'][0]['width'] = 2
        with self.assertRaises(ValueError):
            probe.sample_channels(source, 0.)


class MappingTests(unittest.TestCase):
    def test_negative_out_of_range_and_noninteger_capture_indices_rejected(self):
        for index in (-1, 3, True, 1.5):
            with self.subTest(index=index):
                rig, skeleton, traits = mapping_fixture()
                skeleton['humanBones']['LeftUpperArm'] = index
                with self.assertRaises(ValueError):
                    probe.bone_mapping(rig, skeleton, traits)

    def test_duplicate_capture_paths_rejected_even_for_unbound_nodes(self):
        rig, skeleton, traits = mapping_fixture()
        skeleton['nodes'].append({'path': 'root/arm'})
        with self.assertRaises(ValueError):
            probe.bone_mapping(rig, skeleton, traits)

    def test_negative_out_of_range_and_noninteger_human_indices_rejected(self):
        for index in (-1, 3, True, 1.5):
            with self.subTest(index=index):
                rig, skeleton, traits = mapping_fixture()
                rig['avatar']['m_Avatar']['m_HumanSkeletonReverseIndexArray'][1] = index
                with self.assertRaises(ValueError):
                    probe.bone_mapping(rig, skeleton, traits)

    def test_all_index_spaces_and_paths_are_joined_independently(self):
        rig, skeleton, traits = mapping_fixture()
        mapped, omitted = probe.bone_mapping(rig, skeleton, traits)
        arm, head = mapped
        self.assertEqual([arm[k] for k in ('trait_index', 'capture_index',
                         'avatar_index', 'human_index', 'axes_index')], [0, 2, 1, 2, 1])
        self.assertEqual([head[k] for k in ('trait_index', 'capture_index',
                         'avatar_index', 'human_index', 'axes_index')], [1, 0, 2, 0, 2])
        self.assertEqual(arm['path'], 'root/arm')
        self.assertEqual(arm['muscles'], [38, -1, 36])
        self.assertEqual(arm['scale'], [-2., 3., .5])
        self.assertEqual(len(omitted), 52)

    def test_reordering_capture_nodes_preserves_path_and_axes_identity(self):
        rig, skeleton, traits = mapping_fixture()
        before, _ = probe.bone_mapping(rig, skeleton, traits)
        skeleton['nodes'].reverse()
        skeleton['humanBones'] = {'LeftUpperArm': 0, 'Head': 2}
        after, _ = probe.bone_mapping(rig, skeleton, traits)
        for old, new in zip(before, after):
            self.assertNotEqual(old['capture_index'], new['capture_index'])
            self.assertEqual({k: v for k, v in old.items() if k != 'capture_index'},
                             {k: v for k, v in new.items() if k != 'capture_index'})

    def test_missing_duplicate_paths_and_nonreciprocal_indices_rejected(self):
        for kind in ('missing_avatar', 'duplicate_avatar', 'missing_capture', 'reverse'):
            with self.subTest(kind=kind):
                rig, skeleton, traits = mapping_fixture()
                if kind == 'missing_avatar':
                    rig['avatar']['m_TOS'].pop()
                elif kind == 'duplicate_avatar':
                    rig['avatar']['m_TOS'][2][1] = 'root/arm'
                elif kind == 'missing_capture':
                    skeleton['nodes'][2]['path'] = 'other/arm'
                else:
                    rig['avatar']['m_Avatar']['m_HumanSkeletonReverseIndexArray'][1] = 0
                with self.assertRaises(ValueError):
                    probe.bone_mapping(rig, skeleton, traits)

    def test_schema_muscle_and_axes_rejections(self):
        for kind in ('tdof', 'muscle_count', 'bone_count', 'names', 'muscle_index', 'axes_type'):
            with self.subTest(kind=kind):
                rig, skeleton, traits = mapping_fixture()
                human = rig['avatar']['m_Avatar']['m_Human']['data']
                if kind == 'tdof': human['m_HasTDoF'] = True
                elif kind == 'muscle_count': traits['muscles'].pop()
                elif kind == 'bone_count': traits['bones'].pop()
                elif kind == 'names': traits['bones'][1]['name'] = 'LeftUpperArm'
                elif kind == 'muscle_index': traits['bones'][0]['muscleIndices'][0] = 92
                else: human['m_Skeleton']['data']['m_AxesArray'][1]['m_Type'] = 2
                with self.assertRaises(ValueError):
                    probe.bone_mapping(rig, skeleton, traits)

    def test_nonfinite_or_singular_scale_rejected(self):
        for scale in (0., 1e-10, math.nan, math.inf, -math.inf):
            with self.subTest(scale=scale):
                rig, skeleton, traits = mapping_fixture()
                rig['avatar']['m_Avatar']['m_DefaultPose']['data']['m_X'][1]['s']['x'] = scale
                with self.assertRaises(ValueError):
                    probe.bone_mapping(rig, skeleton, traits)

    def test_candidate_uses_attribute42_offset_axis_order_and_signed_scale(self):
        rig, skeleton, traits = mapping_fixture()
        arm = probe.bone_mapping(rig, skeleton, traits)[0][0]
        seen = []
        def rotation(values, axes):
            seen.append(values)
            return [0., 0., math.sqrt(.5), math.sqrt(.5)]
        result = probe.candidate_matrix(arm, {a: a / 10. for a in range(7, 134)}, rotation)
        self.assertEqual(seen, [[8., 0., 7.8]])
        expected = [[0., -3., 0., 0.], [-2., 0., 0., 0.],
                    [0., 0., .5, 0.], [0., 0., 0., 1.]]
        for row, want in zip(result, expected):
            for actual, value in zip(row, want):
                self.assertAlmostEqual(actual, value, places=12)

    def test_pre_and_inverse_post_rotation_order(self):
        rig, skeleton, traits = mapping_fixture()
        arm = probe.bone_mapping(rig, skeleton, traits)[0][0]
        arm['scale'] = [1., 1., 1.]
        v = math.sqrt(.5)
        arm['axes']['m_PreQ'] = dict(x=v, y=0., z=0., w=v)
        arm['axes']['m_PostQ'] = dict(x=0., y=v, z=0., w=v)
        result = probe.candidate_matrix(arm, {a: 0. for a in range(7, 134)},
                                        lambda values, axes: [0., 0., 0., 1.])
        expected = [[0., 0., -1., 0.], [-1., 0., 0., 0.],
                    [0., 1., 0., 0.], [0., 0., 0., 1.]]
        for row, want in zip(result, expected):
            for actual, value in zip(row, want):
                self.assertAlmostEqual(actual, value, places=12)


def loop_source():
    return {'m_LoopBlend': True, 'm_LoopTime': True, 'm_Mirror': False,
            'm_CycleOffset': 0., 'm_StartTime': 0., 'm_StopTime': 2.,
            'm_DeltaPose': {'m_DoFArray': [i + .25 for i in range(52)],
                            'm_LeftHandPose': {'m_DoFArray': [-i - .5 for i in range(20)]},
                            'm_RightHandPose': {'m_DoFArray': [100. + i for i in range(20)]}}}


class LoopCorrectionTests(unittest.TestCase):
    def test_exact_body_left_right_partition_order_without_clamping(self):
        source = loop_source()
        before = copy.deepcopy(source)
        result = probe.loop_muscle_delta(source)
        self.assertEqual(result['duration'], 2.)
        self.assertEqual(result['delta'], [i + .25 for i in range(52)] +
                         [-i - .5 for i in range(20)] + [100. + i for i in range(20)])
        self.assertEqual(source, before)

    def test_loop_off_requires_no_unused_loop_payload(self):
        for blend, time in ((False, False), (False, True), (True, False)):
            self.assertIsNone(probe.loop_muscle_delta({'m_LoopBlend': blend, 'm_LoopTime': time}))

    def test_nonboolean_loop_flag_rejected(self):
        for flag in (0, 1, None, 'true'):
            with self.subTest(flag=flag), self.assertRaises(ValueError):
                probe.loop_muscle_delta({'m_LoopBlend': flag})

    def test_unqualified_loop_branches_rejected(self):
        for key, values in (('m_LoopTime', [1, None]),
                            ('m_Mirror', [True, 0, None]),
                            ('m_CycleOffset', [.25, -.25, math.nan, math.inf, '0']),
                            ('m_StartTime', [.25, -.25, math.nan, math.inf, '0'])):
            for value in values:
                with self.subTest(key=key, value=value):
                    source = loop_source(); source[key] = value
                    with self.assertRaises(ValueError): probe.loop_muscle_delta(source)

    def test_loop_duration_requires_positive_finite_number(self):
        for duration in (0., -1., math.nan, math.inf, -math.inf, True, '2', None):
            with self.subTest(duration=duration):
                source = loop_source(); source['m_StopTime'] = duration
                with self.assertRaises(ValueError): probe.loop_muscle_delta(source)

    def test_each_partition_has_its_own_exact_width(self):
        for index in range(3):
            for change in (-1, 1):
                with self.subTest(index=index, change=change):
                    source = loop_source(); pose = source['m_DeltaPose']
                    parts = [pose['m_DoFArray'], pose['m_LeftHandPose']['m_DoFArray'],
                             pose['m_RightHandPose']['m_DoFArray']]
                    if change == -1: parts[index].pop()
                    else: parts[index].append(0.)
                    with self.assertRaises(ValueError): probe.loop_muscle_delta(source)
        # Preserving total92 is insufficient if the body/hand boundary moves.
        source = loop_source(); pose = source['m_DeltaPose']
        pose['m_LeftHandPose']['m_DoFArray'].append(pose['m_DoFArray'].pop())
        with self.assertRaises(ValueError): probe.loop_muscle_delta(source)

    def test_nonfinite_or_nonnumeric_delta_rejected_in_every_partition(self):
        for index in range(3):
            for value in (math.nan, math.inf, -math.inf, True, '1', None):
                with self.subTest(index=index, value=value):
                    source = loop_source(); pose = source['m_DeltaPose']
                    parts = [pose['m_DoFArray'], pose['m_LeftHandPose']['m_DoFArray'],
                             pose['m_RightHandPose']['m_DoFArray']]
                    parts[index][0] = value
                    with self.assertRaises(ValueError): probe.loop_muscle_delta(source)

    def test_compare_corrects_only_muscles_at_absolute_bounded_phase(self):
        clip = channels()
        original = copy.deepcopy(clip)
        loop = probe.loop_muscle_delta(loop_source())
        matrix = [[float(r == c) for c in range(4)] for r in range(4)]
        times = [0., .5, 2., 3.]
        rows = [{'authoredTime': t, 'local': [[v for row in matrix for v in row]]}
                for t in times]
        mapping = [{'bone': 'spy', 'capture_index': 0}]
        observed = []
        def record(binding, scalar, rotation):
            observed.append(dict(scalar))
            return matrix
        with mock.patch.object(probe, 'candidate_matrix', side_effect=record):
            report = probe.compare(clip, rows, mapping, None, loop=loop)
        for time, actual in zip(times, observed):
            raw = probe.sample_channels(clip, time)
            phase = min(time / 2., 1.)
            self.assertEqual(set(actual), set(range(7, 134)))
            for attr in range(7, 42):
                self.assertEqual(actual[attr], raw[attr])
            for muscle in range(92):
                self.assertEqual(actual[42 + muscle], raw[42 + muscle] + phase * loop['delta'][muscle])
        self.assertLess(observed[-1][94], -1.)  # Left-hand negative value retained.
        self.assertGreater(observed[-1][133], 1.)  # Right-hand positive value retained.
        self.assertEqual(report['spy']['samples'], 4)
        self.assertEqual(report['spy']['max_local_basis_error'], 0.)
        self.assertEqual(clip, original)
        observed.clear()
        with mock.patch.object(probe, 'candidate_matrix', side_effect=record):
            probe.compare(clip, rows, mapping, None)
        self.assertEqual(observed, [probe.sample_channels(clip, t) for t in times])


class ProvenanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.capture = self.folder / 'capture.jsonl'
        self.capture.write_text('synthetic capture\n')
        self.asset = self.folder / 'source.assets'
        self.asset.write_bytes(b'synthetic source bytes')
        self.library = self.folder / 'rotation.so'
        self.library.write_bytes(b'synthetic library bytes; never loaded')
        self.launch_path = self.folder / 'capture.launch.json'
        self.validated = {
            'skeleton': {'avatar': 'FemaleAvatar', 'gender': 'female'},
            'header': {'sampleRate': 30},
            'clips': [{'name': 'Attack6', 'sourcePathId': 231, 'isLooping': False,
                       'length': 1.06666672, 'sourceFrameRate': 30.},
                      {'name': 'NormalIdle', 'sourcePathId': 684, 'isLooping': True,
                       'length': 2.00001526, 'sourceFrameRate': 30.}]}
        self.launch = {
            'schema': 'pe-animator-bake-launch-1', 'capture': str(self.capture),
            'plan': {'avatar': 'FemaleAvatar', 'gender': 'female', 'sample_rate': 30,
                     'asset_sha256': probe.sha(self.asset), 'clips': [
                         {'name': c['name'], 'path_id': c['sourcePathId'],
                          'loop': c['isLooping'], 'length': c['length'],
                          'frame_rate': c['sourceFrameRate']}
                         for c in self.validated['clips']]}}
        self.proof = {'schema': 'pe-unity5-human-rotation-1',
                      'binary_sha256': '1eb5ad6ff19dddb960a71453603ed7c67403403186c9dfeadc798a6021c4d2a6',
                      'comparison': {'bitwise_mismatches': 0, 'maximum_component_error': 0.,
                                     'native_rsqrt': True,
                                     'library_sha256': probe.sha(self.library),
                                     'c_source_sha256': probe.sha(
                                         Path(probe.__file__).with_name('human_rotation_math.c'))}}

    def validate_launch(self):
        self.launch_path.write_text(json.dumps(self.launch))
        return probe.validate_launch(self.launch_path, self.capture, self.validated, self.asset)

    def test_matching_launch_provenance_passes_without_mutation(self):
        before = copy.deepcopy(self.validated)
        self.assertIsNone(self.validate_launch())
        self.assertEqual(self.validated, before)

    def test_launch_schema_capture_identity_and_source_hash_rejected(self):
        for key, value in (('schema', 'another-schema'), ('capture', str(self.asset))):
            with self.subTest(key=key):
                before = copy.deepcopy(self.launch)
                self.launch[key] = value
                with self.assertRaises(ValueError): self.validate_launch()
                self.launch = before
        self.asset.write_bytes(b'source changed after launch')
        with self.assertRaises(ValueError): self.validate_launch()

    def test_launch_avatar_gender_rate_and_clip_closure_rejected(self):
        for key, value in (('avatar', 'MaleAvatar'), ('gender', 'male'),
                           ('sample_rate', 60), ('asset_sha256', '0' * 64),
                           ('clips', self.launch['plan']['clips'][:1])):
            with self.subTest(key=key):
                before = copy.deepcopy(self.launch)
                self.launch['plan'][key] = value
                with self.assertRaises(ValueError): self.validate_launch()
                self.launch = before

    def test_launch_clip_name_id_loop_length_rate_and_order_rejected(self):
        for key, value in (('name', 'Attack7'), ('path_id', 240), ('loop', True),
                           ('length', 1.08), ('frame_rate', 24.)):
            with self.subTest(key=key):
                before = copy.deepcopy(self.launch)
                self.launch['plan']['clips'][0][key] = value
                with self.assertRaises(ValueError): self.validate_launch()
                self.launch = before
        self.launch['plan']['clips'].reverse()
        with self.assertRaises(ValueError): self.validate_launch()

    def test_launch_and_bake_clip_clocks_require_positive_finite_numbers(self):
        for side in ('launch', 'bake'):
            for key in ('length', 'frame_rate' if side == 'launch' else 'sourceFrameRate'):
                for value in (math.nan, math.inf, -math.inf, 0., -.25,
                              True, False, '30', None, [30]):
                    with self.subTest(side=side, key=key, value=value):
                        target = (self.launch['plan']['clips'][0] if side == 'launch'
                                  else self.validated['clips'][0])
                        original = target[key]
                        target[key] = value
                        with self.assertRaisesRegex(ValueError, 'Invalid launch/bake clip clock'):
                            self.validate_launch()
                        target[key] = original

    def test_matching_rotation_proof_is_bound_to_actual_helper_bytes(self):
        self.assertIsNone(probe.validate_rotation_proof(self.proof, self.library))
        self.library.write_bytes(b'different helper with unchanged report')
        with self.assertRaises(ValueError):
            probe.validate_rotation_proof(self.proof, self.library)

    def test_rotation_proof_rejects_wrong_schema_original_binary_and_hashes(self):
        for key, value in (('schema', 'other'), ('binary_sha256', '0' * 64)):
            with self.subTest(key=key):
                proof = copy.deepcopy(self.proof); proof[key] = value
                with self.assertRaises(ValueError):
                    probe.validate_rotation_proof(proof, self.library)
        for key in ('library_sha256', 'c_source_sha256'):
            with self.subTest(key=key):
                proof = copy.deepcopy(self.proof); proof['comparison'][key] = '0' * 64
                with self.assertRaises(ValueError):
                    probe.validate_rotation_proof(proof, self.library)

    def test_rotation_proof_requires_exact_arithmetic_and_native_rsqrt(self):
        for key, value in (('bitwise_mismatches', 1), ('maximum_component_error', 1e-12),
                           ('maximum_component_error', math.nan),
                           ('native_rsqrt', False), ('native_rsqrt', 1)):
            with self.subTest(key=key, value=value):
                proof = copy.deepcopy(self.proof); proof['comparison'][key] = value
                with self.assertRaises(ValueError):
                    probe.validate_rotation_proof(proof, self.library)

    def main_args(self, output):
        result = ['probe_humanoid_rotation.py']
        for name, path in (('rig', self.asset), ('traits', self.asset),
                           ('capture', self.capture), ('library', self.library),
                           ('game', self.folder), ('output', output),
                           ('rotation-proof', self.asset)):
            result.extend(['--' + name, str(path)])
        return result

    def test_existing_output_rejected_before_comparison_or_overwrite(self):
        original = self.capture.read_bytes()
        with mock.patch.object(sys, 'argv', self.main_args(self.capture)), \
                mock.patch.object(probe, 'run') as run:
            with self.assertRaisesRegex(ValueError, 'fresh output'):
                probe.main()
            run.assert_not_called()
        self.assertEqual(self.capture.read_bytes(), original)

    def test_output_created_during_comparison_is_never_overwritten(self):
        output = self.folder / 'new-report.json'
        def concurrent_creation(args):
            output.write_bytes(b'other writer evidence')
            return {'samples': 1}
        with mock.patch.object(sys, 'argv', self.main_args(output)), \
                mock.patch.object(probe, 'run', side_effect=concurrent_creation):
            with self.assertRaises(FileExistsError):
                probe.main()
        self.assertEqual(output.read_bytes(), b'other writer evidence')


if __name__ == '__main__':
    unittest.main()
