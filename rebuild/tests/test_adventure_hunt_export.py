import json
import copy
from contextlib import closing
from pathlib import Path
import sqlite3
import struct
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_adventure_hunt as exporter
from audit_reference_hunt import decode_random_missions,active_missions

class HuntExportTests(unittest.TestCase):
    def test_regeneration(self):
        capture=exporter.DEFAULT_CAPTURE if exporter.DEFAULT_CAPTURE.is_file() else None
        for name,data in exporter.build_outputs(exporter.DEFAULT_DATABASE,capture).items():
            self.assertEqual((exporter.DEFAULT_OUTPUT/name).read_bytes(),data)
    def test_missing_reference_is_unavailable(self):
        result=exporter.build_outputs(exporter.DEFAULT_DATABASE,None,None)
        self.assertIn(b'PE_HUNT_REFERENCE_AVAILABLE 0',result['pe_adventure_hunt_source.h'])
        self.assertFalse(json.loads(result['pe_adventure_hunt_source.json'])['available'])
        self.assertIn(b'PE_HUNT_RETURN_REFERENCE_AVAILABLE 0',result['pe_adventure_hunt_source.h'])
    def test_return_capture_validation(self):
        original=json.loads(exporter.DEFAULT_RETURN_CAPTURE.read_text())
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'return.json'
            for label,change in [('completion',lambda e:e['completed_missions'].pop('9126')),
                ('binding',lambda e:e['random_missions']['9127'].update(npc=20016)),
                ('reward',lambda e:e['random_missions']['9127']['items'].update(reward=[[592,1]])),
                ('target',lambda e:e['random_missions']['9127']['targets'][0].update(position=[float('nan'),25,136]))]:
                evidence=copy.deepcopy(original);change(evidence);path.write_text(json.dumps(evidence))
                with self.subTest(label=label),self.assertRaises(ValueError):exporter.build_outputs(exporter.DEFAULT_DATABASE,None,path)
            path.write_text(json.dumps(original));out=exporter.build_outputs(exporter.DEFAULT_DATABASE,None,path)
            self.assertIn(b'PE_HUNT_RETURN_REFERENCE_AVAILABLE 1',out['pe_adventure_hunt_source.h'])
            self.assertIn(b'{-266.0f,25.0f,136.0f}',out['pe_adventure_hunt_source.h'])
    def test_changed_source_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'source.db'
            with closing(sqlite3.connect(exporter.DEFAULT_DATABASE.resolve().as_uri()+'?mode=ro',uri=True)) as source,closing(sqlite3.connect(path)) as changed:
                source.backup(changed);changed.execute("UPDATE AdQuest_EnterArea SET DistPos='1_0_130_20' WHERE TargetID='4524'")
                changed.commit()
            with self.assertRaisesRegex(ValueError,'discovery'):exporter.build_outputs(path,None)
    def test_archive_discovery_and_truncation(self):
        # A synthetic serializer fixture, never exported as reference evidence.
        integer=lambda n:struct.pack('<i',n)
        blob=integer(1)+integer(9075)+b'\x08Bug Hunt'+struct.pack('<iiii',20018,20018,1,1)+b'\x00'
        blob+=integer(1)+struct.pack('<iii3f',4524,4,1,-350.,30.,250.)
        blob+=integer(2)+struct.pack('<ii',4103,0)+integer(0)*12
        result=decode_random_missions(blob)
        self.assertEqual(result[9075]['targets'][0]['position'],[-350.,30.,250.])
        self.assertEqual(result[9075]['player_talk'],[4103,0])
        nonfinite=bytearray(blob)
        at=result[9075]['targets'][0]['position_record_offset']
        nonfinite[at:at+4]=struct.pack('<f',float('nan'))
        for bad in (blob[:-1],blob+b'\x00',bytes(nonfinite)):
            with self.assertRaises(ValueError):decode_random_missions(bad)

    def test_capture_cross_record_validation(self):
        # Serializer evidence fixture only: never written to default capture.
        target=dict(id=4524,kind=4,present=True,position=[-350.,30.,250.])
        evidence=dict(schema=1,archives_unchanged=True,input_sha256={'synthetic':'a'*64},archive_guid='fixture',
                      hunt_target=target,active_missions={'9075':[]},completed_missions={'9072':1},
                      random_missions={'9075':dict(npc=20018,targets=[copy.deepcopy(target)])})
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'capture.json';path.write_text(json.dumps(evidence))
            output=exporter.build_outputs(exporter.DEFAULT_DATABASE,path)
            self.assertIn(b'PE_HUNT_REFERENCE_AVAILABLE 1',output['pe_adventure_hunt_source.h'])
            changed=copy.deepcopy(evidence);changed['hunt_target']['position'][0]=-351
            path.write_text(json.dumps(changed))
            with self.assertRaisesRegex(ValueError,'differs'):exporter.build_outputs(exporter.DEFAULT_DATABASE,path)
            changed=copy.deepcopy(evidence);changed['completed_missions']={}
            path.write_text(json.dumps(changed))
            with self.assertRaisesRegex(ValueError,'completed medicine'):exporter.build_outputs(exporter.DEFAULT_DATABASE,path)
            changed=copy.deepcopy(evidence);changed['archives_unchanged']=False
            path.write_text(json.dumps(changed))
            with self.assertRaisesRegex(ValueError,'validated'):exporter.build_outputs(exporter.DEFAULT_DATABASE,path)
    def test_duplicate_active_mission_rejected(self):
        # Header: empty preceding IDs, mission pairs and completed list.
        data=struct.pack('<iiiiiiii',0,0,0,2,9075,0,9075,0)
        with self.assertRaisesRegex(ValueError,'Duplicate active'):active_missions(data)

if __name__=='__main__':unittest.main()
