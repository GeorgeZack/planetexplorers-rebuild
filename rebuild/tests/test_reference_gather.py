"""Launcher safeguards only; these tests never launch the original game."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import reference_gather as launcher


class GatherLauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.output = self.root / 'capture.jsonl'

    def tearDown(self):
        self.temp.cleanup()

    def test_default_build_never_launches_or_changes_profile(self):
        with mock.patch.object(launcher, 'build', return_value=(self.root/'mono', self.root/'shim', self.root/'dll')), \
             mock.patch.object(launcher.launch_reference, 'launch_command', return_value=(['game'], {})), \
             mock.patch.object(launcher.launch_reference, 'prepare_settings') as settings, \
             mock.patch.object(launcher.launch_reference, 'prepare_unity_preferences') as prefs, \
             mock.patch.object(launcher.subprocess, 'run') as run:
            self.assertEqual(launcher.main(['--user-data', str(self.root)]), 0)
            settings.assert_not_called()
            prefs.assert_not_called()
            run.assert_not_called()
            self.assertEqual(list(self.root.iterdir()), [])

    def test_profile_escape_and_existing_sidecars_fail_closed(self):
        with self.assertRaises(ValueError):
            launcher.paths(self.root, self.root.parent/'outside.jsonl')
        for suffix in ('.arm', '.stop', '.launch.json', '.exit.json'):
            sidecar = self.output.with_suffix(suffix)
            sidecar.symlink_to(self.root/'missing')
            try:
                with self.subTest(suffix=suffix), self.assertRaises(ValueError):
                    launcher.paths(self.root, self.output)
            finally:
                sidecar.unlink()

    def test_preload_refused_before_build_or_launch(self):
        with mock.patch.object(launcher.launch_reference, 'launch_command', return_value=(['game'], {'LD_PRELOAD':'existing'})), \
             mock.patch.object(launcher, 'build') as build:
            self.assertEqual(launcher.main(['--launch', '--user-data', str(self.root)]), 1)
            build.assert_not_called()

    def test_explicit_launch_binds_output_bounds_and_provenance(self):
        for name in ('mono', 'shim', 'dll'):
            (self.root/name).write_bytes(b'fixture')
        with mock.patch.object(launcher, 'build', return_value=tuple(self.root/name for name in ('mono','shim','dll'))), \
             mock.patch.object(launcher, 'digest', return_value='a'*64), \
             mock.patch.object(launcher.launch_reference, 'launch_command', return_value=(['game'], {})), \
             mock.patch.object(launcher.launch_reference, 'prepare_settings'), \
             mock.patch.object(launcher.launch_reference, 'prepare_unity_preferences'), \
             mock.patch.object(launcher.subprocess, 'run', return_value=mock.Mock(returncode=-15)) as run:
            self.assertEqual(launcher.main(['--launch','--user-data',str(self.root),'--output',str(self.output)]), 143)
            env = run.call_args.kwargs['env']
            self.assertEqual(env['PE_POSE_FRAMES'], '3600')
            self.assertEqual(env['PE_POSE_SECONDS'], '120')
            self.assertEqual(env['PE_GATHER_PROBE_SHA256'], 'a'*64)
            self.assertEqual(env['PE_POSE_OUTPUT'], str(self.output))
            manifest = json.loads(self.output.with_suffix('.launch.json').read_text())
            self.assertEqual(manifest['schema'], 'pe-gather-launch-1')
            self.assertEqual(manifest['probe_source_sha256'], 'a'*64)
            self.assertEqual(json.loads(self.output.with_suffix('.exit.json').read_text())['returncode'], -15)


if __name__ == '__main__':
    unittest.main()
