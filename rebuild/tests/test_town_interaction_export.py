"""Source vendor identity, deterministic catalog and fail-closed contract checks."""
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_town_interaction as exporter
class TownExportTests(unittest.TestCase):
    def test_archived_catalog(self):
        outputs=exporter.build_outputs()
        for name,data in outputs.items():self.assertEqual((exporter.ROOT/'rebuild/generated'/name).read_bytes(),data)
        rows=json.loads(outputs['pe_town_offers.json'])['offers']
        self.assertEqual(len(rows),284)
        self.assertEqual({r['entity_id'] for r in rows},{20002,20011,20014,20028,20031,20032,20033,20037})
        catalog=json.loads((exporter.ROOT/'rebuild/generated/content_manifest.json').read_bytes())
        supported={r['id'] for r in catalog['items']}
        self.assertTrue(all(r['item_id'] in supported for r in rows))
        self.assertEqual(next(r['price'] for r in rows if r['store_id']==1184),158)
        self.assertEqual([(r['entity_id'],r['store_id']) for r in rows[:17]],
                         [(entity,store) for entity,_,ids in exporter.VENDORS for store in ids])
        self.assertEqual(len({(r['entity_id'],r['store_id']) for r in rows}),len(rows))
        self.assertEqual({r['store_id'] for r in rows if r['entity_id']==20028 and r['item_id'] in (239,235,241,238,233)}, {212,208,214,211,206})
    def changed(self,sql):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'source.db'
            with sqlite3.connect(exporter.DEFAULT_DATABASE.resolve().as_uri()+'?mode=ro',uri=True) as source,sqlite3.connect(path) as output:
                source.backup(output);output.execute(sql);output.commit()
            with self.assertRaises(ValueError):exporter.build_outputs(database=path)
    def test_reject_other_mode_only(self):self.changed("UPDATE NPCstore SET adven_single='0' WHERE storenpc_id='141'")
    def test_reject_unimplemented_extra_cost(self):self.changed("UPDATE store SET extra_demand='99' WHERE store_id='1184'")
    def test_reject_unknown_quest_gate(self):self.changed("UPDATE store SET PreQuestLimit='2:9032' WHERE store_id='1184'")
if __name__=='__main__':unittest.main()
