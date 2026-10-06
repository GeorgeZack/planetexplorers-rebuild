"""Synthetic exporter gates; synthetic motion never establishes source evidence."""
import copy
import json
from pathlib import Path
import struct
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import export_running_knife_motion as exporter
from test_reference_root_motion import frame_fixture


class ExportTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.game = self.root / 'game'
        self.asset = self.game / 'PE_Client_Data/sharedassets1.assets'
        self.asset.parent.mkdir(parents=True)
        self.asset.write_bytes(b'synthetic test asset')
        engine = self.asset.parent / 'Managed/UnityEngine.dll'
        engine.parent.mkdir()
        engine.write_bytes(b'synthetic engine')
        self.source = self.root / 'probe.cs'
        self.source.write_bytes(b'synthetic source')
        self.dll = self.root / 'probe.dll'
        self.dll.write_bytes(b'synthetic assembly')
        self.profile = self.root / 'profile'
        self.capture = self.profile / 'pose-captures/capture.jsonl'
        self.capture.parent.mkdir(parents=True)
        self.plan = self.root / 'plan.json'
        self.plan_data = dict(schema='pe-root-motion-plan-1', asset=str(self.asset),
            asset_sha256=exporter.reference.digest(self.asset), unity='5.2.4f1', avatar='FemaleAvatar',
            controller='FemaleAnimator2', controller_path_id=779, skill_id=20110014,
            source_state=dict(path='FullBody.Attack.RunSwordAttack', speed=1, leaf_duration=1),
            clip=dict(name='RunSwordAttack', path_id=408, length=exporter.reference.LENGTH,
                      frame_rate=30, loop=False, humanoid_bindings=127))
        self.plan.write_text(json.dumps(self.plan_data))
        self.launch = self.capture.with_suffix('.launch.json')
        self.launch_data = dict(schema='pe-root-motion-launch-1', plan=self.plan_data,
            plan_sha256=exporter.reference.digest(self.plan), capture=str(self.capture), profile=str(self.profile),
            command=[str(self.game / 'PE_Client.x86_64'), '-userpath', str(self.profile)],
            sampling_profile='dense', probe_source_sha256=exporter.reference.digest(self.source),
            assembly_sha256=exporter.reference.digest(self.dll), unity_engine_sha256=exporter.reference.digest(engine))
        self.launch.write_text(json.dumps(self.launch_data))
        self.write(frame_fixture((.1, .05)))
        self.patch = mock.patch.object(exporter, 'ASSET_SHA256', self.plan_data['asset_sha256'])
        self.patch.start()
    def tearDown(self):
        self.patch.stop()
        self.tmp.cleanup()
    def write(self, rows):
        self.capture.write_text(''.join(json.dumps(row) + '\n' for row in rows))
    def outputs(self):
        return exporter.outputs(self.capture, self.launch, self.plan, self.game, self.source, self.dll)
    def export(self, target, check=False):
        return exporter.export(self.capture, self.launch, self.plan, self.game, self.source, self.dll, target, check)
    def test_preserves_knots_xyz_terminal_overshoot_and_provenance(self):
        output = self.outputs()
        meta = json.loads(output['provenance.json'])
        selected = [r for r in frame_fixture((.1, .05)) if r.get('type') == 'sample' and r['trial'] == 1]
        self.assertEqual(meta['samples'][-1]['raw_sample'], selected[-1])
        self.assertEqual(meta['source_clip'], 408)
        self.assertFalse(meta['gameplay_motion_validated'])
        binary = output['running-knife.perm']
        self.assertEqual(struct.unpack('<4s5Id', binary[:32]), (b'PERM', 1, 408, 20110014, len(selected), 0, exporter.reference.LENGTH))
        self.assertEqual(struct.unpack('<4d', binary[32:64]), (0, 0, 0, 0))
        end = struct.unpack('<4d', binary[-32:])
        self.assertGreater(end[0], exporter.reference.LENGTH)
        self.assertAlmostEqual(end[3], end[0])
        self.assertEqual(len(binary), 32 + 32 * len(selected))
    def test_denser_explicit_profile_and_exported_native_samples(self):
        self.write(frame_fixture((.05, .025)))
        launch = copy.deepcopy(self.launch_data)
        launch['sampling_profile'] = 'denser'
        self.launch.write_text(json.dumps(launch))
        target = self.root / 'native'
        self.export(target)
        repo = Path(__file__).resolve().parents[1]
        runner = self.root / 'read.c'
        runner.write_text('''#include "pe_root_motion.h"
#include <math.h>
int main(int argc,char **argv){
 if(argc!=2)return 1;
 PeRootMotion *m=pe_root_motion_load(argv[1],0,0);if(!m)return 2;
 for(unsigned i=0;i<=100;i++){
  double t=pe_root_motion_duration(m)*i/100.;PeVec3 p;
  if(!pe_root_motion_sample(m,t,&p)||fabsf(p.x)>1e-6f||fabsf(p.y)>1e-6f||fabs(p.z-t)>1e-6)return 3;
 }
 PeVec3 p;if(!pe_root_motion_delta(m,.314,1.414,&p)||fabsf(p.z-1.1f)>1e-6f)return 4;
 pe_root_motion_free(m);return 0;
}
''')
        binary = self.root / 'read'
        tmp = repo / 'build/compiler-tmp'
        tmp.mkdir(parents=True, exist_ok=True)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                        '-I'+str(repo/'src'), str(runner), str(repo/'src/pe_root_motion.c'),
                        '-lm', '-o', str(binary)], check=True, env={**os.environ, 'TMPDIR': str(tmp)})
        subprocess.run([str(binary), str(target/'running-knife.perm')], check=True)
    def test_ultra_dense_explicit_profile_retains_strict_gates(self):
        rows = frame_fixture((.05, .025))
        rows[0].update(playbackSpeeds=[.025, .0125], samplingProfile='ultra-dense', trialWallLimitSeconds=180)
        for row in rows:
            if row.get('type') == 'sample':
                row['playbackSpeed'] /= 2
                row['dt'] *= 2
                row['directorTime'] *= 2
        self.write(rows)
        launch = copy.deepcopy(self.launch_data)
        launch['sampling_profile'] = 'ultra-dense'
        self.launch.write_text(json.dumps(launch))
        meta = json.loads(self.outputs()['provenance.json'])
        self.assertEqual(meta['selection']['playback_speed'], .0125)
        self.assertTrue(meta['qualification']['clone_motion_qualified'])
        self.assertTrue(meta['qualification']['all_authored_gaps_within_60hz'])
        rows[0]['trialWallLimitSeconds'] = 120
        self.write(rows)
        with self.assertRaises(ValueError):
            self.outputs()
    def test_deterministic_check_fresh_output_and_tamper(self):
        target = self.root / 'fresh'
        self.export(target)
        self.export(target, True)
        with self.assertRaises(FileExistsError):
            self.export(target)
        with (target / 'running-knife.perm').open('ab') as f:
            f.write(b'x')
        with self.assertRaises(ValueError):
            self.export(target, True)
    def test_qualified_coarse_curve_is_still_rejected(self):
        self.write(frame_fixture())
        launch = copy.deepcopy(self.launch_data)
        launch['sampling_profile'] = 'standard'
        self.launch.write_text(json.dumps(launch))
        self.assertTrue(exporter.analyzer.analyze(self.capture, True)['clone_motion_qualified'])
        with self.assertRaises(ValueError):
            self.export(self.root / 'must-not-exist')
        self.assertFalse((self.root / 'must-not-exist').exists())
    def test_unqualified_motion_and_incomplete_capture_rejected(self):
        rows = frame_fixture((.1, .05))
        for row in rows:
            if row.get('type') == 'sample' and row['trial'] == 1:
                row['position'][2] += row['playableTime']
                row['deltaPosition'][2] *= 2
                row['cumulativeDeltaPosition'][2] *= 2
        self.write(rows)
        with self.assertRaises(ValueError):
            self.outputs()
        self.write(frame_fixture((.1, .05))[:-1])
        with self.assertRaises(ValueError):
            self.outputs()
    def test_launch_hash_path_and_profile_rejections(self):
        for key, value in [('assembly_sha256', '0'*64), ('probe_source_sha256', '0'*64),
                           ('unity_engine_sha256', '0'*64), ('plan_sha256', '0'*64),
                           ('capture', '/wrong/capture'), ('profile', '/wrong/profile'),
                           ('sampling_profile', 'standard'), ('command', ['bad-executable'])]:
            launch = copy.deepcopy(self.launch_data)
            launch[key] = value
            self.launch.write_text(json.dumps(launch))
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.outputs()
    def test_changed_installed_source_or_embedded_plan_rejected(self):
        launch = copy.deepcopy(self.launch_data)
        launch['plan']['clip']['path_id'] = 315
        self.launch.write_text(json.dumps(launch))
        with self.assertRaises(ValueError):
            self.outputs()
        self.launch.write_text(json.dumps(self.launch_data))
        self.asset.write_bytes(b'changed')
        with self.assertRaises(ValueError):
            self.outputs()


if __name__ == '__main__':
    unittest.main()
