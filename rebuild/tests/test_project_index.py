#!/usr/bin/env python3
"""Structural-index boundaries and graph correctness; no game or desktop needed."""
import importlib.util
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('project_index', Path(__file__).parents[1] / 'tools/index_project.py')
index = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(index)


class ProjectIndexTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def put(self, path, data):
        p = self.root / path
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data if isinstance(data, bytes) else data.encode())
        return p

    def rows(self, query):
        with sqlite3.connect(self.root / index.OUTPUT) as db:
            return db.execute(query).fetchall()

    def test_includes_docs_provenance_and_excluded_files(self):
        self.put('rebuild/src/core.h', '// header\n')
        self.put('rebuild/src/core.c', '#include "core.h"\n#include <math.h>\n#include "absent.h"\n')
        self.put('docs/map.md', '[core](../rebuild/src/core.c)\n[broken](missing.md)\n[remote](https://example.com/)\n')
        self.put('rebuild/generated/manifest.json', json.dumps({'input': {'path': 'rebuild/src/core.h', 'sha256': 'a' * 64}}))
        self.put('rebuild/build/private.cs', 'namespace Secret {}')
        self.put('.git/config', 'private')
        self.put('rebuild/src/ignored.py', 'def bad(): pass').unlink()
        (self.root / 'rebuild/src/link.h').symlink_to('core.h')
        result = index.build(self.root)
        self.assertEqual(result['nodes']['file'], 4)
        edges = self.rows('SELECT source,target,relation,line FROM edges')
        self.assertIn(('file:rebuild/src/core.c', 'file:rebuild/src/core.h', 'includes', 1), edges)
        self.assertIn(('file:docs/map.md', 'file:rebuild/src/core.c', 'documents', 1), edges)
        self.assertIn(('file:rebuild/generated/manifest.json', 'file:rebuild/src/core.h', 'records_source_hash', 0), edges)
        self.assertEqual(result['unresolved'], {'documents': 1, 'includes': 1})

    def test_guid_resolution_ambiguity_and_binary_boundary(self):
        guid = 'abcdef0123456789abcdef0123456789'
        self.put('Assets/thing.prefab', b'\0binary guid: ' + guid.encode())
        self.put('Assets/thing.prefab.meta', 'guid: ' + guid + '\n')
        self.put('Assets/ref.mat', '%YAML 1.1\nm_Texture: {guid: ' + guid + '}\n')
        first = index.build(self.root)
        self.assertEqual(first['edges']['references_asset'], 1)
        self.assertEqual(first['metadata']['file_scan_counts']['binary_or_non_utf8'], 1)
        self.assertEqual(self.rows("SELECT source FROM edges WHERE relation='references_guid'"), [('file:Assets/ref.mat',)])
        self.put('Assets/duplicate.txt', 'other')
        self.put('Assets/duplicate.txt.meta', 'guid: ' + guid + '\n')
        second = index.build(self.root)
        self.assertNotIn('references_asset', second['edges'])
        self.assertEqual(self.rows("SELECT reason FROM unresolved WHERE relation='references_asset'"), [('ambiguous guid',)])

    def test_python_definitions_imports_and_namespace_evidence(self):
        self.put('tools/helper.py', 'def helper():\n    pass\n')
        self.put('tools/main.py', 'import helper\nimport os\nclass Outer:\n    def method(self):\n        pass\n')
        self.put('tools/broken.py', 'def :\n')
        self.put('Assets/example.cs', 'using UnityEngine;\nnamespace Pathea.Sample { }\n')
        result = index.build(self.root)
        edges = self.rows('SELECT source,target,relation,line FROM edges')
        self.assertIn(('file:tools/main.py', 'file:tools/helper.py', 'imports', 1), edges)
        self.assertIn(('python:tools/main.py:Outer:3', 'python:tools/main.py:Outer.method:4', 'defines', 4), edges)
        self.assertIn(('file:Assets/example.cs', 'namespace:Pathea.Sample', 'declares_namespace', 2), edges)
        self.assertEqual(result['unresolved']['python_parse'], 1)

    def test_rebuild_removes_stale_nodes_and_has_stable_inventory(self):
        file = self.put('docs/a.md', '# Map\n')
        first = index.build(self.root)
        second = index.build(self.root)
        self.assertEqual(first['metadata']['inventory_fingerprint'], second['metadata']['inventory_fingerprint'])
        file.unlink()
        index.build(self.root)
        self.assertEqual(self.rows("SELECT id FROM nodes WHERE kind='file'"), [])

    def test_failure_keeps_previous_database(self):
        self.put('a.py', 'x = 1\n')
        index.build(self.root)
        previous = (self.root / index.OUTPUT).read_bytes()
        with patch.object(index, 'extract_text', side_effect=RuntimeError('injected failure')):
            with self.assertRaises(RuntimeError):
                index.build(self.root)
        self.assertEqual(previous, (self.root / index.OUTPUT).read_bytes())
        self.assertEqual(list((self.root / index.OUTPUT).parent.glob('*.tmp')), [])

    def test_external_links_orphans_and_large_text_are_not_followed(self):
        self.put('Assets/orphan.prefab.meta', 'guid: ' + '1' * 32 + '\n')
        self.put('docs/a.md', '[external](../../outside.md)\n')
        self.put('large.cs', ' ' * 1025)
        with patch.object(index, 'MAX_TEXT', 1024):
            result = index.build(self.root)
        self.assertEqual(result['metadata']['file_scan_counts']['too_large'], 1)
        self.assertEqual(result['unresolved'], {'documents': 1, 'metadata_for': 1})
        self.assertEqual(self.rows('PRAGMA foreign_key_check'), [])


if __name__ == '__main__':
    unittest.main()
