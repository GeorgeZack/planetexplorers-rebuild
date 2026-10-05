"""Check native mound data against original create/clear source methods."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_training_terrain as export

class MoundExportTests(unittest.TestCase):
    def test_generated_fixture_matches_source(self):
        self.assertEqual(export.DESTINATION.read_text(),export.generated())
        self.assertEqual(export.generated().count('    {'),758)
    def test_anchors(self):
        samples=export.anchors()
        self.assertEqual(len(samples),32)
        self.assertIn([17,1,10,255,1],samples)
        self.assertIn([13,5,9,255,1],samples)
        original=export.ANCHORS.read_text(encoding='utf-8-sig')
        before,after=original.split('void DestroyTerrian()',1)
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'anchors.cs';source.write_text(before+'void DestroyTerrian()'+after.replace('i = 17','i = 16',1))
            with patch.object(export,'ANCHORS',source):
                with self.assertRaisesRegex(ValueError,'Anchor clear'):export.generated()
    def test_creation_and_cleanup_must_cover_identical_coordinates(self):
        original=export.TERRAIN.read_text(encoding='utf-8-sig')
        changed=original.replace('void DestroyTerrian()','void DestroyTerrian()',1)
        before,after=changed.split('void DestroyTerrian()',1)
        after=after.replace('i = -2','i = -3',1)
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'source.cs';source.write_text(before+'void DestroyTerrian()'+after)
            with patch.object(export,'TERRAIN',source):
                with self.assertRaisesRegex(ValueError,'clear samples'):export.generated()
    def test_changed_schema_rejected(self):
        original=export.TERRAIN.read_text(encoding='utf-8-sig').replace('i <= 15','i <= 16',1)
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'source.cs';source.write_text(original)
            with patch.object(export,'TERRAIN',source):
                with self.assertRaisesRegex(ValueError,'schema changed'):export.generated()
if __name__=='__main__':unittest.main()
