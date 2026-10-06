#ifndef PE_TREE_COLLISION_H
#define PE_TREE_COLLISION_H
#include "pe_scene_collision.h"

/* Upright capsule, center in world meters; height includes both hemispheres.
 * Source tree11 uses unit scale. Species/TreeInfo scaling remains host policy. */
typedef struct { PeVec3 center; float radius,height; } PeTreeCapsule;
PeTreeCapsule pe_tree_collision_source(PeVec3 root);
bool pe_tree_collision_valid(const PeTreeCapsule *tree);
/* Exact parallel-capsule Minkowski geometry. Strict penetration only: touching
 * is clear. Invalid overlap conservatively returns true; invalid sweep no hit.
 * Sweeps are continuous translation; tangent/separating contact does not block.
 * Hit position lies on the tree, normal points toward the moving capsule. */
bool pe_tree_collision_overlap(const PeTreeCapsule *tree,PeVec3 feet,float radius,float height);
/* Exact zero-radius ray against the finite cylinder and exposed hemispheres.
 * Direction is normalized internally; finite positive range includes its end.
 * Tangency can hit, unlike a movement sweep. Strictly interior origins do not
 * return an exit (Unity collider-ray policy); invalid inputs return no hit.
 * Analytic hits have triangle=UINT32_MAX, and fraction=distance/range. */
PeSceneHit pe_tree_collision_ray(const PeTreeCapsule *tree,PeVec3 origin,PeVec3 direction,float range);
PeSceneHit pe_tree_collision_sweep(const PeTreeCapsule *tree,PeVec3 feet,float radius,float height,PeVec3 delta);
#endif
