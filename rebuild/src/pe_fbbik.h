#ifndef PE_FBBIK_H
#define PE_FBBIK_H
#include "pe_world.h"
#include <stdbool.h>
#define PE_FBBIK_MAX_BONES 32u
#define PE_FBBIK_CHAINS 5u
#define PE_FBBIK_LIMBS 4u
#define PE_FBBIK_EFFECTORS 9u
#define PE_FBBIK_CONSTRAINTS 4u
#define PE_FBBIK_MAX_SPINE 4u
#define PE_FBBIK_NODES (PE_FBBIK_CHAINS*3u)
#define PE_FBBIK_NONE 0xffu
/* Disconnected installed FinalIK IKSolverFullBodyBiped update: ReadPose, the
 * chain solve and the spine/limb mapping, in compiled method order. Effector
 * and chain ordinals are FullBodyBipedEffector/FullBodyBipedChain. A node is
 * chain*3+index; chain 0 (body) has one node, each limb chain three.
 *
 * No host calls this. Engine Quaternion/Vector3 natives (LookRotation,
 * FromToRotation, Lerp, Slerp, OrthoNormalize) are conventional float
 * adapters, not qualified engine arithmetic. The Animator pose, FixTransforms,
 * GrounderFBBIK, bend goals, effector targets and solver delegates are caller
 * concerns; all bones have unit scale. State is transient and unsaved. */
typedef struct {float x,y,z,w;} PeFbbikQuat;
typedef struct {PeVec3 position; PeFbbikQuat rotation;} PeFbbikTransform;
typedef struct {
    unsigned bone_count;
    signed char parent[PE_FBBIK_MAX_BONES];      /* -1 or a lower index */
    unsigned root;                               /* IKSolver.root: its up axis feeds PullBody */
    unsigned iterations;
    unsigned char chain_bone[PE_FBBIK_CHAINS][3];
    float pin[PE_FBBIK_CHAINS],pull[PE_FBBIK_CHAINS],push[PE_FBBIK_CHAINS];
    float push_parent[PE_FBBIK_CHAINS],reach[PE_FBBIK_CHAINS];
    unsigned char reach_smoothing[PE_FBBIK_CHAINS],push_smoothing[PE_FBBIK_CHAINS];
    struct {unsigned char chain1,chain2; float push_elasticity,pull_elasticity;} constraint[PE_FBBIK_CONSTRAINTS];
    unsigned char effector_node[PE_FBBIK_EFFECTORS];
    unsigned char effector_plane[PE_FBBIK_EFFECTORS][3];   /* PE_FBBIK_NONE when absent */
    unsigned char body_child[2];                           /* body effector childBones */
    bool effect_child_nodes[PE_FBBIK_EFFECTORS];
    unsigned spine_count,spine_root_index,spine_iterations;
    unsigned char spine[PE_FBBIK_MAX_SPINE];
    float spine_twist_weight;
    unsigned char limb_parent[PE_FBBIK_LIMBS];             /* clavicles, or PE_FBBIK_NONE */
    float limb_maintain_rotation[PE_FBBIK_LIMBS],limb_weight[PE_FBBIK_LIMBS];
    unsigned char head; float head_maintain_rotation;
    float spine_stiffness,pull_body_vertical,pull_body_horizontal;
} PeFbbikRig;
typedef struct {
    const PeFbbikRig *rig;
    bool initiated,bend_local_set[PE_FBBIK_LIMBS];
    /* IKConstraintBend and parent BoneMap values taken once from the pose at
     * Initiate. Which pose the original holds at Start is not qualified. */
    PeVec3 bend_direction[PE_FBBIK_LIMBS],bend_default_local[PE_FBBIK_LIMBS],bend_default_child[PE_FBBIK_LIMBS];
    PeVec3 parent_swing_axis[PE_FBBIK_LIMBS];
    PeVec3 effector_position[PE_FBBIK_EFFECTORS];
    PeFbbikQuat effector_rotation[PE_FBBIK_EFFECTORS];
} PeFbbik;
typedef struct {
    PeVec3 position_offset,position;
    PeFbbikQuat rotation;
    float position_weight,rotation_weight,maintain_relative_position_weight;
} PeFbbikEffector;
typedef struct {
    /* Animated local pose; a bone without a parent is relative to the world. */
    PeFbbikTransform local[PE_FBBIK_MAX_BONES];
    float weight;                                /* IKPositionWeight */
    float bend_weight[PE_FBBIK_LIMBS];           /* IKConstraintBend.weight */
    PeFbbikEffector effector[PE_FBBIK_EFFECTORS];
} PeFbbikInput;
typedef struct {
    bool solved;                                 /* false: weight<=0, pose passed through */
    PeFbbikTransform local[PE_FBBIK_MAX_BONES];
    PeVec3 node[PE_FBBIK_NODES];                 /* final solver positions, diagnostics */
} PeFbbikOutput;
/* The installed female rig and its serialized solver settings. */
const PeFbbikRig *pe_fbbik_female_rig(void);
unsigned pe_fbbik_female_bone_id(unsigned bone);
/* The female prefab's serialized local pose, the only recovered candidate for
 * the initiation pose. */
bool pe_fbbik_female_rest(PeFbbikTransform local[PE_FBBIK_MAX_BONES]);
bool pe_fbbik_init(PeFbbik *state,const PeFbbikRig *rig,const PeFbbikTransform *local);
/* Animated pose with zero weights/offsets and the effector pose from Initiate. */
bool pe_fbbik_input(const PeFbbik *state,const PeFbbikTransform *local,PeFbbikInput *out);
bool pe_fbbik_world(const PeFbbikRig *rig,const PeFbbikTransform *local,unsigned bone,PeFbbikTransform *out);
/* One IKSolverFullBody.OnUpdate. Invalid or nonfinite input, or a nonfinite
 * result, returns false and leaves the output unchanged. The original zeroes
 * every positionOffset afterwards; offsets here are per-call inputs. */
bool pe_fbbik_solve(const PeFbbik *state,const PeFbbikInput *input,PeFbbikOutput *out);
#endif
