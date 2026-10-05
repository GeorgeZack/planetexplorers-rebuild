#!/usr/bin/env python3
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_adventure_medicine as exporter

class MedicineExportTest(unittest.TestCase):
    def test_regeneration(self):
        for name,contents in exporter.build_outputs(exporter.DEFAULT_DATABASE).items():
            self.assertEqual((exporter.ROOT/'rebuild/generated'/name).read_bytes(),contents)
    def test_source_transactions(self):
        data=json.loads(exporter.build_outputs(exporter.DEFAULT_DATABASE)['pe_adventure_medicine_source.json'])
        self.assertEqual(data['acceptance']['grant'],{'item':877,'count':1})
        self.assertEqual(data['completion']['consume'],{'item':917,'count':5})
        self.assertEqual(data['completion']['rewards'],[{'item':925,'count':10},{'item':554,'count':1}])
        self.assertEqual(data['target']['TargetID'],'2531')
        self.assertEqual(data['continuation']['quest'],9074)
        self.assertFalse(data['continuation']['implemented'])
    def test_source_change_rejected(self):
        import sqlite3
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'changed.db'
            with sqlite3.connect(exporter.DEFAULT_DATABASE) as source,sqlite3.connect(path) as changed:
                source.backup(changed)
                changed.execute("UPDATE AdRandomQuest_List SET CoRemoveItem='917_4' WHERE ID='9072'")
            with self.assertRaisesRegex(ValueError,'CoRemoveItem'):
                exporter.build_outputs(path)

if __name__=='__main__':unittest.main()
