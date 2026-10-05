#!/usr/bin/env python3
"""Read one small installed Unity bundle and emit metadata, never asset payloads.

Optional dependency: UnityPy==1.25.4 in an isolated Python environment.
--probe-meshes also verifies OBJ conversion in memory and records its checksum.
"""
import argparse
from collections import Counter
import hashlib
import importlib.metadata
import json
from pathlib import Path
import sys


FIELDS = {
    'GameObject': ('m_Name', 'm_IsActive', 'm_Component'),
    'Transform': ('m_GameObject', 'm_Father', 'm_Children', 'm_LocalPosition',
                  'm_LocalRotation', 'm_LocalScale'),
    'MeshFilter': ('m_GameObject', 'm_Mesh'),
    'MeshRenderer': ('m_GameObject', 'm_Enabled', 'm_Materials'),
    'MeshCollider': ('m_GameObject', 'm_Enabled', 'm_IsTrigger', 'm_Mesh', 'm_Convex'),
    'Material': ('m_Name', 'm_Shader', 'm_SavedProperties'),
    'Texture2D': ('m_Name', 'm_Width', 'm_Height', 'm_TextureFormat', 'm_MipCount'),
    'Mesh': ('m_Name', 'm_LocalAABB', 'm_SubMeshes'),
}


def inventory(path, probe_meshes=False):
    import UnityPy
    environment = UnityPy.load(str(path))
    rows = []
    objects = environment.objects
    for obj in objects:
        kind = obj.type.name
        if kind not in FIELDS:
            continue
        tree = obj.read_typetree()
        row = {'file': obj.assets_file.name, 'path_id': obj.path_id, 'type': kind,
               'fields': {key: tree[key] for key in FIELDS[kind] if key in tree}}
        if kind == 'Mesh':
            data = obj.read()
            row['vertex_count'] = data.m_VertexData.m_VertexCount
            if probe_meshes:
                encoded = data.export().encode('utf-8')
                lines = encoded.splitlines()
                row['obj_probe'] = {'bytes': len(encoded), 'sha256': hashlib.sha256(encoded).hexdigest(),
                                    'vertices': sum(line.startswith(b'v ') for line in lines),
                                    'faces': sum(line.startswith(b'f ') for line in lines)}
        rows.append(row)
    with path.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    return {'source': str(path), 'bytes': path.stat().st_size, 'sha256': digest,
            'unitypy': importlib.metadata.version('UnityPy'),
            'unity_versions': sorted({asset.unity_version for asset in environment.assets}),
            'type_counts': dict(sorted(Counter(obj.type.name for obj in objects).items())),
            'containers': sorted(environment.container),
            'objects': sorted(rows, key=lambda row: (row['file'], row['path_id']))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('--output', type=Path, help='metadata JSON destination; default stdout')
    parser.add_argument('--probe-meshes', action='store_true')
    parser.add_argument('--max-input-mib', type=int, default=64,
                        help='reject large inputs to bound audit memory use (default 64)')
    args = parser.parse_args()
    try:
        path = args.bundle.expanduser().resolve()
        if not path.is_file():
            raise ValueError(f'Not a file: {path}')
        if args.max_input_mib <= 0 or path.stat().st_size > args.max_input_mib * 1024 * 1024:
            raise ValueError('Input exceeds audit size limit; choose a small individual bundle.')
        if args.output and args.output.resolve() == path:
            raise ValueError('Metadata output must not overwrite the original bundle.')
        result = json.dumps(inventory(path, args.probe_meshes), indent=2, sort_keys=True) + '\n'
        if args.output:
            args.output.write_text(result, encoding='utf-8')
        else:
            sys.stdout.write(result)
    except ImportError:
        parser.exit(1, 'Install UnityPy==1.25.4 in a Python virtual environment to run this tool.\n')
    except (OSError, ValueError) as error:
        parser.exit(1, f'Asset inventory failed: {error}\n')


if __name__ == '__main__':
    main()
