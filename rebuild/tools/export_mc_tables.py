#!/usr/bin/env python3
"""Extract the original CPU marching-cubes tables, preserving provenance."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Assets/Terrain/Voxelform2/Voxelform/Source/Scripts/Voxel/MarchingCubes/Cpu/MarchingCubesSW_Tables.cs'
OUTPUT = ROOT / 'rebuild/generated/pe_mc_tables.h'
MANIFEST = ROOT / 'rebuild/generated/mc_manifest.json'

def extract(text):
    def numbers(name):
        pattern = rf'\b{name}\s*=\s*new\s+int\s*\[[,\s]*\]\s*\{{(.*?)\}}\s*;'
        match = re.search(pattern, text, re.S)
        if not match:
            raise ValueError(f'missing {name} initializer')
        body = re.sub(r'//[^\n]*|/\*.*?\*/', '', match.group(1), flags=re.S)
        return [int(x, 0) for x in re.findall(r'-?0x[0-9a-fA-F]+|-?\d+', body)]
    edges, triangles, pairs = numbers('edgeTable'), numbers('triTable'), numbers('edgeInfo')
    if len(edges) != 256 or len(triangles) != 256*16 or len(pairs) != 24:
        raise ValueError('unexpected marching-cubes table dimensions')
    rows = [triangles[i*16:(i+1)*16] for i in range(256)]
    for case, row in enumerate(rows):
        if -1 not in row or row.index(-1) % 3 or any(v != -1 for v in row[row.index(-1):]):
            raise ValueError(f'invalid terminator in case {case}')
        crossing = sum(1 << e for e in range(12) if bool(case & (1 << pairs[e*2])) != bool(case & (1 << pairs[e*2+1])))
        if edges[case] != crossing:
            raise ValueError(f'edge mask inconsistent with case {case}')
        if any(v < -1 or v > 11 or (v >= 0 and not crossing & (1 << v)) for v in row):
            raise ValueError(f'invalid triangle edge in case {case}')
    return edges, rows, pairs

def outputs():
    raw=SOURCE.read_bytes()
    edges, rows, pairs=extract(raw.decode('utf-8-sig'))
    digest=hashlib.sha256(raw).hexdigest()
    text=('/* Generated from the supplied Planet Explorers CPU marching-cubes tables.\n'
          ' * Original table file attribution: Mark Davis; based in part on Paul Bourke.\n'
          ' * Preserve the original project terms; this export grants no new license.\n'
          f' * SHA-256: {digest}\n */\n'
          '#ifndef PE_MC_TABLES_H\n#define PE_MC_TABLES_H\n#include <stdint.h>\n'
          'static const uint16_t pe_mc_edges[256] = {\n')
    text+=''.join('    '+', '.join(f'0x{x:03x}' for x in edges[i:i+8])+',\n' for i in range(0,256,8))+'};\n'
    text+='static const int8_t pe_mc_triangles[256][16] = {\n'
    text+=''.join('    {'+', '.join(map(str,row))+'},\n' for row in rows)+'};\n'
    text+='static const uint8_t pe_mc_edge_corners[12][2] = {\n'
    text+=''.join(f'    {{{pairs[i]}, {pairs[i+1]}}},\n' for i in range(0,24,2))+'};\n#endif\n'
    manifest=json.dumps({'source':str(SOURCE.relative_to(ROOT)), 'sha256':digest,'cases':256,
                         'iso_threshold':128,'case_bit':'corner volume < 128','max_triangles_per_cell':5},indent=2)+'\n'
    return [(OUTPUT,text),(MANIFEST,manifest)]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    try:
        for path,text in outputs():
            if args.check:
                if not path.exists() or path.read_text()!=text:
                    raise ValueError(f'stale generated file: {path}')
            else:
                path.parent.mkdir(parents=True,exist_ok=True)
                path.write_text(text)
    except (OSError,ValueError) as exc:
        parser.exit(1,f'{exc}\n')
    print('Marching-cubes source tables verified.' if args.check else 'Exported 256 source marching-cubes cases.')
if __name__=='__main__':
    main()
