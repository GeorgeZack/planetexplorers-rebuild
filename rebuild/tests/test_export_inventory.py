import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

TOOLS=Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0,str(TOOLS))
import export_inventory as exporter

class InventorySourceTests(unittest.TestCase):
    def test_reproducible(self):
        for name,data in exporter.build_outputs().items():
            self.assertEqual(data,(exporter.ROOT/'rebuild/generated'/name).read_bytes())
    def test_contract(self):
        manifest=json.loads(exporter.build_outputs()['pe_inventory_source.json'])
        self.assertEqual(manifest['contract']['category_capacity'],420)
        self.assertEqual(manifest['contract']['categories'],['Item','Equipment','Resource','Armor'])
        rows={r['id']:r for r in manifest['items']}
        self.assertEqual(rows[916]['category'],0)
        self.assertEqual(rows[12]['category'],1)
        self.assertEqual(rows[279]['category'],2)
        self.assertLess(rows[33]['sort_label'],rows[12]['sort_label'])
        self.assertLess(rows[279]['sort_label'],rows[280]['sort_label'])
        self.assertTrue(rows[1332]['quest_item'])
        self.assertFalse(rows[916]['quest_item'])
        self.assertEqual(manifest['contract']['discard']['confirmation'],8000055)
        self.assertTrue(manifest['contract']['discard']['whole_selected_stack'])
    def test_catalog_coverage(self):
        rows={r['id']:r for r in json.loads(exporter.build_outputs()['pe_inventory_source.json'])['items']}
        content=json.loads((exporter.ROOT/'rebuild/generated/content_manifest.json').read_text())
        for item in content['items']:
            self.assertEqual(rows[item['id']]['category'],item['tab'])
    def test_mutation_guard(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            relative=next(iter(exporter.GUARDS))
            path=root/relative;path.parent.mkdir(parents=True)
            path.write_bytes((exporter.ROOT/relative).read_bytes().replace(b'PkgCapacity = 420',b'PkgCapacity = 32'))
            with self.assertRaisesRegex(ValueError,'Inventory source changed'):
                exporter.build_outputs(root)

if __name__=='__main__':unittest.main()
