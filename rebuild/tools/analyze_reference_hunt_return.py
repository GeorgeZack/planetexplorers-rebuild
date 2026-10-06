#!/usr/bin/env python3
"""Summarize a closed original return observation; no gameplay or save mutation."""
import argparse
import hashlib
import json
import math
from pathlib import Path

MAX_BYTES = 16 * 1024 * 1024
TARGET = [-266, 25, 136]
ITEMS = ('592', '593', '594', '595')


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def integer(value, low=0):
    return type(value) is int and value >= low


def snapshot(frame):
    player = frame['player']
    return dict(sample=frame['sample'], frame=frame['frame'], time=frame['time'], realtime=frame['realtime'],
                player=player, target_xz_distance_squared=None if player is None else
                (player['position'][0]-TARGET[0])**2+(player['position'][2]-TARGET[2])**2,
                missions=frame['missions'], inventory=frame['inventory'],
                missionInventory=frame['missionInventory'], talk=frame['talk'])


def analyze(path):
    data = path.read_bytes()
    if not data or len(data) > MAX_BYTES:
        raise ValueError('Empty capture or observation byte bound exceeded')
    rows = [json.loads(line) for line in data.splitlines()]
    if len(rows) < 2 or any(not isinstance(row, dict) for row in rows):
        raise ValueError('Missing structured header/end')
    header, end = rows[0], rows[-1]
    if (header.get('type') != 'header' or header.get('schema') != 1
            or header.get('kind') != 'observed-original-hunt-return'
            or header.get('mission') != 9127 or header.get('target') != 4525
            or header.get('phase') != 'WaitForEndOfFrame'):
        raise ValueError('Unexpected return schema')
    digest = header.get('probeSourceSha256')
    if not isinstance(digest, str) or len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest):
        raise ValueError('Missing probe source hash')
    frames = rows[1:-1]
    if (end.get('type') != 'end' or end.get('reason') not in ('stop-file', 'time-limit', 'capture-limit', 'wait-limit')
            or not integer(end.get('frames')) or end['frames'] != len(frames) or len(frames) > 3600):
        raise ValueError('Missing clean bounded capture closure')
    previous = None
    transitions = []
    first_active = first_completed = first_reward_gain = None
    talk_first = {}
    initial_inventory = last_inventory = None
    for index, frame in enumerate(frames):
        if frame.get('type') != 'frame' or type(frame.get('sample')) is not int or frame['sample'] != index:
            raise ValueError('Nonsequential observation samples')
        if not integer(frame.get('frame')) or not all(finite(frame.get(k)) for k in ('time', 'realtime')):
            raise ValueError('Invalid observation clocks')
        if previous and (frame['frame'] <= previous['frame'] or frame['time'] < previous['time'] or frame['realtime'] < previous['realtime']):
            raise ValueError('Observation clocks reversed')
        for key in ('player', 'missions', 'inventory', 'missionInventory', 'talk'):
            if key not in frame:
                raise ValueError('Missing observation field: '+key)
        player = frame['player']
        if player is not None:
            if (not isinstance(player, dict) or not integer(player.get('id'), 1)
                    or not isinstance(player.get('position'), list) or len(player['position']) != 3
                    or not all(finite(v) for v in player['position'])):
                raise ValueError('Invalid player observation')
        missions = frame['missions']
        if missions is not None:
            if not isinstance(missions, dict) or set(missions) != {'9127', '9077'}:
                raise ValueError('Invalid mission observation')
            for state in missions.values():
                if not isinstance(state, dict) or type(state.get('active')) is not bool or not integer(state.get('completed')):
                    raise ValueError('Invalid mission state/count')
        for key in ('inventory', 'missionInventory'):
            inventory = frame[key]
            if inventory is not None and (not isinstance(inventory, dict) or set(inventory) != set(ITEMS)
                                          or not all(integer(v) for v in inventory.values())):
                raise ValueError('Invalid inventory counts')
        talk = frame['talk']
        if talk is not None:
            if (not isinstance(talk, dict) or any(type(talk.get(k)) is not bool for k in ('active', 'playing', 'canClose'))
                    or not integer(talk.get('index'), -1) or not integer(talk.get('mode'))
                    or type(talk.get('selectionMission')) is not int
                    or talk.get('npcEntity') is not None and not integer(talk['npcEntity'], 1)
                    or not isinstance(talk.get('pages'), list) or len(talk['pages']) > 256):
                raise ValueError('Invalid talk metadata')
            for page in talk['pages']:
                if not isinstance(page, dict) or any(type(page.get(k)) is not int for k in ('id', 'npc', 'missionTrigger', 'takeType')):
                    raise ValueError('Invalid talk page')
            current = talk['pages'][talk['index']]['id'] if 0 <= talk['index'] < len(talk['pages']) else None
            if talk.get('currentId') != current:
                raise ValueError('Talk index/current ID disagree')
            if talk['active'] and current in (4105, 4106) and str(current) not in talk_first:
                talk_first[str(current)] = snapshot(frame)
        if missions is not None:
            if missions['9127']['active'] and first_active is None:
                first_active = snapshot(frame)
            if first_active is not None and first_completed is None and missions['9127']['completed'] > first_active['missions']['9127']['completed']:
                first_completed = dict(before=None if previous is None else snapshot(previous), after=snapshot(frame))
        inventory = frame['inventory']
        if inventory is not None:
            if initial_inventory is None:
                initial_inventory = dict(sample=index, counts=inventory)
            last_inventory = dict(sample=index, counts=inventory)
            if first_reward_gain is None and all(inventory[k] >= initial_inventory['counts'][k]+1 for k in ITEMS):
                first_reward_gain = dict(before=None if previous is None else snapshot(previous), after=snapshot(frame))
        state = {k: frame[k] for k in ('missions', 'inventory', 'missionInventory', 'talk')}
        old = None if previous is None else {k: previous[k] for k in state}
        if state != old:
            transitions.append(dict(before=None if previous is None else snapshot(previous), after=snapshot(frame)))
        previous = frame
    return dict(schema=1, evidence='Closed original end-of-frame field observations; no native behavior implied.',
                capture=str(path.resolve()), capture_sha256=hashlib.sha256(data).hexdigest(), header=header,
                end=end, frames=len(frames), source_target_xyz=TARGET, first_active=first_active,
                completion_transition=first_completed, four_reward_gain_transition=first_reward_gain,
                first_visible_talk=talk_first, initial_player_inventory=initial_inventory,
                final_player_inventory=last_inventory, state_transitions=transitions,
                limitations=['Events between two frame samples are not ordered within that interval.',
                             'Displayed talk means NPC talk object active with its indexed talk ID; text, player choice and acknowledgments require operator evidence.',
                             'Inventory deltas do not independently prove causal reward origin; correlate mission transitions and operator inputs.',
                             'Null player/package/mission/UI fields mean unavailable, never synthesized empty state.',
                             'Target coordinate is the archived source4525 reference, not a reconstructed live mission target.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--require-return', action='store_true', help='require active-to-completed9127 and all four inventory gains')
    args = parser.parse_args()
    result = analyze(args.capture)
    if args.require_return and (result['completion_transition'] is None or result['four_reward_gain_transition'] is None):
        raise ValueError('No observed return completion plus four reward gains')
    if args.output.resolve() == args.capture.resolve():
        raise ValueError('Refusing to replace capture')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x') as stream:
        json.dump(result, stream, indent=2);stream.write('\n')
    print('Frames:', result['frames'], 'completion:', result['completion_transition'] is not None,
          'four gains:', result['four_reward_gain_transition'] is not None,
          'talk IDs:', ','.join(result['first_visible_talk']))


if __name__ == '__main__':
    main()
