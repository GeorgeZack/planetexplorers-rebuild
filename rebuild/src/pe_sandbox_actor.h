#ifndef PE_SANDBOX_ACTOR_H
#define PE_SANDBOX_ACTOR_H
#include "pe_game.h"
#include "pe_sandbox_actions.h"
#include "pe_actor.h"
#include "pe_combat.h"

typedef struct PeSandboxActor PeSandboxActor;
/* Requires an active graphics context. asset_root/player/recording.peas must
 * contain all twelve recovered directional locomotion clips. Optional stowed knife33/shovel12
 * and complete shovel12/safety-axe1527/wood-axe26/pickaxe19 action groups retain original rigs and
 * clocks. Supplied partial/malformed action groups fail the whole load; entirely
 * absent optional groups are allowed and action_ready then returns false.
 * Pickaxe requires all four folders: player-pickaxe, player-pickaxe-stowed,
 * player-mine and player-pickaxe-mine. Mine scenes contain exact female source
 * Mine1/2/3 clocks; no shovel pose substitution or independent edit timer.
 * Optional Torch10 requires both player-torch and player-torch-stowed scenes;
 * no attack group is inferred from this held/stowed lighting capability.
 * Optional player-knife-drawn requires the stowed scene and all four matching
 * source Attack6..9 clips in BODY/held scenes. Stowed-only remains supported.
 * RunSwordAttack is optional, but if any body/held/stowed knife scene contains
 * it and Knife attachments are present, all three must contain the exact
 * female408 clock before GPU upload. The optional coherent33 Wood Sword/shield
 * group may load independently of absent Knife assets and validates its own408.
 * Fixed recovered female starter appearance, not general avatar customization. */
PeSandboxActor *pe_sandbox_actor_load(const char *asset_root,char *error,size_t capacity);
void pe_sandbox_actor_unload(PeSandboxActor *actor);
/* Call once after movement with unpaused simulation seconds (zero freezes).
 * Horizontal gait uses actual displacement, not requested blocked velocity.
 * Teleports reset presentation instead of synthesizing locomotion. Smoothed weighted
 * poses and sprint/run fallback remain explicit presentation approximations.
 * This renderer never applies action damage, inventory edits or tool swings. */
void pe_sandbox_actor_update(PeSandboxActor *actor,float dt,const PeGame *game);
/* Free-pointer movement turns the body independently of camera yaw. Reset on
 * load/init; action lock preserves the heading used by authoritative contacts. */
void pe_sandbox_actor_facing_mode(PeSandboxActor *actor,bool free_pointer);
void pe_sandbox_actor_facing_lock(PeSandboxActor *actor,bool locked);
void pe_sandbox_actor_reset_motion(PeSandboxActor *actor,const PeGame *game);
void pe_sandbox_actor_face_camera(PeSandboxActor *actor,const PeGame *game);
float pe_sandbox_actor_yaw(const PeSandboxActor *actor,const PeGame *game);
/* World body heading only; camera yaw/pitch remain owned by PeGame. */
bool pe_sandbox_actor_face_yaw(PeSandboxActor *actor,float yaw_degrees);
/* Host owns BeginMode3D; reads the same transient body yaw as contact queries. */
void pe_sandbox_actor_draw(PeSandboxActor *actor,const PeGame *game);
/* Read-only exact body selection shared with world rendering. clip is borrowed
 * until actor unload; seconds is the renderer's unwrapped sampled clock (the
 * actor sampler handles looping). Includes normal or grounded Sword locomotion,
 * item-specific Knife/Sword Attack6..9
 * and supported Spade/Felling/Mine actions with the same instance/life guards.
 * This never returns the preview pose, advances time or substitutes a clip.
 * A separate defence actor must resolve this exact name and matching duration/
 * loop policy; unavailable clips must fail closed, never fall back to idle.
 * Invalid inputs/unavailable presentation leave the output unchanged. */
typedef struct {
    const char *clip;
    double seconds;
    float duration;
    bool loop;
    float yaw;
    size_t blend_count;
    struct { const char *clip; double seconds; float duration,weight; bool loop; } blend[PE_ACTOR_BLEND_MAX];
} PeSandboxBodyPose;
bool pe_sandbox_actor_body_pose(const PeSandboxActor *actor,const PeGame *game,PeSandboxBodyPose *pose);
/* Compatible with PeSandboxUIPreview. UI owns the transparent RenderTexture;
 * callback owns Begin/EndMode3D. Draws idle without advancing animation state. */
void pe_sandbox_actor_preview(void *context,const PeGame *game);
/* Draw failures are sticky and reported once to stderr; host can fail a smoke. */
bool pe_sandbox_actor_ok(const PeSandboxActor *actor);
/* Transient diagnostic coverage: bit N means locomotion enum N was selected.
 * No effect on animation or saved gameplay state. */
unsigned pe_sandbox_actor_locomotion_seen(const PeSandboxActor *actor);
/* True only for an exact recovered item with complete, clock-validated held,
 * action-body and action-tool scenes (and shovel/pickaxe stowed scene). Host action
 * admission must use this capability; no tool/prototype visual substitution.
 * A bound passive shield disables unsupported primary tool/Knife readiness. */
bool pe_sandbox_actor_action_ready(const PeSandboxActor *actor,int item_id);
/* Knife33 requires the complete held/stowed/body melee group.
 * Torch10 has exact held/stowed rigid attachment scenes but no attack clips.
 * Holding capability must not be treated as permission to run tool/combat code. */
bool pe_sandbox_actor_hold_ready(const PeSandboxActor *actor,int item_id);
/* Presentation readiness only; host separately requires qualified root motion.
 * Existing directional assets without RunSwordAttack retain stationary melee. */
bool pe_sandbox_actor_running_ready(const PeSandboxActor *actor);
/* Current held torch's source emitter through the same sampled mount palette
 * and root transform used for its visible mesh. False leaves output unchanged;
 * inactive/stowed/dead/replaced instances never produce an emitter. */
bool pe_sandbox_actor_torch_light(const PeSandboxActor *actor,const PeGame *game,PeVec3 *position);
/* Separate source flame origin; identical instance/pose guards as the light. */
bool pe_sandbox_actor_torch_flame(const PeSandboxActor *actor,const PeGame *game,PeVec3 *position);
/* Copies one authoritative gameplay snapshot; never advances its clock. NULL
 * clears presentation state. Rejects unknown clips, unsupported held items and
 * invalid times atomically. Draw also rechecks equipped instance identity so
 * unequip/replacement cannot render a stale action. Call after action steps,
 * cancellations and equipment changes, including paused/modal frames. */
bool pe_sandbox_actor_set_action(PeSandboxActor *actor,const PeSandboxActionPose *pose);

/* Borrowed first-part actors from complete Knife33 scenes; valid until unload.
 * Matches the scene's shared skeleton/clip clocks. Host loads its corresponding
 * knife-drawn.pehb against *knife and initializes pe_sandbox_melee separately.
 * Failure leaves both outputs unchanged. No GPU/asset ownership transfer. */
bool pe_sandbox_actor_melee_assets(const PeSandboxActor *actor,const PeActor **body,const PeActor **knife);
/* Copies the authoritative female TP combat stage/clip_time; running uses
 * RunSwordAttack at stage0 and Attack7..9 thereafter, without locomotion blends.
 * Never
 * advances it. NULL combat means held locomotion or stowed, not an attack.
 * instance is the currently presented equipped Knife33 identity; zero is valid
 * only with held=false and no active combat. Invalid snapshots are atomic.
 * General set_action clears this snapshot; set_melee replaces general action.
 * A natural end (event_index5) starts a native150ms inactive pose recovery;
 * action/contact/root clocks remain untouched. Body/held/defence share weights
 * and the continuing clip at rate1. NULL combat, explicit NULL action, stow,
 * reset, death, airborne or an instance change cancels it immediately.
 * Draw rechecks equipment identity and life, and ignores attacks in preview. */
bool pe_sandbox_actor_set_melee(PeSandboxActor *actor,uint32_t instance,bool held,const PeCombat *combat);

/* Optional coherent33-clip Wood Sword1/Shield34 group; entirely absent is legal.
 * These are presentation capabilities, not permission to equip/use/block. */
bool pe_sandbox_actor_melee_assets_for_item(const PeSandboxActor *actor,int item_id,const PeActor **body,const PeActor **weapon);
bool pe_sandbox_actor_set_melee_item(PeSandboxActor *actor,int item_id,uint32_t instance,bool held,const PeCombat *combat);
bool pe_sandbox_actor_running_ready_for_item(const PeSandboxActor *actor,int item_id);
/* Ground TP Sword adapter only; false for sprint/jump/air/land contributors.
 * Host must gate unsupported movement while Sword is held. Exact source shared
 * TP turns and backward441/708 use the one normalized phase snapshot; no FP,
 * downhill755, full controller graph, or stationary-vs-running air guess.
 * Outgoing air weights also fail closed: a safe grounded presentation reset
 * is required before drawing Sword after a prior jump until air is integrated. */
bool pe_sandbox_actor_sword_ground_ready(const PeSandboxActor *actor,const PeGame *game);
/* Passive forearm only. Initial coexistence is unarmed0/Sword1; unsupported
 * primary tools/Knife report false. No block animation/protection inferred.
 * set_shield copies a transient identity; zero clears it. Draw requires the
 * same game shield selection and bag-owned item34/count1. Reset clears binding. */
bool pe_sandbox_actor_shield_ready(const PeSandboxActor *actor);
bool pe_sandbox_actor_shield_ready_for_item(const PeSandboxActor *actor,int item_id);
bool pe_sandbox_actor_set_shield(PeSandboxActor *actor,uint32_t instance);
/* Optional second group holding the isolated hit-reaction, get-up and
 * sprint-attack leaves: player-reaction, player-reaction-wood-sword-stowed,
 * player-reaction-wood-sword-drawn and player-reaction-wood-shield. All four
 * or none; each must hold exactly the25 ordered clocks of capture
 * female-reaction-01, with NormalIdle equal to the body's own. It is kept
 * apart from the33-clip group so the defence actor stays within64 clips.
 * Loaded and validated only: nothing draws or selects these clips, and
 * readiness is not permission to present or enable a reaction, get-up,
 * sprint attack or block. The poses already carry source clip mirroring. */
/* Exact optional female401 body/drawn/stowed Knife group; no root or host
 * admission implied. Wood Sword sprint remains unavailable. */
bool pe_sandbox_actor_sprint_ready(const PeSandboxActor *actor);
bool pe_sandbox_actor_reaction_ready(const PeSandboxActor *actor);
/* True only when the separate reaction defence actor holds those same25
 * ordered clocks. Capsule binding stays the host's hitbox load. */
bool pe_sandbox_actor_reaction_defence_ready(const PeSandboxActor *actor,const PeActor *defence);

#endif
