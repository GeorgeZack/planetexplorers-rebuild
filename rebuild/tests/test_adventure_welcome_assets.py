"""Bounded welcome identity and multi-slot armor reconstruction regressions."""
from contextlib import closing
from pathlib import Path
import sqlite3
import tempfile
import unittest

from test_adventure_supplier import fixture
from read_reference_adventure import decode


class WelcomeAssetsTest(unittest.TestCase):
    def fixture(self, directory, prototype_id=17):
        db=fixture(directory,entity_id=20008,prototype_id=prototype_id,role=1,
                   family='Nangle',given='Westina')
        with closing(sqlite3.connect(db)) as connection:
            connection.execute("UPDATE PrototypeItem SET _modelPath='Model/female05-Helmet',_replacePos=3073")
            connection.commit()
        return db

    def test_welcome_identity_composite_helmet_and_read_only(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);db=self.fixture(directory)
            before={p.name:p.read_bytes() for p in directory.iterdir()}
            appearance=decode(directory,20008,db)
            self.assertEqual(appearance['name'],'Westina Nangle')
            self.assertEqual(appearance['prototype_id'],17)
            self.assertEqual(appearance['adventure_role'],1)
            self.assertEqual(appearance['sex'],'female')
            self.assertEqual(appearance['slots'][:3],['Model/female05-Helmet']*3)
            self.assertEqual(len(dict.fromkeys(appearance['slots'])),6)
            self.assertEqual(before,{p.name:p.read_bytes() for p in directory.iterdir()})

    def test_wrong_prototype_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);db=self.fixture(directory,prototype_id=22)
            with self.assertRaisesRegex(ValueError,'Expected archived'):
                decode(directory,20008,db)

    def test_wrong_source_role_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);db=self.fixture(directory)
            with closing(sqlite3.connect(db)) as connection:
                connection.execute('UPDATE AdRandNPCMission SET RNPC_ID=22');connection.commit()
            with self.assertRaisesRegex(ValueError,'Adventure role'):
                decode(directory,20008,db)

    def test_unknown_slot_mask_and_male_equipment_rejected(self):
        for column,value,error in (('_replacePos',3073|32,'replacement mask'),('sex',2,'sex disagrees')):
            with tempfile.TemporaryDirectory() as temporary:
                directory=Path(temporary);db=self.fixture(directory)
                with closing(sqlite3.connect(db)) as connection:
                    connection.execute(f'UPDATE PrototypeItem SET {column}=?',(value,));connection.commit()
                with self.assertRaisesRegex(ValueError,error):decode(directory,20008,db)


if __name__=='__main__':unittest.main()
