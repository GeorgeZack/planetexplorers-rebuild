#!/usr/bin/env python3
"""Synthetic observations only; no original-game launch or parity claim."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import analyze_reference_gather as tool
from test_reference_pose_analysis import fixture


def rows():
    base, _ = fixture()
    header = base[0]
    header.update(kind='observed-original-gather-pose', callbackPhase='synchronous-event-dispatch', probeSourceSha256='a'*64)
    handlers = [dict(targetType='Probe', declaringType='Probe', method='Observe', observer=True),
                dict(targetType='Pathea.Action_Gather', declaringType='Pathea.Action_Gather', method='OnAnimEvent', observer=False)]
    def state(running):
        return dict(gatherMask=running, actions=[dict(id=i, running=running if i == 32 else False,
                    waitingRelation=False if i == 32 and running else None,
                    doActionPending=False if i == 32 and running else None) for i in sorted(tool.ACTIONS)])
    first = base[1]; first['motion'] = state(True)
    first['animators'][0]['layers'][0]['name'] = 'FullBody'
    second = base[2]; second['motion'] = state(False)
    def event(seq, kind, name, action, frame, time, real, running):
        return dict(type=kind, sequence=seq, frame=frame, time=time, realtime=real, delta=.1,
                    event=name, actionId=action, handlers=copy.deepcopy(handlers), motion=state(running),
                    animator=copy.deepcopy(first['animators'][0]))
    return [header, dict(type='subscription', frame=100, handlers=handlers),
            event(1, 'action-admission', None, 32, 100, 3., 4.9, False),
            event(2, 'animation-event', 'Gather', -1, 100, 3., 4.95, True), first,
            event(3, 'animation-event', 'GatherEnd', -1, 101, 4., 5.9, True),
            event(4, 'action-end-notification', None, 32, 101, 4., 5.95, False), second,
            dict(type='end', reason='stop-file', frames=2, eventRecords=4)]


class GatherAnalysisTests(unittest.TestCase):
    def analyze(self, records, source_hash="a"*64, outcomes=None):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'gather.jsonl'
            path.write_text(''.join(json.dumps(r)+'\n' for r in records))
            manifest = dict(schema='pe-gather-launch-1', capture=str(path), probe_source_sha256=source_hash, frames=10)
            if outcomes is not None: manifest['outcomes'] = outcomes
            path.with_suffix('.launch.json').write_text(json.dumps(manifest))
            return tool.analyze(path)

    def test_admission_is_not_execution_or_harvest(self):
        report = self.analyze(rows())
        self.assertEqual(report['frames'], 2)
        self.assertEqual(report['eventRecords'], 4)
        cycle = report['cycles'][0]
        self.assertFalse(cycle['edges'][0]['motion']['gatherMask'])
        self.assertTrue(cycle['firstExecutingSample']['gatherMask'])
        self.assertEqual(cycle['edges'][1]['handlerPhase'], 'before-Gather-handler')
        self.assertNotIn('harvestCommit', cycle)

    def test_missing_handler_and_late_observer_are_distinct(self):
        records = rows()
        records[2]['handlers'].reverse()
        records[3]['handlers'] = records[3]['handlers'][:1]
        report = self.analyze(records)
        self.assertEqual(report['cycles'][0]['edges'][0]['handlerPhase'], 'after-Gather-handler')
        self.assertEqual(report['cycles'][0]['edges'][1]['handlerPhase'], 'Gather-handler-absent')

    def test_bad_closure_counts_order_and_hierarchy_fail(self):
        cases = []
        r = rows(); r.pop(); cases.append(r)
        r = rows(); r[-1]['eventRecords'] = 5; cases.append(r)
        r = rows(); r[-1]['reason'] = 'event-error'; cases.append(r)
        r = rows(); r[3]['sequence'] = 9; cases.append(r)
        r = rows(); r[3]['realtime'] = 4.; cases.append(r)
        r = rows(); r[4]['transforms'][1]['world'][3] += 2; cases.append(r)
        r = rows(); r[3]['handlers'].append(copy.deepcopy(r[3]['handlers'][0])); cases.append(r)
        r = rows(); r.append(copy.deepcopy(r[-1])); cases.append(r)
        for index, records in enumerate(cases):
            with self.subTest(case=index), self.assertRaises(ValueError):
                self.analyze(records)

    def test_source_digest_must_match_launch_manifest(self):
        with self.assertRaisesRegex(ValueError, 'differs from launch manifest'):
            self.analyze(rows(), source_hash='b'*64)

    def test_partial_cycle_remains_explicit(self):
        records = rows()
        records[5]['event'] = 'Other'
        records[6]['actionId'] = 1
        report = self.analyze(records)
        self.assertIsNone(report['cycles'][0]['endNotificationFrame'])


def outcome_rows(post_after_gather=True, attach=True, post=True, pre_before_gather=True, intervening=None):
    """Legacy fixture plus opt-in outcome rows around its Gather receipt (synthetic)."""
    records = rows()
    header = records[0]; header['outcomes'] = True
    gather = dict(targetType='Pathea.Action_Gather', declaringType='Pathea.Action_Gather', method='OnAnimEvent', observer=False)
    pre = dict(targetType='Probe', declaringType='Probe', method='ObserveAnimation', observer=True, role='pre')
    after = dict(targetType='Probe', declaringType='Probe', method='ObserveAnimationPost', observer=True, role='post')
    def handlers(with_post=True):
        order = [pre, gather] if pre_before_gather else [gather, pre]
        if with_post: order = order + [after] if post_after_gather else [after] + order
        return copy.deepcopy(order)
    for r in records:
        if 'handlers' in r: r['handlers'] = handlers(r['type'] != 'subscription')
    records[1]['handlers'] = [copy.deepcopy(pre)]
    def item(slot, instance, proto, count): return dict(slot=slot, instanceId=instance, protoId=proto, stackCount=count)
    def bag(items): return dict(slotLists=[dict(slots=42, items=items), None, dict(slots=42, items=[]), dict(slots=10, items=[])])
    tree = dict(terrainIndex=3, localPosition=[1., 2., 3.], prototype=15, worldPosition=[11., 2., 13.])
    def snap(role, hp, items, frame=100):
        return dict(type='outcome-snapshot', frame=frame, time=3., realtime=4.95, handlers=handlers(), role=role, gatherAction=True,
                    selected=tree, found=tree, selectedHp=hp, bag=bag(items))
    extra = []
    if attach:
        extra.append((2, dict(type='outcome-attach', frame=100, time=3., realtime=4.92, handlers=handlers())))
    receipt = 3 if attach else 2
    tail = [snap('pre', dict(instance=False), [item(0, 7, 916, 9)])]
    tail.append(dict(type='item-grant', frame=100, time=3., realtime=4.95, handlers=handlers(), grant=dict(protoId=236, count=2)))
    if post:
        tail.append(snap('post', dict(instance=True, entry=True, hp=-9745.), [item(0, 7, 916, 9), item(1, 8, 236, 2)]))
    rows_out = records[:2]
    seq = 0
    body = records[2:-1]
    for i, r in enumerate(body):
        if r['type'] in tool.EVENTS:
            if attach and r['sequence'] == 2:
                seq += 1; a = extra[0][1]; a['sequence'] = seq; rows_out.append(a)
            seq += 1; r['sequence'] = seq; rows_out.append(r)
            if r['type'] == 'animation-event' and r['event'] == 'Gather':
                receipt_id = seq
                for t in tail:
                    if intervening is not None and t.get('role') == 'post':
                        # An original notification raised from inside the Gather handler.
                        extra_edge = copy.deepcopy(intervening); seq += 1; extra_edge['sequence'] = seq; rows_out.append(extra_edge)
                    seq += 1; t['sequence'] = seq; t['gatherReceipt'] = receipt_id; rows_out.append(t)
        else:
            rows_out.append(r)
    end = records[-1]; end['eventRecords'] = seq
    return rows_out + [end]


class GatherOutcomeTests(GatherAnalysisTests):
    def test_paired_snapshots_report_raw_differences_only(self):
        report = self.analyze(outcome_rows(), outcomes=True)
        self.assertTrue(report['outcomes']); self.assertEqual(report['outcomeRecords'], 4)
        pair = report['cycles'][0]['outcomePairs'][0]
        self.assertEqual(pair['status'], 'paired')
        self.assertEqual(pair['preHp'], dict(instance=False)); self.assertTrue(pair['postHp']['entry'])
        self.assertEqual(pair['grantsBetween'], [dict(protoId=236, count=2)])
        self.assertEqual(pair['bagChanges'], [dict(instanceId=8, before=None, after=dict(tab=0, slot=1, protoId=236, stackCount=2))])
        self.assertNotIn('expectedItems', pair)
        self.assertIn('pending runtime observation', report['limits'][-1])
        self.assertIn('outcome snapshots', report['limits'][0])

    def test_pair_order_frame_and_time_must_match_the_receipt(self):
        def renumber(records):
            for i, r in enumerate(x for x in records if 'sequence' in x): r['sequence'] = i + 1
            receipt = next(r for r in records if r.get('event') == 'Gather')['sequence']
            for r in records:
                if r['type'] in ('outcome-snapshot', 'item-grant'): r['gatherReceipt'] = receipt
        for change in ('future', 'frame', 'time'):
            records = outcome_rows()
            snaps = [r for r in records if r['type'] == 'outcome-snapshot']
            if change == 'future':
                # Same receipt ID, but pre recorded after post.
                a, b = records.index(snaps[0]), records.index(snaps[1])
                records[a], records[b] = records[b], records[a]
            elif change == 'frame':
                # Post recorded on the next frame, after that frame's GatherEnd receipt (clocks stay monotonic).
                records.remove(snaps[1])
                later = next(i for i, r in enumerate(records) if r.get('event') == 'GatherEnd')
                snaps[1].update(frame=101, time=4., realtime=5.9); records.insert(later + 1, snaps[1])
            else:
                # Same frame, but the receipt and pre were stamped at an earlier time than post.
                for r in records:
                    if r['type'] in ('action-admission',) or r.get('event') == 'Gather' or r.get('role') == 'pre' or r['type'] in ('item-grant', 'outcome-attach'):
                        if r.get('frame') == 100: r['time'] = 2.95 if r['type'] != 'action-admission' else 2.9
            renumber(records)
            with self.subTest(change=change):
                self.assertEqual(self.analyze(records, outcomes=True)['outcomePairs'][0]['status'], 'invalid-pair')

    def test_late_gather_receipt_after_action_end_stays_reported(self):
        records = outcome_rows()
        end = next(i for i, r in enumerate(records) if r['type'] == 'action-end-notification')
        late = copy.deepcopy(next(r for r in records if r.get('event') == 'Gather'))
        late.update(frame=101, time=4., realtime=5.96)
        records.insert(end + 1, late)
        for i, r in enumerate(x for x in records if 'sequence' in x): r['sequence'] = i + 1
        receipt = next(r for r in records if r.get('event') == 'Gather')['sequence']
        for r in records:
            if r['type'] in ('outcome-snapshot', 'item-grant'): r['gatherReceipt'] = receipt
        records[-1]['eventRecords'] += 1
        report = self.analyze(records, outcomes=True)
        self.assertEqual([q['status'] for q in report['outcomePairs']], ['paired', 'partial-pair'])
        self.assertEqual(len(report['cycles'][0]['outcomePairs']), 1)

    def test_legacy_limits_still_say_inventory_was_not_recorded(self):
        self.assertEqual(self.analyze(rows())['limits'][0], 'Physical keys and inventory were not recorded.')

    def test_intervening_original_edge_keeps_the_pair(self):
        records = outcome_rows()
        end_edge = copy.deepcopy(next(r for r in records if r['type'] == 'action-end-notification'))
        end_edge.update(frame=100, time=3., realtime=4.95)
        records = outcome_rows(intervening=end_edge)
        end_edge_count = sum(r['type'] == 'action-end-notification' for r in records)
        self.assertEqual(end_edge_count, 2)
        pair = self.analyze(records, outcomes=True)['cycles'][0]['outcomePairs'][0]
        self.assertEqual(pair['status'], 'paired'); self.assertEqual(pair['grantsBetween'], [dict(protoId=236, count=2)])

    def test_duplicate_instances_and_slot_bounds_fail(self):
        for change in ('duplicate', 'capacity'):
            records = outcome_rows()
            post = next(r for r in records if r.get('role') == 'post' and r['type'] == 'outcome-snapshot')
            if change == 'duplicate':
                post['bag']['slotLists'][2]['items'] = [dict(slot=0, instanceId=7, protoId=916, stackCount=1)]
            else:
                post['bag']['slotLists'][3]['slots'] = tool.MAX_SLOTS_PER_TAB + 1
            with self.subTest(change=change), self.assertRaisesRegex(ValueError, 'Duplicate bag item|slot bound'):
                self.analyze(records, outcomes=True)

    def test_receipt_references_and_nesting_fail_closed(self):
        records = outcome_rows()
        post = next(r for r in records if r.get('role') == 'post' and r['type'] == 'outcome-snapshot')
        post['gatherReceipt'] = 99
        with self.assertRaisesRegex(ValueError, 'references no recorded Gather'):
            self.analyze(records, outcomes=True)
        post['gatherReceipt'] = None
        with self.assertRaisesRegex(ValueError, 'without a Gather receipt'):
            self.analyze(records, outcomes=True)
        records = outcome_rows()
        pre = next(r for r in records if r.get('role') == 'pre' and r['type'] == 'outcome-snapshot')
        index = records.index(pre)
        duplicate = copy.deepcopy(pre); records.insert(index + 1, duplicate)
        for i, r in enumerate(x for x in records if 'sequence' in x):
            r['sequence'] = i + 1
        receipt = next(r for r in records if r.get('event') == 'Gather')['sequence']
        for r in records:
            if 'gatherReceipt' in r: r['gatherReceipt'] = receipt
        records[-1]['eventRecords'] += 1
        self.assertEqual(self.analyze(records, outcomes=True)['cycles'][0]['outcomePairs'][0]['status'], 'nested-gather-unsupported')

    def test_missing_post_late_attach_and_order_are_not_paired(self):
        self.assertEqual(self.analyze(outcome_rows(post=False), outcomes=True)['cycles'][0]['outcomePairs'][0]['status'], 'partial-pair')
        self.assertEqual(self.analyze(outcome_rows(attach=False), outcomes=True)['cycles'][0]['outcomePairs'][0]['status'],
                         'post-not-attached-before-Gather')
        self.assertEqual(self.analyze(outcome_rows(pre_before_gather=False), outcomes=True)['cycles'][0]['outcomePairs'][0]['status'],
                         'ambiguous-order')

    def test_post_before_gather_handler_cannot_attach(self):
        with self.assertRaisesRegex(ValueError, 'Post observer attached'):
            self.analyze(outcome_rows(post_after_gather=False), outcomes=True)

    def test_mode_roles_and_counts_are_strict(self):
        with self.assertRaisesRegex(ValueError, 'Outcome mode differs'):
            self.analyze(outcome_rows())
        legacy = rows(); legacy[2]['handlers'][0]['role'] = 'pre'
        with self.assertRaisesRegex(ValueError, 'roles'):
            self.analyze(legacy)
        doubled = outcome_rows(); doubled[3]['handlers'][2]['role'] = 'pre'
        with self.assertRaises(ValueError):
            self.analyze(doubled, outcomes=True)
        miscounted = outcome_rows(); miscounted[-1]['eventRecords'] -= 4
        with self.assertRaisesRegex(ValueError, 'mismatched record counts'):
            self.analyze(miscounted, outcomes=True)
        # A legacy-mode capture can carry neither outcome roles nor outcome rows.
        stripped = outcome_rows(); stripped[0].pop('outcomes')
        for r in stripped:
            for h in r.get('handlers', []): h.pop('role', None)
        with self.assertRaisesRegex(ValueError, 'Unknown/unsubscribed|roles'):
            self.analyze(stripped)

    def test_absent_entry_and_null_bag_stay_explicit(self):
        records = outcome_rows()
        snapshot = next(r for r in records if r.get('role') == 'post' and r['type'] == 'outcome-snapshot')
        snapshot['selectedHp'] = dict(instance=True, entry=False, hp=None); snapshot['bag'] = None
        pair = self.analyze(records, outcomes=True)['cycles'][0]['outcomePairs'][0]
        self.assertEqual(pair['postHp'], dict(instance=True, entry=False, hp=None)); self.assertIsNone(pair['bagChanges'])
        snapshot['selectedHp'] = dict(instance=True, entry=False, hp=255.)
        with self.assertRaisesRegex(ValueError, 'without an entry'):
            self.analyze(records, outcomes=True)


if __name__ == '__main__':
    unittest.main()
