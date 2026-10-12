# Wentfly, Knocked and GetUp lifecycle requests

October 12, 2026. Source-derived from checked-in C#, action relations and the
player prefab; implemented in the disconnected `pe_ragdoll_action`. No gameplay
host calls it. No game was launched, and no ragdoll or get-up pose is recovered
by this increment.

## Requests and source callers

`Motion_Beat.Start` registers all three actions. Its view-build message binds
Wentfly and Knocked to HumanPhyCtrl. The reaction selector's Wentfly **and
Knocked** outcomes both request Wentfly with the attacking call's parameter
entity ID, direction, force and hit transform name. The Knocked outcome does
not select `Action_Knocked`. Repulsed can also escalate to Wentfly, supplying
the actor's own ID and the configured apply-force bone. See
[reaction decisions](HIT_REACTION.md) and [Repulsed](REPULSED_ACTION.md).

Wentfly looks up the exact parameter entity and its ragdoll transform by name,
then gives that transform, its world position and direction × force to the
current actor's view. Preserve that lookup; do not replace it with a nearest
bone of the current actor. The C caller supplies the qualified lookup result.
The hit normal is the negated normalized force; force itself is not normalized.

## Wentfly (action/mask 7)

`DoAction` first returns if the actor has no view. Otherwise it sets the mask,
reads its required parameter, freezes model physics by the action class,
looks up the parameter entity/bone and requests ragdoll activation with
get-up readiness false. Missing entity or bone skips activation; it still
clears desired movement and cancels HumanPhyCtrl requests if that motor exists.
A second request calls inherited empty `ResetAction` and does nothing.

While the view reports an active ragdoll, `Update` waits for
`IsReadyGetUp()`. Once ready it clears the mask, releases its freeze request
and starts GetUp through `DoActionImmediately`, then returns true. If the
view exists but reports no ragdoll, it cleans up and returns true without
requesting GetUp. Readiness is not a guessed elapsed-time threshold.

A graceful `EndAction` cleans up and requests immediate GetUp. It does not
itself remove the manager registration: GetUp's relations end Wentfly.
`EndImmediately`, including model teardown, cleans up without requesting GetUp.

Three source null dereferences are explicit C result faults rather than native
crashes or invented successful cleanup: missing Wentfly parameter after the
mask is set; a found parameter entity with no biology view after freezing;
and an absent actor view in Wentfly `Update`. The last case has a nested
`!viewCmpt.IsRagdoll` even inside the null-view branch. Preserve requests
already issued before the fault; explicit immediate cancellation can release
them. Invalid numerical input instead fails atomically.

## Knocked (action/mask 8)

With a view, `DoAction` activates a ragdoll only if it is not already active,
then calls `ActivateInjured(false)`. It sets its mask, freezes model physics,
starts the automatic get-up countdown and clears/cancels motor requests.
The class initializer says 3 seconds; **EntityPlayer's serialized action says
2 seconds**, which is the supported player's actual value.

Each `Update` subtracts delta time, then checks `remaining <= 0`. On expiry it
clears its auto-get-up latch, releases freeze, clears the mask, schedules
`ActivateInjured(true, 1)` if the view exists and requests immediate GetUp.
It still returns **false**: Knocked remains registered, although its mask and
freeze are now clear. Further updates request nothing. GetUp's relation ends
the registered Knocked action immediately, calling the cleanup again.
That repeated defence-enable request is subsequently replaced by GetUp's
`ActivateInjured(false)`.

Repeated Knocked requests use inherited no-op `ResetAction`, preserving the
countdown. Its graceful end uses inherited manager `EndImmediately`, with the
same cleanup and no GetUp request. Cancel does not invent a readiness check or
clear the source's private timer/latch fields; the next fresh start replaces
them.

## GetUp (action/mask 10)

`DoAction` requests non-immediate ragdoll deactivation and disables injured
admission if a view exists, then freezes by its own action class and sets its
mask. Its explicit `ResetAction` calls `DoAction` again, so a repeated running
request reissues those calls rather than behaving like Wentfly or Knocked.

`Update` waits while the view reports an active ragdoll. When the view is gone
or inactive it clears its mask, releases freeze, and, with a view, requests
immediate deactivation followed by injured admission after 1.5 seconds. It
returns true. Immediate/graceful end and model teardown use that same cleanup.
No nominal clip-length timer determines completion.

The normal relation row requires **both Knocked and Lie masks**, and forbids
Death, Fall, GetOnTrain and Ride. But Wentfly and Knocked invoke
`DoActionImmediately`, which bypasses normal dependency checks. GetUp's row
immediately ends actions 7 and 8. All three rows pause action 40 (HoldFlashLight);
manager removal resumes it. The large source ending lists and all positive/
negative mask dependencies are exported unchanged.

## View, physics and callback boundary

`BiologyViewCmpt.IsRagdoll` reads the inherited controller's `active` property;
it does not call the custom `PERagdollController.IsRagdoll()` flag accessor.
The installed AnimFollow base lifecycle remains unqualified.

`PERagdollController.Activate` sets `shotByBullet`, enables `animFollow`, stores
get-up readiness and starts AddForce. With valid hit info, that coroutine waits
for the next FixedUpdate; if the hit transform has a Rigidbody, it calls
`AddForceAtPosition(hitForce * 100, hitPoint)` with default **Force** mode.
This is not a VelocityChange impulse. The C helper returns the scaled force
only; the caller owns the deferred wake, Rigidbody and physics step.

`IsReadyGetUp` is `falling && rootRigidbody.velocity.magnitude < settledSpeed`.
The C helper preserves strict `<` with caller-supplied actual speed and
inherited threshold. No threshold is inferred from an image or another timer.

`Deactivate(false)` only sets get-up readiness. `Deactivate(true)` clears
falling/gettingUp, disables `animFollow` and calls OnGetupFinished. The wrapper's
OnFallBegin and OnGetupFinished set/clear its custom ragdoll flag and forward
view messages. BiologyViewCmpt forwards those messages and toggles monster
colliders; the Motion_Beat branch that would end actions on
`View_Ragdoll_Getup_Finished` is commented out. This does not supply an active
action-completion callback to port.

`ActivateInjured` controls the defence trigger's `active` admission, not HP.
It stores a target value, cancels the older delayed Invoke and applies or
schedules the replacement. The C output reports these ordered requests; it
does not invent an Invoke scheduler or enable live invulnerability.

`FreezePhyState` is a set of action **classes**, not a counter. Overall model
freeze is any class request OR system freeze OR network freeze. Ragdoll physics
is instead enabled whenever system freeze is false: an action or network
freeze alone does not disable it. The pure C reduction uses three native bits
for these class identities and retains caller-supplied other class requests,
system and network flags. Caller still owns the real manager/view.

## Implemented in C and verification

[pe_ragdoll_action](../../rebuild/src/pe_ragdoll_action.h) reports operations in
source order, source faults, manager completion and resume requests. It has
one disconnected lifecycle per action, a strict readiness helper, deferred
force scaling and class-keyed freeze reduction. State is transient and unsaved.
No host, damage, quest or PECSv22 save change is made.

[export_ragdoll_actions.py](../../rebuild/tools/export_ragdoll_actions.py)
guards action classes, base enums and relation loader, manager dispatch/removal,
view wrappers, ragdoll readiness/deferred force and callback methods, exact
rows 7/8/10 and the player action defaults. Generated files retain source
hashes and logical paths. Regenerate them; do not edit them.

The core tests cover exact operation order, all three fresh/repeat paths,
graceful versus immediate end, view/motor/entity/bone absence, partial source
faults, exact two-second expiry and overshoot, Knocked's retained registration,
GetUp dependency bypass and waiting, class freeze overlap, external freezes,
readiness boundaries and atomic numerical failure. Six exporter cases include
eight source mutations, five relation mutations, prefab mutations and
deterministic regeneration. Full outcomes are in [PLAYTEST](../PLAYTEST.md).

Still provisional: installed CIL equivalence for these source methods, base
AnimFollow state/settling/trigger selection, ragdoll bodies/joints/collisions,
coroutine/Invoke scheduling, Mono arithmetic and every reaction/get-up pose.
All live reaction and shield gates remain closed. Next useful offline work is
static recovery of the installed AnimFollow `RagdollControl_AF` and
`AnimFollow_AF` lifecycle, followed by a bounded solver/pose qualification.
The prepared reaction bake is a separate engine need, not replaced by these
headless tests.
