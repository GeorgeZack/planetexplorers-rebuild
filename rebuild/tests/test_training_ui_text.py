"""Headless regression checks for source mission/target UI text export."""
from pathlib import Path
from contextlib import contextmanager
import json
import sqlite3
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
import export_training_ui_text as exporter


@contextmanager
def connection(path):
    db=sqlite3.connect(path)
    try:
        with db: yield db
    finally:
        db.close()


class TrainingUiTextTests(unittest.TestCase):
    def fixture(self, directory):
        path = Path(directory) / 'source.db'
        with connection(path) as db:
            db.executescript('''
                CREATE TABLE Quest_List(ID, MissionName, Type, Description, TargetIDList);
                CREATE TABLE Translation(ID, English);
                CREATE TABLE PrototypeItem(id, _nameID);
                CREATE TABLE PrototypeMonster(id, TranslationID);
                CREATE TABLE Quest_KillMonster(TargetID, Desc, MonsterID);
                CREATE TABLE Quest_LootItem(TargetID, Desc, ItemID, ItemNum);
                CREATE TABLE Quest_EnterArea(TargetID, Desc);
                CREATE TABLE Quest_UseItem(TargetID, Desc, ItemID, UseNum);
            ''')
            db.executemany('INSERT INTO Quest_List VALUES(?,?,?,?,?)',
                [(str(i), '0' if i==718 else '1', '2', '2', '0') for i in exporter.MISSIONS])
            db.executemany('INSERT INTO Translation VALUES(?,?)', [
                ('1','Title'),('2','Description'),('3','Acquire "itemnum%" units of "itemid%"'),
                ('4','Herbal Juice'),('5','Kill "monsternum%" "monsterid%"s'),('6','Lepus Hare')])
            db.execute("INSERT INTO PrototypeItem VALUES('916','4')")
            db.execute("INSERT INTO PrototypeMonster VALUES('13','6')")
            db.execute("INSERT INTO Quest_LootItem VALUES('2091','3','916','5')")
            db.execute("INSERT INTO Quest_KillMonster VALUES('1025','5','13_1_1')")
            db.execute("UPDATE Quest_List SET TargetIDList='2091' WHERE ID='741'")
            db.execute("UPDATE Quest_List SET TargetIDList='1025' WHERE ID='746'")
        return path

    def test_original_catalog_and_checked_in_outputs(self):
        outputs=exporter.build_outputs(exporter.DEFAULT_DATABASE,exporter.DEFAULT_LOCALIZATION)
        for name,data in outputs.items():
            self.assertEqual((exporter.DEFAULT_OUTPUT/name).read_bytes(),data)
        data=json.loads(outputs['pe_training_ui_text.json'])
        missions={m['mission_id']:m for m in data['missions']}
        targets={t['target_id']:t for t in data['objectives']}
        self.assertEqual(len(missions),19)
        self.assertEqual(missions[718]['title'],'')
        self.assertEqual(missions[718]['objective_count'],0)
        self.assertEqual(missions[747]['title'],'Move')
        self.assertEqual(missions[748]['title'],'Gathering and Foraging')
        self.assertEqual(missions[753]['title'],'Digging and Building')
        self.assertEqual(missions[756]['title'],'Create')
        self.assertEqual(targets[4114]['text'],'Press F5 to toggle to Control Mode')
        self.assertEqual(targets[2091]['text'],'Acquire 5 units of Herbal Juice')
        self.assertEqual(targets[1025]['text'],'Kill 1 lepus hares')
        self.assertEqual(targets[4066]['max_count'],0)
        self.assertEqual(targets[2032]['max_count'],5)
        self.assertEqual(targets[4106]['text'],'Build two walls to cover up all the lights')

    def test_expansion_order_empty_title_and_native_accessors(self):
        with tempfile.TemporaryDirectory() as directory:
            path=self.fixture(directory)
            out=exporter.build_outputs(path,None)
            (Path(directory)/'pe_training_ui_text.h').write_bytes(out['pe_training_ui_text.h'])
            source=Path(directory)/'check.c'
            source.write_text('''#include "pe_training_ui_text.h"
#include <string.h>
int main(void){
 const PeTrainingUiMissionText *m=pe_training_ui_mission_text(718);
 if(!m||m->title[0]||m->objective_count)return 1;
 const PeTrainingUiObjectiveText *t=pe_training_ui_mission_objective(741,0);
 if(!t||t->target_id!=2091||t->max_count!=5||strcmp(t->text,"Acquire 5 units of Herbal Juice"))return 2;
 if(pe_training_ui_mission_objective(741,1)||pe_training_ui_mission_text(-1)||pe_training_ui_objective_text(-1))return 3;
 return 0;
}
''')
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(source),'-o',str(Path(directory)/'check')],check=True,capture_output=True)
            subprocess.run([str(Path(directory)/'check')],check=True,capture_output=True)

    def test_localization_override_and_escaped_strings(self):
        with tempfile.TemporaryDirectory() as directory:
            path=self.fixture(directory);external=Path(directory)/'translation.db'
            with connection(external) as db:
                db.execute('CREATE TABLE Translation(ID,English)')
                db.executemany('INSERT INTO Translation VALUES(?,?)', [('1',''),('4','Juice "A"\\B – test')])
            out=exporter.build_outputs(path,external)
            data=json.loads(out['pe_training_ui_text.json'])
            self.assertEqual(data['missions'][0]['title'],'Title')
            self.assertEqual(data['objectives'][0]['text'],'Acquire 5 units of Juice "A"\\B – test')
            self.assertIn(b'\\042A\\042\\134B',out['pe_training_ui_text.h'])

    def test_missing_target_and_duplicate_target_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path=self.fixture(directory)
            with connection(path) as db:db.execute("UPDATE Quest_List SET TargetIDList='9999' WHERE ID='739'")
            with self.assertRaisesRegex(ValueError,'missing/unsupported target'):exporter.build_outputs(path,None)
            with connection(path) as db:db.execute("UPDATE Quest_List SET TargetIDList='2091,2091' WHERE ID='739'")
            with self.assertRaisesRegex(ValueError,'duplicate target'):exporter.build_outputs(path,None)

    def test_missing_translation_and_unknown_placeholder_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path=self.fixture(directory)
            with connection(path) as db:db.execute("DELETE FROM Translation WHERE ID='3'")
            with self.assertRaisesRegex(ValueError,'translation 3'):exporter.build_outputs(path,None)
            with connection(path) as db:db.execute('INSERT INTO Translation VALUES(?,?)',('3','Unknown "other%"'))
            with self.assertRaisesRegex(ValueError,'unresolved target placeholder'):exporter.build_outputs(path,None)

    def test_cli_check_rejects_stale_without_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            path=self.fixture(directory);out=Path(directory)/'out'
            args=[sys.executable,str(TOOLS/'export_training_ui_text.py'),'--database',str(path),'--embedded-only','--output',str(out)]
            subprocess.run(args,check=True,capture_output=True)
            subprocess.run(args+['--check'],check=True,capture_output=True)
            file=out/'pe_training_ui_text.h';file.write_text('stale')
            result=subprocess.run(args+['--check'],capture_output=True)
            self.assertNotEqual(result.returncode,0)
            self.assertEqual(file.read_text(),'stale')


if __name__=='__main__':unittest.main()
