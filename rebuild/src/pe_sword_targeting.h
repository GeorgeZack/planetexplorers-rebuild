#ifndef PE_SWORD_TARGETING_H
#define PE_SWORD_TARGETING_H
#include "pe_hitbox.h"

/* EntityPlayer.prefab Motion_Equip11400022, source SHA256
 * 7119a434d3e1527cca656614ab347f87593913510a5302bef117a2e7834e4d3f.
 * These override Action_Sword's C# defaults; installed equivalence is pending. */
#define PE_SWORD_TARGET_RADIUS 3.0f
#define PE_SWORD_TARGET_HEIGHT 1.5f
#define PE_SWORD_TARGET_ANGLE 115.0f
#define PE_SWORD_TARGET_MASK UINT32_C(33554432)
#define PE_SWORD_TARGET_CANDIDATES 64u
#define PE_SWORD_TARGET_PARTS 64u
typedef struct {PeVec3 center;float radius;uint32_t layer_mask;} PeSwordTargetQuery;
typedef struct {
    PeHitboxCapsule capsule;
    bool enabled,center_bone;
} PeSwordTargetPart;
typedef struct {
    /* Preserve actual OverlapSphere enumeration order and view identity.
     * Geometry here cannot establish broadphase collider overlap or faction. */
    uint64_t view_id;
    unsigned layer;
    bool overlaps_query,self,ragdoll,conflicting;
    bool has_defence,center_present;
    PeVec3 center;
    const PeSwordTargetPart *parts;
    size_t part_count;
} PeSwordTargetCandidate;
typedef struct {
    bool selected,aim_active;
    size_t candidate_index,part_index; /* SIZE_MAX for no selection/no defence. */
    uint64_t view_id;
    PeVec3 point,direction; /* Source full direction only; no IK or rotation. */
} PeSwordTargetResult;
bool pe_sword_target_query(PeVec3 player,PeSwordTargetQuery *out);
/* Source TP ChangeAttackTarget + PEDefenceTrigger.GetClosest, with the source
 * serialized115deg/3m/1.5m configuration. Input attack direction must have a
 * nonzero horizontal component. Call at attack/combo start, not every frame.
 * A defence candidate DOES NOT update the outer distance ranking: preserve
 * supplied collider order, first-view dedupe and center-bone priority. A
 * collider without Defence ranks by full center distance, initially<100.
 * No query, source ordering reconstruction, target lock, permission inference,
 * action admission, pose sampling, IK or host integration is provided.
 * Invalid inputs leave output untouched; success with selected=false is a
 * valid no-target result. An inside-capsule selection may have zero direction
 * and aim_active=false, matching the source's zero-direction outcome. */
bool pe_sword_target_acquire(PeVec3 player,PeVec3 attack_direction,
    const PeSwordTargetCandidate *candidates,size_t count,PeSwordTargetResult *out);
#endif
