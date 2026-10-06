#ifndef PE_HITBOX_H
#define PE_HITBOX_H
#include "pe_actor.h"
#include "pe_world.h"
#define PE_HITBOX_DISABLED UINT32_MAX
#define PE_HITBOX_RIGHT 1u
#define PE_HITBOX_LEFT 2u

typedef enum { PE_HITBOX_DEFENCE, PE_HITBOX_ATTACK } PeHitboxKind;
typedef struct {
    char name[32];
    uint32_t kind, group, joint, form, material;
    float damage_scale;
    float bind_points[4][3]; /* segment A/B and radius origin/local-X point */
} PeHitboxDefinition;
typedef struct PeHitboxes PeHitboxes;
typedef struct { PeVec3 a,b; float radius; } PeHitboxCapsule;
typedef struct { PeVec3 point,normal,self_axis,target_axis; float axis_distance; } PeHitboxContact;
/* Bounded PEHB1 loader, verified against PEAC geometry/skin/skeleton hash.
 * Preserves disabled source records. No graphics, implicit paths or mutation. */
PeHitboxes *pe_hitboxes_load(const char *path,const PeActor *actor,char *error,size_t capacity);
void pe_hitboxes_free(PeHitboxes *boxes);
size_t pe_hitboxes_count(const PeHitboxes *boxes);
const PeHitboxDefinition *pe_hitboxes_get(const PeHitboxes *boxes,size_t index);
/* Mesh bind-space points use actor palettes; outer root is +Y yaw degrees and
 * positive uniform scale. Radius reproduces bone local-X lossy scale from the
 * exported radius vector. Invalid/disabled records leave output unchanged. */
bool pe_hitbox_pose(const PeHitboxes *boxes,const PeActor *actor,size_t index,
                    size_t clip,double seconds,PeVec3 root,float yaw,float scale,
                    PeHitboxCapsule *output);
/* Blend all four authored bind points before constructing the capsule. This
 * agrees with actor joint/vertex blending; it is not a blend of final radii. */
bool pe_hitbox_pose_blend(const PeHitboxes *boxes,const PeActor *actor,size_t index,
                          const PeActorBlendSample *samples,size_t count,
                          PeVec3 root,float yaw,float scale,PeHitboxCapsule *output);
/* Instantaneous source strict overlap (<, not <=): closest segment distance
 * versus sum of radii; degenerate sphere segments supported. No swept-history,
 * broadphase, trigger timing, permission, dedupe, damage, or mission effects.
 * Returns false for no hit/invalid input and leaves optional contact untouched. */
bool pe_hitbox_contact(PeHitboxCapsule self,PeHitboxCapsule target,PeHitboxContact *contact);
#endif
