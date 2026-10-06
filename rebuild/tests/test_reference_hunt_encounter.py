"""Offline malformed-capture/identity tests; no original process is launched."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from analyze_reference_hunt_encounter import analyze


def entity(identity, prototype=13, kind=3):
    return dict(id=identity, prototype=prototype, prototypeType=kind, position=[-340, 20, 230], rotation=[0, 0, 0, 1], scale=[1, 1, 1],
                hp=212, maxHp=212, attack=64, defense=29, campId=45, damageId=45, defaultPlayerId=4)


def capture():
    head = dict(type='header', schema=1, kind='observed-original-hunt-agents', mission=9126, target=1518,
                phase='WaitForEndOfFrame', probeSourceSha256='a'*64)
    frame = dict(type='frame', sample=0, frame=100, time=10., realtime=15., missionActive=True, missionPointPresent=True,
                 counter='13_0', player=entity(20000, 0, 0), agents=[dict(sourceOrder=i, agentId=i+1, prototype=13, step='Created',
                 agentPosition=[-340+i, 20, 230], agentScale=[1, 1, 1], agentRotation=[0, 0, 0, 1], canRide=False,
                 entity=entity(30000+i)) for i in range(6)])
    second = copy.deepcopy(frame);second.update(sample=1, frame=101)
    return [head, frame, second, dict(type='end', reason='time-limit', frames=2)]


class EncounterTests(unittest.TestCase):
    def read(self, rows):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'capture.jsonl'
            path.write_text(''.join(json.dumps(row)+'\n' for row in rows))
            return analyze(path)

    def test_complete_membership_preserves_anchor_and_live_pose(self):
        rows = capture();rows[1]['agents'][0]['entity']['position'][0] -= 4
        result = self.read(rows)
        first = result['first_complete_six_alive_frame']
        self.assertEqual(first['agents'][0]['agentPosition'][0], -340)
        self.assertEqual(first['agents'][0]['entity']['position'][0], -344)
        self.assertIsNone(result['center'])
        self.assertEqual(len(result['observed_entity_ids']), 6)

    def test_missing_or_uncreated_agents_never_invented(self):
        for change in ('missing', 'uncreated', 'dead', 'wrong_prototype', 'inactive'):
            rows = capture()
            for frame in rows[1:3]:
                if change == 'missing':frame['agents'].pop()
                if change == 'uncreated':frame['agents'][0]['entity'] = None
                if change == 'dead':frame['agents'][0]['entity']['hp'] = 0
                if change == 'wrong_prototype':frame['agents'][0]['entity']['prototype'] = 14
                if change == 'inactive':frame['missionActive'] = False
            self.assertIsNone(self.read(rows)['first_complete_six_alive_frame'], change)

    def test_replaced_cohort_not_treated_as_stable(self):
        rows = capture();rows[2]['agents'][0]['entity']['id'] += 100
        self.assertIsNone(self.read(rows)['first_complete_six_alive_frame'])

    def test_duplicate_agent_or_entity_rejected(self):
        for key in ('agentId', 'entity'):
            rows = capture();rows[1]['agents'][1][key] = copy.deepcopy(rows[1]['agents'][0][key])
            with self.assertRaises(ValueError):self.read(rows)

    def test_nonfinite_or_invalid_geometry_rejected(self):
        for key, value in [('hp', float('nan')), ('scale', [1, 0, 1]), ('position', [0, 0])]:
            rows = capture();rows[1]['agents'][0]['entity'][key] = value
            with self.assertRaises(ValueError):self.read(rows)

    def test_incomplete_error_and_miscounted_capture_rejected(self):
        rows = capture()
        with self.assertRaises(ValueError):self.read(rows[:-1])
        rows[-1]['reason'] = 'error'
        with self.assertRaises(ValueError):self.read(rows)
        rows = capture();rows[-1]['frames'] = 99
        with self.assertRaises(ValueError):self.read(rows)

    def test_wrong_contract_and_frame_order_rejected(self):
        rows = capture();rows[0]['mission'] = 9075
        with self.assertRaises(ValueError):self.read(rows)
        rows = capture();rows[2]['frame'] = 99
        with self.assertRaises(ValueError):self.read(rows)


if __name__ == '__main__':
    unittest.main()
