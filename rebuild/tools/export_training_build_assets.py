#!/usr/bin/env python3
"""Recover tutorial753 receivers, beam textures and exact placement metadata.

Optional UnityPy1.25.4; reads original files without modifying them. The receiver
shader and particle simulation are not reproduced by this diffuse OBJ export.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import tempfile

from export_reference_assets import (IDENTITY, floats, determinant, transform_point,
                                     transform_normal, property_map, material_symbols)
from export_reference_player import Assets
from export_training_gathering import PREFAB, ROOT
from export_training_gathering_assets import checked_mesh, hierarchy_matrix, check_same_transform

RECEIVERS = ((431966, 483900, 'receiver-upright'), (444642, 433338, 'receiver-flat'))


def beam_query(matrix):
    position = transform_point(matrix, [0, 0, 0])
    right = [matrix[i][0] for i in range(3)]
    length = math.sqrt(sum(v*v for v in right))
    if not all(math.isfinite(v) for v in position+right) or length < 1e-9:
        raise ValueError('Invalid beam transform')
    direction = [v/length for v in right]
    return dict(position=position, direction=direction,
                origin=[p-15*d for p, d in zip(position, direction)], distance=20., layer=12, mask=4096)


class BuiltinAssets(Assets):
    def resolve(self, owner, reference):
        if reference['m_FileID']:
            name = owner.assets_file.externals[reference['m_FileID']-1].path
            if name == 'library/unity default resources':
                return self.file('unity default resources').objects[reference['m_PathID']]
        return super().resolve(owner, reference)

    def file(self, name):
        # Unity player external references use this fixed built-in filename.
        if name in ('unity default resources', 'unity_builtin_extra'):
            name = 'Resources/'+name
        return super().file(name)


def recover(game, output):
    import UnityPy
    from UnityPy.helpers.MeshHelper import MeshHandler
    env = UnityPy.load(str(PREFAB))
    objects = {o.path_id: o for o in env.objects}
    trees = {i:o.read_typetree() for i,o in objects.items() if o.type.name == 'Transform'}
    gos = {i:o.read_typetree() for i,o in objects.items() if o.type.name == 'GameObject'}
    got = {t['m_GameObject']['m_PathID']:i for i,t in trees.items()}
    def chain(tid):
        result, seen = [], set()
        while tid:
            if tid in seen or len(seen) >= 64: raise ValueError('Invalid source hierarchy')
            seen.add(tid); result.append(tid); tid = trees[tid]['m_Father']['m_PathID']
        return result[::-1]
    def matrix(tid): return hierarchy_matrix([trees[i] for i in chain(tid)])
    def source_path(tid): return '/'.join(gos[trees[i]['m_GameObject']['m_PathID']]['m_Name'] for i in chain(tid)[1:])
    assets = BuiltinAssets(game/'PE_Client_Data')
    manager = next(o for o in assets.file('mainData').objects.values() if o.type.name == 'ResourceManager')
    root, = [assets.resolve(manager,p) for n,p in manager.read_typetree()['m_Container'] if n == 'trainingmanager_new']
    installed = {}
    def lookup(tid):
        if tid in installed: return installed[tid]
        ids = chain(tid); go = root
        transform, = assets.components(go,'Transform'); check_same_transform(trees[ids[0]], transform.read_typetree())
        for sid in ids[1:]:
            wanted = gos[trees[sid]['m_GameObject']['m_PathID']]['m_Name']
            parent, = assets.components(go,'Transform'); candidates = []
            for ptr in parent.read_typetree()['m_Children']:
                child = assets.resolve(parent,ptr); child_tree = child.read_typetree()
                child_go = assets.resolve(child,child_tree['m_GameObject'])
                if child_go.read_typetree()['m_Name'] != wanted: continue
                try: check_same_transform(trees[sid],child_tree)
                except ValueError: continue
                candidates.append(child_go)
            if len(candidates) != 1: raise ValueError('Nonunique or changed installed transform: '+source_path(sid))
            go = candidates[0]
        installed[tid] = go
        return go
    def identity(obj):
        return dict(file=obj.assets_file.name,path_id=obj.path_id,sha256=hashlib.sha256(obj.get_raw_data()).hexdigest())
    materials = {}
    def add_material(owner, ptr):
        material = assets.resolve(owner,ptr); key = (material.assets_file.name,material.path_id)
        materials[key] = material
        return key
    beams = []
    for tid in trees[464894]['m_Children']:
        tid = tid['m_PathID']; go = lookup(tid)
        lr, = assets.components(go,'LineRenderer')
        beam = dict(source_transform=tid,source_path=source_path(tid),world_matrix=matrix(tid),
                    installed_game_object=go.path_id,query=beam_query(matrix(tid)),line_renderer=lr.read_typetree())
        for ptr in beam['line_renderer']['m_Materials']: add_material(lr,ptr)
        beams.append(beam)
    if len(beams) != 50: raise ValueError('Expected fifty source beams')
    parts, colliders = [], []
    for tid, ctid, name in RECEIVERS:
        go = lookup(tid); mat = matrix(tid)
        filt, = assets.components(go,'MeshFilter'); renderer, = assets.components(go,'MeshRenderer')
        mesh_obj = assets.resolve(filt,filt.read_typetree()['m_Mesh'])
        mesh = MeshHandler(mesh_obj.read()); mesh.process()
        vertices,normals,uvs = list(mesh.m_Vertices),list(mesh.m_Normals),list(mesh.m_UV0)
        faces = list(mesh.get_triangles()); checked_mesh(vertices,normals,uvs,faces)
        mids = [add_material(renderer,p) for p in renderer.read_typetree()['m_Materials']]
        if len(mids) != len(faces): raise ValueError('Material/submesh count mismatch')
        parts.append(dict(name=name,source_transform=tid,source_path=source_path(tid),world_matrix=mat,
                          installed_game_object=go.path_id,mesh=identity(mesh_obj),materials=mids,
                          vertices=[transform_point(mat,v) for v in vertices],normals=[transform_normal(mat,n) for n in normals],uvs=uvs,faces=faces))
        cgo = lookup(ctid); collider, = assets.components(cgo,'BoxCollider'); actual = collider.read_typetree()
        source_go = gos[trees[ctid]['m_GameObject']['m_PathID']]
        source_col, = [objects[p[1]['m_PathID']].read_typetree() for p in source_go['m_Component'] if objects[p[1]['m_PathID']].type.name == 'BoxCollider']
        fields = ('m_Size','m_Center','m_IsTrigger','m_Enabled')
        if any(actual[k] != source_col[k] for k in fields) or cgo.read_typetree()['m_Layer'] != 12 or source_go['m_Layer'] != 12:
            raise ValueError('Receiver collider differs from source')
        colliders.append(dict(name=name,source_transform=ctid,world_matrix=matrix(ctid),layer=12,
                              installed_collider=identity(collider),fields={k:actual[k] for k in fields}))
    # These are particles, not static emitter meshes. Preserve exact authored fields
    # and expose their original circle/X textures for a separately rendered marker.
    effects = []
    first = trees[464894]['m_Children'][0]['m_PathID']
    for pointer in trees[first]['m_Children']:
        tid = pointer['m_PathID']; go = lookup(tid)
        particle, = assets.components(go,'ParticleSystem'); renderer, = assets.components(go,'ParticleSystemRenderer')
        mids = [add_material(renderer,p) for p in renderer.read_typetree()['m_Materials']]
        particle_tree = particle.read_typetree(); renderer_tree = renderer.read_typetree()
        name = go.read_typetree()['m_Name']
        if renderer_tree['m_RenderMode'] != 4: raise ValueError('Expected source mesh particles')
        mesh_obj = assets.resolve(renderer, renderer_tree['m_Mesh'])
        mesh = MeshHandler(mesh_obj.read()); mesh.process()
        vertices, normals, uvs = list(mesh.m_Vertices), list(mesh.m_Normals), list(mesh.m_UV0)
        faces = list(mesh.get_triangles()); checked_mesh(vertices,normals,uvs,faces)
        if len(mids) != len(faces): raise ValueError('Particle material/submesh count mismatch')
        output_name = name.replace('_','-')
        parts.append(dict(name=output_name,source_transform=tid,source_path=source_path(tid),world_matrix=IDENTITY,
                          coordinate_space='Original particle mesh local space; apply particle/emitter pose at runtime',
                          installed_game_object=go.path_id,mesh=identity(mesh_obj),materials=mids,
                          vertices=vertices,normals=normals,uvs=uvs,faces=faces))
        effects.append(dict(name=name,source_transform=tid,mesh_obj=output_name+'.obj',
                            source_local_transform=trees[tid],particle_system=particle_tree,
                            renderer=renderer_tree,materials=mids))
    symbols = material_symbols(sorted(materials)); rows = []; blocks = {}
    for key, material in sorted(materials.items()):
        data = material.read_typetree(); saved = data['m_SavedProperties']; symbol = symbols[key]
        colors = property_map(saved.get('m_Colors',[])); color = colors.get('_Color',dict(r=1.,g=1.,b=1.,a=1.))
        lines = [f'newmtl {symbol}\n','Kd '+floats([color[a] for a in 'rgb'])+'\n','d '+floats([color['a']])+'\n','illum 1\n']
        row = dict(**identity(material),name=data['m_Name'],symbol=symbol,source_properties=saved)
        texture = property_map(saved.get('m_TexEnvs',[])).get('_MainTex')
        if texture and texture['m_Texture']['m_PathID']:
            tex = assets.resolve(material,texture['m_Texture'])
            if tex.type.name != 'Texture2D': raise ValueError('Expected Texture2D')
            image = tex.read().image
            if image.width < 1 or image.height < 1 or image.width*image.height > 16777216: raise ValueError('Invalid texture dimensions')
            filename = f'material-{symbol}.png'; image.save(output/filename)
            lines.append(f'map_Kd {filename}\n'); row.update(diffuse_file=filename,texture=identity(tex))
        blocks[key] = ''.join(lines)+'\n'; rows.append(row)
    reports = []
    for part in parts:
        name = part['name']; mids = part['materials']
        (output/(name+'.mtl')).write_text('# Source material slots; diffuse-only preview conversion\n'+''.join(blocks[k] for k in dict.fromkeys(mids)))
        lines = ['# Baked Unity XYZ meters; original vertex/normal coordinate convention\n',f'mtllib {name}.mtl\n']
        lines += ['v '+floats(v)+'\n' for v in part['vertices']]
        lines += ['vt '+floats(v[:2])+'\n' for v in part['uvs']]
        lines += ['vn '+floats(v)+'\n' for v in part['normals']]
        for key, faces in zip(mids,part['faces']):
            lines.append('usemtl '+symbols[key]+'\n')
            for face in faces:
                if determinant(part['world_matrix']) < 0: face = tuple(reversed(face))
                lines.append('f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in face)+'\n')
        (output/(name+'.obj')).write_text(''.join(lines))
        report = {k:v for k,v in part.items() if k not in ('vertices','normals','uvs','faces','materials')}
        report.update(obj=name+'.obj',vertices=len(part['vertices']),triangles=sum(map(len,part['faces'])),
                      world_bounds=[[min(v[a] for v in part['vertices']) for a in range(3)],[max(v[a] for v in part['vertices']) for a in range(3)]])
        reports.append(report)
    for effect in effects: effect['material_symbols'] = [symbols[k] for k in effect.pop('materials')]
    return dict(format='PE-training-building-assets-1',source_prefab=str(PREFAB.relative_to(ROOT)),
                source_sha256=hashlib.sha256(PREFAB.read_bytes()).hexdigest(),installed_root_id=root.path_id,
                source_installed_transforms_match=True,coordinates='Unity XYZ world meters; draw OBJ at origin/unit scale',
                props=reports,receiver_colliders=colliders,beams=beams,effects=effects,materials=rows,
                limitations=['Receiver procedural/hologram shader, particle simulation and fade are not reproduced.',
                             'Emitter roots have no static mesh; marker mesh particles export in local space with exact fields/textures, not simulated particle poses.',
                             'Layer12 gameplay queries must not use unrelated room/player collision meshes.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    parser.add_argument('--output',type=Path,default=ROOT/'rebuild/build/reference-assets/building')
    args = parser.parse_args(); game = args.game_dir.expanduser().resolve(); output = args.output.expanduser().resolve()
    if output.exists() or output == game or game in output.parents: raise ValueError('Output must be new and outside original installation')
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.building-assets-',dir=output.parent) as temporary:
        staging = Path(temporary); report = recover(game,staging)
        (staging/'manifest.json').write_text(json.dumps(report,indent=2,sort_keys=True,allow_nan=False)+'\n')
        staging.rename(output)
    print(f'Exported {len(report["props"])} receiver/marker meshes and {len(report["beams"])} beam placements to {output}')

if __name__ == '__main__': main()
