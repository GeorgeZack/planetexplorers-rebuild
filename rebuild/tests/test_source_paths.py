"""Moving a checkout must preserve provenance and reject substituted inputs."""
import hashlib
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import read_reference_adventure as paths


class SourcePaths(unittest.TestCase):
    def test_move_preserves_relative_and_historical_sources_without_rewriting(self):
        with tempfile.TemporaryDirectory() as directory:
            old = Path(directory)/'former checkout'
            new = Path(directory)/'new checkout'
            source = old/'rebuild/build/captured/body.peas'
            source.parent.mkdir(parents=True)
            source.write_bytes(b'exact recovered bytes')
            record = dict(path=str(source), sha256=hashlib.sha256(source.read_bytes()).hexdigest())
            old.rename(new)
            with patch.object(paths, 'ROOT', new):
                current = new/'rebuild/build/captured/body.peas'
                self.assertEqual(paths.source_label(current), 'rebuild/build/captured/body.peas')
                self.assertEqual(paths.source_path('rebuild/build/captured/body.peas'), current)
                self.assertEqual(paths.source_path(record), current)
                self.assertEqual(record['path'], str(source))
                current.write_bytes(b'substituted bytes')
                with self.assertRaises(ValueError):
                    paths.source_path(record)

    def test_relative_labels_do_not_depend_on_working_directory_or_follow_root_links(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            actual = root/'reference/unity/Assets/input'
            actual.parent.mkdir(parents=True)
            actual.write_bytes(b'source')
            (root/'Assets').symlink_to('reference/unity/Assets', target_is_directory=True)
            with patch.object(paths, 'ROOT', root):
                self.assertEqual(paths.source_label(root/'Assets/input'), 'Assets/input')
                self.assertEqual(paths.source_path('Assets/input'), root/'Assets/input')

    def test_external_sources_are_not_guessed_or_rebased_without_a_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root/'rebuild/build/body'
            source.parent.mkdir(parents=True)
            source.write_bytes(b'body')
            with patch.object(paths, 'ROOT', root):
                old = '/absent/checkouts/rebuild/build/body'
                self.assertEqual(paths.source_path(old), Path(old))
                with self.assertRaises(ValueError):
                    paths.source_path('/absent/external/body', hashlib.sha256(b'body').hexdigest())
                with self.assertRaises(ValueError):
                    paths.source_path('../escape')
                with self.assertRaises(ValueError):
                    paths.source_path(source, 'invalid hash')


if __name__ == '__main__':
    unittest.main()
