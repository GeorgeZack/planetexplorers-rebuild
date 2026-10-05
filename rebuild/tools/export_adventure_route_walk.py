#!/usr/bin/env python3
"""Record sampled original walking evidence and emit a native movement fixture.

Only actual recorded player positions become waypoints. Sampling does not
establish the exact intervening trajectory, input durations or native parity.
The evidence requires a completed terrain capture. It records whether any
sample entered Annot's radius and stops at the first such sample when present.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path


def derive(walk, capture):
    records = [json.loads(line) for line in walk.read_text().splitlines() if line.strip()]
    if not records:
        raise ValueError('No original walking observations')
    previous = -math.inf
    for row in records:
        if (not isinstance(row.get('time'), (int, float)) or not math.isfinite(row['time']) or row['time'] <= previous or
                not isinstance(row.get('player'), list) or len(row['player']) != 3 or
                any(not isinstance(v, (int, float)) or not math.isfinite(v) or abs(v) > 1000000 for v in row['player'])):
            raise ValueError('Invalid or nonmonotonic original walking sample')
        previous = row['time']
    metadata_path = Path(str(capture)+'.json')
    metadata = json.loads(metadata_path.read_text())
    if metadata.get('complete') is not True or metadata.get('kind') != 'observed-original-loaded-terrain-route':
        raise ValueError('Complete original route capture required')
    original_count = len(records)
    origin, size = metadata['origin'], metadata['size']
    excluded = []
    bounded = []
    for index, row in enumerate(records):
        x, y, z = row['player']
        if not (origin[0]+.3 <= x <= origin[0]+size[0]-1-.3 and origin[2]+.3 <= z <= origin[2]+size[2]-1-.3 and origin[1] <= y <= origin[1]+size[1]-1-1.8):
            excluded.append(dict(source_record=index, reason='Outside captured native player capsule bounds'))
        else:
            bounded.append(dict(row, source_record=index))
    records = bounded
    if not records:
        raise ValueError('No player observations within native route bounds')
    arrival = next((i for i, row in enumerate(records) if row['player'][1] >= 0 and
                    math.hypot(row['player'][0]+284, row['player'][2]-134) < 15), None)
    prefix = records if arrival is None else records[:arrival+1]
    gaps = [b['time']-a['time'] for a, b in zip(prefix, prefix[1:])]
    distance = sum(math.dist(a['player'], b['player']) for a, b in zip(prefix, prefix[1:]))
    evidence = dict(schema=1, kind='observed-original-sampled-route-survey',
                    source_walk=str(walk.resolve()), source_walk_sha256=hashlib.sha256(walk.read_bytes()).hexdigest(),
                    source_terrain=str(capture.resolve()), source_terrain_sha256=hashlib.sha256(capture.read_bytes()).hexdigest(),
                    source_metadata_sha256=hashlib.sha256(metadata_path.read_bytes()).hexdigest(),
                    unity=metadata.get('unity'), origin=metadata.get('origin'), size=metadata.get('size'),
                    started_utc=datetime.fromtimestamp(prefix[0]['time'], timezone.utc).isoformat(),
                    last_observation_utc=datetime.fromtimestamp(prefix[-1]['time'], timezone.utc).isoformat(),
                    arrival_utc=None if arrival is None else datetime.fromtimestamp(prefix[-1]['time'], timezone.utc).isoformat(),
                    all_recorded_samples=original_count, samples_through_capture=len(prefix), excluded_samples=excluded,
                    observed_destination=[-284, 25, 134], source_XZ_radius=15, reached_Annot_radius=arrival is not None,
                    sampled_polyline_meters=distance, maximum_observation_gap_seconds=max(gaps, default=0),
                    observation_limit='Periodic PeTrans positions from the original inside captured player bounds; omitted samples are listed by source row. Intervening trajectory, exact inputs, and quest UI completion are not inferred.',
                    towns=metadata.get('towns'), observations=prefix)
    return evidence, prefix


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--walk', type=Path, required=True)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--waypoints', type=Path, required=True)
    args = parser.parse_args()
    evidence, records = derive(args.walk, args.capture)
    contents = json.dumps(evidence, indent=2)+'\n'
    for path, data in ((args.output, contents), (args.waypoints, ''.join(' '.join(map(str, row['player']))+'\n' for row in records))):
        if path.exists() and path.read_text() != data:
            raise ValueError('Refusing to overwrite different walking evidence: '+str(path))
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists():
            with path.open('x') as stream:
                stream.write(data)
    print(f'Observed {len(records)} positions; Annot radius observed={evidence["reached_Annot_radius"]}; {evidence["sampled_polyline_meters"]:.1f} m sampled polyline')


if __name__ == '__main__':
    main()
