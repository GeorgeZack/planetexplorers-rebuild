"""Guard the exact active source paths used by the bounded native UI adapters."""
import hashlib
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
PATHS = {
    'item': ('Assets/Scripts/Items/ItemObj.cs', 'd7704733cafc8fdb2a8ae853e089459df6185fb1498e904f22ee3b3dbeb2e00a'),
    'shop': ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIShopWnd.cs', 'b61cb1367e99b20b1d862d14ae7295fa44618fbeab43f91e4201ed419fc70f51'),
    'grid': ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/Widget/Grid_N.cs', 'b6b787e3315c81dc119a89de94d3e7c82c2873735abccc43c6f54a2a692000c7'),
    'repair': ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIRepairWnd.cs', '1f78d87aefc87d7dee8ee8ff57a73dca39244801011cc80586130a412ac227ca'),
}

def source(name):
    return (ROOT / PATHS[name][0]).read_bytes().decode('utf-8', errors='replace')

class SourceContracts(unittest.TestCase):
    def test_complete_active_sources(self):
        for name, (path, digest) in PATHS.items():
            with self.subTest(source=name):
                self.assertEqual(hashlib.sha256((ROOT / path).read_bytes()).hexdigest(), digest)

    def test_quantity_active_input_and_commit(self):
        text = source('shop')
        for token in ('public UIInput mOpNumLabel;', 'void OnOpOkBtn()',
                      'Buy(mOpGrid, (int)mCurrentNum);', 'Sell(mOpGrid, (int)mCurrentNum);',
                      'mCurrentNum = Mathf.Clamp(System.Convert.ToInt32(mOpNumLabel.text), 1, mOpGrid.Item.GetCount());',
                      'if (!UICamera.inputHasFocus)', 'mOpNumLabel.text = mCurrentNum.ToString();'):
            self.assertIn(token, text)

    def test_component_tooltip_and_first_formula(self):
        self.assertIn('tipStr = Item.GetTooltip();', source('grid'))
        text = source('item')
        for token in ('text = CmptProcess(text);', 'text = SellPriceProcess(text);',
                      'GetKnownFormula(formulaId[0])', 'PELocalization.GetString(4000001)',
                      'Mathf.CeilToInt(value.current / 100f)', 'Mathf.CeilToInt(valueMax / 100f)'):
            self.assertIn(token, text)

    def test_repair_distinct_multiply_expression(self):
        text = source('repair')
        self.assertIn('void UpdateItem(Repair obj)', text)
        self.assertIn('Mathf.CeilToInt(obj.GetValue().current*0.01f)', text)
        self.assertIn('Mathf.CeilToInt(obj.GetValue().ExpendValue *0.01f)', text)

if __name__ == '__main__':
    unittest.main()
