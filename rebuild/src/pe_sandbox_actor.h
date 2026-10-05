#ifndef PE_SANDBOX_ACTOR_H
#define PE_SANDBOX_ACTOR_H
#include "pe_game.h"
#include "pe_sandbox_actions.h"

typedef struct PeSandboxActor PeSandboxActor;
/* Requires an active graphics context. asset_root/player/recording.peas must
 * contain all six recovered locomotion clips. Optional stowed knife33/shovel12
 * and complete shovel12/safety-axe1527/wood-axe26/pickaxe19 action groups retain original rigs and
 * clocks. Supplied partial/malformed action groups fail the whole load; entirely
 * absent optional groups are allowed and action_ready then returns false.
 * Pickaxe requires all four folders: player-pickaxe, player-pickaxe-stowed,
 * player-mine and player-pickaxe-mine. Mine scenes contain exact female source
 * Mine1/2/3 clocks; no shovel pose substitution or independent edit timer.
 * Fixed recovered female starter appearance, not general avatar customization. */
PeSandboxActor *pe_sandbox_actor_load(const char *asset_root,char *error,size_t capacity);
void pe_sandbox_actor_unload(PeSandboxActor *actor);
/* Call once after movement with unpaused simulation seconds (zero freezes).
 * Horizontal gait uses actual displacement, not requested blocked velocity.
 * Teleports reset presentation instead of synthesizing locomotion. Forward-only
 * clips, discrete transitions and sprint/run fallback remain approximations.
 * This renderer never applies action damage, inventory edits or tool swings. */
void pe_sandbox_actor_update(PeSandboxActor *actor,float dt,const PeGame *game);
/* Host owns BeginMode3D; source model yaw = 180 - game yaw in degrees. */
void pe_sandbox_actor_draw(PeSandboxActor *actor,const PeGame *game);
/* Compatible with PeSandboxUIPreview. UI owns the transparent RenderTexture;
 * callback owns Begin/EndMode3D. Draws idle without advancing animation state. */
void pe_sandbox_actor_preview(void *context,const PeGame *game);
/* Draw failures are sticky and reported once to stderr; host can fail a smoke. */
bool pe_sandbox_actor_ok(const PeSandboxActor *actor);
/* True only for an exact recovered item with complete, clock-validated held,
 * action-body and action-tool scenes (and shovel/pickaxe stowed scene). Host action
 * admission must use this capability; no tool/prototype visual substitution. */
bool pe_sandbox_actor_action_ready(const PeSandboxActor *actor,int item_id);
/* Copies one authoritative gameplay snapshot; never advances its clock. NULL
 * clears presentation state. Rejects unknown clips, unsupported held items and
 * invalid times atomically. Draw also rechecks equipped instance identity so
 * unequip/replacement cannot render a stale action. Call after action steps,
 * cancellations and equipment changes, including paused/modal frames. */
bool pe_sandbox_actor_set_action(PeSandboxActor *actor,const PeSandboxActionPose *pose);

#endif
