#ifndef PE_SHIELD_ACTION_H
#define PE_SHIELD_ACTION_H
#include <stdbool.h>
#include <stdint.h>

/* Source PEActionMask positions, not PEActionType indices or item masks. */
enum {
    PE_SHIELD_MASK_GROUND=0, PE_SHIELD_MASK_IN_AIR=1, PE_SHIELD_MASK_IN_WATER=2,
    PE_SHIELD_MASK_SPRINT=4, PE_SHIELD_MASK_REPULSED=6, PE_SHIELD_MASK_WENTFLY=7,
    PE_SHIELD_MASK_KNOCKED=8, PE_SHIELD_MASK_DEATH=9, PE_SHIELD_MASK_GETUP=10,
    PE_SHIELD_MASK_REVIVE=11, PE_SHIELD_MASK_STEP=12, PE_SHIELD_MASK_EQUIPMENT_HOLD=13,
    PE_SHIELD_MASK_SWORD_ATTACK=14, PE_SHIELD_MASK_HOLD=15,
    PE_SHIELD_MASK_SLEEP=24, PE_SHIELD_MASK_EAT=25, PE_SHIELD_MASK_GATHER=29,
    PE_SHIELD_MASK_PICKUP=30, PE_SHIELD_MASK_ON_VEHICLE=31, PE_SHIELD_MASK_GLIDER=34,
    PE_SHIELD_MASK_CLIMB=35, PE_SHIELD_MASK_BUILD=36, PE_SHIELD_MASK_STUNED=38,
    PE_SHIELD_MASK_SIT=40, PE_SHIELD_MASK_LIE=44, PE_SHIELD_MASK_TRAIN=45,
    PE_SHIELD_MASK_CUTSCENE=46, PE_SHIELD_MASK_WHACKED=47, PE_SHIELD_MASK_CURE=48,
    PE_SHIELD_MASK_HAND=51, PE_SHIELD_MASK_RIDE=55, PE_SHIELD_MASK_COUNT=56
};
#define PE_SHIELD_ACTION_BIT(index) (UINT64_C(1) << (index))
/* Source action IDs used by row15 relation requests. */
enum {PE_SHIELD_ACTION_SPRINT=2,PE_SHIELD_ACTION_STEP=12,
    PE_SHIELD_ACTION_EQUIPMENT_HOLD=13,PE_SHIELD_ACTION_SWORD_ATTACK=14,
    PE_SHIELD_ACTION_HOLD=15,PE_SHIELD_ACTION_FLASHLIGHT=40,
    PE_SHIELD_ACTION_LEISURE=58,PE_SHIELD_ACTION_COUNT=63};
typedef enum {PE_SHIELD_ACTION_IDLE,PE_SHIELD_ACTION_WAITING,PE_SHIELD_ACTION_HOLDING} PeShieldActionPhase;
typedef enum {PE_SHIELD_REQUEST_NONE,PE_SHIELD_REQUEST_HOLD,PE_SHIELD_REQUEST_RELEASE} PeShieldActionRequest;
typedef struct {
    PeShieldActionPhase phase;
    uint32_t instance_id; /* Transient binding only; bag owns the actual item. */
} PeShieldAction;
typedef struct {
    PeShieldActionRequest request;
    uint32_t shield_instance_id; /* Zero means no source shield object. */
    uint64_t source_masks,running_actions;
    bool animator_present,move_present,hide_equipment_inactive,action_paused;
    /* Native capability/ownership gates, deliberately separate from source
     * dependencies. No ordinary frontend enables this before composed poses.
     * cancel is an already-accepted relation/model/host cancellation, not an
     * unaccepted attack input. It wins over a held request for this call. */
    bool host_capable,cancel;
} PeShieldActionInput;
typedef struct {
    bool admitted,started,ended,denied;
    bool holding; /* Snapshot: SheildBlock, HoldShield and EnableShieldBlock35. */
    int hold_buff_id; /* Metadata30200177; not a damage reduction multiplier. */
    bool mount_buff,cancel_buff;
    uint64_t pause_actions,end_actions,end_immediately_actions,continue_actions;
    bool set_sword_style,restore_base_style;
    bool set_shield_visible,shield_visible;
} PeShieldActionUpdate;

/* Exact row15 mask dependency set; no Ground/InAir/InWater/equipment-held,
 * stamina, durability or movement-speed requirement. Source relations gate
 * admission only: repeated requests for a running action do not recheck them. */
uint64_t pe_shield_action_forbidden_masks(void);
bool pe_shield_action_source_admissible(bool shield_present,uint64_t masks);
/* Zero-init/reset on host creation/load. No HP, stamina, item, save or clock
 * owner. Effect/relation requests are emitted once per transition; caller must
 * execute them in its larger transaction. WAITING protects nothing until the
 * source EndAction12/58 requests finish and action_paused clears. NONE means
 * input caller was skipped (it is not an implicit release).
 * Native component/capability loss or instance replacement cancels cleanly;
 * a replacement can start only on a subsequent held request. Invalid input is
 * atomic for state and required output. This is not an Animator pose sampler. */
bool pe_shield_action_step(PeShieldAction *action,PeShieldActionInput input,
    PeShieldActionUpdate *update);
#endif
