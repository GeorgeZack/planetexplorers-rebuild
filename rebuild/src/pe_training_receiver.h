#ifndef PE_TRAINING_RECEIVER_H
#define PE_TRAINING_RECEIVER_H
#include "pe_world.h"
#define PE_TRAINING_RECEIVER_RAYS 50u
#define PE_TRAINING_RECEIVER_LAYER 12u
typedef struct { PeVec3 origin,direction; float range; } PeTrainingReceiverRay;
/* receiver: 0=any other layer12 collider, 1=upright, 2=horizontal.
 * Query must return the nearest enabled collider on layer12 only. A miss uses
 * hit=false,receiver=0,distance=0. Equal-distance PhysX ordering is unspecified:
 * the host supplies its tie policy instead of this evaluator inventing one. */
typedef struct { bool hit; unsigned receiver; float distance; } PeTrainingReceiverHit;
typedef bool (*PeTrainingReceiverQuery)(void *context,const PeTrainingReceiverRay *ray,PeTrainingReceiverHit *hit);
typedef struct { uint64_t receiver_mask; int score; bool complete; } PeTrainingReceiverResult;
/* Stable order: upright x-major/y-minor25, horizontal x-major/z-minor25. */
bool pe_training_receiver_ray(unsigned index,PeTrainingReceiverRay *ray);
/* Nearest intersection of the two exact source BoxColliders only; useful to
 * merge with the host's real terrain/build collider query. */
bool pe_training_receiver_boxes(const PeTrainingReceiverRay *ray,PeTrainingReceiverHit *hit);
/* Allocates72 collision-only vertices for both source boxes; caller frees
 * with pe_mesh_free. Destination must be empty. Failure leaves it untouched. */
bool pe_training_receiver_mesh(PeMeshData *mesh);
/* Exact EmitlineRayCaster score: receiver -100, other hit OR miss +1.
 * All50 queries must succeed. Invalid output leaves result untouched. */
bool pe_training_receivers_evaluate(PeTrainingReceiverQuery query,void *context,PeTrainingReceiverResult *result);
#endif
