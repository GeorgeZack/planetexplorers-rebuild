#ifndef PE_IK_DRIVE_H
#define PE_IK_DRIVE_H
#include "pe_actor.h"
#include "pe_fbbik.h"
/* Presentation owner for the always-on FinalIK solve of the female player:
 * blends the staged solver poses of the named clips, runs pe_fbbik with no
 * effector offsets, and returns one rigid motion per solver bone for skinning.
 *
 * Adapters, not original behavior: the pose read during a blend is the
 * weighted blend of single-clip bone poses (the original solves the Animator's
 * own controller pose), the solver initiates from the prefab rest pose, and
 * finger/toe/mount joints follow their solver ancestor. Only the Halt stop
 * effect supplies offsets; hit and grounder offsets are not connected. State
 * is immutable after load and never saved. */
_Static_assert(PE_ACTOR_DRIVE_BONES==PE_FBBIK_MAX_BONES,"driven skin bone capacity");
typedef struct PeIkDrive PeIkDrive;
typedef struct {const char *clip; double seconds; float weight;} PeIkDriveSample;
typedef struct {float delta[PE_FBBIK_MAX_BONES][12];} PeIkDriveFrame;
/* True when either staged group (player-ik-pose, player-ik-drivers) exists
 * under asset_root. Absent groups keep the undriven presentation; a present
 * group must then load completely. */
bool pe_ik_drive_present(const char *asset_root);
/* Strict bounded PEIK1 + PEID1 load bound to the female rig. No graphics or
 * global state. Failure returns NULL. */
PeIkDrive *pe_ik_drive_load(const char *asset_root,char *error,size_t error_capacity);
void pe_ik_drive_free(PeIkDrive *drive);
/* Borrowed per-joint solver bone indices for one staged part, or NULL unless
 * the group, part, joint count and exact skeleton hash all match. */
const unsigned char *pe_ik_drive_drivers(const PeIkDrive *drive,const char *group,size_t part,const PeActor *actor);
/* count 1..13 named clip samples with the pe_actor blend rules. arms_mapped
 * false is the original's held-equipment state (arm mapping weight 0). False,
 * with the output untouched, when a positive-weight clip has no staged pose
 * or the solve fails; the caller then draws the undriven pose. */
bool pe_ik_drive_solve(const PeIkDrive *drive,const PeIkDriveSample *samples,size_t count,bool arms_mapped,PeIkDriveFrame *out);
/* As above with additive FinalIK effector position offsets: PE_FBBIK_EFFECTORS
 * model-space vectors in FullBodyBipedEffector order, or NULL for none. The
 * Halt stop effect is the only source of them; hit and grounder offsets stay
 * disconnected. */
bool pe_ik_drive_solve_offsets(const PeIkDrive *drive,const PeIkDriveSample *samples,size_t count,bool arms_mapped,
                               const PeVec3 *effector_offsets,PeIkDriveFrame *out);
/* Model-space positions of the left and right foot effector bones in the
 * blended animated pose. False leaves the outputs untouched. */
bool pe_ik_drive_feet(const PeIkDrive *drive,const PeIkDriveSample *samples,size_t count,PeVec3 *left,PeVec3 *right);
/* The actor root transform's rotation: model x,z to world by the body yaw the
 * views draw with, and back. Vectors only; no translation. */
PeVec3 pe_ik_drive_model_vector(PeVec3 world,float yaw_degrees);
PeVec3 pe_ik_drive_world_vector(PeVec3 model,float yaw_degrees);
#endif
