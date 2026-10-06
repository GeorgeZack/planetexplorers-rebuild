"""Offline observation validation; does not launch the original client."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from analyze_reference_hunt_return import analyze


def fixture():
    header = dict(type='header', schema=1, kind='observed-original-hunt-return', mission=9127,
                  target=4525, phase='WaitForEndOfFrame', probeSourceSha256='a'*64)
    frames=[]
    for i in range(4):
        talk = dict(active=i in (1,2), playing=False, canClose=True, mode=0, index=int(i >= 2),
                    selectionMission=9127, npcEntity=20018, currentId=4105+int(i >= 2),
                    pages=[dict(id=t, npc=20018, missionTrigger=0, takeType=0) for t in (4105,4106)])
        frames.append(dict(type='frame', sample=i, frame=100+i, time=i*.1, realtime=50+i*.1,
                           player=dict(id=20000, position=[-271+i,25,136]),
                           missions={'9127':dict(active=i==0, completed=int(i>0)),
                                     '9077':dict(active=i==3, completed=0)},
                           inventory={str(k):int(i>0) for k in range(592,596)},
                           missionInventory=None, talk=talk))
    return [header]+frames+[dict(type='end', reason='stop-file', frames=4)]


class ReturnTests(unittest.TestCase):
    def read(self, rows):
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp)/'return.jsonl';p.write_text(''.join(json.dumps(r)+'\n' for r in rows))
            return analyze(p)

    def test_completion_rewards_before_final_talk_are_preserved(self):
        result=self.read(fixture())
        self.assertEqual(result['completion_transition']['after']['sample'],1)
        self.assertEqual(result['four_reward_gain_transition']['after']['sample'],1)
        self.assertEqual(result['first_visible_talk']['4106']['sample'],2)
        self.assertEqual(result['completion_transition']['before']['target_xz_distance_squared'],25)

    def test_unavailable_is_not_empty_inventory(self):
        rows=fixture();rows[1]['inventory']=None
        result=self.read(rows)
        self.assertIsNone(result['four_reward_gain_transition'])
        self.assertEqual(result['initial_player_inventory']['sample'],1)

    def test_existing_completed_checkpoint_does_not_fabricate_transition(self):
        rows=fixture()
        for row in rows[1:-1]:row['missions']['9127']=dict(active=False,completed=1)
        self.assertIsNone(self.read(rows)['completion_transition'])

    def test_missing_error_and_wrong_count_closure_rejected(self):
        rows=fixture()
        with self.assertRaises(ValueError):self.read(rows[:-1])
        for key,value in [('reason','error'),('frames',3),('frames',True)]:
            changed=copy.deepcopy(rows);changed[-1][key]=value
            with self.assertRaises(ValueError):self.read(changed)

    def test_invalid_inventory_missions_position_and_talk_rejected(self):
        for key,value in [('inventory',{'592':-1}),('missions',{'9127':True}),
                          ('player',dict(id=1,position=[0,float('nan'),0])),
                          ('talk',dict(active=True))]:
            rows=fixture();rows[1][key]=value
            with self.assertRaises(ValueError):self.read(rows)
        rows=fixture();rows[1]['talk']['currentId']=4106
        with self.assertRaises(ValueError):self.read(rows)

    def test_reversed_clock_and_sample_rejected_pause_allowed(self):
        rows=fixture();rows[2]['time']=rows[1]['time'];self.read(rows)
        rows[2]['time']=-1
        with self.assertRaises(ValueError):self.read(rows)
        rows=fixture();rows[2]['sample']=9
        with self.assertRaises(ValueError):self.read(rows)

    def test_hidden_queued_talk_is_not_visible(self):
        rows=fixture()
        for row in rows[1:-1]:row['talk']['active']=False
        self.assertEqual(self.read(rows)['first_visible_talk'],{})

    def test_mission_inventory_is_separate(self):
        rows=fixture()
        for row in rows[1:-1]:
            row['missionInventory']=row['inventory'];row['inventory']={str(k):0 for k in range(592,596)}
        self.assertIsNone(self.read(rows)['four_reward_gain_transition'])


if __name__ == '__main__':unittest.main()
