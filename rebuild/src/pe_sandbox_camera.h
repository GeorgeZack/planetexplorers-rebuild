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
#endif
