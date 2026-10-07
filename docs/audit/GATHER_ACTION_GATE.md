# General Gather animation: offline qualification gate

The existing tutorial assets do **not** supply a reusable player Gather animation. An October7 source-only audit found no `Gather`, `Herb`, `HerbDown` or `HerbUp` clip in335 recovered PEAC files under `rebuild/build/reference-assets`, including retained backups. The gathering/gathering-complete packages described in [GATHERING_ASSETS](GATHERING_ASSETS.md) contain plant/tree props. The tutorial renderer selects locomotion, melee, axe and mining poses; its herb action never selects a Gather pose.

Accordingly, this audit adds no speculative timer or runtime action. The immediate transactional herb harvesting documented in [SANDBOX_HARVESTING](SANDBOX_HARVESTING.md) remains unchanged. No original game, desktop input or graphics session was used.

## Qualified source action and installed graph

`Assets/Scripts/PeEntity/Cmpt/Motion/PEAction/Action_Gather.cs` admits a selected resource when the actor has a transform and is not aiming. `PreDoAction` retains the selected tree identity and sets the Gather mask; `DoAction` resets `ResetFullBody` and triggers `Gather`. It starts skill20110003 only after the `Gather` animation notification and only if the retained resource still has HP. `GatherEnd` sets a completion flag; the subsequent action update clears the mask. Immediate cancellation clears both resource references and the mask, sets `ResetFullBody`, and resets the Gather trigger. The general action does not use the tutorial's independent1.1s depletion/1.966s activity clocks.

The existing vegetation exporter already guards the relevant `ActionRelation` row32 and C# dependency logic. Gather immediately ends Move1, Sprint2 and Rotate3 among its listed actions; other equipment/action entries can require orderly termination before `DoAction`. The Gather mask and deferred action admission are player-action ownership, not a global NPC/simulation pause. Future integration must preserve the source relation checks instead of reusing modal `consume_world` to stop gameplay.

The read-only installed `PE_Client_Data/sharedassets1.assets` has SHA256 `d9c65f820a2186116d9eb73d815fe07d9f46924ddfd103ee9510bf254f9a3d67`. Its FemaleAnimator2 controller779 contains the following actual FullBody machine1 states:

| State index / name | Clip array index / path ID | Source clip duration | Outgoing transition |
| --- | --- | --- | --- |
|22 / `FullBody.Gather.HerbDown`|405 /267, `HerbDown`|.5333347321s|To23; exit time.5312511921, duration.4687487781|
|23 / `FullBody.Gather.Herb`|406 /273, `Herb`|.8000011444s|To24; exit time.6874997020, duration.3125002980|
|24 / `FullBody.Gather.HerbUp`|407 /280, `HerbUp`|.6333332062s|To submachine exit30009; exit time.6052631140, duration.3947369158|

All three states have speed1, offset0 and nonfixed transition durations. Those normalized transitions overlap clip evaluation. All three installed clips use humanoid bindings (`classID95`, `customType8`), not directly applicable generic Transform curves. Their `m_Events` arrays are empty. The absence of clip events does not mean the action lacks events: they are attached to the controller states.

Controller behavior1119 is bound to `FullBody.Gather.Herb`; behavior1219 is bound to `FullBody.Gather.HerbUp`. Each references external sharedassets0 MonoScript1916, confirmed as class `AnimatorEventSender` in `Assembly-CSharp.dll`. Their serialized enter events are empty, exit events are respectively `Gather` and `GatherEnd`, and timed-event arrays are empty. The records are52 and56 bytes respectively. The installed MonoBehaviour's generic type tree omits its custom fields, so reading it as that generic tree rejects trailing bytes; its full raw payload was decoded against the identified class's serialized field order.

`Assets/Scripts/Anim/AnimatorEventSender.cs` sends `m_EixtEvent` from `OnStateExit`. Therefore the general harvest trigger is **Herb state exit**, and completion is **HerbUp state exit**. Neither event can safely be mapped to tutorial1.1s, a clip's first frame, or the sum of its unblended clip lengths. A transition's exit threshold begins a blend; it is not by itself proof of the wall-clock time at which Unity delivers `OnStateExit`.

## Exact remaining gate

Before adding the visible general Gather lifecycle, recover and qualify the complete female third-person HerbDown/Herb/HerbUp pose group on the existing player rig, with matching worn/stowed attachments and player defence poses. The body and incoming defence must follow the same action clock. The current humanoid source channels and controller transitions are available offline, but no compatible Gather PEAC poses or existing qualified capture were found in the recovered packages.

An offline humanoid evaluator could remove the capture need only after its retargeted poses and transition/event scheduling are qualified against existing engine evidence. The present generic TRS exporter cannot establish that equivalence for these muscle clips. Alternatively, a future small isolated engine capture can qualify this specific group and its state-exit notifications; it does not require another broad exploratory play session. No such capture was requested or launched by this audit.

Once qualified, the transient owner should retain the selected plant, advance the verified composed action clock, commit the existing atomic harvest only on the qualified Herb-exit event, and release its player mask on HerbUp exit or cancellation. It must keep capacity failure retryable, prevent duplicate grants, and preserve the source's independent NPC clocks. Until then, adding a delayed invisible gathering lock would replace the current known adapter with another unqualified behavior.

Local inspection evidence is retained under ignored `rebuild/build/gather-action-audit`: the335-file PEAC clip scan, decoded controller779, three source clip records, and raw state behavior records. These are source/asset findings, not graphical acceptance or a passing general-Gather runtime test.
