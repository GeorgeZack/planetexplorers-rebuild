#!/usr/bin/env python3
"""Exercise the reference launcher without Steam, Unity, or a display server."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from xml.dom import minidom

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/launch_reference.py'
spec = importlib.util.spec_from_file_location('launch_reference', SCRIPT)
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class ReferenceLauncherTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.game = self.root / 'Planet Explorers'
        self.plugins = self.game / 'PE_Client_Data/Plugins/x86_64'
        self.plugins.mkdir(parents=True)
        self.executable = self.game / 'PE_Client.x86_64'
        self.executable.write_text(
            '#!' + sys.executable + '\n'
            'import json, os, pathlib, sys\n'
            'pathlib.Path(os.environ["REFERENCE_TEST_RECORD"]).write_text(json.dumps({\n'
            '"argv": sys.argv[1:], "cwd": os.getcwd(),\n'
            '"environment": {k: os.environ.get(k) for k in '
            '["SteamAppId", "SteamGameId", "LD_LIBRARY_PATH", "LANG", "XDG_CONFIG_HOME"]}}))\n'
            'sys.exit(7)\n')
        self.executable.chmod(0o755)
        self.profile = self.root / 'profile'
        self.record = self.root / 'record.json'
        self.environment = os.environ.copy()
        self.config_home = self.root / 'config'
        self.prefs_relative = Path('unity3d/Pathea Games/Planet Explorers/prefs')
        self.prefs = self.profile / 'engine-config' / self.prefs_relative
        self.environment.update(XDG_CONFIG_HOME=str(self.config_home),
                                REFERENCE_TEST_RECORD=str(self.record),
                                LD_LIBRARY_PATH='/existing/plugins', HOME=str(self.root))

    def run_launcher(self, *arguments, profile=True):
        command = [sys.executable, str(SCRIPT), '--game-dir', str(self.game)]
        if profile:
            command += ['--user-data', str(self.profile)]
        return subprocess.run(command + list(arguments), env=self.environment,
                              capture_output=True, text=True, cwd=self.root)

    def test_rendering_diagnostic_preserves_game_argument_parser_and_is_opt_in(self):
        result = self.run_launcher('--single-threaded-rendering')
        self.assertEqual(result.returncode, 7, result.stderr)
        record = json.loads(self.record.read_text())
        arguments = record['argv']
        self.assertEqual(arguments[0], '-force-gfx-direct')
        self.assertEqual(arguments[-2:], ['-screen-fullscreen', '0'])
        # The original parser reads the following token for every dash option.
        for index, value in enumerate(arguments):
            if value.startswith('-'):
                self.assertLess(index + 1, len(arguments))
        result = self.run_launcher()
        self.assertEqual(result.returncode, 7, result.stderr)
        self.assertNotIn('-force-gfx-direct', json.loads(self.record.read_text())['argv'])

    def test_prepare_creates_minimal_settings_and_is_idempotent(self):
        result = self.run_launcher('--prepare-only')
        self.assertEqual(result.returncode, 0, result.stderr)
        config = self.profile / launcher.CONFIG_RELATIVE
        root = minidom.parse(str(config)).documentElement
        self.assertEqual(root.tagName, 'SystemData')
        self.assertEqual(root.getAttribute('Version'), '0.795')
        self.assertEqual([(n.tagName, n.getAttribute('Index')) for n in root.childNodes],
                         [('QualityLevel', '6'), ('AntiAliasing', '0')])
        before = (config.read_bytes(), config.stat().st_mtime_ns)
        self.assertEqual(self.run_launcher('--prepare-only').returncode, 0)
        self.assertEqual((config.read_bytes(), config.stat().st_mtime_ns), before)
        self.assertEqual(list(config.parent.glob('*.bak')), [])
        self.assertFalse(self.record.exists())

    def test_existing_settings_preserved_with_exact_backup(self):
        config = self.profile / launcher.CONFIG_RELATIVE
        config.parent.mkdir(parents=True)
        original = (b'<?xml version="1.0"?><!--outside--><SystemData Version="custom">'
                    b'<?keep value?><QualityLevel Index="2" Other="kept"/>'
                    b'<AntiAliasing Index="1"/><Sound Volume="0.25"/><!--inside-->'
                    b'<InputSettings><Attack Key="323"/></InputSettings></SystemData>')
        config.write_bytes(original)
        config.chmod(0o640)
        result = self.run_launcher('--prepare-only')
        self.assertEqual(result.returncode, 0, result.stderr)
        backups = list(config.parent.glob('*.bak'))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), original)
        document = minidom.parse(str(config))
        self.assertEqual(document.documentElement.getAttribute('Version'), 'custom')
        self.assertEqual(document.getElementsByTagName('QualityLevel')[0].getAttribute('Other'), 'kept')
        self.assertEqual(document.getElementsByTagName('Sound')[0].getAttribute('Volume'), '0.25')
        self.assertEqual(document.getElementsByTagName('Attack')[0].getAttribute('Key'), '323')
        for content in ('<!--outside-->', '<!--inside-->', '<?keep value?>'):
            self.assertIn(content, config.read_text())
        self.assertEqual(config.stat().st_mode & 0o777, 0o640)
        self.assertEqual(self.run_launcher('--prepare-only').returncode, 0)
        self.assertEqual(list(config.parent.glob('*.bak')), backups)

    def test_launch_uses_native_executable_environment_cwd_and_valued_last_flag(self):
        result = self.run_launcher()
        self.assertEqual(result.returncode, 7, result.stderr)
        record = json.loads(self.record.read_text())
        self.assertEqual(record['cwd'], str(self.game))
        self.assertEqual(record['argv'], ['-from-launcher', '-rw', '1280', '-rh', '720',
                                         '-ql', '0', '-language', 'English', '-ocl', 'Cpu',
                                         '-userpath', str(self.profile), '-screen-fullscreen', '0'])
        self.assertEqual(record['environment'], {
            'SteamAppId': '237870', 'SteamGameId': '237870', 'LANG': 'en',
            'LD_LIBRARY_PATH': str(self.plugins) + ':/existing/plugins',
            'XDG_CONFIG_HOME': str(self.profile / 'engine-config')})
        self.assertTrue((self.profile / launcher.CONFIG_RELATIVE).is_file())

    def test_default_profile_is_durable_and_outside_repository(self):
        result = self.run_launcher('--prepare-only', profile=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        expected = self.root / '.local/share/planet-explorers-reference' / launcher.CONFIG_RELATIVE
        self.assertTrue(expected.is_file())
        self.assertFalse(self.profile.exists())

    def test_whitespace_profile_rejected_before_writing(self):
        for suffix in ('with spaces', 'with\ttab', 'with\nnewline'):
            self.profile = self.root / suffix
            result = self.run_launcher('--prepare-only')
            self.assertEqual(result.returncode, 1)
            self.assertIn('whitespace', result.stderr)
            self.assertFalse(self.profile.exists())

    def test_invalid_installation_does_not_modify_profile(self):
        self.executable.unlink()
        result = self.run_launcher('--prepare-only')
        self.assertEqual(result.returncode, 1)
        self.assertIn('Native executable', result.stderr)
        self.assertFalse(self.profile.exists())

    def test_invalid_settings_are_not_overwritten_or_launched(self):
        config = self.profile / launcher.CONFIG_RELATIVE
        config.parent.mkdir(parents=True)
        for original in (b'<SystemData', b'<Unrelated/>'):
            config.write_bytes(original)
            result = self.run_launcher()
            self.assertEqual(result.returncode, 1)
            self.assertEqual(config.read_bytes(), original)
            self.assertEqual(list(config.parent.glob('*.bak')), [])
            self.assertFalse(self.record.exists())

    def test_unity_quality_reset_preserves_other_preferences_and_is_repeatable(self):
        self.prefs.parent.mkdir(parents=True)
        original = (b'<?xml version="1.0"?><!--shared--><unity_prefs version_major="1" '
                    b'version_minor="1"><pref name="UnityGraphicsQuality" type="int">3</pref>'
                    b'<pref name="Other" type="string">keep &amp; preserve</pref>'
                    b'<!--inside--></unity_prefs>')
        self.prefs.write_bytes(original)
        result = self.run_launcher('--prepare-only')
        self.assertEqual(result.returncode, 0, result.stderr)
        backups = list(self.prefs.parent.glob('*.bak'))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), original)
        document = minidom.parse(str(self.prefs))
        prefs = {node.getAttribute('name'): node for node in document.getElementsByTagName('pref')}
        self.assertEqual(prefs['UnityGraphicsQuality'].getAttribute('type'), 'int')
        self.assertEqual(prefs['UnityGraphicsQuality'].firstChild.data, '0')
        self.assertEqual(prefs['Other'].firstChild.data, 'keep & preserve')
        self.assertIn('<!--shared-->', self.prefs.read_text())
        self.assertIn('<!--inside-->', self.prefs.read_text())
        before = (self.prefs.read_bytes(), self.prefs.stat().st_mtime_ns)
        self.assertEqual(self.run_launcher('--prepare-only').returncode, 0)
        self.assertEqual((self.prefs.read_bytes(), self.prefs.stat().st_mtime_ns), before)
        self.assertEqual(list(self.prefs.parent.glob('*.bak')), backups)
        # Model a successful Unity shutdown persisting quality 3 again.
        self.prefs.write_bytes(original)
        self.assertEqual(self.run_launcher().returncode, 7)
        self.assertEqual(len(list(self.prefs.parent.glob('*.bak'))), 2)
        self.assertIn('>0</pref>', self.prefs.read_text())

    def test_missing_unity_preference_created_inside_profile_without_inherited_xdg(self):
        self.environment.pop('XDG_CONFIG_HOME')
        result = self.run_launcher('--prepare-only')
        self.assertEqual(result.returncode, 0, result.stderr)
        root = minidom.parse(str(self.prefs)).documentElement
        self.assertEqual(root.tagName, 'unity_prefs')
        self.assertEqual(root.getAttribute('version_major'), '1')
        node = root.getElementsByTagName('pref')[0]
        self.assertEqual(node.getAttribute('name'), 'UnityGraphicsQuality')
        self.assertEqual(node.getAttribute('type'), 'int')
        self.assertEqual(node.firstChild.data, '0')
        self.assertFalse((self.root / '.config').exists())

    def test_shared_preferences_remain_untouched_even_when_malformed(self):
        shared_paths = [self.config_home / self.prefs_relative,
                        self.root / '.config' / self.prefs_relative]
        original = b'malformed shared preferences must never be read or changed'
        for prefs in shared_paths:
            prefs.parent.mkdir(parents=True)
            prefs.write_bytes(original)
        before = [(prefs.read_bytes(), prefs.stat().st_mtime_ns) for prefs in shared_paths]
        result = self.run_launcher()
        self.assertEqual(result.returncode, 7, result.stderr)
        for prefs, expected in zip(shared_paths, before):
            self.assertEqual((prefs.read_bytes(), prefs.stat().st_mtime_ns), expected)
            self.assertEqual(list(prefs.parent.iterdir()), [prefs])
        self.assertTrue(self.prefs.is_file())
        record = json.loads(self.record.read_text())
        self.assertEqual(record['environment']['XDG_CONFIG_HOME'],
                         str(self.profile / 'engine-config'))


    def test_invalid_unity_preferences_prevent_launch_without_overwrite(self):
        self.prefs.parent.mkdir(parents=True)
        for original in (b'<unity_prefs', b'<Unrelated/>',
                         b'<unity_prefs><pref name="UnityGraphicsQuality"><bad/></pref></unity_prefs>'):
            self.prefs.write_bytes(original)
            result = self.run_launcher()
            self.assertEqual(result.returncode, 1)
            self.assertEqual(self.prefs.read_bytes(), original)
            self.assertEqual(list(self.prefs.parent.glob('*.bak')), [])
            self.assertFalse(self.record.exists())


    def test_failed_atomic_replace_preserves_original_and_backup(self):
        config = self.profile / launcher.CONFIG_RELATIVE
        config.parent.mkdir(parents=True)
        original = b'<SystemData><AntiAliasing Index="1"/></SystemData>'
        config.write_bytes(original)
        with patch.object(launcher.os, 'replace', side_effect=OSError('simulated write failure')):
            with self.assertRaises(OSError):
                launcher.prepare_settings(self.profile)
        self.assertEqual(config.read_bytes(), original)
        backups = list(config.parent.glob('*.bak'))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), original)
        self.assertEqual(len(list(config.parent.iterdir())), 2)


if __name__ == '__main__':
    unittest.main()
