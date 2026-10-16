#!/usr/bin/env python3
"""Validate closed Gather observations and summarize recorded action/event edges.

No scheduler, input inference, pose qualification or harvest-commit inference.
Frame hierarchy checks reuse the ordinary observation validator.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import analyze_reference_pose as pose

EVENTS = {'animation-event', 'action-admission', 'action-end-notification'}
OUTCOMES = {'outcome-attach', 'outcome-snapshot', 'item-grant'}
ACTIONS = {1, 2, 3, 4, 13, 16, 32}
MAX_BAG_ITEMS = 1024
MAX_SLOTS_PER_TAB = 1024


def motion(row):
    pose.boolean(row['gatherMask'], 'Gather mask')
    actions = row['actions']
    if len(actions) != len(ACTIONS) or {a['id'] for a in actions} != ACTIONS:
        pose.fail('Unexpected motion action identities')
    for action in actions:
        pose.integer(action['id'], 'action id')
        running = pose.boolean(action['running'], 'running')
        for key in ('waitingRelation', 'doActionPending'):
            if running:
                pose.boolean(action[key], key)
            elif action[key] is not None:
                pose.fail('Stopped action has pending state')
    return next(a for a in actions if a['id'] == 32)


def handler_roles(handlers, outcomes=False):
    """Observer positions relative to the original Gather handler, by role.

    Legacy captures have one unlabelled observer (role pre). Outcome captures
    label each observer handler pre or post; a post handler is optional.
    """
    if not isinstance(handlers, list) or not 1 <= len(handlers) <= 256:
        pose.fail('Invalid event handler count')
    roles = {}; gather = []
    for i, handler in enumerate(handlers):
        for key in ('targetType', 'declaringType', 'method'):
            if handler[key] is not None and not isinstance(handler[key], str):
                pose.fail('Invalid handler identity')
        if pose.boolean(handler['observer'], 'observer'):
            role = handler.get('role') if outcomes else 'pre'
            if (not outcomes and 'role' in handler) or role not in ('pre', 'post') or role in roles:
                pose.fail('Ambiguous observer handler roles')
            roles[role] = i
        elif 'role' in handler:
            pose.fail('Role on a non-observer handler')
        if handler['targetType'] == 'Pathea.Action_Gather' and handler['method'] == 'OnAnimEvent':
            gather.append(i)
    if 'pre' not in roles or len(gather) > 1:
        pose.fail('Ambiguous observer/Gather handler order')
    return {role: ('Gather-handler-absent' if not gather else
                   'before-Gather-handler' if index < gather[0] else 'after-Gather-handler')
            for role, index in roles.items()}


def handler_phase(handlers, outcomes=False):
    return handler_roles(handlers, outcomes)['pre']


def tree(value):
    if value is None:
        return None
    pose.integer(value['terrainIndex'], 'terrain index'); pose.integer(value['prototype'], 'tree prototype', 0)
    for key in ('localPosition', 'worldPosition'):
        if not isinstance(value[key], list) or len(value[key]) != 3:
            pose.fail('Invalid tree position')
        for v in value[key]:
            pose.finite_number(v, 'tree position')
    return value


def bag(value):
    """Exact item instances by tab and slot; null stays null (never inferred)."""
    if value is None or value['slotLists'] is None:
        return value
    lists = value['slotLists']
    if not isinstance(lists, list) or len(lists) != 4:
        pose.fail('Invalid bag slot lists')
    items = 0; instances = set()
    for listing in lists:
        if listing is None:
            continue
        slots = pose.integer(listing['slots'], 'slot capacity', 0)
        if slots > MAX_SLOTS_PER_TAB:
            pose.fail('Bag slot bound exceeded')
        seen = set()
        for item in listing['items']:
            items += 1
            slot = pose.integer(item['slot'], 'slot', 0)
            if slot >= slots or slot in seen or items > MAX_BAG_ITEMS:
                pose.fail('Invalid bag slot')
            seen.add(slot)
            if pose.integer(item['instanceId'], 'item instance') in instances:
                pose.fail('Duplicate bag item instance')
            instances.add(item['instanceId']); pose.integer(item['protoId'], 'item prototype', 0)
            pose.integer(item['stackCount'], 'stack count', 0)
    return value


def outcome(row):
    kind = row['type']
    pose.integer(row['sequence'], 'sequence', 1)
    roles = handler_roles(row['handlers'], True)
    result = dict(type=kind, sequence=row['sequence'], frame=row['frame'], time=row['time'], roles=roles)
    if kind != 'outcome-attach':
        receipt = row['gatherReceipt']
        if receipt is not None:
            pose.integer(receipt, 'Gather receipt', 1)
        elif kind == 'outcome-snapshot':
            pose.fail('Outcome snapshot without a Gather receipt')
        result['gatherReceipt'] = receipt
    if kind == 'outcome-attach':
        if roles.get('post') != 'after-Gather-handler':
            pose.fail('Post observer attached without following the Gather handler')
    elif kind == 'item-grant':
        grant = row['grant']
        pose.integer(grant['protoId'], 'granted prototype', 0); pose.integer(grant['count'], 'granted count')
        result['grant'] = dict(protoId=grant['protoId'], count=grant['count'])
    else:
        if row['role'] not in ('pre', 'post') or row['role'] not in roles:
            pose.fail('Outcome snapshot role is not a recorded observer')
        hp = row['selectedHp']
        pose.boolean(hp['instance'], 'subterrain instance')
        if hp['instance']:
            if hp['entry'] is not None:
                pose.boolean(hp['entry'], 'HP entry')
            if hp['entry'] is True:
                pose.finite_number(hp['hp'], 'tree HP')
            elif hp['hp'] is not None:
                pose.fail('HP value without an entry')
        elif set(hp) != {'instance'}:
            pose.fail('Subterrain fields without an instance')
        result.update(role=row['role'], position=roles[row['role']], gatherAction=pose.boolean(row['gatherAction'], 'Gather action'),
                      selected=tree(row['selected']), found=tree(row['found']), selectedHp=hp, bag=bag(row['bag']))
    return result


def bag_items(value):
    if value is None or value['slotLists'] is None:
        return None
    return {item['instanceId']: dict(tab=tab, slot=item['slot'], protoId=item['protoId'], stackCount=item['stackCount'])
            for tab, listing in enumerate(value['slotLists']) if listing for item in listing['items']}


def check_receipts(events, outcomes):
    """Every outcome receipt reference must name a recorded Gather animation event."""
    gathers = {e['sequence'] for e in events if e['type'] == 'animation-event' and e['event'] == 'Gather'}
    for o in outcomes:
        if o.get('gatherReceipt') is not None and o['gatherReceipt'] not in gathers:
            pose.fail('Outcome row references no recorded Gather receipt')


def outcome_pairs(events, outcomes):
    """Pair the pre/post snapshots by their Gather receipt ID, for every Gather receipt.

    Report raw differences only. Receipts after an action end (for example on
    cancellation) stay in the list; cancellation itself remains unqualified.

    Original notifications may fall between the snapshots (for example an
    action end raised from the Gather handler); they do not break a pair.
    """
    attached = next((o['sequence'] for o in outcomes if o['type'] == 'outcome-attach'), None)
    gathers = [e for e in events if e['type'] == 'animation-event' and e['event'] == 'Gather']
    pairs = []
    for receipt in gathers:
        mine = [o for o in outcomes if o.get('gatherReceipt') == receipt['sequence']]
        pres = [o for o in mine if o['type'] == 'outcome-snapshot' and o['role'] == 'pre']
        posts = [o for o in mine if o['type'] == 'outcome-snapshot' and o['role'] == 'post']
        pre = pres[0] if len(pres) == 1 else None
        post = posts[0] if len(posts) == 1 else None
        nested = post is not None and any(receipt['sequence'] < g['sequence'] < post['sequence'] for g in gathers)
        if attached is None or attached > receipt['sequence']:
            status = 'post-not-attached-before-Gather'
        elif len(pres) > 1 or len(posts) > 1 or nested:
            status = 'nested-gather-unsupported'
        elif pre is None or post is None:
            status = 'partial-pair'
        elif (not receipt['sequence'] < pre['sequence'] < post['sequence']
              or pre['frame'] != receipt['frame'] or post['frame'] != receipt['frame']
              or pre['time'] != receipt['time'] or post['time'] != receipt['time']):
            status = 'invalid-pair'
        elif pre['position'] != 'before-Gather-handler' or post['position'] != 'after-Gather-handler':
            status = 'ambiguous-order'
        else:
            status = 'paired'
        grants = [o['grant'] for o in mine if o['type'] == 'item-grant'
                  and pre and post and pre['sequence'] < o['sequence'] < post['sequence']]
        before, after = (bag_items(pre['bag']) if pre else None), (bag_items(post['bag']) if post else None)
        changes = None
        if before is not None and after is not None:
            changes = [dict(instanceId=k, before=before.get(k), after=after.get(k))
                       for k in sorted(set(before) | set(after)) if before.get(k) != after.get(k)]
        pairs.append(dict(receiptSequence=receipt['sequence'], frame=receipt['frame'], status=status,
                          preHp=pre['selectedHp'] if pre else None, postHp=post['selectedHp'] if post else None,
                          selectedBefore=pre['selected'] if pre else None, selectedAfter=post['selected'] if post else None,
                          grantsBetween=grants, bagChanges=changes))
    return pairs


def edge(row, outcomes=False):
    result = {k: row[k] for k in ('type', 'sequence', 'frame', 'time', 'realtime', 'event', 'actionId', 'motion')}
    result['handlerPhase'] = handler_phase(row['handlers'], outcomes)
    full = [layer for layer in row['animator']['layers'] if layer['name'] == 'FullBody']
    if len(full) != 1:
        pose.fail('Expected one FullBody layer')
    result['fullBody'] = full[0]
    return result


def analyze(path):
    path = Path(path)
    if not path.is_file() or path.stat().st_size > pose.MAX_BYTES:
        pose.fail('Missing/oversized Gather capture')
    manifest_bytes = path.with_suffix('.launch.json').read_bytes()
    if len(manifest_bytes) > pose.MAX_LINE:
        pose.fail('Oversized Gather launch manifest')
    manifest = pose.decode(manifest_bytes)
    if manifest['schema'] != 'pe-gather-launch-1' or Path(manifest['capture']).absolute() != path.absolute():
        pose.fail('Mismatched Gather launch manifest')
    header = end = None; subscription = 0; frames = []; events = []; outcomes = []
    last_frame = -1; last_time = -1.; last_real = -1.; maximum = 0.
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        while line := stream.readline(pose.MAX_LINE + 1):
            digest.update(line)
            if len(line) > pose.MAX_LINE or stream.tell() > pose.MAX_BYTES or not line.endswith(b'\n'):
                pose.fail('Oversized/truncated Gather line')
            row = pose.decode(line)
            if not isinstance(row, dict) or end is not None:
                pose.fail('Invalid row/data after end')
            kind = row['type']
            if header is None:
                if (kind != 'header' or row['schema'] != 1 or row['kind'] != 'observed-original-gather-pose'
                        or row['callbackPhase'] != 'synchronous-event-dispatch'
                        or row['phase'] != 'WaitForEndOfFrame' or row['matrixOrder'] != 'row-major'):
                    pose.fail('Unsupported Gather header')
                if not 1 <= pose.integer(row['maxFrames'], 'frame bound') <= pose.MAX_FRAMES or row['maxBytes'] != pose.MAX_BYTES:
                    pose.fail('Invalid Gather bounds')
                sha = row['probeSourceSha256']
                if not isinstance(sha, str) or len(sha) != 64 or any(c not in '0123456789abcdef' for c in sha):
                    pose.fail('Invalid probe source digest')
                if sha != manifest['probe_source_sha256'] or row['maxFrames'] != manifest['frames']:
                    pose.fail('Probe digest or frame bound differs from launch manifest')
                if row.get('outcomes', False) is not manifest.get('outcomes', False) or ('outcomes' in row and row['outcomes'] is not True):
                    pose.fail('Outcome mode differs from launch manifest')
                header = row; continue
            if kind == 'humanTraits':
                if frames or subscription:
                    pose.fail('Late human traits')
                continue
            if kind == 'subscription':
                subscription += 1
                if subscription != 1 or frames or events:
                    pose.fail('Duplicate/late subscription')
                handler_phase(row['handlers'], header.get('outcomes', False)); continue
            if kind == 'end':
                pose.integer(row['frames'], 'end frame count', 0)
                pose.integer(row['eventRecords'], 'end event count', 0)
                if row['reason'] not in ('capture-limit', 'stop-file') or row['frames'] != len(frames) or row['eventRecords'] != len(events) + len(outcomes):
                    pose.fail('Failed closure or mismatched record counts')
                end = row; continue
            if kind in OUTCOMES and header.get('outcomes', False) and subscription == 1:
                frame = pose.integer(row['frame'], 'frame', 0)
                time = pose.finite_number(row['time'], 'time'); real = pose.finite_number(row['realtime'], 'realtime')
                if frame < last_frame or time < last_time or real < last_real:
                    pose.fail('Nonmonotonic Gather clock')
                last_frame, last_time, last_real = frame, time, real
                if len(events) + len(outcomes) >= 4096 or row['sequence'] != len(events) + len(outcomes) + 1:
                    pose.fail('Nonsequential/oversized event sequence')
                outcomes.append(outcome(row)); continue
            if kind not in EVENTS | {'frame'} or subscription != 1:
                pose.fail('Unknown/unsubscribed Gather record')
            frame = pose.integer(row['frame'], 'frame', 0)
            time = pose.finite_number(row['time'], 'time'); real = pose.finite_number(row['realtime'], 'realtime')
            if frame < last_frame or time < last_time or real < last_real or pose.finite_number(row['delta'], 'delta') < 0:
                pose.fail('Nonmonotonic Gather clock')
            last_frame, last_time, last_real = frame, time, real
            action = motion(row['motion'])
            if kind == 'frame':
                pose.integer(row['sample'], 'sample', 0)
                if row['sample'] != len(frames) or len(frames) >= header['maxFrames'] or (frames and frame <= frames[-1]['frame']):
                    pose.fail('Nonsequential/oversized frame sequence')
                _, _, error = pose.validate_frame(row); maximum = max(maximum, error)
                position = [row['playerRoot']['world'][i] for i in (3, 7, 11)]
                frames.append(dict(frame=frame, time=time, realtime=real, position=position,
                                   gatherMask=row['motion']['gatherMask'], action=action))
            else:
                pose.integer(row['sequence'], 'sequence', 1)
                if len(events) + len(outcomes) >= 4096 or row['sequence'] != len(events) + len(outcomes) + 1:
                    pose.fail('Nonsequential/oversized event sequence')
                pose.integer(row['actionId'], 'event action')
                if kind == 'animation-event':
                    if row['actionId'] != -1 or not isinstance(row['event'], str) or len(row['event']) > 1024:
                        pose.fail('Invalid animation event')
                elif row['event'] is not None or row['actionId'] < 0:
                    pose.fail('Invalid action edge')
                events.append(edge(row, header.get('outcomes', False)))
    if end is None or not frames:
        pose.fail('Missing closed capture frames')
    check_receipts(events, outcomes)
    pairs = outcome_pairs(events, outcomes) if header.get('outcomes', False) else None
    cycles = []
    for admission in (e for e in events if e['type'] == 'action-admission' and e['actionId'] == 32):
        finish = next((e for e in events if e['sequence'] > admission['sequence'] and e['type'] == 'action-end-notification' and e['actionId'] == 32), None)
        selected = [f for f in frames if admission['frame'] <= f['frame'] < (finish['frame'] if finish else math.inf)]
        executing = next((f for f in selected if f['action']['running'] and f['gatherMask'] and not f['action']['waitingRelation'] and not f['action']['doActionPending']), None)
        position = selected[0]['position'] if selected else None
        high = finish['sequence'] if finish else math.inf
        extra = (dict(outcomePairs=[q for q in pairs if admission['sequence'] <= q['receiptSequence'] <= high])
                 if header.get('outcomes', False) else {})
        cycles.append(dict(admissionFrame=admission['frame'], firstExecutingSample=executing, **extra,
                           endNotificationFrame=finish['frame'] if finish else None,
                           maximumRootDistanceFromAdmissionSample=max((math.dist(position, f['position']) for f in selected), default=None),
                           edges=[e for e in events if admission['sequence'] <= e['sequence'] <= (finish['sequence'] if finish else math.inf)]))
    return dict(schema='pe-gather-observation-report-1', captureSha256=digest.hexdigest(),
                launchManifestSha256=hashlib.sha256(manifest_bytes).hexdigest(),
                probeSourceSha256=header['probeSourceSha256'], unity=header['unity'],
                frames=len(frames), eventRecords=len(events) + len(outcomes), closure=end,
                outcomes=bool(header.get('outcomes', False)), outcomeRecords=len(outcomes),
                **({'outcomePairs': pairs} if pairs is not None else {}),
                maximumTrsHierarchyError=maximum, cycles=cycles,
                limits=[('Physical keys were not recorded; inventory only as outcome snapshots around Gather receipts.'
                         if header.get('outcomes', False) else 'Physical keys and inventory were not recorded.'),
                        'Callback state is labelled relative to Gather handler order, not harvest commit.',
                        'This report does not qualify a scheduler, blending or native lifecycle.']
                + (['Outcome snapshots report raw HP-entry and bag differences and grant receipts only; '
                    'no expected item, quantity or commit rule is inferred.']
                   if header.get('outcomes', False) else []))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    try:
        report = analyze(args.capture)
        with args.report.open('x') as output:
            json.dump(report, output, indent=2, allow_nan=False); output.write('\n')
        print(f"Validated {report['frames']} frames, {report['eventRecords']} event records, {len(report['cycles'])} Gather admissions")
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, f'Gather analysis: {error}\n')


if __name__ == '__main__':
    main()
