#!/usr/bin/env python3
"""Rebase a verified recovered tree into source-local meters; never alter inputs.

Consumes export_training_gathering_assets.py output; optional UnityPy decodes the
small source prefab to verify its matching mesh and physical capsule. The
new output directory contains a shared tree OBJ/MTL/PNG and provenance JSON.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import tempfile

from export_reference_actor import inverse_affine
from export_reference_assets import determinant, floats, property_map, transform_normal, transform_point

ROOT = Path(__file__).resolve().parents[2]
HEADER = '# Baked Unity XYZ meters; original vertex/normal coordinate convention\n'
MTL_HEADER = '# Source material slots; diffuse-only preview conversion\n'
TREE_PREFAB = ROOT/'Assets/Prefabs/tree-Ambient-Occlusion/tree11_grassland.prefab'


def source_capsule():
    import UnityPy
    from export_training_gathering import PREFAB
    env = UnityPy.load(str(TREE_PREFAB))
    source = {o.path_id:o for o in env.objects}
    serialized = next(iter(env.files.values()))
    mesh = source[3300000].read_typetree()['m_Mesh']
    guid = bytes(serialized.externals[mesh['m_FileID']-1].guid).hex()
    training = UnityPy.load(str(PREFAB))
    objects = {o.path_id:o for o in training.objects}
    leaf = objects[427828].read_typetree()['m_GameObject']['m_PathID']
    same, = [o.read_typetree()['m_Mesh'] for o in objects.values()
             if o.type.name == 'MeshFilter' and o.read_typetree()['m_GameObject']['m_PathID'] == leaf]
    tutorial_guid = bytes(next(iter(training.files.values())).externals[same['m_FileID']-1].guid).hex()
    transform = source[400000].read_typetree()
    if (guid != tutorial_guid or mesh['m_PathID'] != same['m_PathID']
            or transform['m_LocalPosition'] != dict(x=0.,y=0.,z=0.)
            or transform['m_LocalScale'] != dict(x=1.,y=1.,z=1.)
            or transform['m_LocalRotation'] != dict(x=0.,y=0.,z=0.,w=1.)):
        raise ValueError('Tree collider and recovered mesh identity/transform differ')
    capsule = source[13600000].read_typetree()
    if not capsule['m_Enabled'] or capsule['m_IsTrigger'] or capsule['m_Direction'] != 1:
        raise ValueError('Expected enabled nontrigger upright source capsule')
    center = [capsule['m_Center'][axis] for axis in 'xyz']
    radius, height = capsule['m_Radius'], capsule['m_Height']
    if (any(not math.isfinite(v) or abs(v)>20 for v in center)
            or not 0 < radius <= 10 or not 2*radius <= height <= 40):
        raise ValueError('Invalid source capsule')
    binary = b'PESC'+struct.pack('<I5fII',1,*center,radius,height,1,1)
    metadata = dict(prefab=str(TREE_PREFAB.relative_to(ROOT)),prefab_sha256=hashlib.sha256(TREE_PREFAB.read_bytes()).hexdigest(),
        mesh_guid=guid, mesh_path_id=mesh['m_PathID'], source_mesh_matches_tutorial=True,
        component=13600000, center=center,radius=radius,height=height,direction=1,enabled=True,trigger=False,
        near_tree_layer=21, instance_scale='Unit width/height for current PeTree; original scales per TreeInfo')
    return binary, metadata


def bounded_read(path, limit):
    if path.stat().st_size > limit:
        raise ValueError('Oversize recovered input: '+path.name)
    with path.open('rb') as stream:
        data = stream.read(limit+1)
    if len(data) > limit:
        raise ValueError('Oversize recovered input: '+path.name)
    return data


def local_mesh(text, matrix, expected_vertices, expected_triangles):
    if len(text) > 8_000_000 or not text.startswith(HEADER):
        raise ValueError('Expected bounded recovered OBJ')
    if (len(matrix) != 4 or any(len(row) != 4 for row in matrix)
            or any(not math.isfinite(v) or abs(v) > 1000 for row in matrix for v in row)
            or matrix[3] != [0, 0, 0, 1]):
        raise ValueError('Invalid source affine matrix')
    inverse = inverse_affine(matrix)
    lines, vertices, counts, triangles = [HEADER], [], [0, 0, 0], 0
    materials, libraries = set(), 0
    for line in text.splitlines()[1:]:
        t = line.split()
        if not t:
            continue
        if t[0] in ('v', 'vt', 'vn'):
            channel = ('v', 'vt', 'vn').index(t[0])
            width = 2 if channel == 1 else 3
            if len(t) != width + 1:
                raise ValueError('Invalid OBJ channel width')
            values = [float(v) for v in t[1:]]
            if any(not math.isfinite(v) or abs(v) > 1000 for v in values):
                raise ValueError('Invalid OBJ channel value')
            if channel == 0:
                values = transform_point(inverse, values)
                vertices.append(values)
            elif channel == 2:
                if sum(v*v for v in values) < .01:
                    raise ValueError('Invalid normal')
                values = transform_normal(inverse, values)
            counts[channel] += 1
            lines.append(t[0] + ' ' + floats(values) + '\n')
        elif t[0] == 'f':
            if len(t) != 4:
                raise ValueError('Only triangles are accepted')
            for token in t[1:]:
                if not re.fullmatch(r'[1-9][0-9]*/[1-9][0-9]*/[1-9][0-9]*', token):
                    raise ValueError('Invalid face token')
                if any(i > bound for i, bound in zip(map(int, token.split('/')), counts)):
                    raise ValueError('Face index out of bounds')
            order = t[:0:-1] if determinant(matrix) < 0 else t[1:]
            lines.append('f ' + ' '.join(order) + '\n')
            triangles += 1
        elif t[0] == 'mtllib' and t == ['mtllib', 'tree.mtl']:
            libraries += 1
            lines.append(line + '\n')
        elif t[0] == 'usemtl' and t == ['usemtl', 'mAF']:
            materials.add(t[1])
            lines.append(line + '\n')
        else:
            raise ValueError('Unsupported tree OBJ record')
    if counts != [expected_vertices]*3 or triangles != expected_triangles or libraries != 1 or materials != {'mAF'}:
        raise ValueError('Recovered tree channels or topology changed')
    bounds = [[fn(v[a] for v in vertices) for a in range(3)] for fn in (min, max)]
    if max(abs(v) for row in bounds for v in row) > 20:
        raise ValueError('Unexpected tree local bounds')
    return ''.join(lines), bounds


def prepare(source):
    manifest_bytes = bounded_read(source/'manifest.json', 2_000_000)
    report = json.loads(manifest_bytes)
    if report.get('format') != 'PE-training-gathering-assets-1' or report.get('source_installed_transforms_match') is not True:
        raise ValueError('Source/installed gathering identity is unverified')
    tree, = [p for p in report['props'] if p['name'] == 'tree']
    if (tree['source_path'] != 'HoloTree/orgtrees/orgtree' or tree['vertices'] != 3150
            or tree['triangles'] != 4216 or tree['installed_mesh'] != 6039):
        raise ValueError('Unexpected recovered tree identity')
    material, = [m for m in report['materials'] if m['symbol'] == 'mAF']
    props = material['source_properties']
    if material['name'] != 'Grassland_tree_B' or property_map(props['m_Floats']).get('_Cutoff') != .25:
        raise ValueError('Unsupported tree material')
    main = property_map(props['m_TexEnvs'])['_MainTex']
    if main['m_Scale'] != dict(x=1., y=1.) or main['m_Offset'] != dict(x=0., y=0.):
        raise ValueError('Unsupported tree UV transform')
    obj_bytes = bounded_read(source/'tree.obj', 8_000_000)
    localized, bounds = local_mesh(obj_bytes.decode('utf-8'), tree['matrix'], 3150, 4216)
    mtl_bytes = bounded_read(source/'tree.mtl', 16_384)
    tint = property_map(props['m_Colors'])['_Color']
    expected = (MTL_HEADER+'newmtl mAF\nKd '+floats([tint[k] for k in 'rgb'])+
                '\nd '+floats([tint['a']])+'\nillum 1\nmap_Kd material-mAF.png\n\n')
    if mtl_bytes.decode('utf-8') != expected:
        raise ValueError('Tree MTL differs from recovered source properties')
    png = bounded_read(source/'material-mAF.png', 32_000_000)
    if len(png) < 33 or len(png) > 32_000_000 or png[:16] != b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR':
        raise ValueError('Invalid tree diffuse PNG')
    width, height = struct.unpack('>II', png[16:24])
    if not 0 < width <= 4096 or not 0 < height <= 4096 or width*height > 16_777_216:
        raise ValueError('Oversize tree diffuse PNG')
    capsule, collider = source_capsule()
    payloads = {'tree.obj': localized.encode(), 'tree.mtl': mtl_bytes, 'material-mAF.png': png, 'tree-collider.pesc':capsule}
    provenance = dict(format='PE-sandbox-world-assets-1',
        coordinates='Source mesh-local Unity XYZ meters, translate only by each PeTree.position; unit scale',
        mesh={k: tree[k] for k in ('installed_mesh', 'installed_mesh_file', 'installed_mesh_sha256', 'vertices', 'triangles', 'source_path')},
        removed_authored_matrix=tree['matrix'], local_bounds=bounds, material=material, source_collider=collider,
        inputs={name: hashlib.sha256(data).hexdigest() for name, data in
                [('manifest.json', manifest_bytes), ('tree.obj', obj_bytes), ('tree.mtl', mtl_bytes), ('material-mAF.png', png)]},
        outputs={name: hashlib.sha256(data).hexdigest() for name, data in payloads.items()},
        limitations=['One source tree mesh reused at provisional sandbox positions; no biome/species/size selection.',
                     'Diffuse texture/tint and alpha cutoff only; no source wind, ambient occlusion, bump/specular, LOD or shadows.',
                     'PeTree remaining controls visibility; depleted trees disappear, without invented stump geometry.',
                     'Separate source capsule interaction ray; no player collision, species resource tables or save changes.'])
    payloads['manifest.json'] = (json.dumps(provenance, indent=2, sort_keys=True, allow_nan=False)+'\n').encode()
    return payloads


def export(source, output, check=False):
    source, output = source.resolve(), output.resolve()
    if output == source or source in output.parents or output in source.parents:
        raise ValueError('Output must be separate from source directory')
    payloads = prepare(source)
    if check:
        if any(not (output/name).is_file() or (output/name).read_bytes() != data for name, data in payloads.items()):
            raise ValueError('Sandbox world export differs; regenerate into a new directory')
    else:
        if output.exists():
            raise ValueError('Output must be new; existing recovered assets are preserved')
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.sandbox-world-', dir=output.parent) as temporary:
            staging = Path(temporary)
            for name, data in payloads.items():
                (staging/name).write_bytes(data)
            staging.rename(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT/'rebuild/build/reference-assets/gathering')
    parser.add_argument('--output', type=Path, default=ROOT/'rebuild/build/reference-assets/sandbox-world')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    export(args.source, args.output, args.check)
    print(('Verified' if args.check else 'Exported')+' shared source-local sandbox tree')


if __name__ == '__main__':
    main()
