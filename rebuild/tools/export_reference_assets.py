#!/usr/bin/env python3
"""Optionally convert installed room/tool/storage bundles to OBJ/MTL/PNG, never alter sources.

Requires UnityPy==1.25.4. Positions retain Unity XYZ in meters; hierarchy and
room loader placement are baked into meshes. No UnityPy OBJ handedness flip.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import tempfile

ASSETS = {
    'room': 'scene_TrainingRoom', 'wood-axe': 'wood_ax',
    'wood-pickaxe': 'wood_hoe', 'stone-pickaxe': 'stone_hoe',
    'wood-shovel': 'wood_spade', 'stone-shovel': 'stone_spade', 'stone-axe': 'stone_ax',
    'training-kit': 'emergencykit3', 'training-knife': 'knife',
}
DEFAULT_OUTPUT = Path(__file__).resolve().parents[1] / 'build/reference-assets'
IDENTITY = [[float(i == j) for j in range(4)] for i in range(4)]


def multiply(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def trs(position, rotation, scale):
    x, y, z, w = (rotation[k] for k in ('x', 'y', 'z', 'w'))
    rows = [[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
            [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
            [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]]
    return [[rows[i][j] * scale['xyz'[j]] for j in range(3)] + [position['xyz'[i]]]
            for i in range(3)] + [[0., 0., 0., 1.]]


def determinant(m):
    a, b, c = m[0][:3]; d, e, f = m[1][:3]; g, h, i = m[2][:3]
    return a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g)


def transform_point(m, point):
    return [sum(m[i][j] * point[j] for j in range(3)) + m[i][3] for i in range(3)]


def transform_normal(m, normal):
    # Inverse transpose, valid for nonuniform hierarchy scales.
    a, b, c = m[0][:3]; d, e, f = m[1][:3]; g, h, i = m[2][:3]
    det = determinant(m)
    if abs(det) < 1e-12:
        raise ValueError('Singular object transform')
    cofactors = [[e*i-f*h, f*g-d*i, d*h-e*g],
                 [c*h-b*i, a*i-c*g, b*g-a*h],
                 [b*f-c*e, c*d-a*f, a*e-b*d]]
    result = [sum(cofactors[row][j]*normal[j] for j in range(3))/det for row in range(3)]
    length = math.sqrt(sum(v*v for v in result))
    return [v/length for v in result] if length else [0., 1., 0.]


def pointer(value):
    if value.get('m_FileID', 0) != 0:
        raise ValueError('External bundle reference requires explicit dependency recovery')
    return value['m_PathID']


def floats(values):
    if not all(math.isfinite(value) for value in values):
        raise ValueError('Non-finite mesh value')
    return ' '.join(format(value, '.9g') for value in values)


def property_map(entries):
    return {(key['name'] if isinstance(key, dict) else key): value for key, value in entries}


def material_symbols(material_ids):
    """Avoid raylib 6 tinyobj quadratic-probe insertion hangs.

    tinyobj starts with ten buckets and doubles only after all entries fill.
    Pick short ASCII names whose djb2 bucket equals their insertion index at
    the final capacity; modulo each smaller capacity is then collision-free
    for every insertion before that table grows. Source IDs stay in metadata.
    """
    capacity = 10
    while capacity < len(material_ids):
        capacity *= 2
    buckets = {}
    for first in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz':
        for second in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz':
            symbol = 'm' + first + second
            value = 5381
            for character in symbol:
                value = value * 33 + ord(character)
            buckets.setdefault(value % capacity, symbol)
    if any(index not in buckets for index in range(len(material_ids))):
        raise ValueError('Too many materials for collision-free OBJ symbols')
    return {pid: buckets[index] for index, pid in enumerate(material_ids)}


def training_kit_source():
    """Prove the installed kit3 shares kit2's source mesh/material, then use kit2 placement.

    No installed kit2 bundle exists. Its source prefab and kit3 prefab reference
    the same mesh/material GUIDs and object IDs; only kit2 placement/gameplay is used.
    """
    import UnityPy
    base = Path(__file__).resolve().parents[2] / 'Assets/Resources/Prefab/Item/Scene'
    sources = {}
    for name in ('emergencyKit2', 'emergencyKit3'):
        path = base / (name + '.prefab')
        env = UnityPy.load(str(path))
        trees = {obj.type.name: (obj, obj.read_typetree()) for obj in env.objects
                 if obj.type.name in ('MeshFilter', 'MeshRenderer', 'Transform', 'BoxCollider')}
        refs = []
        for kind, field in (('MeshFilter', 'm_Mesh'), ('MeshRenderer', 'm_Materials')):
            obj, tree = trees[kind]
            values = tree[field] if isinstance(tree[field], list) else [tree[field]]
            refs.append([(bytes(obj.assets_file.externals[value['m_FileID']-1].guid).hex(),
                          value['m_PathID']) for value in values])
        sources[name] = {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                         'references': refs, 'transform': trees['Transform'][1],
                         'box_collider': trees['BoxCollider'][1]}
    if sources['emergencyKit2']['references'] != sources['emergencyKit3']['references']:
        raise ValueError('Training kit source mesh/material no longer match installed kit3 counterpart')
    return {'prefab_guid': 'a34b46db1f3f9514bb8df822943d958f', 'warehouse_id': 3,
            'visual_source_reason': 'Kit2 and kit3 source prefab mesh/material GUID and object IDs match',
            'sources': sources, 'root_override': sources['emergencyKit2']['transform'],
            'box_collider': sources['emergencyKit2']['box_collider']}


def training_knife_source():
    """Record current source attachment metadata independently of the older visual bundle."""
    import UnityPy
    import sqlite3
    root = Path(__file__).resolve().parents[2]
    path = root / 'Assets/Resources/Prefab/Item/Equip/Weapon/Sword/knife.prefab'
    database = root / 'Assets/Resources/DataBase/localData.bytes'
    with sqlite3.connect(database.as_uri() + '?mode=ro', uri=True) as connection:
        row = connection.execute('SELECT _modelPath FROM PrototypeItem WHERE CAST(id AS INTEGER)=33').fetchone()
    expected = 'Prefab/Item/Equip/Weapon/Sword/knife'
    if not row or row[0] != expected:
        raise ValueError('Item33 no longer maps to the expected knife prefab')
    env = UnityPy.load(str(path))
    objects = list(env.objects)
    transforms = [obj.read_typetree() for obj in objects if obj.type.name == 'Transform']
    equipment = [obj.read_typetree() for obj in objects if obj.type.name == 'MonoBehaviour']
    equipment = next(tree for tree in equipment if 'm_HandChangeAttr' in tree)
    root_transform = next(tree for tree in transforms if not pointer(tree['m_Father']))
    return {'item_id': 33, 'model_path': expected, 'prefab_source': str(path),
            'prefab_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'database_source': str(database), 'database_sha256': hashlib.sha256(database.read_bytes()).hexdigest(),
            'root_override': root_transform, 'attachment': equipment['m_HandChangeAttr'],
            'prefab_transforms': transforms,
            'attachment_source': 'Assets/Scripts/PeEntity/Cmpt/BiologyViewCmpt.cs AttachObject/Reattach',
            'limitations': 'Static local-space knife only. Hand bone, character pose and swing animation are not exported. Installed visual bundle uses older legacy gameplay; current source equipment metadata is recorded separately.'}


def recover(bundle, name, output, textures):
    import UnityPy
    from UnityPy.helpers.MeshHelper import MeshHandler
    placement = (training_kit_source() if name == 'training-kit' else
                 training_knife_source() if name == 'training-knife' else None)
    environment = UnityPy.load(str(bundle))
    objects = {obj.path_id: obj for obj in environment.objects}
    trees = {pid: obj.read_typetree() for pid, obj in objects.items()
             if obj.type.name in ('GameObject', 'Transform', 'MeshFilter', 'MeshRenderer', 'MeshCollider')}
    transforms = {pid: tree for pid, tree in trees.items() if objects[pid].type.name == 'Transform'}
    by_object = {pointer(tree['m_GameObject']): pid for pid, tree in transforms.items()}
    roots = [pid for pid, tree in transforms.items() if pointer(tree['m_Father']) == 0]
    # Old tool bundles contain both a gameplay root and an unconnected import root.
    scripted = {pointer(obj.read_typetree()['m_GameObject']) for obj in objects.values()
                if obj.type.name == 'MonoBehaviour'}
    root_id = min(roots, key=lambda pid: (pointer(transforms[pid]['m_GameObject']) not in scripted, pid))
    matrices = {}

    def world_matrix(pid, visiting=None):
        if pid in matrices:
            return matrices[pid]
        visiting = set() if visiting is None else visiting
        if pid in visiting:
            raise ValueError('Cyclic transform hierarchy')
        visiting.add(pid)
        tree = transforms[pid]
        position, rotation = tree['m_LocalPosition'], tree['m_LocalRotation']
        if pid == root_id and name == 'room':
            position = dict(x=12., y=1.5, z=12.)
            rotation = dict(x=0., y=-math.sqrt(.5), z=0., w=math.sqrt(.5))
        scale = tree['m_LocalScale']
        if pid == root_id and placement:
            source = placement['root_override']
            position, rotation, scale = source['m_LocalPosition'], source['m_LocalRotation'], source['m_LocalScale']
        local = trs(position, rotation, scale)
        parent = pointer(tree['m_Father'])
        matrices[pid] = multiply(world_matrix(parent, visiting), local) if parent else local
        visiting.remove(pid)
        return matrices[pid]

    def belongs(pid):
        seen = set()
        while pid and pid not in seen:
            if pid == root_id:
                return True
            seen.add(pid)
            pid = pointer(transforms[pid]['m_Father'])
        return False

    def active(go):
        pid = by_object[go]
        while pid:
            owner = pointer(transforms[pid]['m_GameObject'])
            if not trees[owner].get('m_IsActive', True):
                return False
            pid = pointer(transforms[pid]['m_Father'])
        return True

    renderers = {pointer(tree['m_GameObject']): tree for pid, tree in trees.items()
                 if objects[pid].type.name == 'MeshRenderer'}
    parts = []
    for pid, tree in sorted(trees.items()):
        kind = objects[pid].type.name
        if kind not in ('MeshFilter', 'MeshCollider'):
            continue
        owner = pointer(tree['m_GameObject'])
        transform = by_object[owner]
        if not belongs(transform) or not active(owner):
            continue
        collision = kind == 'MeshCollider'
        renderer = renderers.get(owner, {})
        if not (tree.get('m_Enabled', True) if collision else renderer.get('m_Enabled', False)):
            continue
        mesh_id = pointer(tree['m_Mesh'])
        if not mesh_id:
            continue
        material_ids = [] if collision else [pointer(p) for p in renderer['m_Materials']]
        parts.append({'component_path_id': pid, 'game_object_path_id': owner,
                      'name': trees[owner]['m_Name'], 'mesh_path_id': mesh_id,
                      'transform_path_id': transform, 'matrix': world_matrix(transform),
                      'materials': material_ids, 'collision': collision,
                      'is_trigger': bool(tree.get('m_IsTrigger', False)),
                      'convex': bool(tree.get('m_Convex', False))})
    material_ids = sorted({pid for part in parts for pid in part['materials']})
    symbols = material_symbols(material_ids)
    materials = []
    mtl = ['# Source material slots; diffuse-only preview conversion\n']
    for pid in material_ids:
        tree = objects[pid].read_typetree()
        saved = tree['m_SavedProperties']
        colors = property_map(saved.get('m_Colors', []))
        tint = colors.get('_Color', dict(r=1., g=1., b=1., a=1.))
        texenv = property_map(saved.get('m_TexEnvs', []))
        main = texenv.get('_MainTex')
        material = {'path_id': pid, 'name': tree['m_Name'], 'obj_symbol': symbols[pid], 'properties': saved}
        mtl += [f'newmtl {symbols[pid]}\n', 'Kd '+floats([tint[k] for k in ('r', 'g', 'b')])+'\n',
                'd '+format(tint['a'], '.9g')+'\n', 'illum 1\n']
        if main and pointer(main['m_Texture']):
            tex_id = pointer(main['m_Texture'])
            material['diffuse_texture_path_id'] = tex_id
            if textures:
                filename = f'{name}-texture-{tex_id}.png'
                objects[tex_id].read().image.save(output / filename)
                material['diffuse_file'] = filename
                mtl.append(f'map_Kd {filename}\n')
        mtl.append('\n')
        materials.append(material)
    (output / f'{name}.mtl').write_text(''.join(mtl), encoding='utf-8')

    bounds = None
    for collision in (False, True):
        lines = ['# Baked Unity XYZ meters; original vertex/normal coordinate convention\n']
        if not collision:
            lines.append(f'mtllib {name}.mtl\n')
        offset = 0
        for part in parts:
            if part['collision'] != collision:
                continue
            data = objects[part['mesh_path_id']].read()
            mesh = MeshHandler(data); mesh.process()
            vertices = [transform_point(part['matrix'], v) for v in mesh.m_Vertices]
            normals = [transform_normal(part['matrix'], n) for n in mesh.m_Normals]
            if len(normals) != len(vertices):
                raise ValueError(f'Missing normals in {data.m_Name}; explicit recovery required')
            uvs = mesh.m_UV0 or [(0., 0.)] * len(vertices)
            if len(uvs) != len(vertices):
                raise ValueError('UV count mismatch')
            lines.append(f'o object_{part["game_object_path_id"]}\n')
            lines += ['v '+floats(v)+'\n' for v in vertices]
            lines += ['vt '+floats(uv[:2])+'\n' for uv in uvs]
            lines += ['vn '+floats(n)+'\n' for n in normals]
            if not collision:
                for v in vertices:
                    if bounds is None:
                        bounds = [v.copy(), v.copy()]
                    for axis in range(3):
                        bounds[0][axis] = min(bounds[0][axis], v[axis])
                        bounds[1][axis] = max(bounds[1][axis], v[axis])
            triangles = 0
            submeshes = list(mesh.get_triangles())
            if not collision and len(part['materials']) != len(submeshes):
                raise ValueError('Renderer material count does not match mesh submeshes')
            for index, faces in enumerate(submeshes):
                lines.append(f'g object_{part["game_object_path_id"]}_submesh_{index}\n')
                if not collision:
                    lines.append(f'usemtl {symbols[part["materials"][index]]}\n')
                for face in faces:
                    # A negative hierarchy scale changes winding, unlike a rotation.
                    order = tuple(reversed(face)) if determinant(part['matrix']) < 0 else face
                    lines.append('f '+' '.join(f'{i+offset+1}/{i+offset+1}/{i+offset+1}' for i in order)+'\n')
                    triangles += 1
            part['vertices'] = len(vertices)
            part['triangles'] = triangles
            part['submeshes'] = len(submeshes)
            offset += len(vertices)
        if offset:
            suffix = '-collision' if collision else ''
            (output / f'{name}{suffix}.obj').write_text(''.join(lines), encoding='utf-8')
    with bundle.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    return {'source': str(bundle), 'sha256': digest, 'bytes': bundle.stat().st_size,
            'unity_versions': sorted({a.unity_version for a in environment.assets}),
            'selected_root_transform_path_id': root_id, 'visual_bounds': bounds,
            'placement_provenance': placement,
            'parts': parts, 'materials': materials,
            'hierarchy': [{'path_id': pid, **tree} for pid, tree in sorted(transforms.items())]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path,
                        default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument('--asset', action='append', choices=sorted(ASSETS),
                        help='repeat to select assets; default room and wood-axe; training-kit/training-knife add tutorial props')
    parser.add_argument('--no-textures', action='store_true', help='retain diffuse material colors only')
    args = parser.parse_args()
    try:
        import importlib.metadata
        version = importlib.metadata.version('UnityPy')
        if version != '1.25.4':
            raise ValueError(f'Expected UnityPy1.25.4, found {version}; use the documented virtual environment')
        game_dir = args.game_dir.expanduser().resolve()
        output = args.output.expanduser().resolve()
        if output == game_dir or game_dir in output.parents:
            raise ValueError('Output must be outside the original installation')
        names = sorted(set(args.asset or ['room', 'wood-axe']))
        bundles = {name: game_dir/'AssetBundles/Item'/f'{ASSETS[name]}.unity3d' for name in names}
        for bundle in bundles.values():
            if not bundle.is_file():
                raise ValueError(f'Missing source bundle: {bundle}')
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.pe-asset-export-', dir=output.parent) as temporary:
            staging = Path(temporary)
            manifest = {'format_version': 1, 'unitypy': version,
                        'coordinates': 'Unity XYZ meters, source hierarchy baked, no axis reflection',
                        'room_loader_position': [12., 1.5, 12.], 'room_loader_rotation_degrees': [0., -90., 0.],
                        'textures': not args.no_textures, 'assets': {}}
            for name in names:
                manifest['assets'][name] = recover(bundles[name], name, staging, not args.no_textures)
            manifest['files'] = {p.name: {'bytes': p.stat().st_size,
                                 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                                 for p in sorted(staging.iterdir())}
            # An optional kit-only run must not replace the existing room manifest.
            manifest_name = f'{names[0]}-manifest.json' if len(names) == 1 and names[0].startswith('training-') else 'manifest.json'
            (staging/manifest_name).write_text(json.dumps(manifest, indent=2, sort_keys=True)+'\n')
            output.mkdir(exist_ok=True)
            for path in sorted(staging.iterdir(), key=lambda p: p.name == manifest_name):
                path.replace(output/path.name)
        print(f'Exported {", ".join(names)} to {output}')
        print('Visual preview only: gameplay scripts, collision simulation and Unity shaders are not imported.')
    except (ImportError, importlib.metadata.PackageNotFoundError):
        parser.exit(1, 'Install UnityPy==1.25.4 in an isolated virtual environment first.\n')
    except (OSError, ValueError) as error:
        parser.exit(1, f'Reference export failed: {error}\n')


if __name__ == '__main__':
    main()
