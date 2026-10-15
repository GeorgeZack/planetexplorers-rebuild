#!/usr/bin/env python3
"""Replay the source FullBody Gather clock against a closed original observation.

Offline only: no launch, no input, no native code. Serialized controller values
come from the SHA-bound clip plan (inventory_reference_bake.py). The engine
scheduling rules below were read from the captures, not from source; agreement
qualifies the replay only for the recorded frame deltas. Layer weight, pose
application and clip-info caching are not replayed.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path

import analyze_reference_gather as observation
from inventory_reference_bake import GATHER_CLIPS

pose = observation.pose

TOLERANCE = 1e-5
IDLE_BEHAVIOURS = [dict(script='AnimatorEventSender', enter_event='OnEndFullAnim', exit_event='', timed_events=0),
                   dict(script='AnimatorLayerWeightCtr', fadeout_speed=5.0)]
CAPTURED_RULES = [
    'Time zero is the first executing sample; HerbDown is the NEXT state there, at normalized time 0.',
    'The initial DoNothing normalized phase is copied from that sample as an initial condition.',
    'A motionless state advances at 1 normalized unit per second (the 0.1 entry lasts 0.1s).',
    'A transition starts exactly at its source exit threshold; the destination clock starts there.',
    'A threshold is crossed on the first update whose accumulated delta reaches it (t >= threshold).',
    'Exit events arrive on the update that completes a transition; the idle enter event on the update that starts one.',
]
LIMITS = [
    'Qualifies the FullBody controller state clock only: not action admission, deferred DoAction start, '
    'Gather mask release, poses or the harvest commit.',
    'Qualified only within the recorded delta domain; larger steps, multi-threshold updates, interruptions, '
    'cancellation and other speeds remain unqualified even where synthetic tests pass.',
    'FullBody layer weight and pose application are not replayed. A one-update weight-application lag is a '
    'supported provisional hypothesis only (see GATHER_ENTRY_POSE_DIAGNOSTIC).',
    'Clip-info arrays can lag completed blends; state hashes and the transition flag are compared instead.',
    'Event receipts are compared by frame only; handler side effects are not modelled.',
]


def fail(message):
    raise ValueError(message)


def signed(value):
    return value - (1 << 32) if value >= 1 << 31 else value


def finite(value, label, low=0.0):
    if type(value) not in (int, float) or not math.isfinite(value) or value < low:
        fail('Invalid ' + label)
    return float(value)


def digest(path):
    path = Path(path)
    if path.stat().st_size > pose.MAX_LINE:
        fail('Oversized replay input: ' + path.name)
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load_json(path):
    """Bounded strict decode: duplicate keys and nonfinite constants fail."""
    path = Path(path)
    if path.stat().st_size > pose.MAX_LINE:
        fail('Oversized replay input: ' + path.name)
    value = pose.decode(path.read_text())
    if not isinstance(value, dict):
        fail('Replay input must be an object: ' + path.name)
    return value


def exact(value, wanted):
    # Strict: True never stands for 1, nor 1.0 for True.
    return type(value) is type(wanted) and value == wanted


def load_plan(path, asset_sha256):
    plan = load_json(path)
    if not isinstance(plan, dict) or plan.get('schema') != 1 or plan.get('asset_sha256') != asset_sha256:
        fail('Plan does not match the observed asset')
    control = plan.get('gather_controller')
    if not isinstance(control, dict) or control.get('layer') != 'FullBody':
        fail('Plan lacks the FullBody Gather controller; regenerate the inventory')
    idle, entry, states, moves = control['idle'], control['entry'], control['states'], control['transitions']
    names = [s['path'] for s in states]
    if (idle['path'] != 'FullBody.DoNothing' or not exact(idle['motion'], False) or entry['trigger'] != 'Gather'
            or not exact(entry['has_exit_time'], False) or not exact(entry['fixed_duration'], False) or not exact(entry['offset'], 0.0)
            or names != ['FullBody.Gather.' + n for n in GATHER_CLIPS]
            or any(not exact(s['motion'], True) for s in states) or len(moves) != 3
            or [(m['source'], m['destination']) for m in moves] != list(zip(names, names[1:] + [idle['path']]))
            or any(not exact(m['has_exit_time'], True) or not exact(m['fixed_duration'], False) or not exact(m['offset'], 0.0)
                   for m in moves)):
        fail('Unsupported Gather controller graph')
    for s in [idle] + states:
        if (not exact(s['speed'], 1.0) or not exact(s['cycle_offset'], 0.0) or s['speed_parameter'] is not None
                or s['cycle_offset_parameter'] is not None or not exact(s['mirror'], False)):
            fail('Gather state clock configuration is outside the replayed source graph')
        for key in ('name_hash', 'full_path_hash'):
            if type(s[key]) is not int or not 0 <= s[key] < 1 << 32:
                fail('Invalid state hash')
    clips = {c['name']: c for c in plan['genders']['female']['clips']}
    lengths = []
    for state, move in zip(states, moves):
        name = state['path'].rsplit('.', 1)[1]
        clip = clips[name]
        if not exact(clip['path_id'], GATHER_CLIPS[name]):
            fail('Gather clip identity differs: ' + name)
        lengths.append(finite(clip['length'], 'clip length'))
        bound = clip.get('controller_state', {}).get('transitions')
        if (clip.get('controller_state', {}).get('path') != state['path'] or not isinstance(bound, list) or len(bound) != 1
                or bound[0]['m_ExitTime'] != move['exit_time'] or bound[0]['m_TransitionDuration'] != move['duration']):
            fail('Clip transition evidence differs from the controller record')
    return control, lengths


def timeline(control, lengths):
    """Seconds from time zero. Source values multiplied by source-state length."""
    entry = finite(control['entry']['duration'], 'entry duration') * 1.0  # captured: motionless length 1s
    x = [finite(m['exit_time'], 'exit time') for m in control['transitions']]
    d = [finite(m['duration'], 'duration') for m in control['transitions']]
    starts = [0.0]
    for i in range(2):
        starts.append(starts[i] + x[i] * lengths[i])
    exit_start = starts[2] + x[2] * lengths[2]
    completes = [starts[1] + d[0] * lengths[0], starts[2] + d[1] * lengths[1], exit_start + d[2] * lengths[2]]
    order = [entry, starts[1], completes[0], starts[2], completes[1], exit_start, completes[2]]
    if any(not 0 < a < b for a, b in zip(order, order[1:])) or not 0 < order[0]:
        fail('Overlapping Gather transitions are outside the modelled graph')
    return dict(entry_end=entry, starts=starts, exit_start=exit_start, completes=completes, lengths=lengths)


def predict(control, line, t, idle_phase):
    idle = control['idle']; s = control['states']; L = line['lengths']; st = line['starts']
    def state(item, value): return (signed(item['name_hash']), signed(item['full_path_hash']), value)
    none = (0, 0, 0.0)
    if t < line['entry_end']:
        return state(idle, idle_phase + t), state(s[0], t / L[0]), True
    if t < st[1]: return state(s[0], t / L[0]), none, False
    if t < line['completes'][0]: return state(s[0], t / L[0]), state(s[1], (t - st[1]) / L[1]), True
    if t < st[2]: return state(s[1], (t - st[1]) / L[1]), none, False
    if t < line['completes'][1]: return state(s[1], (t - st[1]) / L[1]), state(s[2], (t - st[2]) / L[2]), True
    if t < line['exit_start']: return state(s[2], (t - st[2]) / L[2]), none, False
    if t < line['completes'][2]:
        return state(s[2], (t - st[2]) / L[2]), state(idle, t - line['exit_start']), True
    return state(idle, t - line['exit_start']), none, False


def recorded(capture):
    """Validated observation plus each frame's delta and the qualified player's FullBody layer."""
    report = observation.analyze(capture)
    frames = {}; animator = None
    with Path(capture).open('rb') as stream:
        for line in stream:
            row = pose.decode(line)
            if row['type'] != 'frame':
                continue
            players = [a for a in row['animators'] if a['controller'] == 'FemaleAnimator2' and a['avatar'] == 'FemaleAvatar']
            if len(players) != 1:
                fail('Expected exactly one FemaleAnimator2/FemaleAvatar animator per frame')
            if animator is not None and players[0]['id'] != animator:
                fail('Observed player animator changed inside the capture')
            animator = players[0]['id']
            layers = [l for l in players[0]['layers'] if l['name'] == 'FullBody']
            if len(layers) != 1:
                fail('Expected one FullBody layer')
            frames[row['frame']] = (row['delta'], layers[0])
    return report, frames


def replay_cycle(control, line, cycle, frames):
    mismatches = []; margins = {}; rows = []
    start = cycle['firstExecutingSample']; end = cycle['endNotificationFrame']
    if start is None or end is None:
        return dict(mismatches=['Incomplete cycle: no executing sample or end notification'], frames=0, margins={}, receipts={})
    first = start['frame']
    span = list(range(first, end + 1))
    if any(f not in frames for f in span):
        return dict(mismatches=['Missing frame sample inside cycle'], frames=0, margins={}, receipts={})
    phase = frames[first][1]['current']['normalizedTime']
    thresholds = dict(entry_end=line['entry_end'], herb_start=line['starts'][1], herbdown_exit=line['completes'][0],
                      herbup_start=line['starts'][2], herb_exit=line['completes'][1],
                      idle_enter=line['exit_start'], herbup_exit=line['completes'][2])
    t = 0.0; previous = None; crossed = {}; deltas = []
    for frame in span:
        delta, layer = frames[frame]
        if frame != first:
            t += delta; deltas.append(delta)
        for name, value in thresholds.items():
            if name not in crossed and t >= value:
                crossed[name] = frame
                margins[name] = min(value - previous if previous is not None else math.inf, t - value)
        expected = predict(control, line, t, phase)
        observed = ((layer['current']['shortNameHash'], layer['current']['fullPathHash'], layer['current']['normalizedTime']),
                    (layer['next']['shortNameHash'], layer['next']['fullPathHash'], layer['next']['normalizedTime']),
                    layer['inTransition'])
        for label, e, o in (('current', expected[0], observed[0]), ('next', expected[1], observed[1])):
            if e[:2] != tuple(o[:2]) or abs(e[2] - o[2]) > TOLERANCE:
                mismatches.append(dict(frame=frame, t=t, field=label, expected=list(e), observed=list(o)))
        if expected[2] != observed[2]:
            mismatches.append(dict(frame=frame, t=t, field='inTransition', expected=expected[2], observed=observed[2]))
        rows.append(dict(frame=frame, t=t, error=max(abs(expected[0][2] - observed[0][2]), abs(expected[1][2] - observed[1][2]))))
        previous = t
    per_update = {}
    for frame in crossed.values():
        per_update[frame] = per_update.get(frame, 0) + 1
    if len(crossed) != len(thresholds):
        mismatches.append('Cycle ended before every Gather threshold was crossed')
    events = [e for e in cycle['edges'] if e['type'] == 'animation-event']
    receipts = {}
    for name, threshold in (('Gather', 'herb_exit'), ('GatherEnd', 'herbup_exit'), ('OnEndFullAnim', 'idle_enter')):
        got = [e['frame'] for e in events if e['event'] == name]
        want = [crossed[threshold]] if threshold in crossed else []
        receipts[name] = dict(predicted=want, recorded=got)
        if got != want:
            mismatches.append(dict(field='receipt', event=name, expected=want, observed=got))
    return dict(firstExecutingFrame=first, endNotificationFrame=end, frames=len(span), thresholdFrames=crossed,
                minimumThresholdMarginSeconds=margins, multiThresholdUpdates=sorted(f for f, n in per_update.items() if n > 1),
                deltaDomain=[min(deltas), max(deltas)] if deltas else None,
                maximumNormalizedError=max(r['error'] for r in rows), receipts=receipts, mismatches=mismatches)


def replay(capture, plan, reaction):
    capture = Path(capture)
    manifest = load_json(capture.with_suffix('.launch.json'))
    asset, assembly = manifest.get('asset_sha256'), manifest.get('game_assembly_sha256')
    if any(not isinstance(v, str) or len(v) != 64 for v in (asset, assembly)):
        fail('Launch manifest lacks the observed asset/assembly hashes')
    control, lengths = load_plan(plan, asset)
    contract = load_json(reaction)
    presentation = contract['full_body']
    if (contract['installed_assets'].get('sharedassets1.assets') != asset
            or contract['assemblies'].get('Assembly-CSharp.dll', {}).get('sha256') != assembly):
        fail('Reaction-presentation source contract was not recovered from the observed installation')
    if presentation.get('idle') != control['idle']['path'] or presentation.get('idle_behaviours') != IDLE_BEHAVIOURS:
        fail('Reaction-presentation idle contract differs from the replayed idle state')
    line = timeline(control, lengths)
    report, frames = recorded(capture)
    cycles = [replay_cycle(control, line, c, frames) for c in report['cycles']]
    qualified = bool(cycles) and not any(c['mismatches'] for c in cycles)
    return dict(schema='pe-gather-clock-replay-1', qualified=qualified, tolerance=TOLERANCE,
                captureSha256=report['captureSha256'], launchManifestSha256=report['launchManifestSha256'],
                planSha256=digest(plan), reactionPresentationSha256=digest(reaction), assetSha256=asset,
                gameAssemblySha256=assembly,
                timelineSeconds=line, capturedEngineRules=CAPTURED_RULES, limits=LIMITS, cycles=cycles)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[1]
    p.add_argument('capture', type=Path)
    p.add_argument('--plan', type=Path, default=root / 'build/animator-bake/clip-plan-gather-02.json')
    p.add_argument('--reaction', type=Path, default=root / 'generated/pe_reaction_presentation_source.json')
    p.add_argument('--report', type=Path, required=True)
    a = p.parse_args()
    try:
        result = replay(a.capture, a.plan, a.reaction)
        with a.report.open('x') as stream:
            json.dump(result, stream, indent=2, allow_nan=False); stream.write('\n')
    except (OSError, ValueError, KeyError, TypeError, IndexError) as error:
        p.exit(1, f'Gather clock replay: {error}\n')
    bad = sum(len(c['mismatches']) for c in result['cycles'])
    print(f"{len(result['cycles'])} cycles, {bad} mismatches; qualified={result['qualified']} (observed delta domain only)")
    return 0 if result['qualified'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
