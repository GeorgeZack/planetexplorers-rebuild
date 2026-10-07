"""Pinned wallets, complete component parsing and source lifecycle guard tests."""
import copy
import contextlib
import io
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import export_town_wallets as ex


def blob(data):
    return struct.pack('<i', len(data))+data


def package(balance=553, maximum=2000, increase=342, added=0, slots=None):
    slots = slots or [[-1]*15, [-1]*30, [-1]*10]
    groups = [blob(struct.pack('<ii', 0, len(items))+struct.pack('<'+'i'*len(items), *items)) for items in slots]
    return blob(struct.pack('<i', balance))+groups[0]+groups[1]+blob(struct.pack('<iiB', maximum, increase, added))+groups[2]


class TownWalletExportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.outputs = ex.build_outputs()
        cls.manifest = json.loads(cls.outputs['pe_town_wallets.json'])

    def temporary(self):
        return tempfile.TemporaryDirectory(prefix='wallet-export-test-', dir=ex.ROOT/'rebuild/build')

    def test_regeneration(self):
        self.assertEqual(self.outputs, ex.build_outputs())
        for name, data in self.outputs.items():
            self.assertEqual((ex.ROOT/'rebuild/generated'/name).read_bytes(), data)

    def test_exact_archived_profiles(self):
        rows = self.manifest['profiles']
        self.assertEqual([(r['entity_id'], r['prototype_id']) for r in rows], list(ex.VENDORS))
        self.assertEqual([(r['initial_balance'], r['serialized_max'], r['daily_increase'], r['initial_added']) for r in rows],
                         [(553,2000,342,False),(584,2000,260,False),(587,2000,288,False),
                          (2600,10000,1792,False),(2075,8000,1181,False),(2449,8000,1321,False),
                          (3434,10000,1623,False),(3090,10000,1558,False)])
        for r in rows:
            self.assertEqual(len(r['component_sha256']), 64)
        self.assertEqual(len({r['entity_id'] for r in rows}), 8)

    def test_calendar_and_source_limits(self):
        self.assertEqual(self.manifest['clock']['day_seconds'], 93600)
        self.assertEqual(self.manifest['clock']['half_day_seconds'], 46800)
        self.assertTrue(self.manifest['clock']['new_entity_initial_added'])
        self.assertFalse(self.manifest['semantics']['serialized_max_applied'])
        self.assertTrue(self.manifest['semantics']['ordinary_purchase_credits_vendor'])
        for archive in self.manifest['archives'].values():
            self.assertEqual(archive['session']['scene_mode'], 1)
            self.assertTrue(archive['session']['digital'])
        # A profile above its recorded maximum remains valid; no fake cap.
        self.assertEqual(ex.decode_package(package(balance=2500))['initial_balance'], 2500)

    def test_complete_package_and_latch(self):
        self.assertEqual(ex.decode_package(package()), dict(initial_balance=553, serialized_max=2000, daily_increase=342, initial_added=False))
        self.assertTrue(ex.decode_package(package(added=1))['initial_added'])
        with self.assertRaises(ValueError): ex.decode_package(package(added=2))
        for amount in (-1, -2147483648):
            for field in ('balance', 'maximum', 'increase'):
                with self.subTest(field=field, amount=amount), self.assertRaises(ValueError):
                    ex.decode_package(package(**{field: amount}))

    def test_truncation_trailing_and_oversized(self):
        valid = package()
        for n in range(len(valid)):
            with self.subTest(length=n), self.assertRaises(ValueError): ex.decode_package(valid[:n])
        for invalid in (valid+b'\0', b'\0'*4097, struct.pack('<i', 2**30)+valid[4:]):
            with self.assertRaises(ValueError): ex.decode_package(invalid)

    def test_slot_versions_capacities_and_aliases(self):
        valid = bytearray(package());struct.pack_into('<i', valid, 12, 1)
        with self.assertRaises(ValueError): ex.decode_package(bytes(valid))
        groups = [[-1]*15, [-1]*30, [-1]*10]
        for slot, count in ((0,14),(1,31),(2,9)):
            bad=copy.deepcopy(groups);bad[slot]=[-1]*count
            with self.assertRaises(ValueError): ex.decode_package(package(slots=bad))
        for invalid in (0,-2):
            bad=copy.deepcopy(groups);bad[0][0]=invalid
            with self.assertRaises(ValueError): ex.decode_package(package(slots=bad))
        good=copy.deepcopy(groups);good[0][0]=37
        self.assertEqual(ex.decode_package(package(slots=good))['initial_balance'],553)
        good[2][0]=37
        with self.assertRaises(ValueError): ex.decode_package(package(slots=good))

    def source_mutation(self, path, old, new):
        with self.temporary() as directory:
            root=Path(directory)
            for source in ex.SOURCE_FILES:
                target=root/source;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ex.ROOT/source,target)
            target=root/path;text=target.read_text(encoding='utf-8-sig',errors='replace')
            self.assertIn(old,text);target.write_text(text.replace(old,new,1))
            with self.assertRaises(ValueError):ex.validate_sources(root)

    def test_source_refill_boundary_guard(self):
        self.source_mutation(ex.NPC,'SecondInDay < GameTime.Timer.Day2Sec / 2','SecondInDay <= GameTime.Timer.Day2Sec / 2')

    def test_source_maximum_and_initial_latch_guard(self):
        self.source_mutation(ex.NPC,'mMoney.current += mIncPerDay;','mMoney.current = Mathf.Min(mMax, mMoney.current + mIncPerDay);')
        self.source_mutation(ex.NPC,'new AutoIncreaseMoney(money, max, valuePerDay, true)','new AutoIncreaseMoney(money, max, valuePerDay, false)')

    def test_source_purchase_credit_guard(self):
        self.source_mutation('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIShopWnd.cs',
                             'npcpc.money.current += cost;', 'npcpc.money.current += 1;')

    def test_source_serialization_guard(self):
        self.source_mutation(ex.NPC,'w.Write(mAdded);','w.Write(false);')

    def test_source_calendar_and_mode_guard(self):
        self.source_mutation('Assets/Scripts/Global/PETimer.cs','c_Day2Hour = 26;','c_Day2Hour = 24;')
        self.source_mutation(ex.TIMER,'long hour2Min = 60','long hour2Min = 59')
        self.source_mutation('Assets/PeLauncher/PlayerTypeLoader.cs',
                             'GameTime.Timer.Day = GameTime.StoryModeStartDay;\n                GameTime.Timer.ElapseSpeed = GameTime.NormalTimeSpeed;\n                Money.Digital = true;',
                             'GameTime.Timer.Day = GameTime.StoryModeStartDay;\n                GameTime.Timer.ElapseSpeed = GameTime.NormalTimeSpeed;\n                Money.Digital = false;')

    def test_saved_mode_guard(self):
        rows,_,_=ex.records(ex.DEFAULT_SAVE);valid=rows['Pathea.SinglePlayerTypeArchiveMgr']
        self.assertTrue(ex.decode_session(valid)['digital'])
        bad=bytearray(valid);bad[-1]=0
        with self.assertRaises(ValueError):ex.decode_session(bytes(bad))
        bad=bytearray(valid);struct.pack_into('<i',bad,20,2)
        with self.assertRaises(ValueError):ex.decode_session(bytes(bad))
        with self.assertRaises(ValueError):ex.decode_session(valid+b'\0')

    def test_archive_hash_and_symlink_guard(self):
        with self.temporary() as directory:
            path=Path(directory)
            for name in ('index.arc','world.arc'):shutil.copyfile(ex.DEFAULT_SAVE/name,path/name)
            data=bytearray((path/'world.arc').read_bytes());data[-1]^=1;(path/'world.arc').write_bytes(data)
            with self.assertRaisesRegex(ValueError,'Pinned wallet archive changed'):ex.read_archive(path,'starting')
            (path/'world.arc').unlink();(path/'world.arc').symlink_to(ex.DEFAULT_SAVE/'world.arc')
            with self.assertRaisesRegex(ValueError,'Invalid wallet archive input'):ex.read_archive(path,'starting')

    def test_wrong_entity_and_cross_archive_profile(self):
        first=ex.read_archive(ex.DEFAULT_SAVE,'starting');second=ex.read_archive(ex.ANNOT_SAVE,'route')
        bad=copy.deepcopy(first);bad[0][20002]['prototype_id']=142
        with mock.patch.object(ex,'read_archive',side_effect=[bad,second]),self.assertRaisesRegex(ValueError,'NPC binding'):
            ex.build_outputs()
        bad=copy.deepcopy(second);bad[0][20002]['components']['NpcPackageCmpt']=package(balance=554)
        with mock.patch.object(ex,'read_archive',side_effect=[first,bad]),self.assertRaisesRegex(ValueError,'differs across'):
            ex.build_outputs()

    def test_check_rejects_stale_outputs(self):
        with self.temporary() as directory:
            out=Path(directory)
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as errors:
                self.assertEqual(ex.main(['--output',str(out)]),0)
                self.assertEqual(ex.main(['--output',str(out),'--check']),0)
                (out/'pe_town_wallets.h').write_bytes(b'stale')
                self.assertEqual(ex.main(['--output',str(out),'--check']),1)
            self.assertIn('Stale wallet output pe_town_wallets.h',errors.getvalue())


if __name__=='__main__':
    unittest.main()
