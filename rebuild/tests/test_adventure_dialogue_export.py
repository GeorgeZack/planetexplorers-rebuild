"""Read-only source export checks; no graphics or game process."""
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_adventure_dialogue as exporter

class AdventureDialogueExportTests(unittest.TestCase):
    def test_original_and_regeneration(self):
        outputs=exporter.build_outputs(exporter.DEFAULT_DATABASE,exporter.DEFAULT_LOCALIZATION)
        for name,data in outputs.items():self.assertEqual((exporter.DEFAULT_OUTPUT/name).read_bytes(),data)
        data=json.loads(outputs['pe_adventure_dialogue_text.json']);lines={r['talk_id']:r for r in data['lines']}
        self.assertEqual([lines[i]['speaker'] for i in range(4090,4095)],[2,0,2,0,2])
        welcome=[1949]+list(range(4081,4089))
        self.assertEqual([row['talk_id'] for row in data['lines'][:9]],welcome)
        self.assertEqual([lines[i]['speaker'] for i in welcome],[1,0,1,0,1,0,1,0,1])
        self.assertIn('"seedname%"',lines[1949]['text'])
        self.assertIn('"AdvNPC%"002',lines[4088]['text'])
        self.assertIn('"Town%"004',lines[4090]['text']);self.assertIn('"AdvNPC%"001',lines[4089]['text'])
        self.assertEqual([lines[i]['speaker'] for i in range(4097,4101)],[0,0,0,3])
        self.assertEqual(next(m for m in data['missions'] if m['id']==9072)['title'],'Herb Collecting')
        self.assertEqual(data['arrival_target']['radius'],5)
        self.assertTrue(data['arrival_target']['truncate_target_to_integer'])
    def changed(self,sql):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'source.db'
            with sqlite3.connect(exporter.DEFAULT_DATABASE.resolve().as_uri()+'?mode=ro',uri=True) as source,sqlite3.connect(path) as output:
                source.backup(output);output.execute(sql);output.commit()
            with self.assertRaises(ValueError):exporter.build_outputs(path,None)
    def test_reject_changed_sequence(self):self.changed("UPDATE AdRandomQuest_List SET TalkOP='4094,4090' WHERE ID='9032'")
    def test_reject_changed_welcome_chain(self):self.changed("UPDATE AdRandomQuest_List SET NextQuest='9030' WHERE ID='9028'")
    def test_reject_changed_medicine_choice(self):self.changed("UPDATE AdRandomQuest_List SET player_talk='4098,4099' WHERE ID='9072'")
    def test_reject_changed_medicine_target(self):self.changed("UPDATE AdQuest_LootItem SET ItemNum='4' WHERE TargetID='2531'")
    def test_reject_wrong_speaker(self):self.changed("UPDATE Scenario SET npc_id='9041' WHERE talk_id='4090'")
    def test_reject_missing_english(self):self.changed("UPDATE Translation SET English='' WHERE ID='3004094'")

if __name__=='__main__':unittest.main()
