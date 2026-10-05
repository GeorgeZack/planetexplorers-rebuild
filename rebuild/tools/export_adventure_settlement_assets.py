#!/usr/bin/env python3
"""Recover six archived town buildings and their enabled physical colliders.

Requires UnityPy1.25.4; reads installed bundles without changing them. Prefab
root TRS is replaced by the entity pose as AssetsLoader does; child TRS remains.
Skinned machines use their serialized rest pose. Animation/shaders are not ported.
"""
import argparse
import hashlib
import importlib.metadata
import json
import math
from pathlib import Path
import tempfile

from export_adventure_settlement import ROOT, BUILDINGS, build_manifest
from read_reference_adventure import DEFAULT_SAVE, Reader, records
from export_reference_assets import (multiply,trs,transform_point,transform_normal,
    determinant,pointer,property_map,material_symbols,floats)
from export_training_gathering_assets import checked_mesh
from export_training_npcs import skin_rest

GAME = Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers'
OUTPUT = ROOT/'rebuild/build/reference-assets/adventure-settlement'
CROP_ORIGIN = [-627.,16.,323.]


def matrix(position,rotation,scale=(1,1,1)):
    values = list(position)+list(rotation)+list(scale)
    if len(position)!=3 or len(rotation)!=4 or len(scale)!=3 or not all(math.isfinite(v) for v in values):
        raise ValueError('Invalid transform values')
    if abs(sum(v*v for v in rotation)-1)>1e-5 or any(abs(v)<1e-10 for v in scale):
        raise ValueError('Singular or non-unit transform')
    return trs(dict(zip('xyz',position)),dict(zip('xyzw',rotation)),dict(zip('xyz',scale)))


def box_mesh(center,size):
    if len(center)!=3 or len(size)!=3 or not all(math.isfinite(v) for v in center+size) or any(v<=0 for v in size):
        raise ValueError('Invalid BoxCollider dimensions')
    vertices = [[center[a]+size[a]*(.5 if corner&(1<<a) else -.5) for a in range(3)] for corner in range(8)]
    faces = [(0,4,6),(0,6,2),(1,3,7),(1,7,5),(0,1,5),(0,5,4),
             (2,6,7),(2,7,3),(0,2,3),(0,3,1),(4,5,7),(4,7,6)]
    return vertices,[faces]


def validate_convex(vertices,submeshes):
    """Only preserve a convex input surface; never silently use a concave cook input."""
    unique = set(tuple(v) for v in vertices)
    if len(unique)<4: raise ValueError('Degenerate convex collider')
    edges, plane_count = {},0
    for faces in submeshes:
        for face in faces:
            a,b,c = [vertices[i] for i in face]
            u,v = [b[i]-a[i] for i in range(3)],[c[i]-a[i] for i in range(3)]
            n = [u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
            length = math.sqrt(sum(x*x for x in n))
            if length<1e-10: raise ValueError('Degenerate convex face')
            distances = [sum(n[i]*(p[i]-a[i]) for i in range(3))/length for p in unique]
            if min(distances)<-1e-5 and max(distances)>1e-5:
                raise ValueError('Convex collider requires explicit hull recovery')
            plane_count += 1
            for x,y in ((a,b),(b,c),(c,a)):
                key = tuple(sorted((tuple(x),tuple(y))))
                edges[key] = edges.get(key,0)+1
    if plane_count<4 or any(count!=2 for count in edges.values()):
        raise ValueError('Open convex collider surface')


def alive_building_ids(data):
    r = Reader(data)
    r.version(20160902)
    for _ in range(r.count()):
        r.unpack('ii')
        if not all(math.isfinite(v) for v in r.unpack('dd')): raise ValueError('Nonfinite town state')
    towns = {}
    for _ in range(r.count()):
        town = r.integer()
        if town in towns: raise ValueError('Duplicate alive-building town')
        towns[town] = [r.integer() for _ in range(r.count())]
    r.end()
    if len(towns.get(0,[]))!=len(set(towns.get(0,[]))): raise ValueError('Duplicate alive building')
    return set(towns.get(0,[]))


def recover_bundle(bundle,entry,crop):
    import UnityPy
    from UnityPy.helpers.MeshHelper import MeshHandler
    env = UnityPy.load(str(bundle))
    objects = {o.path_id:o for o in env.objects}
    if len(objects)!=len(list(env.objects)): raise ValueError('Duplicate bundle path IDs')
    kinds = {pid:o.type.name for pid,o in objects.items()}
    selected = {'GameObject','Transform','MeshFilter','MeshRenderer','SkinnedMeshRenderer','MeshCollider','BoxCollider','SphereCollider','CapsuleCollider'}
    trees = {pid:o.read_typetree() for pid,o in objects.items() if kinds[pid] in selected}
    transforms = {pid:t for pid,t in trees.items() if kinds[pid]=='Transform'}
    by_owner = {pointer(t['m_GameObject']):pid for pid,t in transforms.items()}
    if len(by_owner)!=len(transforms): raise ValueError('Duplicate GameObject transform')
    containers = [o.read_typetree()['m_Container'] for o in objects.values() if o.type.name=='AssetBundle']
    if len(containers)!=1 or len(containers[0])!=1: raise ValueError('Ambiguous prefab bundle container')
    container_name,container = containers[0][0]
    root = by_owner[pointer(container['asset'])]
    if pointer(transforms[root]['m_Father']): raise ValueError('Prefab container is not a root')
    # Source OnBoneLoad initializes living actors with ActivateDeathMode(false).
    # Prefabs serialize both intact/death branches active; do not render both.
    forced_active,death_models = {},[]
    for pid,o in objects.items():
        if o.type.name!='MonoBehaviour': continue
        t = o.read_typetree()
        script = objects[pointer(t['m_Script'])].read_typetree()
        if script.get('m_ClassName')!='PEModelController' or not t.get('m_Enabled',True): continue
        death = pointer(t['deathModel'])
        if death:
            owner = pointer(t['m_GameObject'])
            forced_active[owner],forced_active[death] = True,False
            death_models.append(dict(controller_id=pid,living_game_object=owner,death_game_object=death))
    instance = matrix([a-b for a,b in zip(entry['position'],crop)],entry['rotation'])
    matrices = {}
    def world(pid,visiting=None):
        if pid in matrices: return matrices[pid]
        visiting = set() if visiting is None else visiting
        if pid in visiting or len(visiting)>128: raise ValueError('Cyclic/deep hierarchy')
        visiting.add(pid)
        t = transforms[pid]
        if pid==root:
            m = instance
        else:
            parent = pointer(t['m_Father'])
            if not parent: raise ValueError('Unrelated transform root')
            local = matrix([t['m_LocalPosition'][k] for k in 'xyz'],[t['m_LocalRotation'][k] for k in 'xyzw'],[t['m_LocalScale'][k] for k in 'xyz'])
            m = multiply(world(parent,visiting),local)
        matrices[pid] = m
        visiting.remove(pid)
        return m
    def active(owner):
        seen = set()
        pid = by_owner[owner]
        while pid:
            if pid in seen: raise ValueError('Cyclic hierarchy')
            seen.add(pid)
            ancestor = pointer(transforms[pid]['m_GameObject'])
            if not forced_active.get(ancestor,trees[ancestor].get('m_IsActive',True)): return False
            if pid==root: return True
            pid = pointer(transforms[pid]['m_Father'])
        return False
    renderers = {pointer(t['m_GameObject']):t for pid,t in trees.items() if kinds[pid]=='MeshRenderer'}
    parts,skipped = [],[]
    for pid,t in sorted(trees.items()):
        kind = kinds[pid]
        if kind not in ('MeshFilter','SkinnedMeshRenderer','MeshCollider','BoxCollider','SphereCollider','CapsuleCollider'): continue
        owner = pointer(t['m_GameObject'])
        collision = kind.endswith('Collider')
        renderer = renderers.get(owner,{}) if kind=='MeshFilter' else t
        enabled = t.get('m_Enabled',True) if collision else renderer.get('m_Enabled',False)
        reason = 'inactive' if not active(owner) else 'disabled' if not enabled else 'trigger' if collision and t.get('m_IsTrigger',False) else None
        if reason:
            skipped.append(dict(component_id=pid,kind=kind,reason=reason));continue
        if kind in ('SphereCollider','CapsuleCollider'): raise ValueError('Unsupported enabled collider '+kind)
        m = world(by_owner[owner])
        materials = [] if collision else [(entry['entity_id'],pointer(v)) for v in renderer['m_Materials']]
        part = dict(entity_id=entry['entity_id'],prototype_id=entry['prototype_id'],component_id=pid,
                    game_object_id=owner,name=trees[owner]['m_Name'],kind=kind,collision=collision,
                    matrix=m,materials=materials,convex=bool(t.get('m_Convex',False)))
        if kind=='BoxCollider':
            center,size = [t['m_Center'][k] for k in 'xyz'],[t['m_Size'][k] for k in 'xyz']
            vertices,submeshes = box_mesh(center,size)
            normals,uvs = [[0,1,0]]*len(vertices),[[0,0]]*len(vertices)
            part.update(center=center,size=size)
        else:
            mesh_id = pointer(t['m_Mesh'])
            if not mesh_id: raise ValueError('Enabled renderer/collider has no mesh')
            raw = objects[mesh_id].read_typetree()
            mesh = MeshHandler(objects[mesh_id].read());mesh.process()
            vertices,normals = mesh.m_Vertices,mesh.m_Normals
            if collision: normals = normals or [[0,1,0]]*len(vertices)
            uvs = [v[:2] for v in mesh.m_UV0] if mesh.m_UV0 else [[0,0]]*len(vertices)
            submeshes = [list(faces) for faces in mesh.get_triangles()]
            part.update(mesh_id=mesh_id,mesh_sha256=hashlib.sha256(objects[mesh_id].get_raw_data()).hexdigest())
            if kind=='SkinnedMeshRenderer':
                bones = [pointer(v) for v in t['m_Bones']]
                binds = [[[b[f'e{r}{c}'] for c in range(4)] for r in range(4)] for b in raw['m_BindPose']]
                skin = raw['m_Skin']
                if len(binds)!=len(bones) or len(skin)!=len(vertices): raise ValueError('Skin channels differ')
                palette = [multiply(world(b),bind) for b,bind in zip(bones,binds)]
                vertices,normals = skin_rest(vertices,normals,skin,palette)
                part.update(bones=bones,pose='serialized rest pose')
        checked_mesh(vertices,normals,uvs,submeshes)
        if part['convex']: validate_convex(vertices,submeshes)
        if kind!='SkinnedMeshRenderer':
            vertices = [transform_point(m,v) for v in vertices]
            normals = [transform_normal(m,n) for n in normals]
        if determinant(m)<0: submeshes = [[tuple(reversed(face)) for face in faces] for faces in submeshes]
        checked_mesh(vertices,normals,uvs,submeshes)
        if not collision and len(materials)!=len(submeshes): raise ValueError('Material/submesh count differs')
        part.update(vertices=vertices,normals=normals,uvs=uvs,submeshes=submeshes)
        parts.append(part)
    if not any(p['collision'] for p in parts) or not any(not p['collision'] for p in parts): raise ValueError('Building missing visual/collision geometry')
    material_data = {}
    for key in sorted({key for p in parts for key in p['materials']}):
        raw = objects[key[1]].read_typetree()
        props = raw['m_SavedProperties']
        color = property_map(props.get('m_Colors',[])).get('_Color',dict(r=1,g=1,b=1,a=1))
        tex = property_map(props.get('m_TexEnvs',[])).get('_MainTex')
        texture = pointer(tex['m_Texture']) if tex else 0
        rgba = [color[k] for k in 'rgba']
        if not all(math.isfinite(x) for x in rgba): raise ValueError('Nonfinite material tint')
        material_data[key] = dict(name=raw['m_Name'],tint=rgba,properties=props,
            uv_scale=[tex['m_Scale'][k] for k in 'xy'] if tex else [1,1],
            uv_offset=[tex['m_Offset'][k] for k in 'xy'] if tex else [0,0],
            texture_id=texture,image=objects[texture].read().image if texture else None)
    report = dict(entity_id=entry['entity_id'],prototype_id=entry['prototype_id'],
        source=str(bundle),sha256=hashlib.sha256(bundle.read_bytes()).hexdigest(),
        container=container_name,root_transform_id=root,root_override='Entity pose and unit scale, AssetsLoader.AddReq/InstantiateAssetImm',
        original_root_transform=transforms[root],hierarchy=[dict(path_id=pid,**t) for pid,t in sorted(transforms.items())],
        living_model_overrides=death_models,skipped=skipped,missing_shapes=[])
    return parts,material_data,report


def write_obj(path,parts,symbols,materials,collision):
    # pe_reference_validate recognizes this exact generated-format signature.
    # Crop/instance provenance lives in manifest.json, outside the strict grammar.
    lines = ['# Baked Unity XYZ meters; original vertex/normal coordinate convention\n']
    if not collision: lines.append('mtllib settlement.mtl\n')
    offset = triangle_count = 0
    bounds = None
    for part in parts:
        if part['collision']!=collision: continue
        for index,faces in enumerate(part['submeshes']):
            vertices,normals,uvs = part['vertices'],part['normals'],part['uvs']
            lines.append('o entity_%d_component_%d_sub_%d\n'%(part['entity_id'],part['component_id'],index))
            if not collision:
                key = part['materials'][index]
                material = materials[key]
                uvs = [[uv[a]*material['uv_scale'][a]+material['uv_offset'][a] for a in range(2)] for uv in uvs]
            lines += ['v '+floats(v)+'\n' for v in vertices]
            if not collision:
                lines += ['vt '+floats(v)+'\n' for v in uvs]+['vn '+floats(v)+'\n' for v in normals]
                lines.append('usemtl '+symbols[key]+'\n')
            for face in faces:
                ids = [i+offset+1 for i in face]
                lines.append('f '+' '.join(str(i) if collision else f'{i}/{i}/{i}' for i in ids)+'\n')
                triangle_count += 1
            for v in vertices:
                if bounds is None: bounds = [list(v),list(v)]
                for a in range(3): bounds[0][a],bounds[1][a] = min(bounds[0][a],v[a]),max(bounds[1][a],v[a])
            offset += len(vertices)
    if not offset or not triangle_count: raise ValueError('Empty OBJ output')
    path.write_text(''.join(lines))
    return dict(vertices=offset,triangles=triangle_count,bounds=bounds)


def recover(game,output,settlement,crop=CROP_ORIGIN):
    parts,materials,assets = [],{},[]
    for b in settlement['buildings']:
        path = game/'AssetBundles'/b['resource']
        if path.is_symlink() or not path.is_file(): raise ValueError('Missing/linked source bundle '+str(path))
        p,m,a = recover_bundle(path,b,crop)
        parts.extend(p);materials.update(m);assets.append(a)
    symbols = material_symbols(sorted(materials))
    mtl,material_report = ['# Source material slots; diffuse-only preview conversion\n'],[]
    for key,material in sorted(materials.items()):
        color = material['tint']
        mtl += ['newmtl '+symbols[key]+'\n','Kd '+floats(color[:3])+'\n','d '+floats(color[3:])+'\n','illum 1\n']
        filename = None
        if material['image'] is not None:
            filename = 'settlement-%d-%d.png'%(key[0],material['texture_id'])
            material['image'].save(output/filename)
            mtl.append('map_Kd '+filename+'\n')
        mtl.append('\n')
        material_report.append(dict(entity_id=key[0],path_id=key[1],symbol=symbols[key],
            **{k:v for k,v in material.items() if k!='image'},texture_file=filename))
    (output/'settlement.mtl').write_text(''.join(mtl))
    visual = write_obj(output/'settlement.obj',parts,symbols,materials,False)
    collision = write_obj(output/'settlement-collision.obj',parts,symbols,materials,True)
    reports = []
    for p in parts:
        reports.append(dict(**{k:v for k,v in p.items() if k not in ('vertices','normals','uvs','submeshes')},
            vertices=len(p['vertices']),triangles=sum(len(f) for f in p['submeshes']),submeshes=len(p['submeshes'])))
    for a in assets:
        if hashlib.sha256(Path(a['source']).read_bytes()).hexdigest()!=a['sha256']: raise ValueError('Bundle changed during recovery')
    return dict(schema=1,crop_origin=crop,coordinates='Unity XYZ meters; source positions minus crop origin; no axis reflection',
        placement_sources=settlement['sources'],assets=assets,parts=reports,materials=material_report,
        visual=visual,collision=collision,missing_shapes=[],
        limitations=['Serialized rest pose for repair and solar skinned meshes; no machine animation.',
            'Diffuse colors/textures and authored UV scale/offset only; Unity shader lighting, normal/specular/emission maps and animation are not ported.',
            'Convex board collider surface is validated closed/convex; exact PhysX cooking tolerances are not reproduced.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir',type=Path,default=GAME)
    parser.add_argument('--output',type=Path,default=OUTPUT)
    parser.add_argument('--check',action='store_true')
    args = parser.parse_args()
    try:
        if importlib.metadata.version('UnityPy')!='1.25.4': raise ValueError('Requires UnityPy1.25.4')
        game,output = args.game_dir.resolve(),args.output.resolve()
        if game==output or game in output.parents: raise ValueError('Output must be outside installed game')
        settlement = build_manifest()
        rows,_,_ = records(DEFAULT_SAVE)
        if alive_building_ids(rows['VArtifactTownArchiveMgr'])!={row[0] for row in BUILDINGS}:
            raise ValueError('Archived starting buildings are not the expected living set')
        if [(b['entity_id'],b['prototype_id'],b['townhouse_id'],b['slot']) for b in settlement['buildings']]!=list(BUILDINGS):
            raise ValueError('Unexpected settlement building set')
        output.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.settlement-',dir=output.parent) as temp:
            staging = Path(temp)
            manifest = recover(game,staging,settlement)
            manifest['files'] = {p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(staging.iterdir())}
            (staging/'manifest.json').write_text(json.dumps(manifest,indent=2,sort_keys=True)+'\n')
            if args.check:
                for p in staging.iterdir():
                    if not (output/p.name).is_file() or (output/p.name).read_bytes()!=p.read_bytes(): raise ValueError('Stale asset '+p.name)
            else:
                output.mkdir(exist_ok=True)
                for p in sorted(staging.iterdir(),key=lambda p:p.name=='manifest.json'): p.replace(output/p.name)
        print(json.dumps(dict(output=str(output),visual=manifest['visual'],collision=manifest['collision'],missing_shapes=[])))
        return 0
    except (OSError,ValueError,KeyError,ImportError,importlib.metadata.PackageNotFoundError) as error:
        parser.exit(1,'Settlement asset export failed: '+str(error)+'\n')


if __name__=='__main__': raise SystemExit(main())
