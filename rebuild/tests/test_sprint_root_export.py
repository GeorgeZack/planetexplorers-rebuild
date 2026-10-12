"""Synthetic exporter guards; fixtures do not establish engine playback."""
import copy
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import export_sprint_root_motion as tool
from test_player_root_channels import fixture as root_fixture


def fixture():
    clip, avatar = root_fixture()
    clip['m_Name'] = tool.NAME
    avatar['m_Name'] = 'FemaleAvatar'
    avatar['m_Avatar']['m_Human']['data']['m_Scale'] = tool.SCALE
    muscle = clip['m_MuscleClip']
    muscle['m_StopTime'] = tool.DURATION
    dense = muscle['m_Clip']['data']['m_DenseClip']
    dense['m_FrameCount'] = tool.COUNT
    dense['m_SampleArray'] += dense['m_SampleArray'][-127:] * (tool.COUNT-31)
    clip['m_Events'] = [dict(functionName='AnimEvent', data='EndAction', time=tool.END_ACTION)]
    return clip, avatar


class SprintRootTests(unittest.TestCase):
    def test_exact_source_record_and_no_mutation(self):
        curve = tool.evidence.decode(*fixture())
        before = copy.deepcopy(curve)
        data = tool.encode(curve)
        self.assertEqual(curve, before)
        self.assertEqual(len(data), 56+16*tool.COUNT)
        self.assertEqual(struct.unpack_from('<4sIIIdIIddd', data),
                         (b'PESR', 1, 777, 1, tool.SCALE, 401, tool.COUNT, tool.DURATION, 0., 30.))
        self.assertEqual(struct.unpack_from('<dd', data, 56), (10., -20.))

    def test_changed_profile_or_bad_knots_rejected(self):
        for key, value in [('count', 55), ('duration', 1.8), ('begin', .01),
                           ('rate', 60), ('scale', 1), ('name', 'RunSwordAttack')]:
            curve = tool.evidence.decode(*fixture()); curve[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): tool.encode(curve)
        for value in [True, float('nan'), float('inf'), 10001]:
            curve = tool.evidence.decode(*fixture()); curve['roots'][7][0] = value
            with self.assertRaises(ValueError): tool.encode(curve)
        curve = tool.evidence.decode(*fixture()); curve['roots'][9].pop()
        with self.assertRaises(ValueError): tool.encode(curve)

    def test_qualified_case_is_replayed_not_borrowed(self):
        clip, avatar = fixture()
        q = [clip['m_MuscleClip']['m_StartX']['q'][k] for k in 'xyzw']
        proof = dict(cases=[dict(start_q=q)])
        with patch.object(tool.evidence, 'replay_arithmetic', return_value=proof) as replay:
            _, report = tool.qualify(clip, avatar, Path('/pinned-elf'))
        replay.assert_called_once_with(Path('/pinned-elf'), [q])
        self.assertEqual(report['isolated_arithmetic'], proof)
        self.assertEqual(report['candidate_at_clip_end'][1], 0)

    def test_wrong_identity_event_or_replay_rejected(self):
        for mutation in ['name', 'avatar', 'event', 'duplicate', 'missing', 'flag']:
            clip, avatar = fixture()
            if mutation == 'name': clip['m_Name'] = 'RunSwordAttack'
            if mutation == 'avatar': avatar['m_Name'] = 'MaleAvatar'
            if mutation == 'event': clip['m_Events'][0]['time'] += .01
            if mutation == 'duplicate': clip['m_Events'] *= 2
            if mutation == 'missing': clip['m_Events'] = []
            if mutation == 'flag': clip['m_MuscleClip']['m_Mirror'] = True
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                tool.qualify(clip, avatar, Path('/missing-elf'))
        for cases in [[], [dict(start_q=[0,1,0,0])]]:
            with patch.object(tool.evidence, 'replay_arithmetic', return_value=dict(cases=cases)):
                with self.assertRaises(ValueError): tool.qualify(*fixture(), Path('/missing-elf'))

    def test_incomplete_base_proof_fails_before_assets(self):
        for base in [{}, dict(ordinary_source_extraction_qualified=True),
                     dict(ordinary_source_extraction_qualified=1, running_oracle_qualified=True)]:
            with patch.object(tool.evidence, 'verify', return_value=base):
                with self.assertRaises(ValueError): tool.build_outputs(Path('/missing'), Path('/missing'))


if __name__ == '__main__': unittest.main()
