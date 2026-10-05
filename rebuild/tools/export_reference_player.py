#!/usr/bin/env python3
"""Recover default modular player meshes, skin data and Avatar default pose.

Optional UnityPy==1.25.4. This does NOT evaluate humanoid muscle animations.
Outputs are a diffuse OBJ preview and a lossless skin/Avatar JSON research fixture.
"""
from __future__ import annotations
import argparse
from contextlib import closing
import hashlib
import json
import math
from pathlib import Path
import sqlite3
import tempfile
import zlib

from export_reference_assets import IDENTITY, multiply, trs, transform_point, transform_normal, property_map, material_symbols
from export_reference_actor import skin_position

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def default_parts(database, sex):
    with closing(sqlite3.connect(database.as_uri()+'?mode=ro', uri=True)) as connection:
        rows = connection.execute('SELECT ID,Type,Sex,ModelPath FROM CustomMetaData').fetchall()
    selected = [row for row in rows if int(row[2]) == (2 if sex == 'male' else 1)]
    heads = [row for row in selected if int(row[1]) == 1]
    hairs = [row for row in selected if int(row[1]) == 2]
    if not heads or len(hairs) < 2:
        raise ValueError('Missing default head/hair selection')
    body = f'Model/PlayerModel/{sex.capitalize()}'
    parts = [body+'-'+name+'_0' for name in ('torso', 'legs', 'hands', 'feet')]
    parts += [heads[0][3], *hairs[1][3].split(';')]
    if len(parts) != 8 or len(set(parts)) != 8:
        raise ValueError('Expected eight distinct default body parts')
    return parts, {'head_row_id': int(heads[0][0]), 'hair_row_id': int(hairs[1][0]),
                   'head_index': 0, 'hair_index': 1}



def starter_parts(database, sex):
    """SingleGameInitData.AddDefaultClothToPlayer, filtered by item sex."""
    paths, selection = default_parts(database, sex)
    ids = [113,149,210] if sex == 'male' else [95,131,192]
    expected = [(0,2),(1,64),(3,128)]  # torso, legs, feet; AvatarData.GetSlot masks
    clothing = []
    with closing(sqlite3.connect(database.as_uri()+'?mode=ro',uri=True)) as connection:
        for item_id,(part_index,mask) in zip(ids,expected):
            row = connection.execute('SELECT _modelPath,sex,_replacePos FROM PrototypeItem WHERE CAST(id AS INTEGER)=?',(item_id,)).fetchone()
            if not row or int(row[1]) != (2 if sex == 'male' else 1) or int(row[2]) != mask:
                raise ValueError('Starter clothing metadata changed')
            paths[part_index] = row[0]
            clothing.append({'item_id':item_id,'resource':row[0],'replace_mask':mask,'part_index':part_index})
    selection['starter_clothing'] = clothing
    return paths,selection


def avatar_worlds(avatar):
    data = avatar['m_Avatar']
    skeleton = data['m_AvatarSkeleton']['data']
    pose = data['m_DefaultPose']['data']['m_X']
    names = data['m_SkeletonNameIDArray']
    if not len(skeleton['m_Node']) == len(pose) == len(names):
        raise ValueError('Avatar hierarchy/pose counts disagree')
    worlds, by_name = [], {}
    for index, (node, local, name) in enumerate(zip(skeleton['m_Node'], pose, names)):
        parent = node['m_ParentId']
        if parent < -1 or parent >= index:
            raise ValueError('Avatar hierarchy is not parent-first')
        matrix = trs(local['t'], local['q'], local['s'])
        if parent >= 0:
            matrix = multiply(worlds[parent], matrix)
        if name in by_name:
            raise ValueError('Duplicate Avatar bone name hash')
        worlds.append(matrix); by_name[name] = matrix
    return worlds, by_name


def validate_skin(skin, bone_count):
    for vertex in skin:
        weights = [vertex[f'weight[{i}]'] for i in range(4)]
        joints = [vertex[f'boneIndex[{i}]'] for i in range(4)]
        if (any(not math.isfinite(w) or w < 0 or w > 1 for w in weights) or
                abs(sum(weights)-1) > .001 or any(j < 0 or j >= bone_count for j in joints)):
            raise ValueError('Invalid player skin weights or joints')


def packed_channels(clip, avatar):
    """Decode packed scalars without mislabeling humanoid channels as bone TRS."""
    data = clip['m_MuscleClip']['m_Clip']['data']
    dense, constants = data['m_DenseClip'], data['m_ConstantClip']['data']
    if data['m_StreamedClip']['curveCount'] != 0:
        raise ValueError('Streamed player animation curves are unsupported')
    frames, count = dense['m_FrameCount'], dense['m_CurveCount']
    samples = dense['m_SampleArray']
    if frames < 1 or count < 1 or len(samples) != frames*count:
        raise ValueError('Invalid dense player animation dimensions')
    if not all(math.isfinite(value) for value in samples+constants):
        raise ValueError('Nonfinite player animation channels')
    paths = dict(avatar['m_TOS'])
    rows, offset = [], 0
    for binding in clip['m_ClipBindingConstant']['genericBindings']:
        if binding['isPPtrCurve']:
            raise ValueError('Unexpected pointer animation channel')
        if binding['classID'] == 4 and binding['attribute'] in (1,2,3):
            size = 4 if binding['attribute'] == 2 else 3
            kind = 'direct_transform'
        elif binding['classID'] == 95 and binding['customType'] == 8:
            size, kind = 1, 'humanoid_scalar_unretargeted'
        elif binding['classID'] == 95 and binding['customType'] == 0:
            size, kind = 1, 'animator_scalar_unclassified'
        else:
            raise ValueError('Unsupported player animation binding')
        if offset < count < offset+size or offset+size > count+len(constants):
            raise ValueError('Player binding crosses packed channel boundary')
        values = ([samples[f*count+offset:f*count+offset+size] for f in range(frames)]
                  if offset < count else [constants[offset-count:offset-count+size]])
        rows.append({'binding':binding,'kind':kind,'scalar_offset':offset,'width':size,
                     'path':paths.get(binding['path']), 'samples':values})
        offset += size
    if offset != count+len(constants):
        raise ValueError('Player animation scalar coverage mismatch')
    return {'name':clip['m_Name'],'frame_count':frames,'sample_rate':dense['m_SampleRate'],
            'begin_time':dense['m_BeginTime'],'stop_time':clip['m_MuscleClip']['m_StopTime'],
            'dense_scalars':count,'constant_scalars':len(constants),'bindings':rows,
            'bone_poses_evaluated':False}


def zero_muscle_probe(avatar):
    """Diagnostic only: default/T-pose is NOT asserted to mean zero muscles."""
    human = avatar['m_Avatar']['m_Human']['data']
    skeleton = human['m_Skeleton']['data']; poses = human['m_SkeletonPose']['data']['m_X']
    def quat(value): return [value[k] for k in 'xyzw']
    def product(a,b):
        x,y,z,w=a; X,Y,Z,W=b
        return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]
    errors=[]
    for index,node in enumerate(skeleton['m_Node']):
        if node['m_AxesId'] < 0: continue
        axes=skeleton['m_AxesArray'][node['m_AxesId']]
        pre,post=quat(axes['m_PreQ']),quat(axes['m_PostQ'])
        candidate=product(pre,[-post[0],-post[1],-post[2],post[3]])
        expected=quat(poses[index]['q'])
        residual=min(sum((a-b)**2 for a,b in zip(candidate,expected)),
                     sum((a+b)**2 for a,b in zip(candidate,expected)))**.5
        errors.append({'human_skeleton_index':index,'quaternion_residual':residual})
    return {'formula':'preQ * inverse(postQ)', 'comparison':'Human.m_SkeletonPose local rotation',
            'interpretation':'Diagnostic mismatch does not disprove the swing/twist formula: stored default pose need not have zero muscle values. No retargeting validation achieved.',
            'max_residual':max(row['quaternion_residual'] for row in errors),'bones':errors}


class Assets:
    def __init__(self, data):
        import UnityPy
        self.UnityPy = UnityPy
        self.data, self.files = data, {}

    def file(self, name):
        if name not in self.files:
            path = self.data/name
            if not path.is_file():
                raise ValueError(f'Missing installed asset: {path}')
            env = self.UnityPy.load(str(path))
            self.files[name] = (env, next(iter(env.files.values())))
        return self.files[name][1]

    def resolve(self, owner, reference):
        file = owner.assets_file
        if reference['m_FileID']:
            name = file.externals[reference['m_FileID']-1].path
            if '/' in name or '\\' in name:
                raise ValueError(f'Unsupported external asset {name}')
            file = self.file(name)
        return file.objects[reference['m_PathID']]

    def components(self, obj, kind):
        result = []
        for entry in obj.read_typetree()['m_Component']:
            ref = entry['component'] if isinstance(entry, dict) else entry[1]
            component = self.resolve(obj, ref)
            if component.type.name == kind:
                result.append(component)
        return result

    def child(self, obj, name):
        transform, = self.components(obj, 'Transform')
        for ref in transform.read_typetree()['m_Children']:
            child = self.resolve(transform, ref)
            go = self.resolve(child, child.read_typetree()['m_GameObject'])
            if go.peek_name() == name:
                return go
        raise ValueError(f'Missing child {obj.peek_name()}/{name}')


def recover(game_dir, output, sexes, appearance='default'):
    from UnityPy.helpers.MeshHelper import MeshHandler
    assets = Assets(game_dir/'PE_Client_Data')
    manager = next(obj for obj in assets.file('mainData').objects.values() if obj.type.name == 'ResourceManager')
    containers = manager.read_typetree()['m_Container']
    resources = {}
    # Resolve only selected resource paths; avoid loading unrelated large assets.
    def resource(path, kind):
        key = (path.lower(), kind)
        if key not in resources:
            matches = [assets.resolve(manager, ref) for name, ref in containers if name == key[0]]
            matches = [obj for obj in matches if obj.type.name == kind]
            if len(matches) != 1:
                raise ValueError(f'Expected one {kind} at {path}, got {len(matches)}')
            resources[key] = matches[0]
        return resources[key]
    database = ROOT/'Assets/Resources/DataBase/localData.bytes'
    reports = []
    stream_sources = set()
    for sex in sexes:
        paths, selection = (starter_parts(database, sex) if appearance == 'starter' else default_parts(database, sex))
        body_path = f'Model/PlayerModel/{sex.capitalize()}'
        avatar_obj = resource(body_path, 'Avatar')
        avatar = avatar_obj.read_typetree()
        worlds, by_name = avatar_worlds(avatar)
        parts, mats = [], {}
        for path in paths:
            resource_path, child_name = path.rsplit('-', 1)
            root = resource(resource_path, 'GameObject')
            child = assets.child(root, child_name)
            renderer_obj, = assets.components(child, 'SkinnedMeshRenderer')
            renderer = renderer_obj.read_typetree()
            mesh_obj = assets.resolve(renderer_obj, renderer['m_Mesh'])
            raw = mesh_obj.read_typetree()
            mesh = MeshHandler(mesh_obj.read()); mesh.process()
            bones = [assets.resolve(renderer_obj, ref) for ref in renderer['m_Bones']]
            names = [assets.resolve(bone, bone.read_typetree()['m_GameObject']).peek_name() for bone in bones]
            binds = [[[bind[f'e{r}{c}'] for c in range(4)] for r in range(4)] for bind in raw['m_BindPose']]
            if len(binds) != len(bones):
                raise ValueError('Skin bind/bone count mismatch')
            hashes = [zlib.crc32(name.encode()) for name in names]
            extra_bones = []
            extra_local = {}
            def bone_world(bone, seen=None):
                seen = set() if seen is None else seen
                if bone.path_id in seen or len(seen) > 256:
                    raise ValueError('Cyclic player bone ancestry')
                seen.add(bone.path_id)
                tree = bone.read_typetree()
                name = assets.resolve(bone, tree['m_GameObject']).peek_name()
                code = zlib.crc32(name.encode())
                if code in by_name:
                    return by_name[code]
                if not tree['m_Father']['m_PathID']:
                    raise ValueError('Unmapped player bone ancestry')
                if name not in extra_bones: extra_bones.append(name)
                extra_local[name] = tree
                parent = assets.resolve(bone, tree['m_Father'])
                return multiply(bone_world(parent, seen), trs(tree['m_LocalPosition'], tree['m_LocalRotation'], tree['m_LocalScale']))
            palette = [multiply(bone_world(bone), bind) for bone, bind in zip(bones, binds)]
            skin = raw['m_Skin']; validate_skin(skin, len(bones))
            positions, normals, uv = mesh.m_Vertices, mesh.m_Normals, mesh.m_UV0
            if not len(positions) == len(normals) == len(uv) == len(skin):
                raise ValueError('Player vertex channel counts mismatch')
            if not all(math.isfinite(v) for channel in (positions, normals, uv) for row in channel for v in row):
                raise ValueError('Nonfinite player mesh channel')
            posed, posed_normals = [], []
            for pos, normal, row in zip(positions, normals, skin):
                weights = [row[f'weight[{i}]'] for i in range(4)]
                indices = [row[f'boneIndex[{i}]'] for i in range(4)]
                posed.append(skin_position(pos[:3], weights, indices, palette))
                n = [0., 0., 0.]
                for w,j in zip(weights,indices):
                    value = transform_normal(palette[j], normal[:3])
                    for axis in range(3): n[axis] += w*value[axis]
                length = math.sqrt(sum(v*v for v in n))
                if length < 1e-8: raise ValueError('Degenerate skinned normal')
                posed_normals.append([v/length for v in n])
            submeshes = list(mesh.get_triangles())
            if len(submeshes) != len(renderer['m_Materials']):
                raise ValueError('Player material slot count mismatch')
            materials = []
            for ref in renderer['m_Materials']:
                obj = assets.resolve(renderer_obj, ref)
                key = (obj.assets_file.name, obj.path_id)
                materials.append(key); mats[key] = obj
            part = {'resource': path, 'root_id': root.path_id, 'renderer_id': renderer_obj.path_id,
                    'mesh_id': mesh_obj.path_id, 'bone_ids': [b.path_id for b in bones], 'bone_names': names,
                    'inverse_binds': binds, 'avatar_default_palettes': palette, 'extra_bones_source_local_pose':extra_bones, 'extra_bone_transforms':extra_local,
                    'positions': [v[:3] for v in positions], 'normals': [v[:3] for v in normals],
                    'uv': [v[:2] for v in uv], 'skin': skin, 'triangles': submeshes,
                    'materials': materials, 'default_positions': posed, 'default_normals': posed_normals}
            parts.append(part)
        symbols = material_symbols(sorted(mats))
        prefix = f'player-{sex}-{appearance}'
        mtl, material_reports = [], []
        for key in sorted(mats):
            obj = mats[key]; tree = obj.read_typetree(); saved = tree['m_SavedProperties']
            color = property_map(saved['m_Colors']).get('_Color', dict(r=1.,g=1.,b=1.,a=1.))
            mtl += [f'newmtl {symbols[key]}\n', 'Kd '+' '.join(f'{color[k]:.9g}' for k in 'rgb')+'\n',f'd {color["a"]:.9g}\n','illum 1\n']
            texenv = property_map(saved['m_TexEnvs']).get('_MainTex')
            report = {'asset':key[0], 'path_id':key[1], 'symbol':symbols[key], 'name':tree['m_Name'], 'properties':saved}
            if texenv and texenv['m_Texture']['m_PathID']:
                texture = assets.resolve(obj, texenv['m_Texture'])
                filename = f'{prefix}-texture-{texture.path_id}.png'
                texture_tree = texture.read_typetree()
                stream = texture_tree.get('m_StreamData', {})
                if stream.get('path'):
                    stream_path = assets.data/Path(stream['path']).name
                    if not stream_path.is_file():
                        raise ValueError(f'Missing texture stream {stream_path}')
                    stream_sources.add(stream_path)
                    report['diffuse_stream'] = stream
                texture.read().image.save(output/filename)
                mtl += [f'map_Kd {filename}\n']
                report['diffuse_file'] = filename
            material_reports.append(report)
        (output/f'{prefix}.mtl').write_text(''.join(mtl))
        lines = [f'# Unity XYZ; Avatar default pose, not animated\nmtllib {prefix}.mtl\n']; offset = 1
        for index, part in enumerate(parts):
            lines.append(f'o part{index}\n')
            for tag, rows in [('v',part['default_positions']),('vt',part['uv']),('vn',part['default_normals'])]:
                for row in rows: lines.append(tag+' '+' '.join(f'{v:.9g}' for v in row)+'\n')
            for mat, triangles in zip(part['materials'],part['triangles']):
                lines.append(f'usemtl {symbols[mat]}\n')
                for tri in triangles:
                    if len(tri)!=3 or any(v<0 or v>=len(part['positions']) for v in tri):
                        raise ValueError('Invalid player triangle')
                    lines.append('f '+' '.join(f'{v+offset}/{v+offset}/{v+offset}' for v in tri)+'\n')
            offset += len(part['positions'])
        (output/f'{prefix}.obj').write_text(''.join(lines))
        clip_id = 179 if sex == 'male' else 231
        clip = assets.file('sharedassets1.assets').objects[clip_id].read_typetree()
        channels = packed_channels(clip, avatar)
        channels['path_id'] = clip_id
        (output/f'{prefix}-attack6-channels.json').write_text(json.dumps(channels,separators=(',',':'))+'\n')
        rig = {'zero_muscle_probe':zero_muscle_probe(avatar), 'schema':1, 'appearance':appearance, 'sex':sex, 'pose':'Avatar.m_DefaultPose', 'animated':False,
               'selection':selection, 'avatar_id':avatar_obj.path_id, 'avatar':avatar,
               'avatar_worlds':worlds, 'parts':parts, 'materials':material_reports}
        (output/f'{prefix}-rig.json').write_text(json.dumps(rig,separators=(',',':'))+'\n')
        report = {'appearance':appearance,'sex':sex,'obj':f'{prefix}.obj','rig':f'{prefix}-rig.json','selection':selection,
                  'vertices':offset-1, 'triangles':sum(len(t) for p in parts for t in p['triangles']),
                  'parts': [{'resource':p['resource'],'mesh_id':p['mesh_id'],'bones':len(p['bone_names'])} for p in parts],
                  'bounds':[[min(p['default_positions'][i][a] for p in parts for i in range(len(p['positions']))) for a in range(3)],
                            [max(p['default_positions'][i][a] for p in parts for i in range(len(p['positions']))) for a in range(3)]]}
        reports.append(report)
    provenance = [{'path':str(assets.data/name),'sha256':digest(assets.data/name)} for name in sorted(assets.files)]
    source_paths = [database, Path(__file__).resolve(), ROOT/'Assets/AppearBuilder/CustomMetaData.cs', ROOT/'Assets/AppearBuilder/CustomCharactor.cs']
    if appearance == 'starter':
        source_paths += [ROOT/'Assets/PeWorld/SingleGameInitData.cs', ROOT/'Assets/Scripts/PeEntity/Cmpt/EquipmentCmpt.cs', ROOT/'Assets/Scripts/PeEntity/Cmpt/AvatarCmpt.cs']
    provenance += [{'path':str(path),'sha256':digest(path)} for path in [*sorted(stream_sources), *source_paths]]
    manifest = {'schema':1,'sources':provenance,'players':reports,'coordinate_convention':'Unity XYZ, Avatar default pose; no root placement or retargeted animation',
                'limitations':['Source default modular selection with optional starter equipment only; runtime body morphs and other clothing omitted.',
                               'Diffuse materials only; tint/masks/hair transparency differ from Unity shaders.',
                               'Humanoid muscle retargeting and animated knife contact are not evaluated.']}
    (output/'player-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir',type=Path,default=Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--appearance',choices=['default','starter'],default='default')
    parser.add_argument('--sex',choices=['male','female','both'],default='both')
    args=parser.parse_args(); game=args.game_dir.expanduser().resolve(); output=(args.output or ROOT/('rebuild/build/reference-player-starter' if args.appearance=='starter' else 'rebuild/build/reference-player')).expanduser().resolve()
    if output==game or game in output.parents: parser.error('Output must be outside installed game')
    try:
        output.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.player-export-',dir=output.parent) as staging:
            manifest=recover(game,Path(staging),['male','female'] if args.sex=='both' else [args.sex], args.appearance)
            output.mkdir(parents=True,exist_ok=True)
            for file in sorted(Path(staging).iterdir()): file.replace(output/file.name)
        print(json.dumps(manifest['players'],indent=2))
    except (ImportError,OSError,ValueError,KeyError,IndexError) as error:
        parser.exit(1,f'Player export failed: {error}\n')
    return 0

if __name__=='__main__':
    raise SystemExit(main())
