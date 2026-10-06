#!/usr/bin/env python3
"""Summarize closed, bounded original combat observations; never infer hit timing."""
import argparse
import hashlib
import json
from pathlib import Path
from analyze_reference_hunt_encounter import entity_valid, finite, vector

MAX_BYTES = 64 * 1024 * 1024
MELEE = {30100003, 30100004, 30100008, 30100046, 30100047}


def integer(n):
    return type(n) is int


def require(ok, message):
    if not ok:
        raise ValueError(message)


def combat_valid(c):
    require(isinstance(c, dict) and type(c.get('hasView')) is bool and vector(c.get('center'), 3)
            and finite(c.get('maxRadius')) and c['maxRadius'] >= 0, 'Invalid combat entity geometry')
    target = c.get('target')
    if target is not None:
        require(isinstance(target, dict) and integer(target.get('componentFrame')) and target['componentFrame'] >= 0
                and all(type(target.get(k)) is bool for k in ('canAttackFlag', 'canSearchFlag'))
                and all(target.get(k) is None or integer(target[k]) for k in ('selectedEntity', 'rawSelectedEntity', 'escapeEntity')),
                'Invalid selected target')
        sensors = target.get('sensors')
        require(isinstance(sensors, list) and len(sensors) <= 32, 'Invalid sensor list')
        seen = set()
        for s in sensors:
            require(isinstance(s, dict) and integer(s.get('id')) and s['id'] not in seen, 'Invalid sensor identity')
            seen.add(s['id'])
            require(s.get('kind') in ('vision', 'hearing') and isinstance(s.get('path'), str)
                    and all(type(s.get(k)) is bool for k in ('enabled', 'active'))
                    and finite(s.get('radius')) and s['radius'] >= 0
                    and all(vector(s.get(k), n) for k, n in [('position', 3), ('rotation', 4), ('localPosition', 3), ('lossyScale', 3)])
                    and integer(s.get('componentFrame')) and s['componentFrame'] >= 0 and integer(s.get('blockLayer')),
                    'Invalid live sensor settings')
            require(isinstance(s.get('entities'), list) and len(s['entities']) <= 512
                    and all(x is None or integer(x) for x in s['entities']), 'Invalid perception membership')
            if s['kind'] == 'vision':
                require(finite(s.get('angle')) and vector(s.get('axis'), 3) and vector(s.get('worldAxis'), 3), 'Invalid vision axis/angle')
            else:
                require(type(s.get('ignoreBlock')) is bool, 'Invalid hearing occlusion flag')
    skills = c.get('skills')
    require(isinstance(skills, list) and len(skills) <= 64, 'Invalid skill list')
    seen = set()
    for s in skills:
        require(isinstance(s, dict) and all(integer(s.get(k)) for k in ('instance', 'skill', 'guide', 'coolStat', 'hitCount'))
                and s['instance'] > 0 and s['instance'] not in seen and s['hitCount'] >= 0
                and all(type(s.get(k)) is bool for k in ('active', 'execExtEnable'))
                and finite(s.get('startTime')) and (s.get('targetEntity') is None or integer(s['targetEntity'])), 'Invalid skill state')
        seen.add(s['instance'])
    animators = c.get('animators')
    require(isinstance(animators, list) and len(animators) <= 8, 'Invalid animator list')
    for a in animators:
        require(isinstance(a, dict) and integer(a.get('id')) and isinstance(a.get('path'), str)
                and finite(a.get('speed')) and all(type(a.get(k)) is bool for k in ('enabled', 'active'))
                and isinstance(a.get('layers'), list) and len(a['layers']) <= 32, 'Invalid Animator metadata')
        for layer in a['layers']:
            require(isinstance(layer, dict) and isinstance(layer.get('name'), str) and finite(layer.get('weight'))
                    and type(layer.get('transition')) is bool, 'Invalid Animator layer')
            for k in ('current', 'next'):
                state = layer.get(k)
                require(isinstance(state, dict) and all(integer(state.get(x)) for x in ('fullPathHash', 'shortNameHash'))
                        and all(finite(state.get(x)) for x in ('normalizedTime', 'length', 'speed', 'speedMultiplier'))
                        and type(state.get('loop')) is bool, 'Invalid Animator state')
            clips = layer.get('clips')
            require(isinstance(clips, list) and len(clips) <= 64, 'Invalid clip list')
            for clip in clips:
                require(isinstance(clip, dict) and (clip.get('name') is None or isinstance(clip['name'], str))
                        and integer(clip.get('id')) and finite(clip.get('weight'))
                        and (clip.get('length') is None or finite(clip['length'])), 'Invalid clip')


def analyze(path):
    require(path.stat().st_size <= MAX_BYTES, 'Capture exceeds64MiB')
    data = path.read_bytes()
    rows = [json.loads(line) for line in data.splitlines()]
    require(len(rows) >= 2 and all(isinstance(x, dict) for x in rows), 'Missing header/end')
    header, end = rows[0], rows[-1]
    require(header.get('type') == 'header' and header.get('schema') == 1
            and header.get('kind') == 'observed-original-hunt-combat' and header.get('mission') == 9126
            and header.get('target') == 1518 and header.get('phase') == 'WaitForEndOfFrame', 'Unexpected combat schema')
    sha = header.get('probeSourceSha256', '')
    require(isinstance(sha, str) and len(sha) == 64 and all(x in '0123456789abcdef' for x in sha), 'Invalid probe hash')
    frames = rows[1:-1]
    require(end.get('type') == 'end' and end.get('reason') in ('capture-limit', 'time-limit', 'stop-file', 'wait-limit')
            and integer(end.get('frames')) and end['frames'] == len(frames) <= 3600, 'Incomplete/error capture')
    previous_frame, previous_time = -1, -1
    sensors, selected, records, actors = {}, {}, {}, set()
    target_changes, hp_changes = [], []
    last_hp = None
    for index, frame in enumerate(frames):
        require(frame.get('type') == 'frame' and frame.get('sample') == index and integer(frame.get('frame'))
                and frame['frame'] > previous_frame and finite(frame.get('time')) and frame['time'] >= previous_time
                and finite(frame.get('realtime')), 'Invalid frame sequence/clock')
        previous_frame, previous_time = frame['frame'], frame['time']
        require(all(type(frame.get(k)) is bool for k in ('missionActive', 'missionPointPresent')), 'Invalid mission flags')
        player = frame.get('player')
        if player is not None:
            require(entity_valid(player), 'Invalid player')
            combat_valid(player.get('combat'))
            if player['hp'] != last_hp:
                hp_changes.append(dict(sample=index, time=frame['time'], hp=player['hp']))
                last_hp = player['hp']
        agents = frame.get('agents')
        require(isinstance(agents, list) and len(agents) <= 64, 'Invalid agents')
        seen, agent_ids = set(), set()
        for order, agent in enumerate(agents):
            require(isinstance(agent, dict) and agent.get('sourceOrder') == order and integer(agent.get('agentId'))
                    and agent['agentId'] not in agent_ids and integer(agent.get('prototype'))
                    and all(vector(agent.get(k), n) for k, n in [('agentPosition', 3), ('agentScale', 3), ('agentRotation', 4)]), 'Invalid agent')
            agent_ids.add(agent['agentId'])
            e = agent.get('entity')
            if e is None:
                continue
            require(entity_valid(e) and e['id'] not in seen, 'Invalid/duplicate live entity')
            seen.add(e['id']);actors.add(e['id'])
            c = e.get('combat');combat_valid(c)
            target = c['target']
            if target is not None:
                state = target['selectedEntity']
                if e['id'] not in selected or selected[e['id']] != state:
                    target_changes.append(dict(sample=index, time=frame['time'], entity=e['id'], target=state))
                    selected[e['id']] = state
                for sensor in target['sensors']:
                    key = (e['id'], sensor['id'])
                    settings = {k: v for k, v in sensor.items() if k not in ('position', 'rotation', 'worldAxis', 'componentFrame', 'entities', 'lossyScale')}
                    entry = sensors.setdefault(key, dict(entity=e['id'], first_sample=index, first_observation=sensor, settings_variants=[]))
                    if settings not in entry['settings_variants']:
                        entry['settings_variants'].append(settings)
            for skill in c['skills']:
                key = skill['instance']
                record = records.setdefault(key, dict(entity=e['id'], skill=skill['skill'], instance=key,
                    start_time=skill['startTime'], first_sample=index, last_sample=index, observations=0, transitions=[]))
                require(record['entity'] == e['id'] and record['skill'] == skill['skill'], 'Reused skill observation identity')
                record['last_sample'] = index;record['observations'] += 1
                state = {k: skill[k] for k in ('guide', 'active', 'execExtEnable', 'coolStat', 'hitCount', 'targetEntity')}
                if not record['transitions'] or record['transitions'][-1]['state'] != state:
                    record['transitions'].append(dict(sample=index, frame=frame['frame'], time=frame['time'],
                        elapsed=frame['time']-skill['startTime'], state=state, animators=c['animators']))
    skills = list(records.values())
    return dict(schema=1, capture=str(path.resolve()), capture_sha256=hashlib.sha256(data).hexdigest(), header=header,
                frames=len(frames), end_reason=end['reason'], observed_entity_ids=sorted(actors),
                sensors=list(sensors.values()), selected_target_changes=target_changes, player_hp_changes=hp_changes,
                skills=skills, melee_instances=sum(s['skill'] in MELEE for s in skills),
                melee_contact_enabled_instances=sum(s['skill'] in MELEE and any(t['state']['execExtEnable'] for t in s['transitions']) for s in skills),
                limits='End-of-frame observations bracket transitions; they do not prove exact event time, contact damage attribution, or unobserved skills.')


def main():
    p = argparse.ArgumentParser(description=__doc__);p.add_argument('capture', type=Path)
    p.add_argument('--output', type=Path);p.add_argument('--require-melee', action='store_true');args=p.parse_args()
    try:
        result = analyze(args.capture)
        if args.require_melee:
            require(result['melee_contact_enabled_instances'] > 0, 'No contact-enabled melee observed')
        output = json.dumps(result, indent=2) + '\n'
        if args.output:
            args.output.write_text(output)
        else:
            print(output, end='')
        return 0
    except (OSError, ValueError, TypeError, KeyError) as e:
        p.exit(1, f'Combat capture: {e}\n')


if __name__ == '__main__':
    raise SystemExit(main())
