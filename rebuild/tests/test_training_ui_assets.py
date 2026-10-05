#!/usr/bin/env python3
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from export_training_ui import rectangle,sprite_row,validate_font,main,MINIMAP,digest

class TrainingUI(unittest.TestCase):
    def test_pixel_rect_and_nine_slice_borders(self):
        raw=dict(name='panel',outer=dict(x=10.,y=20.,width=40.,height=30.),inner=dict(x=14.,y=25.,width=31.,height=19.),paddingLeft=0.,paddingTop=0.,paddingRight=0.,paddingBottom=0.)
        row=sprite_row(raw,128,128,'ui.png','panel');self.assertEqual(row['rect'],[10,20,40,30]);self.assertEqual(row['border'],[4,5,5,6])
        raw['inner']['height']=31.;row=sprite_row(raw,128,128,'ui.png','panel');self.assertEqual(row['source_border'][3],-6);self.assertEqual(row['border'][3],0);self.assertTrue(row['border_clamped'])
        for bad in [dict(x=100,y=0,width=40,height=1),dict(x=-1,y=0,width=1,height=1),dict(x=0,y=0,width=float('nan'),height=1)]:
            with self.assertRaises(ValueError):rectangle(bad,128,128)

    def test_bitmap_font_bounds_and_counts(self):
        text='common lineHeight=18 base=14 scaleW=32 scaleH=32 pages=1\nchars count=1\nchar id=65 x=0 y=0 width=8 height=12 xoffset=1 yoffset=2 xadvance=9 page=0 chnl=15\n'
        report=validate_font(text,32,32);self.assertEqual(report['glyphs'][0]['xadvance'],9)
        for broken in [text.replace('width=8','width=33'),text.replace('chars count=1','chars count=2'),text+text.splitlines()[-1]+'\n',text.replace('pages=1','pages=2')]:
            with self.assertRaises(ValueError):validate_font(broken,32,32)

    def test_failed_export_and_existing_destination(self):
        with tempfile.TemporaryDirectory() as temporary:
            output=Path(temporary)/'ui'
            def fail(staging):(staging/'partial').write_text('partial');raise ValueError('invalid atlas')
            with patch('sys.argv',['export','--output',str(output)]),patch('export_training_ui.recover',side_effect=fail):
                with self.assertRaises(ValueError):main()
            self.assertFalse(output.exists());self.assertEqual(list(Path(temporary).iterdir()),[])
            output.mkdir();(output/'keep').write_text('keep')
            with patch('sys.argv',['export','--output',str(output)]),patch('export_training_ui.recover') as recover:
                with self.assertRaises(ValueError):main()
                recover.assert_not_called()
            self.assertEqual((output/'keep').read_text(),'keep')

    def test_optional_source_export(self):
        root=Path(__file__).resolve().parents[1]/'build/reference-assets/ui'
        if not (root/'manifest.json').exists():self.skipTest('Optional original UI assets not exported')
        m=json.loads((root/'manifest.json').read_text());self.assertEqual(len(m['sprites']),2474);self.assertEqual(len(m['fonts']),8)
        lines=(root/'atlas.peui').read_text().splitlines();self.assertEqual(lines[0],'PEUI1');self.assertEqual(len(lines),2475)
        names={s['name']:s for s in m['sprites']};self.assertEqual(len(names),2474)
        for n in ['shortbar1bg','shortbar1','bloodlan_6bg','bloodlan_6','bloodlan_7bg','bloodlan_7_1','talkbg','namebg','btn_down','npc_big_Andy','drug_hot01','ax_knife','shovel_wood','safety_axe','build_soil','icons:close','female_icon','male_icon']:
            self.assertIn(n,names)
        self.assertEqual(sum(s['border_clamped'] for s in m['sprites']),2)
        items={r['item_id']:r['sprite'] for r in m['item_icons']};self.assertEqual(items[916],'drug_hot01');self.assertEqual(items[33],'ax_knife')
        for font in m['fonts']:
            self.assertTrue((root/font['texture']).read_bytes().startswith(b'\x89PNG'))
            self.assertTrue((root/font['file']).is_file());self.assertIn('page id=0 file="'+font['texture']+'"',(root/font['file']).read_text())
            self.assertEqual(len(font['glyphs']),305 if font['name']=='ui_Font_27_endbold' else 625)
        self.assertEqual(next(f for f in m['fonts'] if f['name']=='ui_Font_18_talk')['size'],18)
        self.assertEqual((m['minimap']['width'],m['minimap']['height']),(1024,1024))
        self.assertEqual(digest(root/m['minimap']['file']),digest(MINIMAP))

if __name__=='__main__':unittest.main()
