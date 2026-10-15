# Gather entry pose: layer-weight diagnostic

October 15, 2026, following capture checkpoint `730770f`. This is an offline investigation of the four first stable HerbDown frames in [the live capture](GATHER_LIVE_CAPTURE.md). It changes no runtime code, installs no assets and launches no game. The original immediate harvest adapter remains unchanged.

## Existing source contract

The [reaction presentation exporter](../../rebuild/tools/export_reaction_presentation.py) already binds the installed female controller's `FullBody.DoNothing` behaviours to `AnimatorEventSender` and `AnimatorLayerWeightCtr` with `fadeoutSpeed=5`. Its [generated contract](../../rebuild/generated/pe_reaction_presentation_source.json) records the source-file, installed-asset and method-IL hashes. This investigation reuses that owner rather than introducing another extraction path.

`Assets/Scripts/Anim/AnimatorLayerWeightCtr.cs` fades the layer toward zero on `OnStateUpdate`, using `Mathf.Lerp(current, 0, fadeoutSpeed * Time.deltaTime)`. Its `OnStateExit` sets the layer weight to one. These setter values and callback names are **source-derived**. The order in which Unity applies those values to a composed pose remains an engine question; the source file alone does not answer it.

## Numerical diagnostic

The original capture is **observed** data. At frames 9728, 10102, 10260 and 10414, `FullBody` reports a stable HerbDown state and weight one. The previous sample reports an entry transition and weight zero (approximately 3.72e-29 in trial 3).

The director reused the existing bake and observation analyzers to compare all 42 mapped body bones in model space. For each boundary sample, it interpolates isolated NormalIdle at the captured Base Layer clock and HerbDown at the captured FullBody clock. Each hypothesis uses `idle + weight * (HerbDown - idle)` independently for every bone position. The first two hypotheses use recorded weights without fitting. A third least-squares fit is diagnostic only and supplies no accepted engine or native value.

[The full numerical evidence](../reference/gather-20261015/entry-pose-diagnostic.json) retains all bone residuals and input hashes. The ignored diagnostic script and original report are `rebuild/build/gather-probe/diagnose-entry-pose.py` and `entry-pose-diagnostic.json`; the committed report binds the script hash as well as its data inputs.

| Frame | Current reported weight: max / RMS error | Previous reported weight: max / RMS error | Fitted diagnostic weight |
|---|---|---|---:|
| 9728 | 0.204877 / 0.130070m | 0.053066 / 0.009003m | 0.018007 |
| 10102 | 0.202453 / 0.124506m | 0.040481 / 0.009253m | 0.023150 |
| 10260 | 0.185621 / 0.112498m | 0.037251 / 0.007830m | 0.023006 |
| 10414 | 0.197467 / 0.121111m | 0.039260 / 0.009070m | 0.017596 |

In every trial, the largest residual under the previous-weight hypothesis is `Bip01 L Calf`. The rest of the skeleton is retained in the numerical report; no bone or boundary frame was discarded.

## Interpretation and remaining work

The measurements support a **provisional** explanation: the state and weight reported after entry completion can advance before that restored weight appears in the visible pose. The recorded pose is much closer to the base idle pose on these four frames. This does not prove that all layer setters have a one-frame delay, that shifting every clip clock is correct, or that a fitted weight belongs in a native implementation.

Procedural IK, controller composition, source retarget initialization and interpolation remain possible contributors to the residuals. The calf errors are measured, not accepted or explained as a particular solver's output. Controller-clock replay must remain distinct from qualification of pose application, transitions and the skill commit boundary. The Gather integration gate stays closed.
