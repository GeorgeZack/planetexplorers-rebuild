#ifndef PE_SANDBOX_ACTIONS_H
#define PE_SANDBOX_ACTIONS_H
#include "pe_game.h"

typedef enum { PE_SANDBOX_ACTION_NONE, PE_SANDBOX_ACTION_DIG, PE_SANDBOX_ACTION_FELL } PeSandboxActionKind;
typedef enum { PE_SANDBOX_ACTION_IDLE, PE_SANDBOX_ACTION_ENTRY, PE_SANDBOX_ACTION_LOOP, PE_SANDBOX_ACTION_EXIT } PeSandboxActionStage;
typedef struct PeSandboxActionPose {
    int item_id;
    uint32_t instance_id;
    bool held;
    const char *clip; /* Static source name; NULL means ordinary locomotion. */
    double seconds; /* Authored clip seconds, not elapsed wall/game seconds. */
} PeSandboxActionPose;
typedef struct {
    PeSandboxActionKind kind;
    PeSandboxActionStage stage;
    bool male,held,ending,first,stamina_lock;
    int item_id,tree_index,target_x,target_y,target_z;
    uint32_t instance_id;
    double seconds,exit_seconds,ending_seconds,since_hit;
    unsigned change_count,hit_count;
    uint64_t swings;
    float speed;
} PeSandboxActions;
typedef struct {
    bool enabled,held,tool_drawn;
    PeVec3 ray_origin,ray_direction; /* Crop-local, same coordinates as PeGame. */
    /* General Action_Fell requires the same tree under camera100m and player
     * feet+up1/forward2m rays. -1 means miss. Host owns actual tree geometry;
     * these are indices into game.trees, not proximity-only substitutes. */
    int camera_tree,forward_tree;
} PeSandboxActionInput;
typedef struct {
    bool started,swung,changed,removed,capacity_blocked,finished,unsupported;
    int x,y,z,tree_index;
} PeSandboxActionUpdate;
void pe_sandbox_actions_init(PeSandboxActions *action,bool male);
void pe_sandbox_actions_cancel(PeSandboxActions *action);
bool pe_sandbox_actions_supported(int item_id); /* Source-backed12/19/26/1527; host verifies matching art. */
bool pe_sandbox_actions_pose(const PeSandboxActions *action,const PeGame *game,PeSandboxActionPose *pose);
/* One fixed1/60 scaled-game-second tick. Call BEFORE movement, then pass
 * action.stamina_lock through PeInput.stamina_action and suppress movement
 * while stage!=IDLE. No call to legacy pe_game_mine/gather for the same input.
 * enabled is the host's grounded/stationary/third-person/UI/asset permission.
 * Disabling, switching equipment, or losing the tool cancels without edits.
 * A normal release retains authored exit timing; re-press does not undo it.
 * State is external/transient: initialize/cancel after load/import/new game.
 * Source clip events are exact; discrete state transitions approximate Unity
 * blends/IK. Procedural tree prototype/size/yield remain explicit substitutes.
 * Invalid input returns false without changing action, gameplay, or output. */
bool pe_sandbox_actions_step(PeSandboxActions *action,PeGame *game,
    PeSandboxActionInput input,PeSandboxActionUpdate *update);
#endif
