#!/usr/bin/env python3
"""Validate and install a complete observed terrain route; never invent holes.

Each 32-cube file must match the assembled PEVX, with two-frame stability and
its own positive original capture frame. Original reference archives must
still match the clone's baseline. Only terrain assets are created; differing
existing assets fail closed. Captures made while walking are explicitly not
simultaneous snapshots. --coverage reports a running/incomplete probe.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from export_adventure_terrain import baseline_manifests, manual_save_baseline, read_pevx

ROOT = Path(__file__).resolve().parents[2]
ORIGIN = (-640, 0, 64)
SIZE = (416, 64, 416)


def chunk_keys(origin, size):
    return [(x, y, z) for z in range(origin[2]//32, (origin[2]+size[2]-1)//32+1)
            for y in range(origin[1]//32, (origin[1]+size[1]-1)//32+1)
            for x in range(origin[0]//32, (origin[0]+size[0]-1)//32+1)]


def coverage(capture):
    path = Path(str(capture)+'.chunks')/'manifest.json'
    metadata = json.loads(path.read_text())
    keys = chunk_keys(metadata['origin'], metadata['size'])
    frames = metadata.get('frames', {})
    missing = [list(p) for p in keys if ','.join(map(str, p)) not in frames]
    return dict(captured=len(keys)-len(missing), required=len(keys), missing=missing,
                player=metadata.get('player'), playerYaw=metadata.get('playerYaw'),
                cameraYaw=metadata.get('cameraYaw'))


def surface_summary(voxels, origin, size):
    sx, sy, sz = size
    heights = Counter()
    clipped = []
    empty = 0
    for z in range(sz):
        for x in range(sx):
            height = next((y for y in range(sy-1, -1, -1) if voxels[2*((z*sy+y)*sx+x)] >= 128), None)
            if height is None:
                empty += 1
            else:
                heights[height+origin[1]] += 1
                if height == sy-1:
                    clipped.append((origin[0]+x, origin[2]+z))
    return dict(highest_solid_y_min=min(heights, default=None), highest_solid_y_max=max(heights, default=None),
                highest_solid_y_histogram=dict(sorted(heights.items())), empty_columns=empty,
                top_solid_columns=len(clipped), top_solid_examples=clipped[:32])


def validate_capture(capture, expected_origin=ORIGIN, expected_size=SIZE):
    origin, size, voxels = read_pevx(capture)
    if origin != expected_origin or size != expected_size:
        raise ValueError('Route bounds mismatch')
    metadata_path = Path(str(capture)+'.json')
    metadata = json.loads(metadata_path.read_text())
    keys = chunk_keys(origin, size)
    expected = dict(schema=1, kind='observed-original-loaded-terrain-route',
                    origin=list(expected_origin), size=list(expected_size), unity='5.2.4f1',
                    stableConsecutiveFramesPerChunk=2, simultaneousSnapshot=False,
                    complete=True, requiredChunks=len(keys), capturedChunks=len(keys),
                    layout='x-fastest,y,z;volume,type')
    if any(metadata.get(k) != v for k, v in expected.items()):
        raise ValueError('Route bounds/coverage/stability metadata mismatch')
    frames = metadata.get('frames', {})
    if set(frames) != {','.join(map(str, p)) for p in keys} or any(type(v) is not int or v <= 0 for v in frames.values()):
        raise ValueError('Invalid captured chunk frame set')
    chunks = Path(str(capture)+'.chunks')
    hashes = {}
    for coordinate in keys:
        key = ','.join(map(str, coordinate))
        path = chunks/(key+'.pevx')
        chunk_origin, chunk_size, data = read_pevx(path)
        if chunk_origin != tuple(v*32 for v in coordinate) or chunk_size != (32, 32, 32):
            raise ValueError('Invalid chunk bounds: '+key)
        hashes[key] = hashlib.sha256(path.read_bytes()).hexdigest()
        for z in range(max(origin[2], chunk_origin[2]), min(origin[2]+size[2], chunk_origin[2]+32)):
            for y in range(max(origin[1], chunk_origin[1]), min(origin[1]+size[1], chunk_origin[1]+32)):
                x0 = max(origin[0], chunk_origin[0])
                x1 = min(origin[0]+size[0], chunk_origin[0]+32)
                at = 2*(((z-origin[2])*size[1]+y-origin[1])*size[0]+x0-origin[0])
                src = 2*(((z-chunk_origin[2])*32+y-chunk_origin[1])*32+x0-chunk_origin[0])
                if voxels[at:at+2*(x1-x0)] != data[src:src+2*(x1-x0)]:
                    raise ValueError('Assembled terrain differs from observed chunk: '+key)
    sha = hashlib.sha256(capture.read_bytes()).hexdigest()
    if metadata.get('sha256', sha) != sha:
        raise ValueError('Capture hash mismatch')
    return metadata, hashes, surface_summary(voxels, origin, size)


def install_capture(capture, output, source_profile, check=False):
    metadata, chunk_hashes, surface = validate_capture(capture)
    hashes_path = capture.parent.parent/'source-save-hashes.json'
    baseline = json.loads(hashes_path.read_text())
    original_hashes = manual_save_baseline(baseline)
    if not original_hashes:
        raise ValueError('Missing original-save hash baseline')
    for relative, digest in original_hashes.items():
        target = (source_profile/relative).resolve()
        if not target.is_relative_to(source_profile.resolve()) or hashlib.sha256(target.read_bytes()).hexdigest() != digest:
            raise ValueError('Original source save changed: '+relative)
    towns = metadata.get('towns', [])
    starting_town = next((town for town in towns if town.get('id') == 0), None)
    destination_town = next((town for town in towns if town.get('id') == 4), None)
    if (not starting_town or not destination_town or
            not isinstance(starting_town.get('nameId'), int) or
            not isinstance(starting_town.get('name'), str) or not starting_town['name'] or
            any(ord(c) < 32 for c in starting_town['name']) or
            destination_town.get('name') != 'Annot'):
        raise ValueError('Missing observed starting/destination town names')
    data = capture.read_bytes()
    metadata_path = Path(str(capture)+'.json')
    manifest = dict(schema=1, kind=metadata['kind'], source_capture=str(capture.resolve()),
                    sha256=hashlib.sha256(data).hexdigest(), source_metadata=str(metadata_path.resolve()),
                    source_metadata_sha256=hashlib.sha256(metadata_path.read_bytes()).hexdigest(),
                    original_profile=str(source_profile.resolve()), original_save_hashes=str(hashes_path.resolve()),
                    original_save_hashes_sha256=hashlib.sha256(hashes_path.read_bytes()).hexdigest(),
                    original_saves_unchanged=True, original_saves_verified=len(original_hashes),
                    origin=list(ORIGIN), size=list(SIZE), chunk_sha256=chunk_hashes,
                    surface_summary=surface, observation=metadata)
    manifest_path = output.parent/'terrain-manifest.json'
    accepted = baseline_manifests(manifest, baseline, original_hashes)
    files = {output: data, Path(str(output)+'.json'): metadata_path.read_bytes(),
             output.parent/'town-name.txt': (starting_town['name']+'\n').encode(),
             output.parent/'town-names.json': (json.dumps(towns, indent=2)+'\n').encode(),
             manifest_path: accepted[0]}
    for path, contents in files.items():
        if path.exists() and path.read_bytes() not in (accepted if path == manifest_path else [contents]):
            raise ValueError('Refusing to overwrite differing route terrain: '+str(path))
        if check and not path.exists():
            raise ValueError('Missing installed route terrain: '+str(path))
    if not check:
        output.parent.mkdir(parents=True, exist_ok=True)
        for path, contents in files.items():
            if not path.exists():
                with path.open('xb') as stream:
                    stream.write(contents)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT/'rebuild/build/reference-assets/adventure-route/terrain.pevx')
    parser.add_argument('--source-profile', type=Path, default=Path.home()/'.local/share/planet-explorers-sandbox-reference')
    parser.add_argument('--coverage', action='store_true')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    if args.coverage:
        print(json.dumps(coverage(args.capture), indent=2))
    else:
        result = install_capture(args.capture, args.output, args.source_profile, args.check)
        print(json.dumps(dict(sha256=result['sha256'], original_saves_verified=result['original_saves_verified'],
                             surface_summary=result['surface_summary']), indent=2))


if __name__ == '__main__':
    main()
