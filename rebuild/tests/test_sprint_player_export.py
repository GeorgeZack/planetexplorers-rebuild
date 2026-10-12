"""Synthetic PEAC record selection guards, not engine or gameplay evidence."""
import struct
import tempfile
from unittest.mock import patch
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_sprint_player as tool


def fixture():
    prefix=struct.pack('<4s5I',b'PEAC',2,1,3,1,2)+bytes(56+12+48)
    records=[]
    for name,n,duration,loop,value in [('NormalIdle',2,1.,1,17),(tool.NAME,56,1.8000068664550781,0,34)]:
        records.append(struct.pack('<32sfIfI',name.encode(),30.,n,duration,loop)+bytes([value])*n*48)
    return prefix+b''.join(records),prefix,records


class SprintPlayerTests(unittest.TestCase):
    def test_only_sprint_selected_and_prefix_payload_retained(self):
        data,prefix,records=fixture();before=data
        result=tool.select_actor(data)
        expected=bytearray(prefix);struct.pack_into('<I',expected,20,1)
        self.assertEqual(result,bytes(expected)+records[1]);self.assertEqual(data,before)
        self.assertEqual(len(tool.clip_headers(result)),1)

    def test_missing_duplicate_and_bad_clock_rejected(self):
        data,prefix,records=fixture()
        for kind in ('missing','duplicate','fps','duration','loop'):
            bad=bytearray(data)
            if kind=='missing':bad[len(prefix)+len(records[0]):len(prefix)+len(records[0])+32]=bytes(32)
            if kind=='duplicate':bad[len(prefix):len(prefix)+32]=tool.NAME.encode().ljust(32,b'\0')
            if kind=='fps':struct.pack_into('<f',bad,len(prefix)+len(records[0])+32,60.)
            if kind=='duration':struct.pack_into('<f',bad,len(prefix)+len(records[0])+40,1.8)
            if kind=='loop':struct.pack_into('<I',bad,len(prefix)+len(records[0])+44,1)
            with self.subTest(kind=kind),self.assertRaises(ValueError):tool.select_actor(bad)

    def test_installer_preserves_baseline_and_rolls_back_partial_replace(self):
        for fail in (False,True):
            with tempfile.TemporaryDirectory() as d:
                root=Path(d);base=root/'base';stage=root/'stage';dest=root/'assets';motion=root/'motion';backup=root/'backup'
                for parent,value in ((base,b'old'),(dest,b'old'),(stage,b'new')):
                    for group in tool.GROUPS:
                        (parent/group).mkdir(parents=True);(parent/group/'payload').write_bytes(value)
                motion.mkdir();(motion/'curve').write_bytes(b'root')
                before=tool.snapshot(dest);original=Path.rename;injected=[False]
                def rename(path,target):
                    if fail and not injected[0] and path.name==tool.GROUPS[1] and Path(target).parent==dest:
                        injected[0]=True
                        raise OSError('simulated installation failure')
                    return original(path,target)
                with patch.object(Path,'rename',rename):
                    if fail:
                        with self.assertRaises(OSError):tool.install(base,stage,motion,dest,backup)
                    else:tool.install(base,stage,motion,dest,backup)
                self.assertEqual(tool.snapshot(base),before)
                if fail:
                    self.assertEqual(tool.snapshot(dest),before);self.assertFalse((dest/'player-sprint-motion').exists())
                else:
                    self.assertEqual(tool.snapshot(backup),before)
                    self.assertEqual((dest/'player-sprint-motion/curve').read_bytes(),b'root')
                    with self.assertRaises(ValueError):tool.install(base,stage,motion,dest,backup)

    def test_truncated_trailing_and_legacy_rejected(self):
        data,_,_=fixture()
        legacy=bytearray(data);struct.pack_into('<I',legacy,4,1)
        for bad in (data[:-1],data+b'x',legacy):
            with self.assertRaises(ValueError):tool.select_actor(bad)


if __name__=='__main__':unittest.main()
