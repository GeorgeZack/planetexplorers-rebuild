#!/usr/bin/env python3
"""Terrain fixture format and installed source-artifact recovery checks."""
import hashlib
import json
import importlib.util
from pathlib import Path
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]/'tools'
spec = importlib.util.spec_from_file_location('terrain_export', TOOLS/'export_adventure_terrain.py')
export = importlib.util.module_from_spec(spec)
spec.loader.exec_module(export)
ART = Path.home()/'.local/share/Steam/steamapps/common/Planet Explorers/RandomTownArt/town1_8.art'


class TerrainExportTests(unittest.TestCase):
    def test_crop_negative_origin_and_layout(self):
        data = export.encode_pevx({(-2,4,7):(128,45),(-1,5,8):(255,4),(-3,4,7):(17,3)}, (-2,4,7),(2,2,2))
        with tempfile.TemporaryDirectory(dir=TOOLS.parent/'build') as directory:
            path=Path(directory)/'crop.pevx'
            path.write_bytes(data)
            origin,size,voxels=export.read_pevx(path)
            self.assertEqual(origin,(-2,4,7))
            self.assertEqual(size,(2,2,2))
            self.assertEqual(voxels[:2],bytes([128,45]))
            self.assertEqual(voxels[-2:],bytes([255,4]))
            self.assertEqual(voxels[2:-2],bytes(12))
            path.write_bytes(data[:-1])
            with self.assertRaises(ValueError): export.read_pevx(path)

    def test_binary_reader_rejects_invalid_lengths(self):
        with self.assertRaises(ValueError): export.Reader(b'').integer()
        with self.assertRaises(ValueError): export.Reader(b'\xff'*5).string()
        with self.assertRaises(ValueError): export.Reader(b'\x03ab').string()

    def test_capture_install_validates_provenance_and_preserves_assets(self):
        with tempfile.TemporaryDirectory(dir=TOOLS.parent/'build') as directory:
            root=Path(directory); source=root/'source'; source.mkdir()
            (source/'save.arc').write_bytes(b'unchanged archive')
            profile=root/'profile'; captures=profile/'terrain-captures'; captures.mkdir(parents=True)
            (profile/'source-save-hashes.json').write_text(json.dumps({'save.arc':hashlib.sha256(b'unchanged archive').hexdigest()}))
            capture=captures/'starttown.pevx'; capture.write_bytes(export.encode_pevx({}))
            metadata=dict(schema=1,kind='observed-original-loaded-terrain',origin=list(export.ORIGIN),size=list(export.SIZE),
                          stableConsecutiveFrames=2,loadedChunks=30,layout='x-fastest,y,z;volume,type',frame=10,unity='5.2.4f1')
            meta=Path(str(capture)+'.json'); meta.write_text(json.dumps(metadata))
            output=root/'assets'/'terrain.pevx'; output.parent.mkdir()
            unrelated=output.parent/'scene.obj'; unrelated.write_bytes(b'preserve geometry')
            manifest=export.install_capture(capture,output,source)
            self.assertTrue(manifest['original_saves_unchanged'])
            self.assertEqual(output.read_bytes(),capture.read_bytes())
            self.assertEqual(unrelated.read_bytes(),b'preserve geometry')
            export.install_capture(capture,output,source,True)
            metadata['sha256']='0'*64; meta.write_text(json.dumps(metadata))
            with self.assertRaisesRegex(ValueError,'hash mismatch'): export.install_capture(capture,output,source)
            del metadata['sha256']; metadata['stableConsecutiveFrames']=1; meta.write_text(json.dumps(metadata))
            with self.assertRaisesRegex(ValueError,'stability'): export.install_capture(capture,output,source)
            metadata['stableConsecutiveFrames']=2; meta.write_text(json.dumps(metadata)); (source/'save.arc').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError,'save changed'): export.install_capture(capture,output,source)

    def test_capture_baseline_excludes_autosave_slots(self):
        with tempfile.TemporaryDirectory(dir=TOOLS.parent/'build') as directory:
            root=Path(directory); source=root/'source'; auto=source/'PlanetExplorers/GameSave/auto0'; auto.mkdir(parents=True)
            manual=source/'PlanetExplorers/GameSave/save0'; manual.mkdir()
            (auto/'index.arc').write_bytes(b'autosave'); (manual/'index.arc').write_bytes(b'manual checkpoint')
            baseline={str(p.relative_to(source)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (auto/'index.arc',manual/'index.arc')}
            self.assertEqual(list(export.manual_save_baseline(baseline)),['PlanetExplorers/GameSave/save0/index.arc'])
            profile=root/'profile'; captures=profile/'terrain-captures'; captures.mkdir(parents=True)
            (profile/'source-save-hashes.json').write_text(json.dumps(baseline))
            capture=captures/'starttown.pevx'; capture.write_bytes(export.encode_pevx({}))
            metadata=dict(schema=1,kind='observed-original-loaded-terrain',origin=list(export.ORIGIN),size=list(export.SIZE),
                          stableConsecutiveFrames=2,loadedChunks=30,layout='x-fastest,y,z;volume,type',frame=10,unity='5.2.4f1')
            Path(str(capture)+'.json').write_text(json.dumps(metadata))
            output=root/'assets'/'terrain.pevx'; output.parent.mkdir()
            # A later original-game session rewrites only the autosave slot.
            (auto/'index.arc').write_bytes(b'rewritten by a later session')
            export.install_capture(capture,output,source)
            installed=output.parent/'terrain-manifest.json'; record=json.loads(installed.read_text())
            self.assertEqual((record['original_saves_verified'],record['original_autosave_entries_excluded']),(1,1))
            export.install_capture(capture,output,source,True)
            # An installation recorded before the exclusion verified the whole
            # baseline; its literal manifest remains acceptable to --check.
            legacy=dict(record,original_saves_verified=2); del legacy['original_autosave_entries_excluded']
            installed.write_text(json.dumps(legacy,indent=2)+'\n'); export.install_capture(capture,output,source,True)
            legacy['original_saves_verified']=3; installed.write_text(json.dumps(legacy,indent=2)+'\n')
            with self.assertRaisesRegex(ValueError,'differing'): export.install_capture(capture,output,source,True)
            installed.write_text(json.dumps(record,indent=2)+'\n'); (manual/'index.arc').write_bytes(b'changed checkpoint')
            with self.assertRaisesRegex(ValueError,'save changed'): export.install_capture(capture,output,source,True)

    @unittest.skipUnless(ART.is_file(),'Installed original town artifact unavailable')
    def test_installed_artifact_slot_mapping_and_volumes(self):
        metadata,voxels=export.read_art(ART,1)
        self.assertEqual(metadata['size'],[60,80,100])
        self.assertEqual(len(voxels),42350)
        self.assertEqual([m['runtime_guid'] for m in metadata['materials'] if m],[240,45])
        self.assertEqual({t for v,t in voxels.values()},{4,45})
        self.assertTrue(any(0<v<128 for v,t in voxels.values()))
        self.assertTrue(any(128<=v<255 for v,t in voxels.values()))
        self.assertEqual(export.read_art(ART,3)[1].keys(),voxels.keys())
        self.assertEqual({t for v,t in export.read_art(ART,3)[1].values()},{14,45})
        with self.assertRaisesRegex(ValueError,'Biome-relative'): export.read_art(ART)


if __name__=='__main__': unittest.main()
