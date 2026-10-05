# Local structural knowledge graph

The repository has a local SQLite index for navigating files, documented source relationships and Unity asset identity. It is built by [index_project.py](../rebuild/tools/index_project.py), using only Python's standard library. It does not upload code, execute indexed scripts, launch Unity or scan the installed game/user profiles.

## Build and query

From the repository root:

```sh
python3 rebuild/tools/index_project.py build
python3 rebuild/tools/index_project.py stats
python3 rebuild/tools/index_project.py find pe_training_dig
python3 rebuild/tools/index_project.py neighbors rebuild/src/pe_game.h --direction in
python3 rebuild/tools/index_project.py neighbors docs/audit/SANDBOX_ALIGNMENT.md --direction out
python3 rebuild/tools/index_project.py neighbors namespace:Pathea --direction in --limit 20
```

Output: `rebuild/build/knowledge/project.sqlite`, already covered by the rebuild's build-directory ignore rule. CLI results are JSON. `find` is a case-insensitive literal substring search over node IDs; it is not semantic/vector search. `neighbors` accepts an exact node ID or repository-relative file path and defaults to both incoming/outgoing edges, limited to 60 rows. It does not recursively traverse all dependencies. A global `--root PATH` before the subcommand selects another checkout/fixture.

`make -C rebuild index` is the equivalent build target; `make -C rebuild index-test` runs the focused tests.

The index inventories the working tree, including untracked source and docs. Rebuild after editing: it is a snapshot, not a background watcher. Text files have SHA-256 provenance and all inventoried files have size/mtime metadata. The aggregate inventory fingerprint includes those fields; it is not a content hash of every binary asset. Index generation builds and validates a temporary database beside the destination, then replaces the previous index only after success. Avoid simultaneous builds or editing source during a scan; it is not a transaction over the source filesystem.

## Relationships and limits

| Relation | Evidence | Meaning and limits |
|---|---|---|
| `contains` | Filesystem inventory | Directory membership. Symlink entries, `.git`, Python caches, virtual environments, `node_modules` and `rebuild/build` are excluded. Other ignore patterns are not automatically interpreted. |
| `includes` | C/C++/shader include directives | Quoted includes resolve relative to the source, then rebuild src/generated roots. Unresolved/system include names remain separate nodes. No preprocessing, macro includes or compiler dependency resolution. |
| `defines` | Python AST | Classes/functions, nested ownership and source lines; no C/C# definitions or call edges. |
| `imports` | Python AST | Static imports with a bounded same-directory/root lookup. Relative module bases are recognized; from-imported members, custom `sys.path`, dynamic imports and actual installed package resolution are not modeled completely. |
| `declares_namespace`, `uses_namespace` | C# line patterns | Namespace-level navigation only. Alias/static usings, symbol/type resolution, conditional compilation and comment parsing are not implemented. |
| `documents` | Inline Markdown destinations | Links to inventoried local files/directories. URL decoding and relative paths supported; anchors, reference-style links, plain/backtick paths and prose claims are not resolved. Links in code examples can also be indexed. |
| `records_source_hash` | JSON objects with `path` and `sha256` | Links a manifest to its inventoried input. Does not certify that the recorded hash equals today's input or infer which exporter generated it. |
| `metadata_for`, `declares_guid`, `identifies_asset` | Unity `.meta` files | Asset/folder metadata and GUID identity. Orphan metadata is recorded as unresolved. Duplicate GUID owners are preserved. |
| `references_guid`, `references_asset` | Text Unity/YAML GUID fields | Records GUID references; resolves asset edges only for one unambiguous owner. Unknown/duplicate identities are unresolved, never guessed. |

Only selected textual extensions up to 4 MiB are scanned. NUL-containing or non-UTF-8 files remain opaque. In this old Unity project many prefabs/scenes/assets are binary: their file nodes and `.meta` identity are available, but their serialized object references are **not** decoded. Images, audio, worlds and databases are inventoried as metadata; database tables/rows and asset-bundle contents are not indexed. Use the existing exporters/database audits for those questions.

This is a structural knowledge graph, not a semantic call graph or proof of runtime behavior. Missing edges do not prove independence. In particular, two modules sharing a header or namespace are not necessarily callers of one another, and a document linking a file does not verify its claims.

## Database layout

| Table | Columns |
|---|---|
| `nodes` | `id`, `kind`, `label`, `path`, `line`, JSON `detail` |
| `edges` | `source`, `target`, `relation`, `line` |
| `unresolved` | `source`, `relation`, `target`, `line`, `reason` |
| `metadata` | `key`, JSON `value` including schema version, timestamp, root, scan counts and limitations |

Node IDs use prefixes such as `file:`, `dir:`, `python:`, `namespace:` and `guid:`. Edge line numbers belong to the source file; 0 means no line location was recorded. Python symbol nodes retain the owning file path and declaration line. SQLite foreign keys ensure edges reference real nodes.

For a focused read-only query without any additional packages:

```sh
python3 -c 'import sqlite3; from pathlib import Path; p=Path("rebuild/build/knowledge/project.sqlite").resolve(); db=sqlite3.connect(p.as_uri()+"?mode=ro",uri=True); print(*db.execute("SELECT source,relation,line FROM edges WHERE target=? ORDER BY source",("file:rebuild/src/pe_game.h",)).fetchall(),sep="\n")'
```

Inspect unresolved entries before treating a missing relationship as meaningful. References into intentionally excluded build outputs, absent legacy paths and external sources are expected to remain unresolved.

## Validation

```sh
python3 rebuild/tests/test_project_index.py
```

The tests cover dependency/provenance edges, Python nesting/imports, C# namespace evidence, GUID ambiguity, binary boundaries, excluded paths, stale-node removal, inventory repeatability, orphan metadata and preservation of the previous index after a failed rebuild. Index creation also runs SQLite integrity and foreign-key checks. These are index tests, not game tests.

The 2026-10-05 repository run indexed 21,557 files into 34,416 nodes and 68,284 edges. Six tests passed, and live queries returned incoming dependencies of `pe_game.h`, the shovel action files and Pathea namespace declarations. The scan left 173 local includes, 34 orphan metadata owners and 3 asset references unresolved. These are recorded limitations, not silently invented edges. Counts are a dated snapshot; regenerate and use `stats` for current results.
