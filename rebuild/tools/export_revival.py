#!/usr/bin/env python3
"""Guard and export Revival Shot937's source rule; no game or original writes."""
import argparse
from contextlib import closing
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import sys
from export_content import ROOT, DEFAULT_DATABASE, connect_readonly, describe_source, numeric_rows
from export_starter_outfit import body, uncomment, expect

# Canonical full-method digests pin control/event order, not merely token presence.
# Comments/formatting are ignored; semantic edits require deliberate re-audit.
GUARDS = [('Assets/Scripts/PeEntity/Cmpt/UseItemCmpt.cs',
  '',
  'bool\\s+ConsumeItem\\s*\\(',
  '5701d3439d14af8dda539ddd6555407bd63b1b956830e63f407047ea493ec967'),
 ('Assets/Scripts/PeEntity/Cmpt/UseItemCmpt.cs',
  '',
  'public\\s+bool\\s+Revive\\s*\\(',
  '5d09b7b873c7bf6e38ee4c196439a908fa5ddcbfcd73a694810d67461c828458'),
 ('Assets/Scripts/PeEntity/Cmpt/UseItemCmpt.cs',
  '',
  'public\\s+bool\\s+Request\\s*\\(',
  '967f00ff321e314a6071dc84eda65854e8954544717710b8a3e30027bd7a0cd5'),
 ('Assets/Scripts/PeEntity/Cmpt/UseItemCmpt.cs',
  '',
  'public\\s+bool\\s+Use\\s*\\(ItemAsset.ItemObject item\\)',
  'a04750008f4e46d07d91592660f2e2351c30fc7bbad76b5119eaa3b35e149926'),
 ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Live.cs',
  'public class Action_Revive',
  'public\\s+override\\s+bool\\s+CanDoAction\\s*\\(',
  '98e369a2a38cc571351d1fdece7d8db594ac1be9732863bdb93b3e034f37d0e5'),
 ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Live.cs',
  'public class Action_Revive',
  'public\\s+override\\s+void\\s+DoAction\\s*\\(',
  'e16d6abb86058c21ab4ab6551c74fc995c9fff265264f478864d4dcc4e66a8d9'),
 ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Live.cs',
  'public class Action_Revive',
  'public\\s+override\\s+bool\\s+Update\\s*\\(',
  '044cfc364fe2a7e9132c690665e7eed3d945521caf3d01077dbc9d5b351c7672'),
 ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Live.cs',
  'public class Action_Revive',
  'public\\s+override\\s+void\\s+EndImmediately\\s*\\(',
  '49f2bd97b56e6d8b5ed47a3eeb8ba0e5e4ee2eae710d2c17e4a67eebec64017c'),
 ('Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Live.cs',
  'public class Action_Revive',
  'void\\s+OnEndAction\\s*\\(',
  '6654187fc1bdd4276615dcda1024eb9707541337d4f65bd6438a42923f9021ac'),
 ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIRevive.cs',
  '',
  'bool\\s+DoRevive\\s*\\(',
  '38731e10821eea9dd08b6c09acb7d377e1f1df0ffa0beb0ee28d865f3533826a'),
 ('Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIRevive.cs',
  '',
  'void\\s+BtnClick_OnRevive\\s*\\(',
  'd1e5c7e4fad70ec7dba85e5561e4495b5cad66d01bdf0da86fbb777f64ac8407'),
 ('Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs',
  '',
  'public\\s+void\\s+ActivateInjured\\s*\\(',
  'fa159f021a2644e6a5fe5e19e37e58d7d943e62c969298cf0fd8bad484d60ce3'),
 ('Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs',
  '',
  'void\\s+DoActivateInjured\\s*\\(',
  'b97a7fefd7e671e23291d1d75e1a32d5923e479e81d23abd4702fc688e46bdb8'),
 ('Assets/Scripts/Items/ItemObj.cs',
  'public class Consume',
  'public\\s+SkillSystem.SkInst\\s+StartSkSkill\\s*\\(',
  'ed59493a09d63640a1be5879ff800340fd58ebc80e7f2971e74a3e50a9d4f519'),
 ('Assets/Scripts/Items/ItemObj.cs',
  '',
  'static\\s+void\\s+LoadItemType\\s*\\(',
  '37c5b4fdb6053d04f7255b58860437cadbce72f34eed70613c3ae9d63cee9d33'),
 ('Assets/Scripts/Items/ItemObj.cs',
  '',
  'static\\s+ItemObject\\s+Create\\s*\\(',
  '898599098e873051da20b4e1821530e18279db2d5358744d32ab09937e1358e6'),
 ('Assets/Scripts/Items/ItemObj.cs',
  '',
  'public\\s+SkillSystem.SkInst\\s+StartSkSkill\\s*\\(',
  'e4b17fe6f9931b839edf4d154ecd3994841211b196d940dd3a00e3be67b7bc89'),
 ('Assets/Scripts/SkillSystem/SkInst.cs',
  '',
  'public\\s+static\\s+void\\s+LoadData\\s*\\(',
  'cdfa59ab1b3c9bd1aea9e190f611267f921ee587fab5d1e0ff1008e004fcdb14'),
 ('Assets/Scripts/SkillSystem/SkInst.cs',
  '',
  'private\\s+bool\\s+ClassifyTriggerEvents\\s*\\(',
  'a3600fd0f1b5ce7922dcb957e85c2aafbb87b362fbc70bfaa2dab954f2b1fe0e'),
 ('Assets/Scripts/SkillSystem/SkInst.cs',
  '',
  'private\\s+void\\s+ExecEventsInGuide\\s*\\(',
  '2fff7323a5264543583252b41db5cbb3d1cffb633a8b0b788fd8ade4b893baf0'),
 ('Assets/Scripts/SkillSystem/SkInst.cs',
  '',
  'private\\s+IEnumerator\\s+Exec\\s*\\(',
  '5e5938819a83d62a150d11cc3cd878f418594e18f382aaeff5be504679f91ab2'),
 ('Assets/Scripts/SkillSystem/SkTriggerEvent.cs',
  '',
  'public\\s+static\\s+void\\s+LoadData\\s*\\(',
  '79106d85c43655df357a6bbb4a09a4be2afb2877f26f6847ae1bc241a3461214'),
 ('Assets/Scripts/SkillSystem/SkTriggerEvent.cs',
  '',
  'public\\s+bool\\s+Exec\\s*\\(',
  'f840c3e802520f521eeff8d2c030e5ba9fcd1fe0738df9a8b4e56a47a7d867f3'),
 ('Assets/Scripts/SkillSystem/SkCond.cs',
  '',
  'private\\s+static\\s+SkCond\\s+CreateSub\\s*\\(',
  'b17ca48f31114dcd289de41d957f320534c8c4b703b2c9daeaa48e5a7a464a00'),
 ('Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs',
  '',
  'public\\s+enum\\s+AttribType',
  '6624b111906c4abbf29b39562e598917cc721b914814c5355b4ff3ac393f0e43'),
 ('Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs',
  '',
  'protected\\s+void\\s+SetAutoBuffActive\\s*\\(',
  'c6d800ccebe37a56b508fa5f93b00166810adbb890b7519c87b12ca61a06b773'),
 ('Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs',
  '',
  'void\\s+OnDeath\\s*\\(',
  'f9f23ec87b0a8c8afec3482c6bf627f96719e25078f177df7f508af40c628320'),
 ('Assets/Scripts/SkillSystem/SkEntity.cs',
  '',
  'public\\s+void\\s+SetAttribute\\(int type, float value, bool eventOff = true, bool bBoth = true\\)',
  'caac454d644e1a519af499fbbbde619fdcc7648430039fb75d97f5cdccdc9a8d'),
 ('Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs',
  '',
  'public\\s+void\\s+SetAttribute\\(AttribType type, float value, bool eventOff = true\\)',
  'd4908c4fef68cb7b4f035c05ce4a368f607b864d45fc68ea6c51f4229ddbab8b'),
 ('Assets/Scripts/Items/ItemObj.cs',
  '',
  'public\\s+int\\s+skillId',
  '8816249689140bcf43262fc756470f1bcade58cfa9729b8caceef625b9f67ec8'),
 ('Assets/Scripts/SkillSystem/SkEntity.cs',
  '',
  'public\\s+virtual\\s+SkInst\\s+StartSkill\\(SkEntity target, int id, ISkPara para = null, bool bStartImm '
  '= true\\)',
  '6425df3070774e90c184b26f3f256e216e7411904e228ff18ad7b550f4f35657')]
SOURCE_FILES = ('Assets/Scripts/PeEntity/Cmpt/UseItemCmpt.cs',
 'Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Live.cs',
 'Assets/Scripts/Chenzhi/NewUI/Scripts/PE_GameUI/NGUIScripts/UIRevive.cs',
 'Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs',
 'Assets/Scripts/Items/ItemObj.cs',
 'Assets/Scripts/SkillSystem/SkInst.cs',
 'Assets/Scripts/SkillSystem/SkTriggerEvent.cs',
 'Assets/Scripts/SkillSystem/SkCond.cs',
 'Assets/Scripts/PeEntity/Cmpt/PESkEntity.cs',
 'Assets/Scripts/Items/ItemData.cs',
 'Assets/Scripts/SkillSystem/SkAliveEntity.cs',
 'Assets/Scripts/SkillSystem/SkEntity.cs')
EVENTS = {53: 'exp,8,target.raws[8]-=25',
          59: 'exp,1,target.raws[1]=target.sums[0]*0.333f'}


def validate_sources(root):
    texts = {p: uncomment((root / p).read_text(encoding='utf-8-sig', errors='replace')) for p in SOURCE_FILES}
    for path, scope, pattern, expected in GUARDS:
        text = texts[path]
        if scope:
            if scope not in text:
                raise ValueError('Missing source scope ' + scope)
            text = text.split(scope, 1)[1]
        actual = hashlib.sha256(body(text, pattern).encode()).hexdigest()
        if actual != expected:
            raise ValueError('Revival source method changed: ' + path + ' ' + pattern)
    compact = lambda s: re.sub(r'\s+', '', s)
    require = {
        'Assets/Scripts/PeEntity/Cmpt/UseItemCmpt.cs': ['constintReviveItemProtoId=937;'],
        'Assets/Scripts/Items/ItemData.cs': ['protoItem.itemClassId=Convert.ToInt32(reader.GetString(reader.GetOrdinal("_function")));',
                                          'protoItem.skillId=Convert.ToInt32(reader.GetString(reader.GetOrdinal("skillId")));'],
        'Assets/Scripts/SkillSystem/SkAliveEntity.cs': ['caseEMsg.State_Revive:DispatchReviveEvent();SetAutoBuffActive(true);isDead=false;break;'],
    }
    for path, fragments in require.items():
        for fragment in fragments:
            if fragment not in compact(texts[path]):
                raise ValueError('Revival component/lifecycle binding changed: ' + path)
    return [describe_source(root / p) for p in SOURCE_FILES]


def build_outputs(database=DEFAULT_DATABASE, source_root=ROOT):
    source_files = validate_sources(source_root)
    with closing(connect_readonly(database)) as db:
        items, _ = numeric_rows(db, 'PrototypeItem', 'id')
        class_rows = db.execute("SELECT * FROM item_class WHERE id='55'").fetchall()
        if len(class_rows) != 1:
            raise ValueError('Missing/duplicate revival component class55')
        skills, _ = numeric_rows(db, 'skMain', '_id')
        events, _ = numeric_rows(db, 'skTriggerEvent', '_id')
        item, component, skill = dict(items[937]), dict(class_rows[0]), dict(skills[20150031])
        expect(item, {'id': 937, '_function': 55, 'skillId': 20150031, 'buffId': 0,
                      'stacking_num': 999, 'learn': 0, 'bundle': 0}, 'Revival item')
        expect(component, {'classname': 'Property,RecycleReplicate,Consume'}, 'Revival component list')
        expect(skill, {'_interruptable': 0, '_coolingTime': 0, '_coolingTimeType': 8,
                       '_coolingTimeShared': 0, '_cond': 'False', '_triggerEvents': '53,59',
                       **{k: 0 for k in ('_pretimeOfPrepare', '_postimeOfPrepare', '_effPrepare',
                          '_effMainOneTime', '_pretimeOfMain', '_timeOfMain', '_postimeOfMain',
                          '_effMainEachTime', '_pretimeOfEnding', '_postimeOfEnding', '_effEnding')}}, 'Revival skill')
        relation_rows = db.execute("SELECT * FROM ActionRelation WHERE ActionType='11'").fetchall()
        if len(relation_rows) != 1:
            raise ValueError('Missing/duplicate Revive action relation')
        relation = dict(relation_rows[0])
        required = {'ActionType': '11', 'Name': 'Revive', 'PauseAction': '40',
                    'EndAction': '0', 'EndImmediately': '9', 'Death': '1', 'Fall': '2', 'Ride': '2'}
        expect(relation, {key: required.get(key, '0') for key in relation}, 'Revive relation')
        trigger_rows = []
        for identity, expression in EVENTS.items():
            row = dict(events[identity])
            expect(row, {'_cond': 'True', '_modsCaster': 0, '_modsTarget': expression,
                         '_force': 0, '_effOnHitCaster': 0, '_effOnHitTarget': 0}, 'Revival event' + str(identity))
            trigger_rows.append(row)
    rule = dict(item_id=937, skill_id=20150031, comfort_event_id=53, hp_event_id=59,
                comfort_cost=25.0, hp_fraction=.333, minimum_hp=1.0, injured_grace_seconds=1.5,
                private_cooldown_seconds=0.0, shared_cooldown_seconds=0.0,
                ordinary_request_allowed=False, restore_stamina=True, restore_oxygen=True)
    manifest = dict(schema=1, source=describe_source(database), source_files=source_files,
                    method_guards=[dict(path=p, scope=s, pattern=r, canonical_sha256=h) for p,s,r,h in GUARDS],
                    rule=rule, source_item=item, source_component=component, source_skill=skill,
                    source_trigger_events=trigger_rows, source_action_relation=relation,
                    semantics=dict(event_order=[53,59], target='self via Property.StartSkSkill',
                        hp='Assignment max HP * .333f; not addition and not full healing. Action HP floor precedes the item skill.',
                        comfort='Subtract25 from raw comfort; ordinary attribute clamp/summation remains its own owner.',
                        ordinary_use='UseItemCmpt.Request explicitly rejects937, independent of zero skill cooldown.',
                        action_order=['HP floor1', 'oxygen maximum', 'stamina maximum', 'Revive mask', 'freeze physics',
                                      'State_Revive', 'deactivate ragdoll', 'disable injury'],
                        direct_attributes='Action SetAttribute defaults eventOff=true,bBoth=true: raw+sum updates without number callbacks.',
                        state_revive_order=['dispatch revive event', 'remount initial buffs', 'clear isDead'],
                        remount='No last-hurt/last-stamina timestamp reset occurs in the guarded revival handlers.',
                        ui_order='UIRevive calls DoRevive(false); only if action running, calls UseItem.Revive then Hide. Item result is ignored in source UI.',
                        consumption='Successful Consume skill start precedes DestroyItem(item,1), then item-use event dispatch.',
                        action_readiness='No view OR view.IsReadyGetUp(); action manager relations are a separate admission gate.',
                        action_end='Waits while view.IsRagdoll; end clears mask and physics freeze, then schedules injury enable after1.5 seconds.',
                        injury='BiologyView.ActivateInjured cancels previous delayed callback. Grace starts at action END, not button press.',
                        clocks='Zero skill phases and no private/shared cooldown; source guide uses ordered events once (loop condition False).'),
                    scope='Source metadata only. No death UI, ragdoll, respawn relocation, generic consumable enablement or engine scheduling implementation. Source labels do not establish healing magnitude.')
    header = '''/* Generated by export_revival.py; do not edit. */
#ifndef PE_REVIVAL_SOURCE_H
#define PE_REVIVAL_SOURCE_H
#include <stdbool.h>
typedef struct {
    int item_id, skill_id, comfort_event_id, hp_event_id;
    float comfort_cost, hp_fraction, minimum_hp, injured_grace_seconds;
    double private_cooldown_seconds, shared_cooldown_seconds;
    bool ordinary_request_allowed, restore_stamina, restore_oxygen;
} PeRevivalSource;
static const PeRevivalSource pe_revival_source = {
    937, 20150031, 53, 59, 25.0f, .333f, 1.0f, 1.5f,
    0.0, 0.0, false, true, true
};
#endif
'''
    return {'pe_revival_source.h': header.encode(),
            'pe_revival_source.json': (json.dumps(manifest, ensure_ascii=False, indent=2)+'\n').encode()}


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--database', type=Path, default=DEFAULT_DATABASE)
    p.add_argument('--output', type=Path, default=ROOT/'rebuild/generated')
    p.add_argument('--check', action='store_true')
    a=p.parse_args(argv)
    try:
        for name, data in build_outputs(a.database).items():
            path=a.output/name
            if a.check:
                if not path.is_file() or path.read_bytes()!=data:
                    raise ValueError('Stale generated '+name)
            else:
                a.output.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
    except (ValueError, KeyError, OSError, sqlite3.Error) as error:
        print(error, file=sys.stderr); return 1
    print('Revival metadata '+('checked' if a.check else 'generated')); return 0


if __name__=='__main__':
    raise SystemExit(main())
