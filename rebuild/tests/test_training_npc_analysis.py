#!/usr/bin/env python3
import json
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from analyze_training_npcs import convert,IDS
from test_reference_bake_analysis import bake_fixture,write

class NpcAnalysis(unittest.TestCase):
    def inputs(self,root):
        assets=root/'assets';assets.mkdir();base=root/'npc-idles.jsonl'
        for identity in IDS:
            rows,rig=bake_fixture();rows[2]['name']='NormalIdle';rows[2]['isLooping']=True
            rig['npc']={'entity_id':identity};directory=assets/str(identity);directory.mkdir();(directory/'rig.json').write_text(json.dumps(rig))
            capture=base.with_name(base.stem+'.npc-'+str(identity)+'.jsonl');write(capture,rows)
            plan=dict(npc_id=identity,avatar='MaleAvatar',gender='Male',sample_rate=2,
                      clips=[dict(name='NormalIdle',path_id=179,loop=True,length=.75,frame_rate=30)])
            capture.with_suffix('.launch.json').write_text(json.dumps(dict(schema='pe-animator-bake-launch-1',capture=str(capture),plan=plan)))
        return base,assets

    def test_all_nine_native_scenes_and_clock_provenance(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);base,assets=self.inputs(root);out=root/'scenes'
            report=convert(base,assets,out);self.assertEqual([r['npc_id'] for r in report['npcs']],list(IDS))
            for identity in IDS:
                directory=out/str(identity);lines=(directory/'recording.peas').read_text().splitlines();self.assertEqual(lines[0],'PEAS1')
                actor,texture=lines[1].split();self.assertEqual((directory/actor).read_bytes()[:4],b'PEAC')
                self.assertTrue((directory/texture).read_bytes().startswith(b'\x89PNG'))
                data=json.loads((directory/'recording-manifest.json').read_text());self.assertEqual(data['npc_id'],identity)
            with self.assertRaises(ValueError):convert(base,assets,out)

    def test_missing_wrong_identity_and_partial_capture_never_publish(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);base,assets=self.inputs(root);out=root/'scenes'
            rig=assets/'9004/rig.json';data=json.loads(rig.read_text());data['npc']['entity_id']=9041;rig.write_text(json.dumps(data))
            with self.assertRaises(ValueError):convert(base,assets,out)
            self.assertFalse(out.exists())
            data['npc']['entity_id']=9004;rig.write_text(json.dumps(data))
            capture=base.with_name(base.stem+'.npc-9004.jsonl');capture.unlink()
            with self.assertRaises(ValueError):convert(base,assets,out)
            self.assertFalse(out.exists())
            rows,_=bake_fixture();rows[2]['name']='NormalIdle';rows[2]['isLooping']=True;write(capture,rows[:-1])
            with self.assertRaises(subprocess.CalledProcessError):convert(base,assets,out)
            self.assertFalse(out.exists());self.assertFalse(list(root.glob('.npc-scenes-*')))

if __name__=='__main__':unittest.main()
