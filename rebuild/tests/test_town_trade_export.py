"""Source price selection and fail-closed regeneration without engine/assets."""
import contextlib
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_town_trade_prices as ex


class TownTradeExportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.outputs=ex.build_outputs()
        cls.manifest=json.loads(cls.outputs['pe_town_trade_prices.json'])
        cls.prices={r['item_id']:r for r in cls.manifest['prices']}

    def temporary(self):
        return tempfile.TemporaryDirectory(prefix='trade-export-test-',dir=ex.ROOT/'rebuild/build')

    def test_checked_outputs_exact(self):
        for name,data in self.outputs.items():
            self.assertEqual((ex.ROOT/'rebuild/generated'/name).read_bytes(),data)

    def test_catalog_coverage_and_order(self):
        catalog=json.loads((ex.ROOT/'rebuild/generated/content_manifest.json').read_text())
        self.assertEqual(list(self.prices),sorted(catalog['selected_item_ids']))
        self.assertEqual(len(self.prices),342)
        self.assertEqual(self.prices[1228]['base_currency'],280)
        self.assertEqual(self.prices[1228]['factor'],'FULL')

    def test_known_digital_prices(self):
        self.assertEqual(self.prices[33]['base_currency'],14)
        self.assertEqual(self.prices[33]['factor'],'DURABILITY')
        self.assertEqual(self.prices[925]['factor'],'FULL')
        self.assertEqual(self.prices[925]['base_currency'],0)
        self.assertEqual(self.prices[10]['factor'],'FULL')
        self.assertEqual(self.prices[10]['base_currency'],56)

    def test_factor_requires_repair(self):
        row={'_function':'14','_durability':'50','currency_value2':'7'}
        result=ex.price_row(1,row,{14:{'classname':'Durability,Energy'}})
        self.assertEqual(result['factor'],'FULL')

    def test_durability_precedes_life_limit(self):
        row={'_function':'14','_durability':'50','currency_value2':'7'}
        result=ex.price_row(1,row,{14:{'classname':'Repair,LifeLimit,Durability'}})
        self.assertEqual(result['factor'],'DURABILITY')

    def test_life_limit_is_explicit(self):
        values=[r for r in self.prices.values() if r['factor']=='LIFE_LIMIT']
        self.assertEqual([r['item_id'] for r in values],[365,370])
        self.assertTrue(all('Repair' in r['components'] for r in values))

    def test_energy_never_substitutes_condition(self):
        row={'_function':'4','currency_value2':'7'}
        result=ex.price_row(1,row,{4:{'classname':'Repair,EnergySmall'}})
        self.assertEqual(result['factor'],'FULL')

    def test_invalid_price_or_durability_rejected(self):
        for bad in ('-1','1.2','2147483648','NaN'):
            with self.assertRaises(ValueError):
                ex.price_row(1,{'_function':'0','currency_value2':bad},{})
        with self.assertRaises(ValueError):
            ex.price_row(1,{'_function':'9','currency_value2':'14','_durability':'0'},
                         {9:{'classname':'Repair,Durability'}})

    def test_source_drift_fails_closed(self):
        with self.temporary() as tmp:
            root=Path(tmp)
            for path in {p for p,_,_ in ex.GUARDS}:
                dest=root/path;dest.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(ex.ROOT/path,dest)
            ex.validate_sources(root)
            p=root/ex.ITEM
            text=p.read_text(encoding='utf-8-sig')
            self.assertIn('0.8f * factor + 0.2f',text)
            p.write_text(text.replace('0.8f * factor + 0.2f','0.7f * factor + 0.3f'))
            with self.assertRaisesRegex(ValueError,'source contract changed'):
                ex.validate_sources(root)

    def test_source_whitespace_comments_accepted(self):
        with self.temporary() as tmp:
            root=Path(tmp)
            for path in {p for p,_,_ in ex.GUARDS}:
                dest=root/path;dest.parent.mkdir(parents=True,exist_ok=True)
                text=(ex.ROOT/path).read_text(encoding='utf-8-sig',errors='replace')
                dest.write_text(text.replace('0.8f * factor + 0.2f','0.8f /* unit factor */ * factor + 0.2f'))
            ex.validate_sources(root)

    def test_check_mode_does_not_repair_stale_output(self):
        with self.temporary() as tmp:
            root=Path(tmp)
            for name,data in self.outputs.items(): (root/name).write_bytes(data)
            target=root/'pe_town_trade_prices.h';target.write_text('stale')
            with contextlib.redirect_stdout(io.StringIO()),contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(ex.main(['--check','--output',str(root)]),1)
            self.assertEqual(target.read_text(),'stale')


if __name__=='__main__': unittest.main()
