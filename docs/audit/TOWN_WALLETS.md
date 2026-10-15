# Archived vendor wallets and purchase credit

October6,2026. Follow-up to the [source-first checkpoint audit](SOURCE_FIRST_REBUILD.md). The C sandbox now owns the eight archived vendor purses, credits a normal purchase atomically and applies the source observed-update refill latch. This is implemented source-derived behavior; no original or native graphical session was run. Subsequent [selling and buyback](TOWN_SELL_BUYBACK.md) now use these purses; repair remains unimplemented.

## Source and extraction

[export_town_wallets.py](../../rebuild/tools/export_town_wallets.py) decodes the existing pinned starting and Annot archives using their entity/component records. [The generated manifest](../../rebuild/generated/pe_town_wallets.json) retains identities, balances, selected daily increments, serialized maxima, latches and source/archive hashes. It validates the component layout and active C# regions, including normal-purchase NPC credit, money serialization and the configured timer. Regenerate rather than editing generated rows:

```sh
python3 rebuild/tools/export_town_wallets.py
python3 rebuild/tools/export_town_wallets.py --check
make -C rebuild town-wallet-test sandbox-wallet-host-test
```

The eight profiles are stable-order entities20002,20011,20014,20028,20031,20032,20033,20037. They are particular saved residents, not procedural wallet generation. All have archived `added=false`. Exact values and active call sites are in [the progression audit](SOURCE_FIRST_REBUILD.md).

Source `AutoIncreaseMoney.Update` observes the time of day. Before half of the26-hour/93600-second game day it clears the latch; at or after46800 seconds it adds the selected increment only if the latch is false, then sets it. The native evaluator retains the source divide/floor/multiply order for the day fraction; its double world-seconds clock remains an adapter to the original integer-tick timer. It does not count missed days. If no morning update was observed, later afternoons do not create another payment. The serialized `mMax` is provenance: the active update and digital setter do not cap the balance at that value.

## Runtime ownership and transactions

[pe_town_wallet](../../rebuild/src/pe_town_wallet.h) owns initialization, validation, balance queries and one observed refill operation. `PeGame.town_wallet` is the sole saved state; source profiles are immutable. A new Adventure receives the archived balances/latches; Legacy keeps zero wallet records. World replacement, clone/move and ordinary loads preserve this owner.

[pe_town_shop_buy](../../rebuild/src/pe_town_interaction.c) resolves the vendor by exact source entity ID. Inventory grant, player debit, stock debit and vendor credit commit together. Insufficient currency/stock/capacity or wallet overflow leaves these owners unchanged. A successful purchase does not reset the daily latch. Balances can exceed the source serialized maximum.

The native contract admits nonnegative signed32-bit balances and rejects overflow atomically, rather than reproducing unchecked signed wraparound. A refill stages all eight records before committing; one overflow cannot partially pay other vendors. This is a declared bounded failure policy. The host fails closed on an invalid/overflowing update.

The sandbox calls the wallet update once per focused, unpaused display frame, before fixed simulation catch-up, using the previously completed authoritative `game_seconds`. It is independent of player life, open shop, NPC interaction range and player recovery clocks. A modal does not independently stop the wallet update. Fixed movement calls never refill wallets. Native pause/focus scheduling, all-eight resident activity and ordering relative to UI/time updates are explicit adapters; exact original entity activation and Unity script order are not claimed.

## PECSv17 and migration

V17 retains every byte of the literal v16 payload, then appends100 bytes: count8 followed by eight12-byte little-endian records `(entity_id,balance,added)`. The entity order must match the source profiles, balances must fit0..INT32_MAX and latch values must be0 or1. All modes retain the identified records; non-Adventure balances/latches must be zero. No C padding is written. Invalid count/order, duplicate identities, malformed/truncated/trailing payload or invalid mode state is rejected before replacing the current game.

Versions1–16 retain their prior validation/migrations and then initialize this new field from the archived profiles for Adventure, or zero for Legacy. This is a bounded migration: earlier native saves contain no NPC purse or transaction ledger, so past vendor credits/refills cannot be inferred from refreshing stock. Loading an old afternoon save with the archived false latch permits one payment on the next active wallet update. V17 saves instead retain their actual balance and latch and never reinitialize them from the reference archive.

## Verification

The focused suites cover archive profile/vendor closure; exact noon and morning reset; repeated/skipped afternoons; dead-player updates; balances above `mMax`; late-vendor overflow rollback; purchase credit, insufficient funds/full bag and exact signed32 endpoints. V17 tests cover literal-prefix retention, source identities/count/latches, hostile payloads, byte-preserving failed loads/writes, old-save migration and current round trips. The actual display helper is exercised without graphics for pause/focus, death, catch-up isolation and all-wallet rollback.

[PLAYTEST](../PLAYTEST.md) records the executed build/test results and their scope. Historical save fixtures explicitly remove the new100-byte append before testing old envelopes; they are not simply relabeled as v17. No new physical purchase/refill observation, selling/buyback support, general economy simulation or hunt completion is claimed.
