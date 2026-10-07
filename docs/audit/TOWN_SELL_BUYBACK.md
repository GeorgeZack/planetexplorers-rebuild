# Vendor selling and buyback

October 6, 2026. Source-derived implementation alongside the [wallet owner](TOWN_WALLETS.md); no original or native graphics session was run for this increment. See [source extraction](TOWN_SELL_BUYBACK_SOURCE.md) for original call sites, component selection and generated metadata.

## Runtime contract

`pe_town_trade` owns atomic bag-to-vendor sales and repurchases for the eight archived vendors. Quotes use the Adventure prototype currency and source single-precision condition calculation, truncated per unit before multiplying quantity. Sale debits the vendor and credits the player. Buyback debits the player's condition-adjusted price but credits the vendor's full prototype value, retaining the asymmetry in the active original code. Normal catalog purchases retain their separate offer prices and stock owner.

Unstackable items transfer their existing instance ID and durability. Stack sales remove the selected bag slot's requested quantity, creating a separate buyback entry; repeated sales do not silently merge repurchase rows. Stack buyback uses normal bag merging. Exact-instance shortcuts clear when their item is sold; type shortcuts survive depletion. Equipped primary or shield instances require unequipping before sale. Clothing and deployed objects are not bag sale candidates.

Both purses, all affected inventory slots and the repurchase owner commit together. Insufficient currency, invalid quantities, capacity failures, ownership aliases or signed32 currency overflow cannot partially transfer an item or currency. Unsupported condition-dependent items fail closed; native storage does not yet represent the source LifeLimit health needed to quote some towers.

## Session and persistence

`PeGame.town_buyback` is a bounded transient owner: eight vendors with at most32 entries each. Full lists reject another sale rather than discarding an earlier item. Closing/reopening shops, visiting another vendor, cloning/moving the active game and world replacement retain these entries. A successful load or a new game clears them. Failed loads preserve the active session. Saving alone does not clear live buyback entries.

Source `m_SellInfo` and the shop's repurchase list establish per-vendor, in-memory close/reopen behavior; no buyback serialization owner was found. The explicit successful-load/new-game boundary and32-row limit are native adapters. Existing PECSv17 bytes remain unchanged: player bag/money and vendor purses persist, while sold objects do not reappear after load. Save validation rejects malformed transient ownership before writing. No recovered assets or user saves are altered.

## Shop controls and acceptance

The existing draggable shop gains Buy, Sell and Buyback tabs, eight scrolling rows, quantity controls and an action button. Player and vendor balances are visible. Input remains modal through closing-button release. The buyback panel explains that loading or starting a new game clears its list.

Optimized build/full tests, content and23 CLI checks pass; full core and focused UI ASan/UBSan with leak detection pass. New price-exporter tests pass11/11 with no skips; the broader Python run retains51 optional skips, which are not passes. Exact commands and scope are recorded in [PLAYTEST](../PLAYTEST.md). Graphical layout, ordinary vendor interaction and source/native user acceptance remain pending; no live hunt completion is implied by these tests.

When manual testing is available, use a disposable checkpoint for a short native check:

1. Open a vendor and verify all three tabs and both currency labels at the usual window size, then at a smaller size. Drag the panel and scroll long Buy/Sell lists.
2. Try selling selected equipment; verify the unequip message. Unequip a worn item, sell it, close/reopen the shop and buy it back. Check the same wear and the displayed transaction prices.
3. Sell part of a stack and buy back part of it. Verify quantities, purse changes and that conversation digits do not activate a quickslot.
4. Close the shop while holding its close-button press; no world action should occur through release.
5. Save after a sale, then reload: the sale's money/inventory changes remain, but the buyback list clears. This deliberate session boundary is shown in the panel.

These checks are still pending. They can accompany the user's next session without replaying earlier original-game quests.
