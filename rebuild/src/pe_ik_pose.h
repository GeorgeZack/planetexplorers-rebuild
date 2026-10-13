#ifndef PE_IK_POSE_H
#define PE_IK_POSE_H
#include "pe_actor.h"
#include "pe_fbbik.h"
/* Animated input for pe_fbbik: per-clip local transforms of the female solver
 * bones, baked by the installed engine from single clips and staged by
 * export_player_ik_pose.py as PEIK1. Hosts reach it only through pe_ik_drive.
 *
 * It carries no controller blending, procedural IK or root motion, and the
 * interpolation between baked samples is a rebuild adapter. The file must
 * match the given rig's bone count and parents exactly. */
typedef struct PeIkPose PeIkPose;
/* Strict bounded loader; no graphics or global state. Failure returns NULL. */
PeIkPose *pe_ik_pose_load(const char *path,const PeFbbikRig *rig,char *error,size_t error_capacity);
void pe_ik_pose_free(PeIkPose *pose);
size_t pe_ik_pose_clips(const PeIkPose *pose);
const PeActorClip *pe_ik_pose_clip(const PeIkPose *pose,size_t clip);
/* PE_ACTOR_BIND_POSE when absent. */
size_t pe_ik_pose_find_clip(const PeIkPose *pose,const char *name);
/* Same clip clock as pe_actor_pose: loop or clamp by duration, the final
 * sample exactly at duration. Positions lerp, rotations take the normalized
 * shortest lerp. Invalid arguments leave output untouched. */
bool pe_ik_pose_sample(const PeIkPose *pose,size_t clip,double seconds,PeFbbikTransform local[PE_FBBIK_MAX_BONES]);
#endif
