"""Source guard regressions for construction history; no engine/assets needed."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_build_history as ex


class BuildHistoryExportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.outputs=ex.build_outputs()
        cls.manifest=json.loads(cls.outputs['pe_build_history_source.json'])

    def temporary(self):
        return tempfile.TemporaryDirectory(prefix='history-export-test-',dir=ex.ROOT/'rebuild/build')

    def copy_sources(self,root):
        for path in {p for p,_,_ in ex.GUARDS}:
            target=root/path;target.parent.mkdir(parents=True,exist_ok=True)
            target.write_text((ex.ROOT/path).read_text(encoding='utf-8-sig',errors='replace'))

    def mutation(self,path,before,after):
        with self.temporary() as tmp:
            root=Path(tmp);self.copy_sources(root)
            target=root/path;old=target.read_text();self.assertIn(before,old)
            target.write_text(old.replace(before,after))
            with self.assertRaisesRegex(ValueError,'changed'):ex.validate_sources(root)

    def test_exact_outputs(self):
        for name,data in self.outputs.items():
            self.assertEqual((ex.ROOT/'rebuild/generated'/name).read_bytes(),data)

    def test_contract_includes_source_failure_difference(self):
        c=self.manifest['contract']
        self.assertEqual(c['history_capacity'],5)
        self.assertEqual(c['block_resource_divisor'],4)
        self.assertTrue(c['source_failed_action_moves_history'])
        self.assertFalse(c['source_quit_build_clears_history'])
        self.assertFalse(c['source_archive_owner_qualified'])
        self.assertEqual((c['standalone_modifier'],c['undo_key'],c['redo_key']),('Control','Z','X'))

    def test_history_capacity_drift(self):
        self.mutation(ex.HISTORY,'s_MaxCount = 5','s_MaxCount = 6')

    def test_failed_history_move_drift(self):
        self.mutation(ex.HISTORY,'m_Undos[index].Undo();','if (!m_Undos[index].Undo()) return;')

    def test_debit_rounding_drift(self):
        self.mutation(ex.BUILD,'Mathf.CeilToInt( kvp.Value / divisor)','Mathf.FloorToInt( kvp.Value / divisor)')

    def test_refund_rounding_drift(self):
        self.mutation(ex.BUILD,'Mathf.FloorToInt( kvp.Value / divisor)','Mathf.CeilToInt( kvp.Value / divisor)')

    def test_inverse_mode_drift(self):
        self.mutation(ex.MODIFY,'m_OldVoxels, m_NewVoxels, EBSBrushMode.Subtract','m_OldVoxels, m_NewVoxels, EBSBrushMode.Add')

    def test_redo_key_drift(self):
        self.mutation(ex.INPUT,'Input.GetKeyDown(KeyCode.X)','Input.GetKeyDown(KeyCode.Y)')

    def test_standalone_modifier_drift(self):
        self.mutation(ex.INPUT,'s_Undo = s_Control','s_Undo = s_Shift')

    def test_god_mode_default_drift(self):
        self.mutation(ex.BUILD,'public bool IsGod = false','public bool IsGod = true')

    def test_compile_scale_selection_drift(self):
        self.mutation(ex.CONSTANTS,'#define USE_HALF_METER_VOXEL','#define USE_QUARTER_METER_VOXEL')

    def test_close_build_history_drift(self):
        self.mutation(ex.UI,'public void QuitBlock()\n    {','public void QuitBlock()\n    { BSHistory.Clear();')

    def test_whitespace_and_comments_do_not_change_contract(self):
        with self.temporary() as tmp:
            root=Path(tmp);self.copy_sources(root)
            path=root/ex.HISTORY
            path.write_text(path.read_text().replace('s_MaxCount = 5','s_MaxCount /* capacity */ =   5'))
            ex.validate_sources(root)

    def test_check_is_read_only(self):
        with self.temporary() as tmp:
            root=Path(tmp)
            for name,data in self.outputs.items():(root/name).write_bytes(data)
            path=root/'pe_build_history_source.h';path.write_text('stale')
            with contextlib.redirect_stdout(io.StringIO()),contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(ex.main(['--check','--output',str(root)]),1)
            self.assertEqual(path.read_text(),'stale')


if __name__=='__main__':unittest.main()
