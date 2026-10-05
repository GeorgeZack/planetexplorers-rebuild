#!/usr/bin/env python3
"""Decode the installed starttown .art without inventing the surrounding terrain.

Output is an ARTIFACT-ONLY PEVX diagnostic, not the original terrain crop. Use
reference_terrain.py for complete loaded original terrain. Preserves each byte
of volume and maps local material slots through VAMaterial.CalcGUID, matching
VFDataRTGen. Source-derived positions come from the archive settlement manifest.
"""
import argparse
from collections import Counter
import hashlib
import json
import re
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]
ORIGIN = (-627, 16, 323)
SIZE = (64, 32, 128)
HEADER = struct.Struct('<4sI3i3I')


class Reader:
    def __init__(self, data):
        self.data, self.offset = data, 0

    def take(self, count):
        if count < 0 or self.offset + count > len(self.data):
            raise ValueError('Truncated or invalid artifact')
        data = self.data[self.offset:self.offset + count]
        self.offset += count
        return data

    def unpack(self, fmt):
        return struct.unpack('<' + fmt, self.take(struct.calcsize('<' + fmt)))

    def integer(self):
        return self.unpack('i')[0]

    def string(self):
        count = 0
        for shift in range(0, 35, 7):
            b = self.take(1)[0]
            count |= (b & 127) << shift
            if not b & 128:
                return self.take(count).decode('utf-8')
        raise ValueError('Invalid BinaryReader string')


def material(data, serialized_guid):
    r = Reader(data)
    if r.integer() != 0x1000:
        raise ValueError('Unsupported material version')
    name, matter = r.string(), r.integer()
    default = bool(r.take(1)[0])
    # VAMaterial.Import forces m_UseDefault=false, then CalcGUID rounds power.
    bump = r.unpack('f')[0]
    r.take(4)
    strength, power = r.unpack('2f')
    r.take(4)
    tile = r.unpack('f')[0]
    r.take(r.integer())
    r.take(r.integer())
    if r.offset != len(data):
        raise ValueError('Trailing material bytes')
    guid = round(power)  # Unity Mathf.RoundToInt uses ties-to-even.
    if not 0 <= guid <= 255:
        raise ValueError('Invalid runtime material GUID')
    return dict(name=name, matter_id=matter, serialized_guid=serialized_guid,
                runtime_guid=guid, terrain_type=guid & 255, specular_power=power,
                use_default_serialized=default, bump=bump, strength=strength, tile=tile)


def triplanar_rows():
    source = ROOT / "Assets/Scripts/PuJi/Town/Tool/VArtifactUtil.cs"
    body = source.read_text(encoding="utf-8-sig").split("public static string[] triplaner =", 1)[1].split("};", 1)[0]
    body = re.sub(r"//[^\n]*", "", body)
    return [[int(n) for n in row.split(",")] for row in re.findall(r'"([0-9,]+)"', body)]


def read_art(path, map_type=None):
    data = path.read_bytes()
    r = Reader(data)
    if r.string() != 'VCISO':
        raise ValueError('Not VCISO')
    version, category = r.unpack('2i')
    if version not in (0x2000000, 0x2010000, 0x2020000, 0x2020001):
        raise ValueError('Unsupported artifact version')
    if version >= 0x2020000:
        r.string()
    name, description = r.string(), r.string()
    if version >= 0x2020000:
        r.string()
    size = r.unpack('3i')
    if any(n <= 0 or n > 1024 for n in size):
        raise ValueError('Invalid artifact dimensions')
    r.take(r.integer())
    mat_count = r.integer()
    decal_count = r.integer() if version >= 0x2010000 else 0
    component_count, voxel_count, color_count = r.unpack('3i')
    if not 0 <= mat_count <= 64 or not 0 <= decal_count <= 4 or min(component_count, voxel_count, color_count) < 0:
        raise ValueError('Invalid artifact counts')
    materials = []
    for _ in range(mat_count):
        guid = r.unpack('Q')[0]
        materials.append(material(r.take(r.integer()), guid) if guid else None)
    for mat in materials:
        if mat and mat['terrain_type'] >= 240:
            if map_type is None:
                raise ValueError('Biome-relative material requires explicit --map-type; live crop retains per-column resolved types')
            mat['terrain_type'] = triplanar_rows()[map_type-1][mat['terrain_type'] % 240]
    for _ in range(decal_count):
        if r.unpack('Q')[0]:
            r.take(r.integer())
    payload = Reader(zlib.decompress(r.take(r.integer())))
    if r.offset != len(data):
        raise ValueError('Trailing artifact bytes')
    for _ in range(component_count):
        length = payload.integer()
        if length:
            payload.take(length)
            payload.integer()
    signature = payload.unpack('Q')[0] if version >= 0x2020001 else None
    voxels = {}
    for _ in range(voxel_count):
        key, volume, slot = payload.unpack('iBB')
        xyz = (key & 1023, key >> 20, (key >> 10) & 1023)
        if any(v < 0 or v >= limit for v, limit in zip(xyz, size)) or xyz in voxels:
            raise ValueError('Invalid or duplicate voxel key')
        if slot >= len(materials) or materials[slot] is None:
            raise ValueError('Missing voxel material')
        voxels[xyz] = (volume, materials[slot]['terrain_type'])
    payload.take(color_count * 8)
    if payload.offset != len(payload.data):
        raise ValueError('Trailing decompressed bytes')
    return dict(version=version, category=category, name=name, description=description,
                size=list(size), materials=materials, component_count=component_count,
                voxel_count=voxel_count, color_count=color_count, header_signature=signature,
                sha256=hashlib.sha256(data).hexdigest()), voxels


def encode_pevx(voxels, origin=ORIGIN, size=SIZE):
    if any(n <= 0 for n in size):
        raise ValueError('Invalid crop size')
    result = bytearray(2 * size[0] * size[1] * size[2])
    for xyz, pair in voxels.items():
        x, y, z = (p-o for p, o in zip(xyz, origin))
        if 0 <= x < size[0] and 0 <= y < size[1] and 0 <= z < size[2]:
            index = 2 * (x + size[0] * (y + size[1] * z))
            result[index:index+2] = bytes(pair)
    return HEADER.pack(b'PEVX', 1, *origin, *size) + result


def read_pevx(path):
    data = path.read_bytes()
    if len(data) < HEADER.size:
        raise ValueError('Truncated PEVX header')
    magic, version, *values = HEADER.unpack_from(data)
    origin, size = tuple(values[:3]), tuple(values[3:])
    if magic != b'PEVX' or version != 1 or not all(size) or len(data) != HEADER.size + 2*size[0]*size[1]*size[2]:
        raise ValueError('Invalid PEVX')
    return origin, size, data[HEADER.size:]


def install_capture(capture, output, source_profile, check=False):
    origin, size, voxels = read_pevx(capture)
    metadata_path = Path(str(capture)+'.json')
    metadata = json.loads(metadata_path.read_text())
    expected = dict(schema=1, kind='observed-original-loaded-terrain',
                    origin=list(ORIGIN), size=list(SIZE),
                    stableConsecutiveFrames=2, loadedChunks=30,
                    layout='x-fastest,y,z;volume,type')
    if origin != ORIGIN or size != SIZE or any(metadata.get(k) != v for k,v in expected.items()):
        raise ValueError('Capture bounds/stability/loaded-chunk metadata mismatch')
    if not isinstance(metadata.get('frame'), int) or metadata['frame'] <= 0 or metadata.get('unity') != '5.2.4f1':
        raise ValueError('Invalid original-runtime metadata')
    data = capture.read_bytes()
    sha = hashlib.sha256(data).hexdigest()
    if metadata.get('sha256', sha) != sha:
        raise ValueError('Capture hash mismatch')
    hashes_path = capture.parent.parent/'source-save-hashes.json'
    hashes = json.loads(hashes_path.read_text())
    if not hashes:
        raise ValueError('Missing original-save hash baseline')
    for relative, digest in hashes.items():
        target = (source_profile/relative).resolve()
        if not target.is_relative_to(source_profile.resolve()) or hashlib.sha256(target.read_bytes()).hexdigest() != digest:
            raise ValueError('Original source save changed: '+relative)
    manifest = dict(schema=1, kind='observed-original-loaded-terrain',
                    source_capture=str(capture.resolve()), sha256=sha,
                    source_metadata=str(metadata_path.resolve()),
                    source_metadata_sha256=hashlib.sha256(metadata_path.read_bytes()).hexdigest(),
                    original_profile=str(source_profile.resolve()),
                    original_save_hashes=str(hashes_path.resolve()),
                    original_save_hashes_sha256=hashlib.sha256(hashes_path.read_bytes()).hexdigest(),
                    original_saves_verified=len(hashes), original_saves_unchanged=True,
                    origin=list(origin), size=list(size),
                    materials=dict(Counter(str(v) for v in voxels[1::2])),
                    occupied_materials=dict(Counter(str(voxels[i+1]) for i in range(0,len(voxels),2) if voxels[i]>=128)),
                    observation=metadata)
    files = {output:data, Path(str(output)+'.json'):metadata_path.read_bytes(),
             output.parent/'terrain-manifest.json':(json.dumps(manifest,indent=2)+'\n').encode()}
    # Validate every destination before any creation; never touch other assets.
    for path, contents in files.items():
        if path.exists() and path.read_bytes() != contents:
            raise ValueError('Refusing to overwrite differing recovered terrain: '+str(path))
        if check and not path.exists():
            raise ValueError('Missing installed terrain: '+str(path))
    if not check:
        output.parent.mkdir(parents=True, exist_ok=True)
        for path, contents in files.items():
            if not path.exists():
                with path.open('xb') as stream:
                    stream.write(contents)
    print(f'Validated capture {sha}; {len(hashes)} original saves unchanged; {output}')
    return manifest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--art', type=Path, default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers/RandomTownArt/town1_8.art')
    p.add_argument('--settlement', type=Path, default=ROOT/'rebuild/generated/pe_adventure_settlement.json')
    p.add_argument('--output', type=Path, default=None)
    p.add_argument('--check', action='store_true')
    p.add_argument('--capture', type=Path, help='Validate/install a complete original-runtime PEVX capture')
    p.add_argument('--source-profile', type=Path, default=Path.home()/'.local/share/planet-explorers-sandbox-reference')
    p.add_argument('--map-type', type=int, choices=range(1,9), help='Explicit diagnostic biome1..8; does not establish actual per-column mapType')
    args = p.parse_args()
    if args.output is None:
        args.output = ROOT/'rebuild/build/reference-assets/adventure-settlement'/('terrain.pevx' if args.capture else 'town-artifact-only.pevx')
    if args.capture:
        install_capture(args.capture,args.output,args.source_profile,args.check)
        return
    if args.map_type is None:
        p.error('--map-type is required for the artifact-only diagnostic')
    meta, voxels = read_art(args.art, args.map_type)
    settlement = json.loads(args.settlement.read_text())
    lot = settlement['lot']
    if lot['name'] != 'town1_8' or lot['rotation_degrees'] != 0 or meta['size'] != [60,80,100]:
        raise ValueError('Only archived unrotated town1_8 is supported')
    origin = lot['origin']
    if any(v != int(v) for v in origin):
        raise ValueError('Fractional origins require OutputVoxels interpolation')
    placed = {tuple(int(a+b) for a,b in zip(origin, xyz)): pair for xyz,pair in voxels.items()}
    output = encode_pevx(placed)
    meta.update(schema=1, kind='source-derived-artifact-only', source=str(args.art),
                assumed_map_type=args.map_type,
                origin=list(ORIGIN), size_crop=list(SIZE), artifact_origin=origin,
                world_center=[origin[0]+30,origin[1],origin[2]+50],
                missing='VFDataRTGen background, clipping, additive mix, cut and underground fill; use live capture for gameplay',
                crop_sha256=hashlib.sha256(output).hexdigest(),
                materials_used=dict(Counter(str(t) for _,t in voxels.values())),
                settlement_manifest_sha256=hashlib.sha256(args.settlement.read_bytes()).hexdigest())
    metadata = (json.dumps(meta, indent=2)+'\n').encode()
    meta_path = Path(str(args.output)+'.json')
    if args.check:
        if args.output.read_bytes() != output or meta_path.read_bytes() != metadata:
            raise ValueError('Artifact export differs')
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(output)
        meta_path.write_bytes(metadata)
    print(f'{len(voxels)} original artifact voxels; diagnostic only: {args.output}')


if __name__ == '__main__':
    main()
