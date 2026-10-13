#!/usr/bin/env python3
"""The Halt comparison recovers known stops from a synthetic capture. This
checks the analyzer; it says nothing about the original."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'rebuild/tools'))
import analyze_reference_halt as halt

STOPS=((7.7,35.3,0),(4.9,201.4,1))
def capture(replay,scale=1.,grounder=0.):
    lines=[json.dumps(dict(type='header',kind='observed-original-fbbik'))];sample=0
    def frame(row,dt):
        nonlocal sample
        effectors=[dict(positionOffset=row[e*3:e*3+3]) for e in range(9)]
        for e in (7,8):effectors[e]['positionOffset'][1]+=grounder      # the grounder shares the feet
        lines.append(json.dumps(dict(type='frame',sample=sample,dt=dt,effectors=effectors)));sample+=1
    for index,(speed,heading,right) in enumerate(STOPS):
        times=[0.04+0.002*((k+index)%5) for k in range(26)]
        for _ in range(4):frame([0.]*27,0.017)
        _,_,rows=replay.run([(speed,heading,right,times)])[0]
        for row,dt in zip(rows,times):
            for e in (1,2):row[e*3+1]*=scale
            frame(row,dt)
    for _ in range(4):frame([0.]*27,0.017)
    return lines

@unittest.skipUnless(halt.LIBRARY.is_file(),'core library not built')
class Analyzer(unittest.TestCase):
    def setUp(self):
        self.folder=tempfile.TemporaryDirectory(dir=ROOT/'rebuild/build');self.addCleanup(self.folder.cleanup)
        self.replay=halt.Replay(self.folder.name);self.addCleanup(self.replay.close)
    def report(self,lines):
        path=Path(self.folder.name)/'capture.jsonl';path.write_text('\n'.join(lines)+'\n');return halt.analyze(path,self.folder.name)
    def test_known_stops_are_recovered_despite_shared_foot_offsets(self):
        report=self.report(capture(self.replay,grounder=0.03));self.assertEqual(len(report['stops']),len(STOPS))
        for stop,(speed,heading,right) in zip(report['stops'],STOPS):
            self.assertEqual(stop['foot'],'right' if right else 'left')
            self.assertEqual(stop['start_frame'],stop['first_frame']-1)   # the starting frame adds nothing
            self.assertAlmostEqual(stop['fitted_speed'],speed,delta=0.02);self.assertAlmostEqual(stop['fitted_heading_degrees'],heading,delta=0.2)
            self.assertLess(stop['halt_only_error_m']['max'],2e-3);self.assertGreater(stop['grounder_shared_error_m']['max'],0.02)
    def test_a_different_shoulder_curve_is_visible(self):
        report=self.report(capture(self.replay,scale=1.1));self.assertGreater(report['worst_halt_only_error_m'],0.05)
    def test_other_captures_are_refused(self):
        lines=capture(self.replay);lines[0]=json.dumps(dict(type='header',kind='other'))
        with self.assertRaises(ValueError):self.report(lines)
if __name__=='__main__':unittest.main()
