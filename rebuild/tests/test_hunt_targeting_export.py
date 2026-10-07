#!/usr/bin/env python3
"""Offline source/query qualification, mutation guards and deterministic output."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import export_hunt_targeting as e


class SourceGuards(unittest.TestCase):
    def test_active_chain(self):
        e.validate_sources(e.ROOT)

    def test_changed_source_decisions_rejected(self):
        changes = [
            (e.FILES[0], 'float minDis = 100f;', 'float minDis = 3f;'),
            (e.FILES[0], 'viewCmpts.Add(findView);', 'viewCmpts.Clear();'),
            (e.FILES[1], 'if(isCenterBone)', 'if(false)'),
            (e.FILES[2], 'if(dis < scaledRadius)', 'if(dis <= scaledRadius)'),
            (e.FILES[3], 'return m_Forces[sfIdx].EnemyConflict;', 'return true;'),
            (e.FILES[3], '.Allies.Contains (dstForceID)', '.Allies.Contains (srcForceID)'),
            (e.FILES[4], '            PeLauncher.Instance.Add(new LoadRandomStory(bNewGame));', '            PeLauncher.Instance.Add(new LoadStory(bNewGame));'),
            (e.FILES[5], 'ForceSetting/ForceSettings_Story', 'ForceSetting/ForceSettings_Adventure'),
            (e.FILES[5], 'base.Restore();\n                MisRepositoryArchiveMgr.Instance.Restore();', 'base.New();\n                MisRepositoryArchiveMgr.Instance.Restore();'),
            (e.FILES[5], 'class LoadRandomStory : LoadScenario', 'class LoadRandomStory : ModuleLoader'),
        ]
        with tempfile.TemporaryDirectory(prefix='hunt-target-source-') as directory:
            root = Path(directory)
            for path in e.FILES[:-1]:
                destination = root / path
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(e.ROOT / path, destination)
            for path, old, new in changes:
                with self.subTest(path=path, old=old):
                    destination = root / path
                    source = e.uncomment(destination.read_text(encoding='utf-8-sig', errors='replace'))
                    self.assertIn(old, source)
                    destination.write_text(source.replace(old, new, 1))
                    with self.assertRaisesRegex(ValueError, 'source contract changed|Missing source method'):
                        e.validate_sources(root)
                    shutil.copyfile(e.ROOT / path, destination)

    def test_faction_contract_and_mutations(self):
        original = ET.fromstring((e.ROOT / e.FILES[-1]).read_bytes().decode('gb18030'))
        e.qualify_forces(original)
        for tag, identity, key, value in [
            ('Player/PlayerDesc', '1', 'Force', '2'),
            ('Player/PlayerDesc', '4', 'Force', '1'),
            ('Force/ForceDesc', '1', 'Ally', '1,2'),
            ('Force/ForceDesc', '1', 'EnemyConflict', 'false'),
        ]:
            with self.subTest(key=key, identity=identity):
                xml = copy.deepcopy(original)
                node = xml.find(f'./{tag}[@ID="{identity}"]')
                if tag.startswith('Player'):
                    node.set(key, value)
                else:
                    node.find(key).text = value
                with self.assertRaisesRegex(ValueError, 'force conflict'):
                    e.qualify_forces(xml)

    def test_player_query_mutations(self):
        original = dict(m_AttackHeight=1.5, m_AttackChecRange=3.,
                        m_LockMaxAngle=115., m_AttackLayer={'m_Bits': 33554432})
        e.qualify_player_query(original)
        for key, value in [('m_AttackHeight', 1.6), ('m_AttackChecRange', 4.),
                           ('m_LockMaxAngle', 90.), ('m_AttackLayer', {'m_Bits': 1})]:
            with self.subTest(key=key):
                changed = copy.deepcopy(original)
                changed[key] = value
                with self.assertRaisesRegex(ValueError, 'Player query'):
                    e.qualify_player_query(changed)

    def test_header_tracks_report(self):
        report = json.loads((e.ROOT / 'rebuild/generated/pe_hunt_targeting_source.json').read_text())
        self.assertEqual(e.header(report), (e.ROOT / 'rebuild/generated/pe_hunt_targeting_source.h').read_text())
        self.assertEqual(report['layer'], 25)
        self.assertEqual(report['axis'], 2)
        self.assertEqual(report['center'], [0., .5, 0.])
        self.assertEqual(report['parts'][1]['name'], 'Body1')
        self.assertEqual(sum(p['center'] for p in report['parts']), 1)


@unittest.skipUnless(importlib.util.find_spec('UnityPy'), 'optional UnityPy unavailable')
class OriginalGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import UnityPy
        cls.game = Path.home() / '.local/share/Steam/steamapps/common/Planet Explorers'
        cls.bundle = cls.game / e.actor.BUNDLE
        if not cls.bundle.is_file() or not (e.actor.DEFAULT_OUTPUT / 'hare.peac').is_file():
            raise unittest.SkipTest('original hare bundle/qualified actor unavailable')
        cls.environment = UnityPy.load(str(cls.bundle))
        cls.objects = {o.path_id: (o.type.name, o.read_typetree()) for o in cls.environment.objects
                       if o.type.name in ('MonoBehaviour', 'MonoScript', 'Transform', 'GameObject',
                           'CapsuleCollider', 'SphereCollider', 'BoxCollider', 'MeshCollider',
                           'SkinnedMeshRenderer', 'Mesh')}
        cls.data = (e.actor.DEFAULT_OUTPUT / 'hare.peac').read_bytes()
        _, _, cls.binds = e.skeleton_data(cls.data)
        cls.bones = [e.pointer(p) for p in cls.objects[e.actor.RENDERER_ID][1]['m_Bones']]
        cls.player_definition = e.source_definition()

    def changed(self, change):
        objects = copy.deepcopy(self.objects)
        change(objects)
        with self.assertRaises(ValueError):
            e.qualify(objects, self.bones, self.binds)

    def test_live_query_identity(self):
        result = e.qualify(self.objects, self.bones, self.binds)
        self.assertEqual(result['collider'], e.COLLIDER)
        self.assertEqual(result['parts'][1]['center'], True)
        self.assertEqual(len(result['parts']), 15)
        self.assertAlmostEqual(result['radius'], 1.2, places=6)

    def test_query_capsule_mutations(self):
        for key, value in [('m_Enabled', False), ('m_IsTrigger', False), ('m_Direction', 1),
                           ('m_Radius', .5), ('m_Height', 2.), ('m_Radius', float('nan')),
                           ('m_Height', float('nan')), ('m_Center', dict(x=0., y=.75, z=0.))]:
            with self.subTest(key=key, value=value):
                self.changed(lambda o: o[e.COLLIDER][1].__setitem__(key, value))

    def test_layer_and_activation_mutations(self):
        game_object = e.pointer(self.objects[e.DEFENCE][1]['m_GameObject'])
        root_object = e.pointer(self.objects[e.VIEW][1]['m_GameObject'])
        for identity, key, value in [(game_object, 'm_Layer', 10),
                                    (game_object, 'm_IsActive', False),
                                    (root_object, 'm_IsActive', False),
                                    (e.DEFENCE, 'm_Enabled', False)]:
            with self.subTest(identity=identity, key=key):
                self.changed(lambda o: o[identity][1].__setitem__(key, value))

    def test_query_hierarchy_mutations(self):
        for identity in (e.TRANSFORM, e.VIEW):
            with self.subTest(identity=identity):
                self.changed(lambda o: o[identity][1]['m_LocalScale'].__setitem__('x', 2.))
        self.changed(lambda o: o[e.TRANSFORM][1]['m_Father'].__setitem__('m_PathID', 0))
        self.changed(lambda o: o[e.VIEW][1]['m_Father'].__setitem__('m_PathID', e.TRANSFORM))

    def test_additional_query_collider_rejected(self):
        self.changed(lambda o: o.__setitem__(123, copy.deepcopy(o[e.COLLIDER])))

    def test_defence_binding_and_order_mutations(self):
        script = e.pointer(self.objects[e.DEFENCE][1]['m_Script'])
        self.changed(lambda o: o[script][1].__setitem__('m_ClassName', 'PEAttackTrigger'))
        self.changed(lambda o: o[e.DEFENCE][1]['defenceParts'].reverse())
        self.changed(lambda o: o[e.DEFENCE][1]['centerBone'].__setitem__('m_PathID', 0))
        self.changed(lambda o: o[e.DEFENCE][1]['defenceParts'][1]['capsule'].__setitem__('radius', float('nan')))

    def test_regeneration_and_check_outputs(self):
        paths = [self.bundle, e.actor.DEFAULT_OUTPUT / 'hare.peac',
                 e.actor.DEFAULT_OUTPUT / 'hare-manifest.json'] + [e.ROOT / p for p in e.FILES]
        before = [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
        result = e.recover(self.game, e.actor.DEFAULT_OUTPUT)
        committed = json.loads((e.ROOT / 'rebuild/generated/pe_hunt_targeting_source.json').read_text())
        self.assertEqual(result, committed)
        with tempfile.TemporaryDirectory(prefix='hunt-target-output-') as directory:
            argv = ['export_hunt_targeting.py', '--output', directory]
            with patch.object(e, 'recover', return_value=result), patch.object(sys, 'argv', argv):
                e.main()
            with patch.object(e, 'recover', return_value=result), patch.object(sys, 'argv', argv + ['--check']):
                e.main()
                header = Path(directory) / 'pe_hunt_targeting_source.h'
                header.write_text('stale')
                with self.assertRaisesRegex(ValueError, 'Stale'):
                    e.main()
                header.unlink()
                with self.assertRaises(FileNotFoundError):
                    e.main()
        self.assertEqual(before, [hashlib.sha256(p.read_bytes()).hexdigest() for p in paths])

    def test_actor_provenance_and_inverse_bind_mutations(self):
        original = json.loads((e.actor.DEFAULT_OUTPUT / 'hare-manifest.json').read_text())
        for mutation in ('bundle_hash', 'actor_hash', 'bone_order', 'bind'):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory(prefix='hunt-target-assets-') as directory:
                assetdir = Path(directory)
                manifest = copy.deepcopy(original)
                data = self.data
                if mutation == 'bundle_hash': manifest['source_sha256'] = '0' * 64
                if mutation == 'actor_hash': manifest['files']['hare.peac']['sha256'] = '0' * 64
                if mutation == 'bone_order': manifest['joint_transform_path_ids'].reverse()
                if mutation == 'bind':
                    data = bytearray(data)
                    nv, ni = struct.unpack_from('<2I', data, 8)
                    offset = 24 + nv * 56 + ni * 4 + 12  # First inverse bind's X translation.
                    struct.pack_into('<f', data, offset, struct.unpack_from('<f', data, offset)[0] + .25)
                    manifest['files']['hare.peac']['sha256'] = hashlib.sha256(data).hexdigest()
                (assetdir / 'hare.peac').write_bytes(data)
                (assetdir / 'hare-manifest.json').write_text(json.dumps(manifest))
                with patch('UnityPy.load', return_value=self.environment), patch.object(e, 'source_definition', return_value=self.player_definition):
                    with self.assertRaisesRegex(ValueError, 'provenance|joint order|inverse binds'):
                        e.recover(self.game, assetdir)


if __name__ == '__main__':
    unittest.main()
