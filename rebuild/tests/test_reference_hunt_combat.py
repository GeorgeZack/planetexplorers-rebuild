"""Offline validation fixtures; no original client launch or gameplay mutation."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from analyze_reference_hunt_combat import analyze
from test_reference_hunt_encounter import capture


def fixture():
    rows = capture();rows[0]['kind'] = 'observed-original-hunt-combat'
    sensor = dict(kind='vision', id=77, path='hare/Head', enabled=True, active=True, radius=20,
                  position=[1, 2, 3], rotation=[0, 0, 0, 1], localPosition=[0, 1, 0], lossyScale=[1, 1, 1],
                  blockLayer=42, componentFrame=20, angle=60, axis=[-.5, 1, 0], worldAxis=[-.5, 1, 0], entities=[20000])
    combat = dict(hasView=True, center=[0, 1, 0], maxRadius=.5, target=dict(componentFrame=15,
                  canAttackFlag=True, canSearchFlag=True, rawSelectedEntity=20000, selectedEntity=20000,
                  escapeEntity=None, sensors=[sensor]), skills=[], animators=[])
    skill = dict(instance=1, skill=30100003, guide=1, active=True, execExtEnable=False,
                 startTime=9, coolStat=3, targetEntity=20000, hitCount=0)
    for frame in rows[1:-1]:
        frame['time'] += frame['sample']*.1
        for e in [frame['player']] + [a['entity'] for a in frame['agents']]:
            e['combat'] = copy.deepcopy(combat)
        frame['agents'][0]['entity']['combat']['skills'] = [copy.deepcopy(skill)]
    rows[2]['agents'][0]['entity']['combat']['skills'][0]['execExtEnable'] = True
    return rows


class CombatTests(unittest.TestCase):
    def read(self, rows):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d)/'capture.jsonl';path.write_text(''.join(json.dumps(r)+'\n' for r in rows))
            return analyze(path)

    def test_contact_transition_and_sensor_observation(self):
        result=self.read(fixture())
        self.assertEqual(result['melee_instances'], 1)
        self.assertEqual(result['melee_contact_enabled_instances'], 1)
        self.assertEqual(len(result['skills'][0]['transitions']), 2)
        self.assertEqual(result['sensors'][0]['first_observation']['axis'], [-.5, 1, 0])

    def test_empty_skills_are_not_imagined(self):
        rows=fixture()
        for frame in rows[1:-1]:frame['agents'][0]['entity']['combat']['skills']=[]
        self.assertEqual(self.read(rows)['melee_instances'], 0)

    def test_changed_guide_is_same_instance(self):
        rows=fixture();rows[2]['agents'][0]['entity']['combat']['skills'][0]['guide']=2
        result=self.read(rows)
        self.assertEqual(len(result['skills']), 1)
        self.assertEqual(result['skills'][0]['transitions'][-1]['state']['guide'], 2)

    def test_reused_identity_rejected(self):
        rows=fixture();rows[2]['agents'][1]['entity']['combat']['skills']=copy.deepcopy(rows[2]['agents'][0]['entity']['combat']['skills'])
        with self.assertRaises(ValueError):self.read(rows)

    def test_missing_or_error_closure_rejected(self):
        rows=fixture()
        with self.assertRaises(ValueError):self.read(rows[:-1])
        rows[-1]['reason']='error'
        with self.assertRaises(ValueError):self.read(rows)

    def test_bad_sensor_and_skill_values_rejected(self):
        for key,value in [('radius',float('nan')),('axis',[0,1]),('enabled',1)]:
            rows=fixture();rows[1]['agents'][0]['entity']['combat']['target']['sensors'][0][key]=value
            with self.assertRaises(ValueError):self.read(rows)
        rows=fixture();rows[1]['agents'][0]['entity']['combat']['skills'][0]['execExtEnable']=1
        with self.assertRaises(ValueError):self.read(rows)

    def test_frame_time_may_pause_but_not_reverse(self):
        rows=fixture();rows[2]['time']=rows[1]['time'];self.read(rows)
        rows[2]['time']-=1
        with self.assertRaises(ValueError):self.read(rows)

    def test_sensor_settings_change_retained(self):
        rows=fixture();rows[2]['agents'][0]['entity']['combat']['target']['sensors'][0]['radius']=25
        self.assertEqual(len(self.read(rows)['sensors'][0]['settings_variants']),2)


if __name__ == '__main__':unittest.main()
