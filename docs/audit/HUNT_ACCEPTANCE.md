# Ordinary four-hare acceptance session

Prepared October6,2026 against runtime commit2009c2d0. This is the remaining live acceptance gate for the numbered-conversation/hunt goal. The conversation shortcuts are implemented and physically checked; the subsequent source-derived wallet increment is implemented and pushed. Four kills earned in ordinary native play and an actual partial-progress reload remain unverified. A passing arranged test or edited kill counter does not close this gate.

## Prepared native checkpoint

The local directory `rebuild/build/hunt-acceptance-20261006` contains two byte-identical copies of the preserved earned `hunt-qol-20261006/manual-start.pesave`: immutable baseline `start.pesave` and working `progress.pesave`. Their SHA256 is `2b7f763da6bd453da8fb8102a914983e1913c625211803f5151648120ee0ca6f`. Source/start/working hashes were checked after current-loader inspection. No HP, position, inventory or quest fields were edited, and neither game was launched.

The current loader reports hunt travel,0/4 kills, HP500/500, clothing Defence12, eight Medpacks, ten Herbal Juices and ten Revival Shots. The original v13 file receives the documented outfit/comfort/wallet migrations in memory only; inspection did not resave it. Source position is approximately(-271.50,22.50,129.45), with the accepted destination(-349,19.5,233). Quickslot1 is Knife33,2 is Medpack925 and3 is Herbal Juice916; other slots are empty. The initial medpack cooldown is about1.9 seconds and advances during ordinary play.

`session.json` records hashes/provenance and `start-inspection.json` records effective loaded state. These files and the saves remain ignored local evidence, not repository content. This prepared state is a pre-combat checkpoint, not a completed quest fixture.

## Launch only when the desktop is available

The user currently owns the desktop. The pending coordination question has no answer; no elapsed time is treated as a handoff. Either the user runs this command, or the agent runs it after the user returns desktop control. From the repository root:

```sh
PE_SANDBOX_COMBAT_TRACE=1 ./rebuild/build/planet_explorers --sandbox \
  --load rebuild/build/hunt-acceptance-20261006/progress.pesave \
  --save rebuild/build/hunt-acceptance-20261006/progress.pesave \
  >> rebuild/build/hunt-acceptance-20261006/native.log 2>&1
```

This launches the C rebuild, not Steam. It resumes the working copy on subsequent launches and preserves the separate baseline. The log appends; the opt-in trace records at most4096 changed contact/kill events per sandbox host, rather than full pose streams. It is diagnostic evidence, not a synchronized input/video recorder. Do not run the fixture generator or any staged-return save for this acceptance test.

## Short test sequence

1. Check Q for the accepted hunt and I/C for equipment and stats. Stay in third person. Draw the equipped Knife with one fresh world click, release, then attack with another. Z toggles walking; held movement now admits walking attacks. Running without Shift admits the running branch at its qualified speed. Sprint and first-person attacks are unsupported. The tutorial still uses stationary attacks.
2. Follow the Bug Hunt destination and fight the ordinary six-hare encounter. Use the actual blade contacts, stamina/recovery and consumables; do not edit health, stats, enemy count or quest progress. While alive,2 uses a Medpack and3 uses Juice. A death panel instead offers1 to use a Revival Shot or2 to load the last save. Death is useful failure evidence, not permission to invent kills.
3. After the first or second earned kill, confirm1/4 or2/4 in the UI, save with F9 outside dialogue, then pause. Preserve a separate copy of that saved file as `partial-before-load.pesave` and inspect it while the game is paused. Record its hash and the corresponding contact/death log segment.
4. Use Pause→Load game→Resume, confirm the same kill count, save again and pause. Preserve `partial-after-load.pesave` and inspect it. The current bounded implementation recreates the full-HP pinned cohort but retains earned quest kills; it does not persist individual hare injuries. Compare kill/progression fields and unrelated inventory/wear, rather than requiring byte equality after unpaused time has advanced.
5. Continue ordinary combat until the displayed total reaches4/4. Confirm mission-owned hares are removed and the objective changes to returning. Save a separate completion snapshot. Return to Naoko's archived position near P:-266,136, verify one grant each of scripts592–595 and resumable ending pages4105/4106. Ordinary reward delivery has separate prior fixture coverage, but this sequence connects it to earned kills.

Do not overwrite the baseline or claim success from the final count alone. Preserve enough UI/log/save evidence to establish ordinary kills, the partial reload, cleanup and reward ordering. If collision, movement, revival or combat prevents completion, record the first reproducible failure and fix it; do not tune stats just to pass.

## Read-only inspection

The tracked helper loads with the same state validation and migrations as the current game, prints numeric JSON and destroys its temporary state. It does not advance simulation or write saves:

```sh
make -C rebuild hunt-save-inspector
./rebuild/build/inspect_hunt_save \
  rebuild/build/hunt-acceptance-20261006/partial-before-load.pesave
```

An optional second argument selects the asset root. Inspect an immutable copied checkpoint while the writer is paused/closed; bind it to a file hash. The output says `current-loader`: migrated clothing/comfort/wallet defaults are effective native state, not proof those fields were present in an older wire payload. Missing/invalid saves return nonzero with no JSON. The inspector itself cannot prove that a count was earned by ordinary input; correlate it with the session evidence.

Preparation checks: inspector build passes; the preserved0/4 file loads with the expected full-HP inventory/quickslots and matching source/start/working hashes. Usage and missing-file paths return2/1 without creating a save. This is read-only preparation, not a graphics or combat pass. [PLAYTEST](../PLAYTEST.md) remains the authority for completed observations.
