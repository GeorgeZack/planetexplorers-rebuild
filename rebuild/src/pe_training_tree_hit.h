#ifndef PE_TRAINING_TREE_HIT_H
#define PE_TRAINING_TREE_HIT_H
#include "pe_scene_collision.h"
#include "pe_tree_collision.h"
#include <stdbool.h>
/* Source HoloTree root CapsuleCollider13687620, not billboard child collider.
 * Host enables this query only while the task's collider is active. */
PeVec3 pe_training_tree_center(void);
/* Source RaycastAll(new Ray(player.position,player.forward),2f).
 * Normalizes direction; no camera offset or nearest-obstacle restriction.
 * Strictly inside origins and invalid directions miss. Distance optional and
 * written only on success. World-space capsule radius1,height7,vertical. */
bool pe_training_tree_hit(PeVec3 origin,PeVec3 forward,float *distance);
/* Exact solid tree versus fixed training player capsule(radius.3,height1.8).
 * Strict overlap; invalid feet conservatively overlap. Continuous sweep uses
 * the same PeSceneHit convention as room BVH, triangleUINT32_MAX for tree.
 * Initial tangent/separating contacts do not block. No mutable state. */
bool pe_training_tree_overlap(PeVec3 feet);
/* The same root capsule as an ordinary Physics.Raycast target: the HoloTree
 * object is on layer 0 (Default), so a layer mask with Default hits it while
 * the host has the collider enabled. Nearest entry; pe_tree_collision_ray policy. */
PeTreeCapsule pe_training_tree_capsule(void);
PeSceneHit pe_training_tree_ray(PeVec3 origin,PeVec3 direction,float range);
PeSceneHit pe_training_tree_sweep(PeVec3 feet,PeVec3 delta);
#endif
