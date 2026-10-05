#!/usr/bin/env python3
"""Local structural knowledge graph. Stdlib only; never executes indexed code.

File/include/link/namespace/GUID relationships are lexical evidence, not a
semantic call graph. Binary Unity serialized objects remain opaque. Build
outputs and symlinks are excluded. See docs/KNOWLEDGE_GRAPH.md.
"""
from __future__ import annotations

import argparse
import ast
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
import tempfile
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = Path('rebuild/build/knowledge/project.sqlite')
MAX_TEXT = 4 * 1024 * 1024
EXCLUDED = {'.git', '__pycache__', 'node_modules', '.venv', 'venv'}
TEXT_SUFFIXES = {'.c', '.h', '.cpp', '.hpp', '.cs', '.py', '.md', '.json',
                 '.meta', '.shader', '.cginc', '.xml', '.txt', '.yaml', '.yml',
                 '.prefab', '.unity', '.asset', '.mat', '.controller'}
INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^>"\n]+)[>"]', re.M)
GUID = re.compile(r'\bguid:\s*([0-9a-fA-F]{32})\b')
LINK = re.compile(r'\[[^\]\n]*\]\((<[^>\n]+>|[^)\n]+)\)')
SCHEMA = """
PRAGMA foreign_keys=ON;
CREATE TABLE nodes(id TEXT PRIMARY KEY,kind TEXT NOT NULL,label TEXT NOT NULL,
 path TEXT,line INTEGER NOT NULL DEFAULT 0,detail TEXT NOT NULL DEFAULT '{}');
CREATE TABLE edges(source TEXT REFERENCES nodes(id),target TEXT REFERENCES nodes(id),
 relation TEXT NOT NULL,line INTEGER NOT NULL DEFAULT 0,
 PRIMARY KEY(source,target,relation,line));
CREATE TABLE unresolved(source TEXT REFERENCES nodes(id),relation TEXT,target TEXT,
 line INTEGER,reason TEXT);
CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);
"""


def enumerate_files(root):
    """Inventory working files, including untracked source; do not follow links."""
    for folder, dirs, files in os.walk(root, followlinks=False):
        parent = Path(folder)
        dirs[:] = sorted(d for d in dirs if d not in EXCLUDED
                         and not (parent / d).is_symlink()
                         and (parent / d).relative_to(root).as_posix() != 'rebuild/build')
        for name in sorted(files):
            path = parent / name
            if not path.is_symlink() and path.is_file():
                yield path


def file_id(path):
    return 'file:' + path


class Graph:
    def __init__(self, connection, root):
        self.db, self.root = connection, root
        self.paths = set()
        self.directories = {'.'}
        self.node('dir:.', 'directory', '.', '.')

    def node(self, identity, kind, label, path=None, line=0, detail=None):
        self.db.execute('INSERT OR IGNORE INTO nodes VALUES(?,?,?,?,?,?)',
                        (identity, kind, label, path, line, json.dumps(detail or {}, sort_keys=True)))
        return identity

    def edge(self, source, target, relation, line=0):
        self.db.execute('INSERT OR IGNORE INTO edges VALUES(?,?,?,?)',
                        (source, target, relation, line))

    def unresolved(self, source, relation, target, line, reason):
        self.db.execute('INSERT INTO unresolved VALUES(?,?,?,?,?)',
                        (source, relation, target, line, reason))

    def directory(self, path):
        name = path.as_posix()
        identity = 'dir:' + name
        if name not in self.directories:
            parent = self.directory(path.parent)
            self.directories.add(name)
            self.node(identity, 'directory', name, name)
            self.edge(parent, identity, 'contains')
        return identity

    def resolve(self, candidate):
        try:
            rel = candidate.resolve().relative_to(self.root).as_posix()
        except ValueError:
            return None
        # Inventory membership prevents following excluded or external links.
        if rel in self.paths and not candidate.is_symlink():
            return file_id(rel)
        if rel in self.directories and not candidate.is_symlink():
            return 'dir:' + rel
        return None


def line_at(text, offset):
    return text.count('\n', 0, offset) + 1


def python_structure(g, path, text):
    source = file_id(path)
    try:
        tree = ast.parse(text, filename=path)
    except SyntaxError as exc:
        g.unresolved(source, 'python_parse', str(exc), exc.lineno or 0, 'syntax error')
        return

    def definitions(parent, tree_node, scope=''):
        for child in ast.iter_child_nodes(tree_node):
            if isinstance(child, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)):
                qualified = scope + child.name
                identity = f'python:{path}:{qualified}:{child.lineno}'
                kind = 'python_class' if isinstance(child, ast.ClassDef) else 'python_function'
                g.node(identity, kind, qualified, path, child.lineno)
                g.edge(parent, identity, 'defines', child.lineno)
                definitions(identity, child, qualified + '.')
            else:
                definitions(parent, child, scope)

    definitions(source, tree)
    for item in ast.walk(tree):
        if not isinstance(item, (ast.Import, ast.ImportFrom)):
            continue
        modules = [alias.name for alias in item.names] if isinstance(item, ast.Import) else [item.module or '']
        for module in modules:
            label = '.' * getattr(item, 'level', 0) + module
            base = g.root / path
            base = base.parent
            for _ in range(max(0, getattr(item, 'level', 0) - 1)):
                base = base.parent
            suffix = Path(*module.split('.')) if module else Path('.')
            bases = [base] if getattr(item, 'level', 0) else [base, g.root]
            candidates = [b / suffix for b in bases]
            target = next((resolved for p in candidates
                           for candidate in (p.with_suffix('.py'), p / '__init__.py')
                           if (resolved := g.resolve(candidate))), None)
            if target is None:
                target = g.node('module:' + label, 'python_module', label)
            g.edge(source, target, 'imports', item.lineno)


def extract_text(g, path, text, guids):
    source = file_id(path)
    suffix = Path(path).suffix.lower()
    if suffix in {'.c', '.h', '.cpp', '.hpp', '.shader', '.cginc'}:
        for match in INCLUDE.finditer(text):
            delim, name = match.groups()
            target = None
            if delim == '"':
                for base in ((g.root / path).parent, g.root / 'rebuild/src', g.root / 'rebuild/generated'):
                    target = g.resolve(base / name)
                    if target:
                        break
            if target is None:
                target = g.node('include:' + name, 'include_name', name)
                if delim == '"':
                    g.unresolved(source, 'includes', name, line_at(text, match.start()), 'not resolved in local include roots')
            g.edge(source, target, 'includes', line_at(text, match.start()))
    if suffix == '.py':
        python_structure(g, path, text)
    if suffix == '.cs':
        for relation, pattern in (
            ('declares_namespace', r'^\s*namespace\s+([\w.]+)'),
            ('uses_namespace', r'^\s*using\s+([\w.]+)\s*;')):
            for match in re.finditer(pattern, text, re.M):
                name = match.group(1)
                target = g.node('namespace:' + name, 'csharp_namespace', name)
                g.edge(source, target, relation, line_at(text, match.start()))
    if suffix == '.md':
        # Inline local destinations only; no reference-style links or backtick paths.
        for match in LINK.finditer(text):
            raw = match.group(1)
            dest = raw[1:-1] if raw.startswith('<') else raw.split(' "', 1)[0]
            parsed = urlsplit(dest)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            candidate = (g.root / path).parent / unquote(parsed.path)
            target = g.resolve(candidate)
            line = line_at(text, match.start())
            if target:
                g.edge(source, target, 'documents', line)
            else:
                g.unresolved(source, 'documents', dest, line, 'outside inventory or missing')
    if suffix == '.json':
        try:
            content = json.loads(text)
        except ValueError:
            content = None

        def provenance(obj):
            if isinstance(obj, dict):
                if isinstance(obj.get('path'), str) and isinstance(obj.get('sha256'), str):
                    target = g.resolve(g.root / obj['path'])
                    if target:
                        g.edge(source, target, 'records_source_hash')
                for value in obj.values():
                    provenance(value)
            elif isinstance(obj, list):
                for value in obj:
                    provenance(value)
        provenance(content)
    if suffix in {'.meta', '.prefab', '.unity', '.asset', '.mat', '.controller', '.yaml', '.yml'}:
        declaration = re.search(r'^guid:\s*([0-9a-fA-F]{32})\b', text, re.M) if suffix == '.meta' else None
        for match in GUID.finditer(text):
            guid = match.group(1).lower()
            if guid == '0' * 32:
                continue
            line = line_at(text, match.start())
            if declaration and match.start() == declaration.start():
                g.edge(source, 'guid:' + guid, 'declares_guid', line)
                continue
            identity = g.node('guid:' + guid, 'unity_guid', guid)
            g.edge(source, identity, 'references_guid', line)
            targets = guids.get(guid, [])
            if len(targets) == 1:
                g.edge(source, targets[0], 'references_asset', line)
            else:
                g.unresolved(source, 'references_asset', guid, line,
                             'unknown guid' if not targets else 'ambiguous guid')


def build(root):
    root = root.resolve()
    output = root / OUTPUT
    output.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix='project-', suffix='.sqlite.tmp', dir=output.parent)
    os.close(fd)
    db = sqlite3.connect(temporary)
    try:
        db.executescript(SCHEMA)
        g = Graph(db, root)
        texts, guids, statuses = {}, {}, Counter()
        snapshot = hashlib.sha256()
        for file in enumerate_files(root):
            path = file.relative_to(root).as_posix()
            g.paths.add(path)
            stat = file.stat()
            detail = {'bytes': stat.st_size, 'mtime_ns': stat.st_mtime_ns}
            text = None
            if file.suffix.lower() in TEXT_SUFFIXES or file.name in {'Makefile', 'AGENTS.md'}:
                if stat.st_size > MAX_TEXT:
                    detail['scan'] = 'too_large'
                else:
                    data = file.read_bytes()
                    detail['sha256'] = hashlib.sha256(data).hexdigest()
                    try:
                        if b'\0' in data:
                            raise UnicodeError('binary')
                        text = data.decode('utf-8-sig')
                        detail['scan'] = 'text'
                    except UnicodeError:
                        detail['scan'] = 'binary_or_non_utf8'
            else:
                detail['scan'] = 'metadata_only'
            statuses[detail['scan']] += 1
            snapshot.update((path + '\0' + json.dumps(detail, sort_keys=True) + '\n').encode())
            g.node(file_id(path), 'file', file.name, path, detail=detail)
            g.edge(g.directory(Path(path).parent), file_id(path), 'contains')
            if text is not None:
                texts[path] = text
        # Resolve GUID declarations only after the complete file inventory exists.
        for path, text in texts.items():
            if not path.endswith('.meta'):
                continue
            owner = g.resolve(root / path[:-5])
            if owner:
                g.edge(file_id(path), owner, 'metadata_for')
            match = re.search(r'^guid:\s*([0-9a-fA-F]{32})\b', text, re.M)
            if match:
                guid = match.group(1).lower()
                identity = g.node('guid:' + guid, 'unity_guid', guid)
                if owner:
                    guids.setdefault(guid, []).append(owner)
                    g.edge(identity, owner, 'identifies_asset')
                else:
                    g.unresolved(file_id(path), 'metadata_for', path[:-5], 1, 'orphan metadata')
        for path, text in texts.items():
            extract_text(g, path, text, guids)
        metadata = {
            'schema_version': 1, 'root': str(root),
            'built_utc': datetime.now(timezone.utc).isoformat(),
            'inventory_fingerprint': snapshot.hexdigest(),
            'file_scan_counts': dict(statuses), 'max_text_bytes': MAX_TEXT,
            'limitations': 'Structural lexical relationships, not call/data-flow or semantic symbol resolution. Binary Unity assets opaque. No external game/profile scan. Working files, including untracked; rebuild/build, VCS/cache/venv directories and symlinks excluded. C# aliases/static usings and dynamic Python imports omitted. Documentation anchors are not validated. Inventory fingerprint uses text hashes and file stat metadata, not binary content hashes.',
        }
        db.executemany('INSERT INTO metadata VALUES(?,?)',
                       [(key, json.dumps(value, sort_keys=True)) for key, value in metadata.items()])
        db.executescript('CREATE INDEX edge_target ON edges(target); CREATE INDEX node_path ON nodes(path); CREATE INDEX node_label ON nodes(label);')
        db.commit()
        if db.execute('PRAGMA integrity_check').fetchone()[0] != 'ok' or db.execute('PRAGMA foreign_key_check').fetchall():
            raise ValueError('Graph integrity check failed')
        result = stats(db)
        db.close()
        os.replace(temporary, output)
        return result
    finally:
        db.close()
        Path(temporary).unlink(missing_ok=True)


def stats(db):
    return {
        'metadata': {k: json.loads(v) for k, v in db.execute('SELECT key,value FROM metadata ORDER BY key')},
        'nodes': dict(db.execute('SELECT kind,count(*) FROM nodes GROUP BY kind ORDER BY kind')),
        'edges': dict(db.execute('SELECT relation,count(*) FROM edges GROUP BY relation ORDER BY relation')),
        'unresolved': dict(db.execute('SELECT relation,count(*) FROM unresolved GROUP BY relation ORDER BY relation')),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('build')
    sub.add_parser('stats')
    find = sub.add_parser('find'); find.add_argument('query'); find.add_argument('--limit', type=int, default=30)
    neighbors = sub.add_parser('neighbors'); neighbors.add_argument('node')
    neighbors.add_argument('--direction', choices=('in', 'out', 'both'), default='both')
    neighbors.add_argument('--limit', type=int, default=60)
    args = parser.parse_args()
    if getattr(args, 'limit', 1) < 1:
        parser.error('--limit must be positive')
    if args.command == 'build':
        print(json.dumps(build(args.root), indent=2, sort_keys=True)); return
    path = args.root.resolve() / OUTPUT
    if not path.is_file():
        parser.error('No index found; run build first')
    with sqlite3.connect(path.as_uri() + '?mode=ro', uri=True) as db:
        db.row_factory = sqlite3.Row
        if args.command == 'stats':
            print(json.dumps(stats(db), indent=2, sort_keys=True))
        elif args.command == 'find':
            rows = db.execute('SELECT id,kind,path,label,line FROM nodes WHERE instr(lower(id),lower(?)) > 0 ORDER BY id LIMIT ?', (args.query, args.limit))
            print(json.dumps([dict(row) for row in rows], indent=2))
        else:
            identity = args.node
            if not db.execute('SELECT 1 FROM nodes WHERE id=?', (identity,)).fetchone():
                identity = file_id(identity)
            if not db.execute('SELECT 1 FROM nodes WHERE id=?', (identity,)).fetchone():
                parser.error('Unknown node; use find for an exact node ID or repository-relative file path')
            clause = {'in': 'target=?', 'out': 'source=?', 'both': '(source=? OR target=?)'}[args.direction]
            params = [identity] * (2 if args.direction == 'both' else 1) + [args.limit]
            rows = db.execute('SELECT source,relation,target,line FROM edges WHERE ' + clause + ' ORDER BY relation,source,target,line LIMIT ?', params)
            print(json.dumps([dict(row) for row in rows], indent=2))


if __name__ == '__main__':
    main()
