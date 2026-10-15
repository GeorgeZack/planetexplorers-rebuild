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
ACTIONS = {1, 2, 3, 4, 13, 16, 32}


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


def handler_phase(handlers):
    if not isinstance(handlers, list) or not 1 <= len(handlers) <= 256:
        pose.fail('Invalid event handler count')
    observer = []; gather = []
    for i, handler in enumerate(handlers):
        for key in ('targetType', 'declaringType', 'method'):
            if handler[key] is not None and not isinstance(handler[key], str):
                pose.fail('Invalid handler identity')
        if pose.boolean(handler['observer'], 'observer'):
            observer.append(i)
        if handler['targetType'] == 'Pathea.Action_Gather' and handler['method'] == 'OnAnimEvent':
            gather.append(i)
    if len(observer) != 1 or len(gather) > 1:
        pose.fail('Ambiguous observer/Gather handler order')
    return ('Gather-handler-absent' if not gather else
            'before-Gather-handler' if observer[0] < gather[0] else 'after-Gather-handler')


def edge(row):
    result = {k: row[k] for k in ('type', 'sequence', 'frame', 'time', 'realtime', 'event', 'actionId', 'motion')}
    result['handlerPhase'] = handler_phase(row['handlers'])
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
    header = end = None; subscription = 0; frames = []; events = []
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
                header = row; continue
            if kind == 'humanTraits':
                if frames or subscription:
                    pose.fail('Late human traits')
                continue
            if kind == 'subscription':
                subscription += 1
                if subscription != 1 or frames or events:
                    pose.fail('Duplicate/late subscription')
                handler_phase(row['handlers']); continue
            if kind == 'end':
                pose.integer(row['frames'], 'end frame count', 0)
                pose.integer(row['eventRecords'], 'end event count', 0)
                if row['reason'] not in ('capture-limit', 'stop-file') or row['frames'] != len(frames) or row['eventRecords'] != len(events):
                    pose.fail('Failed closure or mismatched record counts')
                end = row; continue
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
                if len(events) >= 4096 or row['sequence'] != len(events) + 1:
                    pose.fail('Nonsequential/oversized event sequence')
                pose.integer(row['actionId'], 'event action')
                if kind == 'animation-event':
                    if row['actionId'] != -1 or not isinstance(row['event'], str) or len(row['event']) > 1024:
                        pose.fail('Invalid animation event')
                elif row['event'] is not None or row['actionId'] < 0:
                    pose.fail('Invalid action edge')
                events.append(edge(row))
    if end is None or not frames:
        pose.fail('Missing closed capture frames')
    cycles = []
    for admission in (e for e in events if e['type'] == 'action-admission' and e['actionId'] == 32):
        finish = next((e for e in events if e['sequence'] > admission['sequence'] and e['type'] == 'action-end-notification' and e['actionId'] == 32), None)
        selected = [f for f in frames if admission['frame'] <= f['frame'] < (finish['frame'] if finish else math.inf)]
        executing = next((f for f in selected if f['action']['running'] and f['gatherMask'] and not f['action']['waitingRelation'] and not f['action']['doActionPending']), None)
        position = selected[0]['position'] if selected else None
        cycles.append(dict(admissionFrame=admission['frame'], firstExecutingSample=executing,
                           endNotificationFrame=finish['frame'] if finish else None,
                           maximumRootDistanceFromAdmissionSample=max((math.dist(position, f['position']) for f in selected), default=None),
                           edges=[e for e in events if admission['sequence'] <= e['sequence'] <= (finish['sequence'] if finish else math.inf)]))
    return dict(schema='pe-gather-observation-report-1', captureSha256=digest.hexdigest(),
                launchManifestSha256=hashlib.sha256(manifest_bytes).hexdigest(),
                probeSourceSha256=header['probeSourceSha256'], unity=header['unity'],
                frames=len(frames), eventRecords=len(events), closure=end,
                maximumTrsHierarchyError=maximum, cycles=cycles,
                limits=['Physical keys and inventory were not recorded.',
                        'Callback state is labelled relative to Gather handler order, not harvest commit.',
                        'This report does not qualify a scheduler, blending or native lifecycle.'])


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
