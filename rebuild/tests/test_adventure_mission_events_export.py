"""Read-only source/event metadata checks; altered inputs are disposable copies."""
from contextlib import closing
import json
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import export_adventure_mission_events as e
class MissionEventExportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.outputs=e.build_outputs();cls.manifest=json.loads(cls.outputs['pe_adventure_mission_events_source.json'])
        cls.missions={r['mission']:r for r in cls.manifest['missions']}
    def test_regeneration(self):
        for name,data in self.outputs.items():self.assertEqual((e.OUTPUT/name).read_bytes(),data)
    def test_prerequisites_are_not_next_acceptance(self):
        for mission,pre in ((9077,9127),(9078,9077),(9079,9078)):
            self.assertEqual(self.missions[mission]['prerequisite'],pre)
            self.assertEqual(self.missions[mission]['next'],0)
        self.assertEqual(self.missions[9080]['next'],9084)
        self.assertEqual(self.missions[9084]['next'],9114)
    def test_typed_acceptance_plot_and_grants(self):
        self.assertEqual(self.missions[9079]['accept_plot'],5)
        self.assertEqual(self.missions[9079]['complete_plot'],0)
        self.assertEqual(self.manifest['plots']['5']['GetMission'],'9080')
        row=self.missions[9080]
        self.assertFalse(row['opening']);self.assertFalse(row['complete_items'])
        self.assertEqual(row['accept_items'],[dict(item_id=i,count=1) for i in (657,662,660)])
        self.assertTrue(row['any_target']);self.assertEqual(row['targets'],[5509,5510,5511])
        for index,target in enumerate(row['targets'],1):
            r=self.manifest['targets'][str(target)]['row']
            self.assertEqual(r['ItemID'],'1127');self.assertEqual(r['Pos'],f'2_{index}_0_100')
            self.assertEqual(r['AllowOld'],'1');self.assertEqual(r['ComMission'],'1')
    def test_pigment_transaction_and_choice_shells(self):
        pigment=self.missions[9082]
        self.assertEqual(pigment['accept_items'],[dict(item_id=597,count=1)])
        self.assertEqual(pigment['complete_remove'],[dict(item_id=1038,count=50)])
        self.assertEqual([x['item_id'] for x in pigment['complete_items']],[478,490,696,465,474,473,481])
        self.assertTrue(pigment['increase_chain']);self.assertFalse(pigment['auto_reply']);self.assertEqual(pigment['ending'],4121)
        self.assertEqual(self.missions[9081]['choices'],[9082,9083])
        self.assertEqual(self.missions[9081]['mutex'],[9082,9083]);self.assertEqual(self.missions[9083]['mutex'],[9082])
        self.assertTrue(self.missions[9081]['repeatable']);self.assertTrue(self.missions[9083]['repeatable'])
    def test_roles_and_source_references_not_invented_npc_positions(self):
        talks={r['id']:r for r in self.manifest['dialogue_roles']}
        self.assertEqual([talks[i]['role'] for i in (4107,4108,4117,4121,4122)],[4,3,5,5,1])
        self.assertEqual(self.manifest['targets']['4504']['row']['DistPos'],'3_2_0_5')
        self.assertEqual(self.manifest['targets']['4505']['row']['DistPos'],'3_5_0_5')
        self.assertEqual(len(self.manifest['guarded_methods']),16)
    def test_changed_event_rows_fail_closed(self):
        changes=("UPDATE Scenario SET npc_id='-9899' WHERE talk_id='4117'",
            "UPDATE AdRandomQuest_List SET NextQuest='9078' WHERE ID='9077'",
            "UPDATE AdRandomQuest_List SET Get_MissionItem='657_2,662_1,660_1' WHERE ID='9080'",
            "UPDATE AdRandomQuest_List SET AdvPlot='3_5' WHERE ID='9079'",
            "UPDATE AdRandomQuest_List SET CoRemoveItem='1038_49' WHERE ID='9082'",
            "UPDATE AdvPlot SET GetMission='9082' WHERE ID='5'",
            "UPDATE AdQuest_UseItem SET ComMission='0' WHERE TargetID='5509'")
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'rows.db'
            with closing(e.connect_readonly(e.DEFAULT_DATABASE)) as original,closing(sqlite3.connect(path)) as changed:
                for sql in changes:
                    with self.subTest(sql=sql):
                        original.backup(changed);changed.execute(sql);changed.commit()
                        with self.assertRaises(ValueError):e.build_outputs(path)
    def test_changed_acceptance_and_recorded_grant_source_rejected(self):
        path='Assets/Scripts/Mission/PlayerMission.cs'
        mutations=((path,'!HasGetRewards(MissionID)','HasGetRewards(MissionID)'),
            (path,'bw.Write(m_GetRewards[i]);','bw.Write(0);'),
            (path,'return data.m_comMission;','return false;'),
            ('Assets/Scripts/Mission/StroyManager.cs','addata.m_getMissionID.Count','0'),
            ('Assets/Scripts/Mission/Mission.cs','Story_Info_Get = 1','Story_Info_Get = 2'),
            ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UINPCTalk.cs','i == talklist.Count - 1','i == 0'))
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            for name,_,_ in e.METHODS:
                target=root/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(e.ROOT/name,target)
            self.assertEqual(len(e.source_guards(root)),16)
            for name,old,new in mutations:
                with self.subTest(change=old):
                    target=root/name;original=target.read_text(encoding='utf-8-sig',errors='replace');self.assertIn(old,original)
                    target.write_text(original.replace(old,new))
                    with self.assertRaises(ValueError):e.source_guards(root)
                    target.write_text(original)
if __name__=='__main__':unittest.main()
