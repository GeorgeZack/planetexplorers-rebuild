# Checkout relocation and source paths

October 9, 2026. Implemented in C and Python; no original-game or graphics session.

The move to a new checkout exposed three dependencies on the previous location:
archived asset manifests retained absolute source paths, the compiled game used
its build-time checkout as the default asset directory, and virtualenv console
scripts and activation stored their installation directory. The files remained
present: sampled relocated inputs matched their recorded hashes.

## Runtime

`pe_paths.h` discovers the supported Linux executable through `/proc/self/exe`.
Default assets are the `reference-assets` directory beside it; `--assets` retains
its explicit override. Settlement's unconfigured default uses the same rule.
The menu's optional source-logo fallback finds the current checkout at runtime.
No checkout-root compiler definition is used. Saved game parsing and payloads
are unchanged.

C reference fixtures find the current checkout through executable ancestors,
then working-directory ancestors, using `rebuild/Makefile` as the marker. The
test-only path owner supplies checked fixture paths; it does not alter gameplay
state or require the caller to recreate the previous checkout name.

## Provenance

Future repository-owned source labels use
`read_reference_adventure.source_label`, preserving logical `Assets/...` paths
through the read-only root symlinks. External capture/profile paths remain
explicit provenance; moving those external inputs is a separate operation.

`source_path` resolves relative labels against the current checkout, independent
of cwd. An absent historical absolute path can be relocated only by an exact
repository-subtree suffix and its recorded SHA256. Missing, substituted or
ambiguous inputs fail; external paths are never searched by basename. Original
manifests, their hashes and historical evidence are preserved, rather than
rewriting recorded labels to claim a different capture location. The starter
pair and actual pickaxe regeneration consumers use this checked resolution.

## Python environment

`setup_unity_tools.py` makes console entry points choose the interpreter beside
them. Bash/Zsh and Fish activation locate their sourced script at runtime;
Csh requires the environment directory as an explicit source argument. Direct
`build/unity-tools/bin/python` invocation needs no activation. Generated venv
creation-command metadata is removed; system-interpreter metadata remains.

`make unity-tools` repairs launchers after dependency installation. Existing
packages can be retained with:

```sh
python3 rebuild/tools/setup_unity_tools.py --repair
```

## Verification boundary

Relocation fixtures move a compiled C path probe and a Python console launcher
to fresh directories containing spaces, then run them from unrelated working
directories. Source-resolution tests preserve old records and reject changed
hashes and traversal. Actual starter-pair checks pass without old-path links;
direct pip and Bash/Zsh activation also work in the moved checkout. Full
regression, sanitizer and CLI results are recorded below. Full content and source
regeneration checks also pass.
The full regression passes 238 jobs (812 Python tests, 53 ordinary-interpreter
skips); the separate UnityPy discovery passes all 915 tests with zero skips.
All 92 ASan/UBSan/leak suites and 26 CLI checks pass. The source-logo fallback
also passes the existing headless main-menu routing checks.

The ordinary sword-motion group was regenerated through its exporter because
the proof pins the changed player's asset-exporter source hash. Its PESR1
payload is byte-identical; the only metadata change is that dependency hash.
The complete prior group is preserved locally under
`rebuild/build/reference-assets-backups/path-portability-20261009/player-sword-motion`.
No archived capture or historical evidence was rewritten. Logs are local and
ignored under `rebuild/build/path-portability-*.log`.

These checks establish path lookup and source identity, not new visual or
original-game acceptance.
