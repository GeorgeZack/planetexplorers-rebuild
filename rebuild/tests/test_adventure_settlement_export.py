"""Bounded archive, authored slot and source identity checks; no graphics/assets."""
from contextlib import closing
import math
from pathlib import Path
import sqlite3
import struct
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]/'tools'
sys.path.insert(0,str(TOOLS))
import export_adventure_settlement as export
from test_adventure_supplier import integer, string, blob, archive


def floats(values): return struct.pack('<'+'f'*len(values),*values)


def entity(identity,prefab,kind,prototype,position,rotation=(0,0,0,1)):
    pose = integer(1)+floats(position)+floats(rotation)+b'\0'+floats([0,0,0])
    data = integer(2)+integer(kind)+integer(prototype)+integer(1)+string('PeTrans')+blob(pose)
    return integer(identity)+string(prefab)+blob(data)


def write_records(directory,rows,yird=False):
    directory.mkdir(parents=True,exist_ok=True)
    offset, index, world = 28, integer(len(rows)), b''
    for key,value in rows.items():
        index += string(key)+string('world')+bytes([yird])+struct.pack('<qq',offset,len(value))
        offset += len(value)
        world += value
    (directory/'index.arc').write_bytes(archive(index))
    (directory/'world.arc').write_bytes(archive(world))


def fixture(directory,broken=None):
    origin = [-623,16,327]
    positions = [(40,11.5,29),(24,11.5,37),(22,12.5,81),(40,12.5,61),(9.5,13.5,77),(12,13.5,77)]
    angles = [270,180,170,250,180,180]
    entities = []
    for i,(identity,prototype,house,slot) in enumerate(export.BUILDINGS):
        position = [a+b for a,b in zip(origin,positions[i])]
        if i==3 and broken=='slot': position[2] += 1
        if i==3 and broken=='nan': position[2] = math.nan
        angle = math.radians(angles[i])/2
        entities.append(entity(identity,'EntityDoodad',5,prototype,position,(0,math.sin(angle),0,math.cos(angle))))
    for role,(identity,prototype) in export.ROLES.items():
        entities.append(entity(identity,'EntityNpc',1,prototype+int(broken=='prototype' and role==2),[-606,27.5,349]))
    creature = integer(0)+blob(integer(0)+integer(len(entities))+b''.join(entities))
    player = integer(0)+entity(20000,'EntityPlayer',0,-1,[-590,25,340])
    progress = integer(18)+integer(9032)*18+integer(0)+integer(5)
    progress += b''.join(integer(i)+integer(1) for i in range(9027,9032))
    progress += integer(1)+integer(9032)+integer(1)+string('STEP')+string('0')
    progress += integer(0)*2+integer(10)+integer(0)*3
    progress += integer(1)+integer(0)+floats([-599.7845,28.5,401.10635])
    progress += integer(0)*4+integer(5)
    progress += b''.join(integer(k)+integer(v) for k,v in ((1,20008),(2,20016),(5,20017),(3,20018),(4,20019)))
    progress += integer(0)*2
    write_records(directory,dict(ArchiveKeyCreature=creature,**{'Pathea.MainPlayer':player},ArchiveKeyMisRepository=progress))
    target = integer(1)+integer(9032)+string('The Delayed Supplies')
    target += b''.join(integer(v) for v in (20016,20016,1,1))
    target += string('Go to "Town%"004')+integer(1)+integer(4503)+integer(4)+integer(1)+floats([-284,25,134])
    target += integer(0)*13
    labels = {k:integer(0) for k in ('PeMap.StaticPoint+Mgr','ArchiveKeyMapUserLabel','PeMap.TowerMark+Mgr')}
    write_records(directory/'MainAdventure',dict(ArchiveKeyAdRMRepository=target,**labels),True)


class SettlementExportTest(unittest.TestCase):
    def test_source_slot_mapping_spawn_and_determinism(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            before = {str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*.arc')}
            m = export.build_manifest(root)
            self.assertEqual(m['lot']['origin'],[-623,16,327])
            self.assertEqual(m['lot']['size_x_z_y'],[55,100,14])
            self.assertEqual(m['player']['initial_spawn']['position'],[-606,28,348])
            self.assertEqual(m['player']['saved_pose']['position'],[-590,25,340])
            self.assertEqual(m['destination']['position'],[-284,25,134])
            self.assertEqual([b['prototype_id'] for b in m['buildings']],[163,164,162,161,145,146])
            self.assertEqual([b['slot'] for b in m['buildings']],list(range(6)))
            self.assertEqual(m['colony_candidates']['coordinates'],None)
            self.assertEqual(set(m['saved_label_counts'].values()),{0})
            self.assertEqual(export.build_outputs(root),export.build_outputs(root))
            self.assertEqual(before,{str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*.arc')})

    def test_slot_disagreement_nonfinite_and_role_identity_rejected(self):
        for broken,message in [('slot','authored lot slot'),('nan','Nonfinite'),('prototype','role/prototype')]:
            with self.subTest(broken=broken),tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                fixture(root,broken)
                with self.assertRaisesRegex(ValueError,message): export.build_manifest(root)

    def test_subworld_guid_range_and_truncation_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            world = root/'MainAdventure/world.arc'
            original = world.read_bytes()
            world.write_bytes(original[:-1])
            with self.assertRaisesRegex(ValueError,'length'): export.build_manifest(root)
            changed = bytearray(original)
            changed[4] ^= 1
            world.write_bytes(changed)
            with self.assertRaisesRegex(ValueError,'GUID'): export.build_manifest(root)
            world.write_bytes(original)
            index = root/'MainAdventure/index.arc'
            changed = bytearray(index.read_bytes())
            struct.pack_into('<q',changed,len(changed)-16,len(original)+1)
            index.write_bytes(changed)
            with self.assertRaisesRegex(ValueError,'range'): export.build_manifest(root)

    def test_authored_xml_change_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            xml = root/'town.xml'
            xml.write_text(export.DEFAULT_XML.read_text().replace('posIndex="0"','posIndex="3"',1))
            with self.assertRaisesRegex(ValueError,'building allocation'): export.build_manifest(root,town_xml=xml)

    def test_database_schema_and_role_mismatch_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            database = root/'source.sqlite'
            with closing(sqlite3.connect(export.DEFAULT_DATABASE.resolve().as_uri()+'?mode=ro',uri=True)) as source:
                with closing(sqlite3.connect(database)) as target: source.backup(target)
            with closing(sqlite3.connect(database)) as db:
                db.execute("UPDATE AdRandNPCMission SET RNPC_ID='23' WHERE ID='2'")
                db.commit()
            with self.assertRaisesRegex(ValueError,'role/prototype'): export.build_manifest(root,database)
            with closing(sqlite3.connect(database)) as db:
                db.execute('DROP TABLE Townhouse')
                db.commit()
            with self.assertRaises(sqlite3.Error): export.build_manifest(root,database)

    def test_target_parser_rejects_trailing_and_wrong_type(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            rows,_,_ = export.yird_records(root/'MainAdventure')
            data = rows['ArchiveKeyAdRMRepository']
            with self.assertRaisesRegex(ValueError,'Trailing'): export.decode_target(data+b'\0')
            changed = data.replace(integer(4503)+integer(4)+integer(1),integer(4503)+integer(3)+integer(1))
            with self.assertRaisesRegex(ValueError,'discovery4503'): export.decode_target(changed)


if __name__ == '__main__': unittest.main()
