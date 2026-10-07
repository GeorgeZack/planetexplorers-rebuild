#ifndef PE_SWORD_ROOT_MOTION_H
#define PE_SWORD_ROOT_MOTION_H
#include "pe_world.h"
#include "pe_combat.h"

typedef struct PeSwordRootMotion PeSwordRootMotion;
/* Owned immutable PESR1 source curves, separate from observed PERM1 motion.
 * Only female Avatar777 Attack6..9 (231/240/247/254) metadata is admitted.
 * Source flags/RootQ cancellation/scale qualification and hashes belong to the
 * exporter; this reader checks the exact bounded profile and binary structure.
 * It does not establish gameplay or arbitrary humanoid root-motion parity. */
PeSwordRootMotion *pe_sword_root_motion_load(const char *path,char *error,size_t capacity);
void pe_sword_root_motion_free(PeSwordRootMotion *motion);
/* Unknown clip/null owner returns0. Duration is full source clip duration,
 * not its earlier EndAction event time. */
double pe_sword_root_motion_duration(const PeSwordRootMotion *motion,uint32_t source_clip);
/* Source dense linear interpolation at(time-begin)*rate, then relative to
 * authored time0 and scaled by Avatar human scale. Unity local XYZ, Y0.
 * No extrapolation, yaw rotation, clock advance, motor cap or collision.
 * Output is unchanged for invalid owner/clip/time/output. */
bool pe_sword_root_motion_sample(const PeSwordRootMotion *motion,uint32_t source_clip,
                                 double seconds,PeVec3 *out);
bool pe_sword_root_motion_delta(const PeSwordRootMotion *motion,uint32_t source_clip,
                                double from,double to,PeVec3 *out);
/* Select from the retained outgoing interval, not the newly reset action.
 * Female TP ordinary stages0..3 and running continuation stages1..3 only;
 * running stage0 remains owned by the captured408/PERM1 wrapper.
 * Rotate by body yaw once (x=c*x+s*z,z=c*z-s*x), then apply the shared
 * horizontal request cap using game dt. No simulation or contact advance. */
bool pe_sword_root_motion_request(const PeSwordRootMotion *motion,
                                  const PeCombatInterval *interval,
                                  PeVec3 current_velocity,double body_yaw_degrees,
                                  double dt,PeVec3 *request);
#endif
