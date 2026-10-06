#!/usr/bin/env python3
"""Validate a closed observational hunt capture; never create gameplay spawns."""
import argparse
import hashlib
import json
import math
from pathlib import Path

MAX_BYTES = 16 * 1024 * 1024


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def vector(value, count):
    return isinstance(value, list) and len(value) == count and all(finite(n) for n in value)


def entity_valid(entity):
    return (isinstance(entity, dict) and type(entity.get('id')) is int and entity['id'] != 0
            and type(entity.get('prototype')) is int and type(entity.get('prototypeType')) is int
            and vector(entity.get('position'), 3) and vector(entity.get('rotation'), 4)
            and vector(entity.get('scale'), 3) and all(n > 0 for n in entity['scale'])
            and all(finite(entity.get(k)) for k in ('hp', 'maxHp', 'attack', 'defense', 'campId', 'damageId', 'defaultPlayerId'))
            and entity['maxHp'] > 0 and 0 <= entity['hp'] <= entity['maxHp'] + .01)


def analyze(path):
    data = path.read_bytes()
    if not data or len(data) > MAX_BYTES:
        raise ValueError('Capture is empty or exceeds16MiB bound')
    rows = [json.loads(line) for line in data.splitlines()]
    if len(rows) < 2 or any(not isinstance(row, dict) for row in rows):
        raise ValueError('Missing structured header/end')
    header, end = rows[0], rows[-1]
    if (header.get('type') != 'header' or header.get('schema') != 1
            or header.get('kind') != 'observed-original-hunt-agents'
            or header.get('mission') != 9126 or header.get('target') != 1518
            or header.get('phase') != 'WaitForEndOfFrame'):
        raise ValueError('Unexpected observation schema')
    source_hash = header.get('probeSourceSha256', '')
    if not isinstance(source_hash, str) or len(source_hash) != 64 or any(c not in '0123456789abcdef' for c in source_hash):
        raise ValueError('Missing probe source hash')
    if end.get('type') != 'end' or end.get('reason') not in ('stop-file', 'time-limit', 'capture-limit', 'wait-limit'):
        raise ValueError('Missing clean bounded capture end')
    frames = rows[1:-1]
    if len(frames) > 3600 or type(end.get('frames')) is not int or end['frames'] != len(frames):
        raise ValueError('Capture frame count disagrees')
    first_complete = None
    previous = None
    previous_identity = None
    observed_ids = set()
    previous_frame = -1
    summaries = []
    for index, frame in enumerate(frames):
        if frame.get('type') != 'frame' or frame.get('sample') != index:
            raise ValueError('Nonsequential frame sample')
        if type(frame.get('frame')) is not int or frame['frame'] <= previous_frame:
            raise ValueError('Nonincreasing Unity frame')
        previous_frame = frame['frame']
        if not all(finite(frame.get(k)) for k in ('time', 'realtime')):
            raise ValueError('Nonfinite frame clock')
        if any(type(frame.get(k)) is not bool for k in ('missionActive', 'missionPointPresent')):
            raise ValueError('Invalid observation flags')
        if frame.get('player') is not None and not entity_valid(frame['player']):
            raise ValueError('Invalid observed player')
        agents = frame.get('agents')
        if not isinstance(agents, list) or len(agents) > 64:
            raise ValueError('Invalid bounded agent list')
        agent_ids, entity_ids = set(), set()
        complete = frame['missionActive'] and frame['missionPointPresent'] and len(agents) == 6 and entity_valid(frame.get('player'))
        for order, agent in enumerate(agents):
            if not isinstance(agent, dict) or agent.get('sourceOrder') != order or type(agent.get('agentId')) is not int:
                raise ValueError('Invalid source agent order/identity')
            identity = agent['agentId']
            if identity and identity in agent_ids:
                raise ValueError('Duplicate agent identity')
            agent_ids.add(identity)
            if (type(agent.get('prototype')) is not int or not vector(agent.get('agentPosition'), 3)
                    or not vector(agent.get('agentScale'), 3) or not vector(agent.get('agentRotation'), 4)
                    or type(agent.get('canRide')) is not bool or agent.get('step') not in ('NotCreated', 'InCreating', 'Created', 'Destroyed')):
                raise ValueError('Invalid agent metadata')
            entity = agent.get('entity')
            if entity is not None:
                if not entity_valid(entity) or entity['id'] in entity_ids:
                    raise ValueError('Invalid or duplicate live entity')
                entity_ids.add(entity['id'])
                observed_ids.add(entity['id'])
            complete &= (identity != 0 and agent['prototype'] == 13 and not agent['canRide']
                         and agent['step'] == 'Created' and entity is not None
                         and entity['prototype'] == 13 and entity['prototypeType'] == 3 and entity['hp'] > 0)
        counter = frame.get('counter')
        if counter is not None and (not isinstance(counter, str) or len(counter) > 64):
            raise ValueError('Invalid observed quest counter')
        identity = tuple((a['agentId'], a['entity']['id']) for a in agents) if complete else None
        if first_complete is None and identity is not None and identity == previous_identity:
            first_complete = previous
        previous, previous_identity = frame, identity
        summaries.append(dict(sample=index, frame=frame['frame'], agents=len(agents), entities=len(entity_ids), counter=counter, six_alive=bool(complete)))
    return dict(schema=1, evidence='Closed original runtime observation; no native spawn installation.',
                capture=str(path.resolve()), capture_sha256=hashlib.sha256(data).hexdigest(), header=header,
                end=end, frames=len(frames), observed_entity_ids=sorted(observed_ids), samples=summaries,
                first_complete_six_alive_frame=first_complete,
                center=None,
                limitations=['The random center is a source local variable and is not observed; no average or inferred center is supplied.',
                             'agentPosition is the source agent anchor after any deferred Y resolution; entity.position is the live pose.',
                             'Six membership identities must agree in two consecutive observed frames; this is not a static-pose or initial-spawn-timing claim.',
                             'A reload capture is a new encounter instance, not evidence of the exact original arrival instance.',
                             'No damage, kill, save, RNG, spawn or player input is issued by the observer.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--require-six', action='store_true')
    args = parser.parse_args()
    result = analyze(args.capture)
    if args.require_six and result['first_complete_six_alive_frame'] is None:
        raise ValueError('No complete six-alive mission cohort observed twice consecutively')
    if args.output.resolve() == args.capture.resolve():
        raise ValueError('Refusing to overwrite raw capture')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print('Observed frames:', result['frames'], 'complete six:', result['first_complete_six_alive_frame'] is not None)


if __name__ == '__main__':
    main()
