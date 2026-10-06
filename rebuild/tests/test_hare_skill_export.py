#!/usr/bin/env python3
from contextlib import closing
import json
from pathlib import Path
import sqlite3
import struct
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'rebuild/tools'))
import export_hare_skill as export

class SkillExport(unittest.TestCase):
    def test_float_and_repeat_last_contract(self):
        self.assertEqual(export.expand('.36;.24',2),[struct.unpack('<f',struct.pack('<f',v))[0] for v in (.36,.24)])
        self.assertEqual(export.expand('0',2),[0,0])
        for value in ('nan','inf','-1','61'):
            with self.assertRaises(ValueError):export.number(value)
        with self.assertRaises(ValueError):export.expand('0;0;0',2)
    def test_regeneration_and_provenance(self):
        output=export.build_outputs()
        for name,data in output.items():self.assertEqual(data,(export.DEFAULT_OUTPUT/name).read_bytes())
        report=json.loads(output['pe_hare_skill_source.json'])
        self.assertEqual([s['skill_id'] for s in report['skills']],list(export.IDS))
        self.assertEqual([s['guide_count'] for s in report['skills']],[1,1,2,1,1])
        self.assertEqual(len(report['sources']),4)
        self.assertEqual(report['skills'][2]['ending_post'],export.number('.51'))
    def test_changed_source_contract_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'changed.db'
            with closing(export.connect_readonly(export.DEFAULT_DATABASE)) as original, closing(sqlite3.connect(path)) as changed:original.backup(changed)
            for field,value in (('_cond','loopcnt,3'),('_effMainOneTime','8'),('_effPrepare','999'),('_triggerEvents','0'),('_timeOfMain','nan')):
                with closing(sqlite3.connect(path)) as changed:
                    old=changed.execute(f'SELECT "{field}" FROM skMain WHERE CAST(_id AS INTEGER)=?',(export.IDS[2],)).fetchone()[0]
                    changed.execute(f'UPDATE skMain SET "{field}"=? WHERE CAST(_id AS INTEGER)=?',(value,export.IDS[2]));changed.commit()
                with self.assertRaises(ValueError):export.build_outputs(path)
                with closing(sqlite3.connect(path)) as changed:changed.execute(f'UPDATE skMain SET "{field}"=? WHERE CAST(_id AS INTEGER)=?',(old,export.IDS[2]));changed.commit()

if __name__=='__main__':unittest.main()
