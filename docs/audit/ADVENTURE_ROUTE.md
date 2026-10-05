# Bounded original terrain route and world ownership

Implementation work, October 5, 2026. The intended route covers source origin
`(-640,0,64)`, dimensions `416×64×416`, including the recovered starting
settlement and Annot's archived target at `(-284,25,134)`. These bounds are a
native fixture choice, not original streaming behavior. Installing a complete
captured rectangle does not itself establish that the route was played in C.
Actual capture and graphical outcomes must be recorded separately.

## Read-only terrain observation

[reference_terrain_route.py](../../rebuild/tools/reference_terrain_route.py)
builds its observation DLL without launching by default. The explicitly
requested `--launch` follows the isolated original launcher. Only the desktop
operator navigates or saves the original. After loading the copied Adventure
checkpoint, creating the printed `.arm` file starts observation. The probe
calls `VFLODDataSource.readChunk` at LOD0; it never generates missing terrain,
changes a player transform, injects input, edits voxels, or saves gameplay.

Each chunk must match on two consecutive end-of-frame observations. The first
stable sample is retained after the original unloads that chunk. Uniform
chunks expand from their actual source byte pair. The create-new files under
`OUTPUT.chunks` are full32³ PEVX chunks, with X-fast volume/material pairs.
A complete PEVX rectangle is emitted only after all338 required chunks exist.
Incomplete coverage remains on disk; missing chunks are never filled with air.

The manifest labels `simultaneousSnapshot:false`: independently observed
chunks can come from different frames while the player walks. It records
per-chunk frame numbers, bounds and stability, Unity version, and once-per-second
read-only player `PeTrans.position/rotation`, camera heading, and town0/4
localized names and bounds. `MainPlayerCmpt.transform` is an entity container,
not the authoritative player world pose; an initial probe revision reporting
zero coordinates is not position evidence. Existing chunk captures from that
revision can still be evaluated separately for terrain evidence.

The probe stops after30 minutes armed,60 minutes installed, a printed `.stop`
file, or an exception. These checks occur on original Unity frames, so a
suspended process must resume to observe them. Adding the hidden transient
component and allocating/copying data affect process performance even though
gameplay is not mutated.

```sh
python3 rebuild/tools/reference_terrain_route.py
python3 rebuild/tools/reference_terrain_route.py --launch \
  --user-data /absolute/fresh-profile \
  --output /absolute/fresh-profile/terrain-captures/annot.pevx
python3 rebuild/tools/export_adventure_route.py \
  --capture /absolute/fresh-profile/terrain-captures/annot.pevx --coverage
```

[export_adventure_route.py](../../rebuild/tools/export_adventure_route.py)
requires complete coverage/stability metadata, every expected chunk file,
matching raw bytes between chunks and rectangle, and unchanged source archives
against the clone's baseline `source-save-hashes.json`. It records all chunk
hashes plus source metadata and archive-baseline hashes. It creates only
`terrain.pevx`, adjacent observation metadata, `terrain-manifest.json`, and
source-observed `town-name.txt`/`town-names.json` in
`build/reference-assets/adventure-route`. Existing differing output fails
closed; building assets in that directory are preserved. `--check` verifies
installed output. Surface diagnostics report maximum occupied elevation,
empty columns and clipped top columns; they do not infer terrain above the
captured height or claim a navigable route.

## Runtime storage and geometry

[pe_world](../../rebuild/src/pe_world.h) retains inline storage for legacy
64×32×64/128 worlds. Other supported dimensions own a heap buffer, bounded by
512×128×512 samples. All samples retain original one-meter spacing and raw
volume/material bytes. Width, height and depth accessors govern general
simulation, action targeting, mesh extraction, collision, placement and save
validation. Tutorial mound and old `.voxelform` imports intentionally retain
their fixed dimensions.

Owning worlds/games must use clone/move/destroy APIs. Destinations must be
zeroed or initialized. Initialization accepts fresh storage; destroy an
existing owning object before initializing it again. Import, save-load,
collision and tutorial edit transactions deep-copy or move as appropriate;
a failed transaction leaves its destination intact. Saved state holds no GPU
objects or collision snapshot pointers.

Marching-cubes cells partition into regions of at most32³, with adjacent
sample/gradient dependencies preserved. Collision snapshots rebuild changed
regions, and retain immutable building geometry by borrowing the appropriate
settlement scene. For heap worlds, an unchanged source-buffer identity,
revision, dimensions and provenance skip voxel scanning. A new world with the
same revision still invalidates by buffer identity. Gameplay edits use
`pe_world_set`/`pe_world_dig`; bulk buffer writers must increment revision after
later edits. This is bounded cached geometry, not streaming or generator parity.

## PECS version8

Versions1–7 retain their prior decoding and migration rules. Version8 preserves
the entire v7 prefix. For an inline world its v7 depth and extraZ samples are
unchanged. For a heap world the v7 prefix contains a bounded64×32×64 local
sample crop and v7 depth64. The v8 footer then writes:

1. Width, height, depth as little-endian uint32 values.
2. For dimensions other than64×32×64/128, the complete X-fast heap sample
   volume/material pairs. Inline worlds have no duplicate payload.
3. Annot completion as uint32 boolean.
4. Seventeen town-stock records, each initialized uint32 boolean, remaining
   uint32 count and IEEE754 float64 creation time.

The full geometry is validated before replacing a running game. Dimensions,
lengths, checksums, booleans, town state and collision-valid player placement
must all pass. Pre-v8 games infer incomplete Annot and uninitialized stocks.
No original Unity archive format is claimed.

## Verification boundaries

The optimized C suite passed with new96×64×160 coverage for independent deep
copies, upper dimensions, rendered/ray/capsule agreement, changed-region
rebuilds, failed import/load atomicity and save round trips. The maximum
512×128×512 allocation/access boundary is exercised. Existing legacy save
migrations and original default-world hash remain covered. Four Python route
exporter/evidence tests cover negative chunk coordinates, complete byte matching,
missing/invalid stability metadata, altered assembly bytes, top clipping and
bounded walking evidence without inferred arrival.
Offline compilation against the installed Unity5.2 assemblies passed. These
checks do not replace actual original capture or graphical travel verification.

## Completed original terrain capture

The operator's fresh isolated `planet-explorers-route-reference` profile
produced `terrain-captures/annot3.pevx` with all338 stable chunk observations.
The installed22,151,200-byte PEVX SHA256 is
`4cd02038bb4127ba37639e8ddb7e2df42a29d0204d113671722aaaf12de920e1`.
Exporter validation and `--check` both passed; all30 original source-save
hashes still matched the baseline. Every column has occupied terrain; highest
occupied samples range from sourceY12 through29. No topY63 column is solid,
so the route terrain does not require an invented upper extension.

The runtime town-name observation resolves Town0 to **Haleiwa**
(nameId8500180), centered(-593,377), boundsX[-620,-566]/Z[327,427], height30.
Town4 is **Annot** (nameId8500008), centered(-284,134),
boundsX[-331,-237]/Z[87,181], height25. Those actual bounds prompted expansion
from the earlier proposed384×64×384 rectangle. Both towns fit the installed
416×64×416 crop.

[Bounded walking samples](../reference/annot-route-original-walk.json) retain45
original `PeTrans` positions through the capture survey, with source-file hashes
and timestamps. Two raw rows atX−640.256 were outside the native crop/player
bounds and are explicitly excluded by source index. The sampled polyline is
1154.5m; this is not an exact intervening trajectory. The terrain probe completed
near(-366.115,15.7905,123.667), before Annot. That file explicitly does **not**
claim Annot arrival; subsequent original screenshots/archive observations and
C travel checks must be assessed separately.

## Native travel proof on captured geometry

With the twelve recovered live building models and4,343 static collision
triangles installed, [the headless movement fixture](../../rebuild/tests/adventure_route_fixture.c)
passed49 waypoints through the actual route. It used the45 bounded original
samples, the operator's three subsequent rounded HUD detourXZ points around a
container, and the final source archive player pose
`(-287.3446044921875,21.51923370361328,145.36329650878906)` from cloned manual
slot3 (`save2`). The HUD points supply navigationXZ only; no screenshot-derived
playerY or hidden precision is asserted.

The C game began at its archived settlement spawn and advanced15,527 normal
fixed movement ticks, including ordinary jump input when briefly obstructed.
It traveled1,273.706m without a post-spawn relocation, terrain edit or capsule
penetration. Midroute PECS save/reload retained terrain, position, velocity,
game clock, stamina and inventory. The resulting source pose
`(-286.706299,21.729929,146.399338)` was within the1.25m navigation tolerance of
the archived endpoint and satisfied the strict source15m AnnotXZ condition.
[The result](../reference/annot-route-rebuild-headless.json) records all49
waypoints, original archive and terrain/building hashes, command and limits.
Fixture setup acknowledged welcome/Arrival/supplies through public C APIs;
this is not a physical dialogue or original quest-UI test.

```sh
make -C rebuild build/adventure_route_fixture
./rebuild/build/adventure_route_fixture \
  rebuild/build/annot-route-native.xyz \
  rebuild/build/annot-route-midpoint.pesave \
  rebuild/build/annot-route-headless-result.json \
  --arrival-save rebuild/build/annot-route-arrived.pesave \
  --pre-annot-save rebuild/build/annot-route-prearrival.pesave \
  --pre-annot-waypoint 46
```

The headless result establishes a traversable native route over the captured
terrain with the recovered building collision and persistence. Graphical input,
source camera/animation parity, vegetation, survival and full NPC behavior are
separate acceptance areas. The final optimized core suite and full
ASan/UBSan/leak suite passed with the actual route assets installed, including
extended dimensions, ownership, settlement save/reload and all25 recovered NPC
placements. Logs are retained locally in
`rebuild/build/annot-route-core-final.log` and
`rebuild/build/annot-route-sanitize-final.log`. The earlier implementation run
had explicitly skipped route population because those assets were absent.

The fixture also preserves an arrived save and a pre-Annot save at source
`(-305.195190,21.501040,140.815475)`, reached by the same ordinary movement.
The pre-Annot save retains incomplete9032, accepted supplies and normal500
currency; only its camera heading is aimed at the next observed waypoint
`(-280,156)` for physical continuation. The arrived save was loaded again and
retained completed Annot, supplies,500currency, position/velocity, inventory and
game clock. Checkpoint hashes are recorded in the native result.

## Final graphical checkpoint

Original Annot arrival and the Haleiwa-bound Naoko introduction were observed. Physical native shop purchase/window/reload and Annot arrival/dialogue checks passed with explicit fixture and process boundaries in [PLAYTEST](../PLAYTEST.md) and [graphical evidence](../reference/town-route-graphical.json). Medicine9072 and Annot vendor catalogs remain unavailable.
