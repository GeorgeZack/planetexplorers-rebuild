"""Guard original harvesting formulas, cumulative probabilities and species."""
import json
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_harvest as e

class HarvestSourceTests(unittest.TestCase):
    def test_regeneration_and_source_identity(self):
        for name,data in e.build_outputs().items():self.assertEqual(data,(e.ROOT/'rebuild/generated'/name).read_bytes())
        report=json.loads(e.build_outputs()['pe_harvest_source.json']);self.assertEqual(report['tree_identity']['prototype_id'],48)
        self.assertEqual(report['tree_identity']['guid'],e.TREE_GUID)
        self.assertEqual([r['prototype_id'] for r in report['resources']],[15,37,40,48,146])
    def test_cumulative_thresholds_not_weights(self):
        primary,extra=e.production('235,70,1583,100;0.1,388,50,940,75,1583,100;')
        self.assertEqual(primary,[{'item_id':235,'threshold':70.},{'item_id':1583,'threshold':100.}])
        self.assertEqual(extra[0]['chance'],.1)
        self.assertEqual([d['threshold'] for d in extra[0]['drops']],[50.,75.,100.])
        self.assertEqual(extra[1],{'chance':0.,'drops':[]})
    def test_actual_tree_and_herb_counts(self):
        rows={r['prototype_id']:r for r in json.loads(e.build_outputs()['pe_harvest_source.json'])['resources']}
        self.assertEqual(rows[48]['row']['durability'],'.48571')
        self.assertEqual(rows[48]['row']['self_num'],'16')
        self.assertEqual(rows[48]['optional']['chance'],1.2)
        for proto,count in ((15,'2'),(37,'3'),(40,'4')):self.assertEqual(rows[proto]['row']['fixed_num'],count)
    def test_source_database_change_rejected(self):
        for field,value in (('durability','1'),('production','279,100;;'),('self_num','8'),('fixed_num','16'),('type','10')):
            with tempfile.TemporaryDirectory() as tmp:
                path=Path(tmp)/'db';shutil.copyfile(e.DEFAULT_DATABASE,path)
                with sqlite3.connect(path) as db:db.execute(f'UPDATE resource SET {field}=? WHERE ID=?',(value,'1048'))
                with self.assertRaisesRegex(ValueError,'Harvest resource contract'):e.build_outputs(database=path)
    def test_all_drop_items_exported(self):
        with sqlite3.connect(e.DEFAULT_DATABASE) as db:
            db.row_factory=sqlite3.Row;ids=e.harvest_item_ids(db)
        catalog=json.loads((e.ROOT/'rebuild/generated/content_manifest.json').read_text())
        self.assertEqual(ids,{238,943,235,1583,388,940,239,944,279,979,1017,1006})
        self.assertEqual(set(catalog['wild_harvest_item_ids']),ids)
        self.assertTrue(ids<=set(catalog['selected_item_ids']))
    def test_malformed_production(self):
        for value in ('279','279,101;;','279,80,280,70;;','279,nan;;','279,100;nan,943,100;'):
            with self.assertRaises((ValueError,OverflowError)):e.production(value)
if __name__=='__main__':unittest.main()
