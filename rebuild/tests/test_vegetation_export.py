"""Guard source wild identities and explicit native fixture adapters offline."""
import json
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_vegetation_source as ex


class VegetationExportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.outputs=ex.build_outputs()
        cls.report=json.loads(cls.outputs['pe_vegetation_source.json'])

    def temporary(self):
        return tempfile.TemporaryDirectory(prefix='vegetation-export-',dir=ex.ROOT/'rebuild/build')

    def copy_sources(self,root):
        for rel in set(ex.INPUT_PINS)|{r[0] for r in ex.GUARDS}:
            target=root/rel;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ex.ROOT/rel,target)

    def mutation(self,path,old,new):
        with self.temporary() as tmp:
            root=Path(tmp);self.copy_sources(root);target=root/path
            txt=ex.source_text(target);self.assertIn(old,txt);target.write_text(txt.replace(old,new))
            with self.assertRaisesRegex(ValueError,'changed'):ex.validate_sources(root)

    def test_exact_outputs(self):
        for name,data in self.outputs.items():self.assertEqual((ex.ROOT/'rebuild/generated'/name).read_bytes(),data)

    def test_qualified_identities_and_fixed_yields(self):
        p=self.report['plants']
        self.assertEqual([(r['prototype_id'],r['fixture_item_id'],r['fixture_count'],r['harvestable']) for r in p],
                         [(15,238,2,True),(37,235,3,True),(40,239,4,True),(146,0,0,False)])
        self.assertTrue(all('tree-Ambient-Occlusion' in r['prefab'] for r in p))
        self.assertTrue(all('Training' not in s['path'] for s in self.report['sources']))

    def test_random_source_drops_not_mislabeled_deterministic(self):
        p=self.report['plants']
        self.assertEqual([r['resource']['production'].split(';')[0] for r in p],
                         ['238,100','235,70,1583,100','239,70,1583,100','1006,50,1583,100'])
        self.assertIn('Primary-only',self.report['adapters'][1])

    def test_effective_range_and_action_masks(self):
        self.assertEqual(self.report['source_gather']['effective_horizontal_range'],2)
        self.assertEqual(self.report['source_gather']['requested_range'],3)
        for key in ('SwordAttack','Dig','Fell','Build'):
            self.assertEqual(self.report['action_relation'][key],'2')

    def test_visual_mesh_contracts(self):
        self.assertEqual([r['triangles'] for r in self.report['plants']],[522,165,647,2282])
        for r in self.report['plants']:
            self.assertTrue(r['two_sided'])
            self.assertAlmostEqual(r['cutoff'],.65,places=6)
            self.assertTrue(all(a<b for a,b in zip(r['minimum'],r['maximum'])))

    def test_range_clamp_source_drift(self):
        self.mutation(ex.RANDOM,'Mathf.Clamp(distance, 0.1f, 2f)','Mathf.Clamp(distance, 0.1f, 3f)')

    def test_gather_type_source_drift(self):
        self.mutation(ex.GATHER,'resFind.m_type == 10','resFind.m_type == 9')

    def test_drop_source_rounding_drift(self):
        self.mutation(ex.DIG,'numGet<(int)resGet','numGet<Mathf.CeilToInt(resGet)')

    def test_prefab_drift_fails_closed(self):
        with self.temporary() as tmp:
            root=Path(tmp);self.copy_sources(root);p=root/ex.PROTOTYPE_PREFAB
            p.write_bytes(p.read_bytes()+b'x')
            with self.assertRaisesRegex(ValueError,'prefab changed'):ex.validate_sources(root)

    def test_database_drop_drift_fails_closed(self):
        with self.temporary() as tmp:
            db=Path(tmp)/'data.bytes';shutil.copyfile(ex.DEFAULT_DATABASE,db)
            with sqlite3.connect(db) as c:c.execute("UPDATE resource SET fixed_num='3' WHERE ID='1015'")
            with self.assertRaisesRegex(ValueError,'resource contract changed'):ex.build_outputs(database=db)

    def test_database_action_drift_fails_closed(self):
        with self.temporary() as tmp:
            db=Path(tmp)/'data.bytes';shutil.copyfile(ex.DEFAULT_DATABASE,db)
            with sqlite3.connect(db) as c:c.execute("UPDATE ActionRelation SET SwordAttack='0' WHERE ActionType='32'")
            with self.assertRaisesRegex(ValueError,'action relation changed'):ex.build_outputs(database=db)

    def test_recovered_assets_match_qualified_hashes(self):
        assets=ex.ROOT/'rebuild/build/reference-assets/sandbox-vegetation'
        if not assets.is_dir():self.skipTest('Optional recovered wild vegetation assets unavailable')
        self.assertTrue(ex.validate_assets(assets))

    def test_recovered_asset_mutation_is_rejected(self):
        assets=ex.ROOT/'rebuild/build/reference-assets/sandbox-vegetation'
        if not assets.is_dir():self.skipTest('Optional recovered wild vegetation assets unavailable')
        with self.temporary() as tmp:
            copied=Path(tmp)/'assets';shutil.copytree(assets,copied)
            target=copied/'plant-15.obj';target.write_bytes(target.read_bytes()+b'\n')
            with self.assertRaisesRegex(ValueError,'asset contents changed'):ex.validate_assets(copied)


if __name__=='__main__':unittest.main()
