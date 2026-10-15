# Local structural index

`index_project.py` builds a standard-library SQLite navigation index. It uploads nothing, executes no indexed code and scans neither installed games nor user profiles.

## Build and query

```sh
python3 rebuild/tools/index_project.py build
python3 rebuild/tools/index_project.py stats
python3 rebuild/tools/index_project.py find pe_training_dig
python3 rebuild/tools/index_project.py neighbors rebuild/src/pe_game.h --direction in
```

Output: ignored `rebuild/build/knowledge/project.sqlite`; CLI results are JSON. `find` searches literal case-insensitive node-ID substrings. `neighbors` accepts exact IDs or relative paths, defaults to both directions/60 rows and does not recurse. Global `--root PATH` selects another checkout. Equivalent Make targets: `index`, `index-test`.

Rebuild after edits; this is a snapshot, not a watcher. It inventories untracked source/docs, SHA-256 for text and size/mtime for all files. Aggregate fingerprint is not a hash of every binary. Builds validate a temporary database before replacing the prior index; avoid concurrent builds or source edits during scanning.

## Relationships and limits

| Relation | Evidence and limits |
|---|---|
| `contains` | Filesystem; excludes symlinks, .git, caches, virtual environments, node_modules and rebuild/build, not every ignore rule |
| `includes` | Static C/C++/shader directives; quoted local/src/generated lookup, no preprocessing or macro resolution |
| `defines`, `imports` | Python AST/nesting and bounded imports; no dynamic sys.path/package resolution |
| `declares_namespace`, `uses_namespace` | C# line patterns only; no type/call/comment/conditional interpretation |
| `documents` | Inline local Markdown destinations; no anchors, reference links, prose claim validation |
| `records_source_hash` | JSON path/sha256 pairs; records provenance without checking current bytes or inferring exporter |
| `metadata_for`, `declares_guid`, `identifies_asset` | Unity .meta identities; retains orphan/duplicate owners |
| `references_guid`, `references_asset` | Text Unity/YAML; resolves only unique GUID owners |

Selected UTF-8, non-NUL text up to4MiB is parsed. Binary Unity data, databases, worlds and bundles remain opaque metadata; no binary dependencies or C/C# call graph. Missing edges never prove independence. Use exporters/database audits for semantic evidence.

## Database layout

`nodes(id,kind,label,path,line,detail JSON)`, `edges(source,target,relation,line)`, `unresolved(source,relation,target,line,reason)`, `metadata(key,value JSON)`. IDs use file/dir/python/namespace/guid prefixes. Edge lines belong to source;0 means unavailable. Foreign keys enforce real endpoints. Inspect unresolved entries before trusting missing relationships.

## Validation

`python3 rebuild/tests/test_project_index.py` checks structural edges, provenance, ambiguity/binary exclusions, repeatability/stale removal and failed-build preservation. Build also checks SQLite integrity/foreign keys. These are index tests, not gameplay proof; use `stats` for current counts.
