#ifndef PE_HUNT_TARGETING_H
#define PE_HUNT_TARGETING_H
#include "pe_hunt_encounter.h"
#include "pe_sword_targeting.h"
/* Read-only bridge for the pinned Adventure cohort. Stable encounter slot order
 * replaces unobserved Unity overlap enumeration. The source layer25 trigger
 * capsule supplies inclusive analytic broadphase; current visible defence poses
 * supply GetClosest. Faction conflict is the qualified player1/hare4 contract,
 * independent of acquired AI threat. No LOS, target tracking or full aim IK.
 * Unprepared/inactive encounter succeeds with no target. Invalid prepared state
 * leaves output untouched. Result indices refer to encounter slot/defence part. */
bool pe_hunt_target_acquire(const PeHuntEncounter *encounter,PeVec3 player,
    PeVec3 attack_direction,PeSwordTargetResult *out);
#endif
