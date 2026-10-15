#!/usr/bin/env python3
"""Synthetic observations only; no original-game launch or parity claim."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import analyze_reference_gather as tool
from test_reference_pose_analysis import fixture


def rows():
    base, _ = fixture()
    header = base[0]
    header.update(kind='observed-original-gather-pose', callbackPhase='synchronous-event-dispatch', probeSourceSha256='a'*64)
    handlers = [dict(targetType='Probe', declaringType='Probe', method='Observe', observer=True),
                dict(targetType='Pathea.Action_Gather', declaringType='Pathea.Action_Gather', method='OnAnimEvent', observer=False)]
    def state(running):
        return dict(gatherMask=running, actions=[dict(id=i, running=running if i == 32 else False,
                    waitingRelation=False if i == 32 and running else None,
                    doActionPending=False if i == 32 and running else None) for i in sorted(tool.ACTIONS)])
    first = base[1]; first['motion'] = state(True)
    first['animators'][0]['layers'][0]['name'] = 'FullBody'
    second = base[2]; second['motion'] = state(False)
    def event(seq, kind, name, action, frame, time, real, running):
        return dict(type=kind, sequence=seq, frame=frame, time=time, realtime=real, delta=.1,
                    event=name, actionId=action, handlers=copy.deepcopy(handlers), motion=state(running),
                    animator=copy.deepcopy(first['animators'][0]))
    return [header, dict(type='subscription', frame=100, handlers=handlers),
            event(1, 'action-admission', None, 32, 100, 3., 4.9, False),
            event(2, 'animation-event', 'Gather', -1, 100, 3., 4.95, True), first,
            event(3, 'animation-event', 'GatherEnd', -1, 101, 4., 5.9, True),
            event(4, 'action-end-notification', None, 32, 101, 4., 5.95, False), second,
            dict(type='end', reason='stop-file', frames=2, eventRecords=4)]


class GatherAnalysisTests(unittest.TestCase):
    def analyze(self, records, source_hash="a"*64):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'gather.jsonl'
            path.write_text(''.join(json.dumps(r)+'\n' for r in records))
            path.with_suffix('.launch.json').write_text(json.dumps(dict(schema='pe-gather-launch-1',
                capture=str(path), probe_source_sha256=source_hash, frames=10)))
            return tool.analyze(path)

    def test_admission_is_not_execution_or_harvest(self):
        report = self.analyze(rows())
        self.assertEqual(report['frames'], 2)
        self.assertEqual(report['eventRecords'], 4)
        cycle = report['cycles'][0]
        self.assertFalse(cycle['edges'][0]['motion']['gatherMask'])
        self.assertTrue(cycle['firstExecutingSample']['gatherMask'])
        self.assertEqual(cycle['edges'][1]['handlerPhase'], 'before-Gather-handler')
        self.assertNotIn('harvestCommit', cycle)

    def test_missing_handler_and_late_observer_are_distinct(self):
        records = rows()
        records[2]['handlers'].reverse()
        records[3]['handlers'] = records[3]['handlers'][:1]
        report = self.analyze(records)
        self.assertEqual(report['cycles'][0]['edges'][0]['handlerPhase'], 'after-Gather-handler')
        self.assertEqual(report['cycles'][0]['edges'][1]['handlerPhase'], 'Gather-handler-absent')

    def test_bad_closure_counts_order_and_hierarchy_fail(self):
        cases = []
        r = rows(); r.pop(); cases.append(r)
        r = rows(); r[-1]['eventRecords'] = 5; cases.append(r)
        r = rows(); r[-1]['reason'] = 'event-error'; cases.append(r)
        r = rows(); r[3]['sequence'] = 9; cases.append(r)
        r = rows(); r[3]['realtime'] = 4.; cases.append(r)
        r = rows(); r[4]['transforms'][1]['world'][3] += 2; cases.append(r)
        r = rows(); r[3]['handlers'].append(copy.deepcopy(r[3]['handlers'][0])); cases.append(r)
        r = rows(); r.append(copy.deepcopy(r[-1])); cases.append(r)
        for index, records in enumerate(cases):
            with self.subTest(case=index), self.assertRaises(ValueError):
                self.analyze(records)

    def test_source_digest_must_match_launch_manifest(self):
        with self.assertRaisesRegex(ValueError, 'differs from launch manifest'):
            self.analyze(rows(), source_hash='b'*64)

    def test_partial_cycle_remains_explicit(self):
        records = rows()
        records[5]['event'] = 'Other'
        records[6]['actionId'] = 1
        report = self.analyze(records)
        self.assertIsNone(report['cycles'][0]['endNotificationFrame'])


if __name__ == '__main__':
    unittest.main()
