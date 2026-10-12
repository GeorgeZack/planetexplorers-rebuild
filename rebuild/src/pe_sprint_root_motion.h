#ifndef PE_SPRINT_ROOT_MOTION_H
#define PE_SPRINT_ROOT_MOTION_H
#include "pe_combat.h"
#include "pe_world.h"
typedef struct PeSprintRootMotion PeSprintRootMotion;
/* Owned immutable single-record PESR1: female Avatar777/clip401 only. Source
 * qualification lives in export_sprint_root_motion.py; metadata and all bytes
 * are bounded here. Never accepts ordinary PESR1 or observed running PERM1. */
PeSprintRootMotion *pe_sprint_root_motion_load(const char *path,char *error,size_t capacity);
void pe_sprint_root_motion_free(PeSprintRootMotion *motion);
double pe_sprint_root_motion_duration(const PeSprintRootMotion *motion);
/* Dense RootT XZ relative to authored0, Avatar scale; Y0. Invalid queries leave
 * output untouched. No extrapolation, clock advance, motor or collision. */
bool pe_sprint_root_motion_sample(const PeSprintRootMotion *motion,double seconds,PeVec3 *out);
/* Only the retained outgoing female TP sprint stage0 interval. Ordinary
 * Attack7..9 continuations remain owned by pe_sword_root_motion. */
bool pe_sprint_root_motion_world_delta(const PeSprintRootMotion *motion,const PeCombatInterval *interval,
                                       double yaw_degrees,double out[3]);
#endif
