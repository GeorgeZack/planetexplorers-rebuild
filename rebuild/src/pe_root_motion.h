#ifndef PE_ROOT_MOTION_H
#define PE_ROOT_MOTION_H
#include "pe_world.h"
#define PE_ROOT_MOTION_MAX_SAMPLES 3600u
#define PE_ROOT_MOTION_CLIP 408u
#define PE_ROOT_MOTION_SKILL 20110014u
#define PE_ROOT_MOTION_DURATION 1.8333396911621094
typedef struct PeRootMotion PeRootMotion;
/* Owned PERM1 curve: female RunSwordAttack408/20110014 only. No default or
 * fallback asset. Loading validates bounded finite doubles, ordered observed
 * timestamps, <=1/60s gaps, zero origin and first crossing of authored end.
 * Provenance/qualification belongs to export_running_knife_motion.py. */
PeRootMotion *pe_root_motion_load(const char *path,char *error,size_t capacity);
void pe_root_motion_free(PeRootMotion *motion);
size_t pe_root_motion_count(const PeRootMotion *motion);
double pe_root_motion_duration(const PeRootMotion *motion);
/* Absolute displacement relative to measured time0 root, original Unity XYZ
 * (+Z forward), interpolated only inside observed brackets. No yaw/reflection,
 * clock advancement, velocity correction or collision; Y is evidence, not an
 * instruction to move a grounded player vertically. Valid time is [0,duration].
 * Delta requires from<=to; both functions leave output unchanged on failure. */
bool pe_root_motion_sample(const PeRootMotion *motion,double seconds,PeVec3 *out);
bool pe_root_motion_delta(const PeRootMotion *motion,double from,double to,PeVec3 *out);
#endif
