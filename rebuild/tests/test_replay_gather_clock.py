#!/usr/bin/env python3
"""Synthetic Gather clock replay checks; no original-game launch or parity claim."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import replay_gather_clock as tool

ASSET = 'a' * 64
ASSEMBLY = 'b' * 64
LENGTHS = {'HerbDown': .5333347320556641, 'Herb': .8000011444091797, 'HerbUp': .6333332061767578}
MOVES = [(.5312511920928955, .4687487781047821), (.6874997019767761, .3125002980232239), (.6052631139755249, .3947369158267975)]


def identity(path, name, full, motion):
    return dict(path=path, name_hash=name, full_path_hash=full, speed=1.0, cycle_offset=0.0,
                speed_parameter=None, cycle_offset_parameter=None, mirror=False, motion=motion)


def plan():
    names = list(LENGTHS)
    states = [identity('FullBody.Gather.' + n, h, f, True) for n, h, f in
              zip(names, (2118454669, 2404236549, 515342059), (3303680790, 594958745, 1680093607))]
    paths = [s['path'] for s in states]
    moves = [dict(source=a, destination=b, exit_time=x, has_exit_time=True, duration=d, fixed_duration=False, offset=0.0)
             for a, b, (x, d) in zip(paths, paths[1:] + ['FullBody.DoNothing'], MOVES)]
    clips = [dict(name=n, path_id=i, length=LENGTHS[n], controller_state=dict(path='FullBody.Gather.' + n,
                  transitions=[dict(m_ExitTime=x, m_TransitionDuration=d)]))
             for (n, i), (x, d) in zip(tool.GATHER_CLIPS.items(), MOVES)]
    return dict(schema=1, asset_sha256=ASSET, genders=dict(female=dict(clips=clips)), gather_controller=dict(
        layer_index=1, layer='FullBody', idle=identity('FullBody.DoNothing', 474258567, 1458450275, False),
        entry=dict(trigger='Gather', selector_full_path_hash=3653407461, exit_time=.9, has_exit_time=False,
                   duration=.10000000149011612, fixed_duration=False, offset=0.0),
        states=states, transitions=moves))


def reaction():
    return dict(installed_assets={'sharedassets1.assets': ASSET}, assemblies={'Assembly-CSharp.dll': dict(sha256=ASSEMBLY)},
                full_body=dict(idle='FullBody.DoNothing', idle_behaviours=copy.deepcopy(tool.IDLE_BEHAVIOURS)))


def layer(expected):
    (cn, cf, ct), (nn, nf, nt), transition = expected
    return dict(current=dict(shortNameHash=cn, fullPathHash=cf, normalizedTime=ct),
                next=dict(shortNameHash=nn, fullPathHash=nf, normalizedTime=nt), inTransition=transition)


class ReplayTests(unittest.TestCase):
    def setUp(self):
        self.control = plan()['gather_controller']
        self.line = tool.timeline(self.control, list(LENGTHS.values()))

    def synthetic(self, delta=.065, phase=97.7):
        """Frames advanced by a fixed delta, observed values taken from the model."""
        frames = {}; t = 0.0; frame = 100; events = []; seen = set()
        names = dict(herb_exit='Gather', herbup_exit='GatherEnd', idle_enter='OnEndFullAnim')
        marks = dict(herb_exit=self.line['completes'][1], herbup_exit=self.line['completes'][2], idle_enter=self.line['exit_start'])
        while True:
            frames[frame] = (delta, layer(tool.predict(self.control, self.line, t, phase)))
            for key, value in marks.items():
                if key not in seen and t >= value:
                    seen.add(key); events.append(dict(type='animation-event', frame=frame, event=names[key]))
            if t >= self.line['completes'][2]:
                break
            frame += 1; t += delta
        cycle = dict(firstExecutingSample=dict(frame=100), endNotificationFrame=frame + 1, edges=events)
        frames[frame + 1] = (delta, layer(tool.predict(self.control, self.line, t + delta, phase)))
        return cycle, frames

    def test_source_timeline_is_state_length_times_normalized_values(self):
        L = list(LENGTHS.values())
        self.assertAlmostEqual(self.line['starts'][1], MOVES[0][0] * L[0], places=12)
        self.assertAlmostEqual(self.line['completes'][1], MOVES[0][0] * L[0] + MOVES[1][0] * L[1] + MOVES[1][1] * L[1], places=12)
        self.assertAlmostEqual(self.line['completes'][2] - self.line['exit_start'], MOVES[2][1] * L[2], places=12)
        self.assertAlmostEqual(self.line['entry_end'], .10000000149011612, places=12)

    def test_threshold_crossing_is_inclusive_and_switches_state(self):
        herb = tool.signed(2404236549); up = 515342059
        exit_ = self.line['completes'][1]
        before = tool.predict(self.control, self.line, exit_ - 1e-9, 97.)
        at = tool.predict(self.control, self.line, exit_, 97.)
        self.assertEqual((before[0][0], before[1][0], before[2]), (herb, up, True))
        self.assertEqual((at[0][0], at[1][0], at[2]), (up, 0, False))
        entry = tool.predict(self.control, self.line, 0.0, 97.25)
        self.assertEqual((entry[0][0], entry[0][2], entry[1][0], entry[1][2]), (474258567, 97.25, 2118454669, 0.0))

    def test_model_consistent_trace_has_no_mismatch(self):
        cycle, frames = self.synthetic()
        result = tool.replay_cycle(self.control, self.line, cycle, frames)
        self.assertEqual(result['mismatches'], [])
        self.assertEqual(result['receipts']['Gather']['predicted'], result['receipts']['Gather']['recorded'])

    def test_state_time_and_tolerance_mismatches_are_reported(self):
        cycle, frames = self.synthetic()
        delta, row = frames[103]
        within = copy.deepcopy(row); within['current']['normalizedTime'] += 5e-6
        frames[103] = (delta, within)
        self.assertEqual(tool.replay_cycle(self.control, self.line, cycle, frames)['mismatches'], [])
        outside = copy.deepcopy(row); outside['current']['normalizedTime'] += 2e-5; outside['inTransition'] = not row['inTransition']
        frames[103] = (delta, outside)
        fields = {m['field'] for m in tool.replay_cycle(self.control, self.line, cycle, frames)['mismatches']}
        self.assertEqual(fields, {'current', 'inTransition'})

    def test_missing_shifted_and_extra_receipts_fail(self):
        cycle, frames = self.synthetic()
        for change in ('missing', 'shifted', 'extra'):
            bad = copy.deepcopy(cycle); gather = next(e for e in bad['edges'] if e['event'] == 'Gather')
            if change == 'missing': bad['edges'].remove(gather)
            elif change == 'shifted': gather['frame'] += 1
            else: bad['edges'].append(dict(gather))
            mismatches = tool.replay_cycle(self.control, self.line, bad, frames)['mismatches']
            with self.subTest(change=change):
                self.assertTrue(any(isinstance(m, dict) and m.get('event') == 'Gather' for m in mismatches))

    def test_incomplete_or_gapped_cycles_fail(self):
        cycle, frames = self.synthetic()
        gapped = dict(frames); del gapped[104]
        self.assertTrue(tool.replay_cycle(self.control, self.line, cycle, gapped)['mismatches'])
        open_cycle = dict(cycle, endNotificationFrame=None)
        self.assertTrue(tool.replay_cycle(self.control, self.line, open_cycle, frames)['mismatches'])
        short = dict(cycle, endNotificationFrame=105)
        self.assertIn('Cycle ended before every Gather threshold was crossed',
                      tool.replay_cycle(self.control, self.line, short, frames)['mismatches'])

    def write(self, directory, name, value):
        path = Path(directory) / name
        path.write_text(value if isinstance(value, str) else json.dumps(value))
        return path

    def test_malformed_or_clock_changing_plan_metadata_fails(self):
        def edit(f):
            p = plan(); f(p); return p
        bad = {
            'asset': edit(lambda p: p.update(asset_sha256='c' * 64)),
            'missing': edit(lambda p: p.pop('gather_controller')),
            'cycle offset': edit(lambda p: p['gather_controller']['states'][1].update(cycle_offset=.5)),
            'speed parameter': edit(lambda p: p['gather_controller']['idle'].update(speed_parameter=12)),
            'bool speed': edit(lambda p: p['gather_controller']['states'][0].update(speed=True)),
            'int offset': edit(lambda p: p['gather_controller']['entry'].update(offset=0)),
            'clip id': edit(lambda p: p['genders']['female']['clips'][1].update(path_id=231)),
            'evidence': edit(lambda p: p['genders']['female']['clips'][0]['controller_state']['transitions'][0].update(m_ExitTime=.9)),
            'fixed': edit(lambda p: p['gather_controller']['transitions'][2].update(fixed_duration=True)),
        }
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(tool.load_plan(self.write(directory, 'ok.json', plan()), ASSET)[1], list(LENGTHS.values()))
            for name, value in bad.items():
                with self.subTest(name=name), self.assertRaises(ValueError):
                    tool.load_plan(self.write(directory, name + '.json', value), ASSET)
            with self.assertRaises(ValueError):
                tool.load_plan(self.write(directory, 'dup.json', '{"schema":1,"schema":1}'), ASSET)
            with self.assertRaises(ValueError):
                tool.load_plan(self.write(directory, 'nan.json', '{"schema":NaN}'), ASSET)
            with mock.patch.object(tool.pose, 'MAX_LINE', 64), self.assertRaises(ValueError):
                tool.load_plan(self.write(directory, 'big.json', plan()), ASSET)

    def test_overlapping_transitions_are_outside_model(self):
        control = plan()['gather_controller']; control['entry']['duration'] = .5
        with self.assertRaises(ValueError):
            tool.timeline(control, list(LENGTHS.values()))

    def test_tampered_provenance_fails_before_reading_capture(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = Path(directory) / 'gather.jsonl'
            launch = dict(asset_sha256=ASSET, game_assembly_sha256=ASSEMBLY)
            plan_path = self.write(directory, 'plan.json', plan())
            for name, change in (('asset', lambda r: r['installed_assets'].update({'sharedassets1.assets': 'c' * 64})),
                                 ('assembly', lambda r: r['assemblies']['Assembly-CSharp.dll'].update(sha256='c' * 64)),
                                 ('idle', lambda r: r['full_body']['idle_behaviours'][1].update(fadeout_speed=4.0))):
                self.write(directory, 'gather.launch.json', launch)
                value = reaction(); change(value)
                path = self.write(directory, name + '-reaction.json', value)
                with self.subTest(name=name), mock.patch.object(tool, 'recorded') as read, self.assertRaises(ValueError):
                    tool.replay(capture, plan_path, path)
                read.assert_not_called()
            self.write(directory, 'gather.launch.json', dict(asset_sha256=ASSET))
            with mock.patch.object(tool, 'recorded') as read, self.assertRaises(ValueError):
                tool.replay(capture, plan_path, self.write(directory, 'r.json', reaction()))
            read.assert_not_called()

    def test_recorded_selects_one_qualified_player_animator(self):
        def frame(animators): return dict(type='frame', frame=1, delta=.1, animators=animators)
        def animator(identity, controller='FemaleAnimator2', avatar='FemaleAvatar'):
            return dict(id=identity, controller=controller, avatar=avatar,
                        layers=[dict(name='FullBody', current={}, next={}, inTransition=False)])
        cases = {'ok': [animator(5), animator(6, 'MenuAnimator')], 'ambiguous': [animator(5), animator(6)],
                 'missing': [animator(5, avatar='MaleAvatar')]}
        with tempfile.TemporaryDirectory() as directory:
            for name, animators in cases.items():
                path = self.write(directory, name + '.jsonl', json.dumps(frame(animators)) + '\n')
                with self.subTest(name=name), mock.patch.object(tool.observation, 'analyze', return_value={}):
                    if name == 'ok':
                        self.assertEqual(list(tool.recorded(path)[1]), [1])
                    else:
                        with self.assertRaises(ValueError):
                            tool.recorded(path)
            changed = '\n'.join(json.dumps(dict(frame([animator(i)]), frame=i)) for i in (1, 2)) + '\n'
            with mock.patch.object(tool.observation, 'analyze', return_value={}), self.assertRaises(ValueError):
                tool.recorded(self.write(directory, 'changed.jsonl', changed))


if __name__ == '__main__':
    unittest.main()
