#!/usr/bin/env python3
"""Recover installed tutorial herb/tree meshes at source-authored world positions.

Optional UnityPy1.25.4; diffuse source geometry preview, not hologram shader parity.
Original files are read-only. Output directory must be new to avoid partial updates.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import tempfile

from export_reference_assets import (multiply, trs, transform_point, transform_normal,
                                     floats, determinant, property_map, material_symbols)
from export_reference_player import Assets
from export_training_gathering import PREFAB, ROOT

PATHS = [f'HoloHerb/orgherbs/orgherb{i}' for i in range(1, 6)] + ['HoloTree/orgtrees/orgtree']
GATHER_IDS = [11438508, 11405274, 11434286, 11409872, 11412708]


def checked_mesh(vertices, normals, uvs, submeshes):
    if not 0 < len(vertices) <= 1000000 or len(normals) != len(vertices) or len(uvs) != len(vertices):
        raise ValueError('Invalid mesh channel sizes')
    for channel, width in ((vertices, 3), (normals, 3), (uvs, 2)):
        if any(len(v) < width or any(not math.isfinite(x) or abs(x) > 1e7 for x in v[:width]) for v in channel):
            raise ValueError('Invalid mesh channels')
    if not 0 < len(submeshes) <= 64 or sum(map(len, submeshes)) > 2000000:
        raise ValueError('Invalid submesh count')
    for faces in submeshes:
        for f in faces:
            if len(f) != 3 or any(not isinstance(i, int) or i < 0 or i >= len(vertices) for i in f):
                raise ValueError('Invalid triangle index')


def produced_matrix(chain):
    """Appearance reaches unit scale on leaf; retain every authored ancestor TRS."""
    if not 1 <= len(chain) <= 64:
        raise ValueError('Invalid hierarchy depth')
    out = None
    for index, tree in enumerate(chain):
        scale = tree['m_LocalScale']
        values = [tree[k][a] for k, axes in (('m_LocalPosition', 'xyz'), ('m_LocalRotation', 'xyzw'), ('m_LocalScale', 'xyz')) for a in axes]
        if not all(math.isfinite(v) and abs(v) < 1e7 for v in values):
            raise ValueError('Invalid source transform')
        if index == len(chain)-1:
            if any(scale[a] != 0 for a in 'xyz'):
                raise ValueError('Expected serialized hidden zero-scale hologram source')
            scale = dict(x=1., y=1., z=1.)
        local = trs(tree['m_LocalPosition'], tree['m_LocalRotation'], scale)
        out = multiply(out, local) if out else local
    if abs(determinant(out)) < 1e-10:
        raise ValueError('Singular produced transform')
    return out


def hierarchy_matrix(chain):
    out = None
    for t in chain:
        local = trs(t['m_LocalPosition'], t['m_LocalRotation'], t['m_LocalScale'])
        out = multiply(out, local) if out else local
    return out


def check_same_transform(a, b):
    for field, axes in (('m_LocalPosition', 'xyz'), ('m_LocalRotation', 'xyzw'), ('m_LocalScale', 'xyz')):
        if any(not math.isfinite(b[field][axis]) or abs(a[field][axis]-b[field][axis]) > 1e-5 for axis in axes):
            raise ValueError('Installed and source hierarchy transforms differ')


def recover(game, output):
    import UnityPy
    from UnityPy.helpers.MeshHelper import MeshHandler
    source = UnityPy.load(str(PREFAB))
    objects = {o.path_id: o for o in source.objects}
    transforms = {i: o.read_typetree() for i, o in objects.items() if o.type.name == 'Transform'}
    go_transform = {t['m_GameObject']['m_PathID']: i for i, t in transforms.items()}
    def source_chain(tid):
        chain, seen = [], set()
        while tid:
            if tid in seen or len(seen) >= 64:
                raise ValueError('Cyclic source hierarchy')
            seen.add(tid); chain.append(transforms[tid]); tid = transforms[tid]['m_Father']['m_PathID']
        return chain[::-1]
    def path(tid):
        return '/'.join(objects[t['m_GameObject']['m_PathID']].peek_name() for t in source_chain(tid)[1:])
    source_paths = {path(tid): tid for tid in transforms}
    assets = Assets(game/'PE_Client_Data')
    manager = next(o for o in assets.file('mainData').objects.values() if o.type.name == 'ResourceManager')
    matches = [assets.resolve(manager, p) for n, p in manager.read_typetree()['m_Container'] if n == 'trainingmanager_new']
    root, = [o for o in matches if o.type.name == 'GameObject']
    parts, materials = [], {}
    for index, name in enumerate(PATHS):
        print('Recovering '+name, flush=True)
        tid = source_paths[name]; chain = source_chain(tid)
        go, installed_chain = root, []
        transform, = assets.components(go, 'Transform'); installed_chain.append(transform.read_typetree())
        for child_name in name.split('/'):
            # UnityPy peek_name caches by class, which aliases source/editor and
            # installed-player GameObject layouts; decode each actual full tree.
            parent_transform, = assets.components(go, 'Transform')
            children = []
            for pointer in parent_transform.read_typetree()['m_Children']:
                child_transform = assets.resolve(parent_transform, pointer)
                child = assets.resolve(child_transform, child_transform.read_typetree()['m_GameObject'])
                if child.read_typetree()['m_Name'] == child_name: children.append(child)
            go, = children
            transform, = assets.components(go, 'Transform'); installed_chain.append(transform.read_typetree())
        if len(chain) != len(installed_chain):
            raise ValueError('Hierarchy length changed')
        for a, b in zip(chain, installed_chain): check_same_transform(a, b)
        matrix = produced_matrix(chain)
        filt, = assets.components(go, 'MeshFilter'); renderer, = assets.components(go, 'MeshRenderer')
        mesh_obj = assets.resolve(filt, filt.read_typetree()['m_Mesh'])
        mesh = MeshHandler(mesh_obj.read()); mesh.process()
        vertices, normals = list(mesh.m_Vertices), list(mesh.m_Normals)
        uvs = list(mesh.m_UV0 or [(0., 0.)]*len(vertices)); submeshes = list(mesh.get_triangles())
        checked_mesh(vertices, normals, uvs, submeshes)
        refs = renderer.read_typetree()['m_Materials']
        if len(refs) != len(submeshes): raise ValueError('Material/submesh mismatch')
        mids = []
        for ref in refs:
            material = assets.resolve(renderer, ref); key = (material.assets_file.name, material.path_id)
            materials.setdefault(key, material); mids.append(key)
        target = f'herb-{index}' if index < 5 else 'tree'
        parts.append(dict(name=target, source_path=name, source_transform=tid, installed_game_object=go.path_id,
                          installed_mesh=mesh_obj.path_id, installed_mesh_file=mesh_obj.assets_file.name,
                          installed_mesh_sha256=hashlib.sha256(mesh_obj.get_raw_data()).hexdigest(),
                          matrix=matrix, vertices=[transform_point(matrix, v) for v in vertices],
                          normals=[transform_normal(matrix, n) for n in normals], uvs=uvs, submeshes=submeshes, materials=mids))
    symbols = material_symbols(sorted(materials)); mtl = []; material_rows = []; material_blocks = {}
    for key, material in sorted(materials.items()):
        tree = material.read_typetree(); saved = tree['m_SavedProperties']
        colors = property_map(saved.get('m_Colors', [])); tint = colors.get('_Color', dict(r=1., g=1., b=1., a=1.))
        texenv = property_map(saved.get('m_TexEnvs', [])); main = texenv.get('_MainTex')
        symbol = symbols[key]
        start = len(mtl)
        mtl += [f'newmtl {symbol}\n', 'Kd '+floats([tint[a] for a in 'rgb'])+'\n', 'd '+floats([tint['a']])+'\n', 'illum 1\n']
        row = dict(file=key[0], path_id=key[1], name=tree['m_Name'], symbol=symbol, source_properties=saved)
        if main and main['m_Texture']['m_PathID']:
            texture = assets.resolve(material, main['m_Texture'])
            if texture.type.name != 'Texture2D': raise ValueError('Diffuse texture is not Texture2D')
            image = texture.read().image
            if image.width < 1 or image.height < 1 or image.width*image.height > 16777216:
                raise ValueError('Oversize diffuse texture')
            filename = f'material-{symbol}.png'; image.save(output/filename)
            mtl.append(f'map_Kd {filename}\n'); row['diffuse_file'] = filename
        mtl.append('\n'); material_rows.append(row); material_blocks[key] = ''.join(mtl[start:])
    reports = []
    for index, part in enumerate(parts):
        material_file = part['name']+'.mtl'
        (output/material_file).write_text('# Source material slots; diffuse-only preview conversion\n'+''.join(material_blocks[mid] for mid in dict.fromkeys(part['materials'])), encoding='utf-8')
        lines = ['# Baked Unity XYZ meters; original vertex/normal coordinate convention\n', 'mtllib '+material_file+'\n']
        lines += ['v '+floats(v)+'\n' for v in part['vertices']]
        lines += ['vt '+floats(uv[:2])+'\n' for uv in part['uvs']]
        lines += ['vn '+floats(n)+'\n' for n in part['normals']]
        for mid, faces in zip(part['materials'], part['submeshes']):
            lines.append('usemtl '+symbols[mid]+'\n')
            for face in faces:
                order = tuple(reversed(face)) if determinant(part['matrix']) < 0 else face
                lines.append('f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in order)+'\n')
        filename = part['name']+'.obj'; (output/filename).write_text(''.join(lines), encoding='utf-8')
        row = {k:v for k,v in part.items() if k not in ('vertices','normals','uvs','submeshes','materials')}
        row.update(obj=filename, vertices=len(part['vertices']), triangles=sum(map(len,part['submeshes'])),
                   world_bounds=[[min(v[a] for v in part['vertices']) for a in range(3)], [max(v[a] for v in part['vertices']) for a in range(3)]])
        if index < 5:
            gather = objects[GATHER_IDS[index]].read_typetree(); appearance = objects[gather['appearance']['m_PathID']].read_typetree()
            gather_chain = source_chain(go_transform[gather['m_GameObject']['m_PathID']]); matrix = hierarchy_matrix(gather_chain)
            collider_go = objects[appearance['col']['m_PathID']].read_typetree()
            collider_ids = [(v['component'] if isinstance(v,dict) else v[1])['m_PathID'] for v in collider_go['m_Component']]
            row.update(gather_index=index, gather_component=GATHER_IDS[index], gather_world_position=transform_point(matrix,[0,0,0]),
                       appearance_fields={k:v for k,v in appearance.items() if not k.startswith('m_')},
                       collider_world_matrix=hierarchy_matrix(source_chain(go_transform[appearance['col']['m_PathID']])),
                       collider_fields=[objects[i].read_typetree() for i in collider_ids if objects[i].type.name=='CapsuleCollider'])
        else:
            row['collider_fields']=[objects[13687620].read_typetree()]
            row['collider_world_matrix']=hierarchy_matrix(source_chain(go_transform[row['collider_fields'][0]['m_GameObject']['m_PathID']]))
        reports.append(row)
    inputs = {str(PREFAB): hashlib.sha256(PREFAB.read_bytes()).hexdigest()}
    return dict(format='PE-training-gathering-assets-1', coordinates='Unity XYZ world meters; DrawModel at zero translation/unit scale',
                source_inputs=inputs, installed_root_id=root.path_id, source_installed_transforms_match=True,
                produced_scale_override='Only orgherb1..5/orgtree leaf local zero scale replaced by1, matching completed Appearance fade',
                limitations=['Diffuse direct source geometry only; offscreen HoloCameraControl, billboard distortion/glow and production fade shader not reproduced.',
                             'Export does not enable interactions, grant resources, complete missions, or claim graphics validation.',
                             'Original physics collider world scaling is preserved as source fields, not replaced by render-mesh collision.'],
                mission_sequence=[719,748,757,721,751,752], props=reports, materials=material_rows)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    parser.add_argument('--output', type=Path, default=ROOT/'rebuild/build/reference-assets/gathering')
    args=parser.parse_args(); game=args.game_dir.expanduser().resolve(); output=args.output.expanduser().resolve()
    if output.exists() or output == game or game in output.parents:
        raise ValueError('Output must be new and outside original installation')
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.gathering-assets-',dir=output.parent) as temporary:
        staging=Path(temporary); report=recover(game,staging)
        (staging/'manifest.json').write_text(json.dumps(report,indent=2,sort_keys=True,allow_nan=False)+'\n',encoding='utf-8')
        staging.rename(output)
    print(f'Exported {len(report["props"])} source props to {output}')

if __name__=='__main__':main()
