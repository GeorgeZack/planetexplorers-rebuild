#ifndef PE_SANDBOX_CAMERA_H
#define PE_SANDBOX_CAMERA_H
#include <stdbool.h>

#define PE_SANDBOX_CAMERA_DEFAULT_DISTANCE 6.0f
#define PE_SANDBOX_CAMERA_MIN_DISTANCE 1.8f
#define PE_SANDBOX_CAMERA_NORMAL_MAX 10.0f
#define PE_SANDBOX_CAMERA_BUILD_MAX 20.0f
/* Original PeCamera obstacle mask: Default, SceneStatic, VFVoxelTerrain,
 * Building, Unwalkable, ProxyPlayer. No tree or near-tree layers. The helper
 * makes NO spatial queries; any host collision query must preserve this split. */
#define PE_SANDBOX_CAMERA_OBSTACLE_MASK 8468481u

typedef struct {
    float desired_distance; /* Source DistLevel, with unported activity factor=1. */
    float distance;         /* Smoothed source Dist, BEFORE host obstruction. */
    float velocity;         /* Source DistVelocity, persistent across frames. */
} PeSandboxCamera;
typedef struct {
    float dt;              /* Scaled frame seconds, finite0..1; no hidden clamp. */
    float wheel_notches;   /* Raylib units; .1 Unity-axis/unit is an adapter. */
    bool building,third_person,input_allowed,paused;
} PeSandboxCameraInput;

void pe_sandbox_camera_init(PeSandboxCamera *camera);
/* Once per display frame, not once per fixed gameplay tick with repeated wheel.
 * Source SmoothDamp(.5,+infinity,dt), independently verified against installed
 * Unity5.2 managed code. Gating suppresses wheel only: an unpaused third-person
 * camera still settles while a modal blocks input. Pause/dt0/first person freeze
 * the clock and wheel; mode bounds still clamp both distances. Outward velocity
 * is cleared when a range clamp would carry the camera beyond its bounds.
 * Invalid input/state returns false without mutation. A mode transition keeps
 * no stale >10m distance when leaving construction, even while paused.
 * Not a full CameraForge port: no activity-space evaluation, two-second clamp
 * recovery, original anchor, dynamic near-plane, rotation or query ownership. */
bool pe_sandbox_camera_step(PeSandboxCamera *camera,PeSandboxCameraInput input);

typedef enum {
    PE_SANDBOX_CAMERA_FREE=0,
    PE_SANDBOX_CAMERA_LOCKED=1,
    PE_SANDBOX_CAMERA_FIRST_PERSON=2
} PeSandboxCameraMode;
typedef struct {
    PeSandboxCameraMode mode;
    bool right_tracking,right_dragged;
    float right_seconds,right_motion;
} PeSandboxCameraControls;
typedef struct {
    float dt,mouse_dx,mouse_dy;
    bool focused,input_allowed,pointer_over_ui,captured;
    bool free_cursor,cycle_pressed;
    bool right_pressed,right_down,right_released;
} PeSandboxCameraControlInput;
typedef struct {
    bool third_person,capture,look,world_input,pointer_cursor;
    bool cycle_accepted,right_click;
} PeSandboxCameraControlResult;

/* Tutorial-compatible mode order/default. The host owns actual cursor capture,
 * its warp suppression and the existing UI's Tab/free_cursor flag. Supply
 * input_allowed AFTER the modal press/held/release gate; visible free mouse is
 * not itself a modal. World movement/actions remain enabled without capture.
 * A cycle resets the effective free_cursor override this frame; on accepted
 * cycles the host must also clear its UI-owned flag.
 * RMB tracking begins only on a fresh world press. Free-mode capture waits
 * until3px accumulated motion or a .25s hold, preserving the screen pointer
 * throughout a short stationary click. A short release (<.25s,
 * matching tutorial stow) becomes right_click for interaction/stow. The native
 * 3px accumulated-motion threshold additionally rejects quick drags; this is an
 * explicit input adapter, not a recovered Unity threshold or the tutorial's
 * immediate capture transition. UI/focus blocking,
 * camera cycling or a missing release cancels that gesture without a click.
 * Pointer hover blocks new capture/gestures, not keyboard world_input.
 * Invalid inputs leave both state and result unchanged. */
void pe_sandbox_camera_controls_init(PeSandboxCameraControls *controls);
void pe_sandbox_camera_controls_cancel(PeSandboxCameraControls *controls);
bool pe_sandbox_camera_controls_step(PeSandboxCameraControls *controls,
    PeSandboxCameraControlInput input,PeSandboxCameraControlResult *result);
#endif
