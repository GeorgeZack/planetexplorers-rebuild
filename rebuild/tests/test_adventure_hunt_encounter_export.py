"""Verify pinned observation export; no runtime, display, save or spawn calls."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from analyze_reference_hunt_encounter import analyze
from export_adventure_hunt_encounter import DEFAULT_CAPTURE, archive_report, generated, validate


class ExportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.evidence = json.loads(DEFAULT_CAPTURE.read_text())

    def test_real_capture_exports_anchor_not_moved_pose(self):
        actors = validate(self.evidence)
        first = actors[0]
        self.assertNotEqual(first['agentPosition'], first['entity']['position'])
        output = generated(self.evidence, DEFAULT_CAPTURE)
        header = output['pe_adventure_hunt_encounter_source.h'].decode()
        self.assertIn('{-351.1564f,19.6848335f,222.139832f}', header)
        self.assertNotIn('-333.2877f', header)
        self.assertIn('2146483648u', header)
        self.assertEqual(output, generated(self.evidence, DEFAULT_CAPTURE))

    def test_missing_replaced_or_duplicate_actor_rejected(self):
        for mutation in ('missing', 'replaced', 'duplicate', 'wrong_prototype', 'wrong_enum'):
            evidence = copy.deepcopy(self.evidence)
            first, second = evidence['observed_frames']
            if mutation == 'missing':first['agents'].pop()
            if mutation == 'replaced':second['agents'][0]['entity']['id'] += 999
            if mutation == 'duplicate':first['agents'][1]['entity']['id'] = first['agents'][0]['entity']['id']
            if mutation == 'wrong_prototype':first['agents'][0]['prototype'] = 14
            if mutation == 'wrong_enum':first['agents'][0]['entity']['prototypeType'] = 2
            with self.assertRaises(ValueError, msg=mutation):validate(evidence)

    def test_outside_route_and_nonfinite_anchor_rejected(self):
        for position in ([-224,20,230],[-641,20,230],[-340,64,230],[-340,20,63],[float('nan'),20,230]):
            evidence = copy.deepcopy(self.evidence)
            for frame in evidence['observed_frames']:frame['agents'][0]['agentPosition'] = position
            with self.assertRaises(ValueError):validate(evidence)

    def test_scale_health_or_faction_changes_rejected(self):
        for key, value in [('scale',[1.4,1.4,1.4]),('scale',[1,.9,1]),('hp',0),('maxHp',1000),('attack',999),('campId',26)]:
            evidence = copy.deepcopy(self.evidence)
            evidence['observed_frames'][0]['agents'][0]['entity'][key] = value
            with self.assertRaises(ValueError):validate(evidence)

    def test_counter_center_source_or_incomplete_capture_rejected(self):
        for mutation in ('counter', 'center', 'source', 'end', 'anchor_changed'):
            evidence = copy.deepcopy(self.evidence)
            if mutation == 'counter':evidence['observed_frames'][0]['counter'] = '13_3'
            if mutation == 'center':evidence['center'] = [-350,20,224]
            if mutation == 'source':evidence['source_sha256']['Assets/Resources/DataBase/localData.bytes'] = '0'*64
            if mutation == 'end':evidence['raw_capture']['end']['reason'] = 'error'
            if mutation == 'anchor_changed':evidence['observed_frames'][1]['agents'][0]['agentPosition'][0] += 1
            with self.assertRaises(ValueError, msg=mutation):validate(evidence)

    def test_archive_rereads_raw_and_keeps_only_two_frames(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory);raw = root/'capture.jsonl';report = root/'report.json'
            frames = copy.deepcopy(self.evidence['observed_frames'])
            third = copy.deepcopy(frames[-1]);third['sample'] += 1;third['frame'] += 1
            frames.append(third)
            rows = [self.evidence['header']]+frames+[dict(type='end',reason='capture-limit',frames=3)]
            raw.write_text(''.join(json.dumps(row)+'\n' for row in rows))
            summary = analyze(raw);report.write_text(json.dumps(summary))
            evidence = archive_report(report)
            self.assertEqual(len(evidence['observed_frames']), 2)
            self.assertEqual(evidence['raw_capture']['frames'], 3)
            summary['first_complete_six_alive_frame']['agents'][0]['agentPosition'][0] += 1
            report.write_text(json.dumps(summary))
            with self.assertRaises(ValueError):archive_report(report)


if __name__ == '__main__':
    unittest.main()
