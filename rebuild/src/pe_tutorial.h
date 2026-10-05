#ifndef PE_TUTORIAL_H
#define PE_TUTORIAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Original Quest_List IDs. Combat consumes an authoritative death event and
 * explicit completion dialogue;756 is the next unsupported gameplay boundary. */
enum {
    PE_TUTORIAL_CAMERA = 739,
    PE_TUTORIAL_MOVEMENT = 747,
    PE_TUTORIAL_MISSION_PANEL = 718,
    PE_TUTORIAL_LOCATION = 740,
    PE_TUTORIAL_STORAGE = 741,
    PE_TUTORIAL_RETURN = 742,
    PE_TUTORIAL_INVENTORY = 743,
    PE_TUTORIAL_QUICKBAR = 744,
    PE_TUTORIAL_CONSUME = 749,
    PE_TUTORIAL_EQUIPMENT = 745,
    PE_TUTORIAL_COMBAT = 746,
    PE_TUTORIAL_GATHER = 719,
    PE_TUTORIAL_TREE = 748,
    PE_TUTORIAL_SCRIPT = 757,
    PE_TUTORIAL_REPLICATE = 721,
    PE_TUTORIAL_MINE = 751,
    PE_TUTORIAL_BUILD = 752,
    PE_TUTORIAL_RECEIVERS = 753,
    PE_TUTORIAL_CREATION = 756
};
typedef enum {
    PE_TUTORIAL_CONTROL_MODE = 1,
    PE_TUTORIAL_FIRST_PERSON,
    PE_TUTORIAL_MOVE,
    PE_TUTORIAL_JUMP,
    PE_TUTORIAL_SPRINT,
    PE_TUTORIAL_AUTORUN,
    PE_TUTORIAL_INTERFACE_ACKNOWLEDGED,
    PE_TUTORIAL_LOCATION_REACHED,
    PE_TUTORIAL_KIT_OPENED,
    PE_TUTORIAL_JUICE_ACQUIRED,
    PE_TUTORIAL_ANDY_REACHED,
    PE_TUTORIAL_ANDY_ACKNOWLEDGED,
    PE_TUTORIAL_INVENTORY_OPENED,
    PE_TUTORIAL_QUICKSLOT_ASSIGNED,
    PE_TUTORIAL_JUICE_USED,
    PE_TUTORIAL_WEAPON_EQUIPPED,
    PE_TUTORIAL_MONSTER_DEFEATED,
    PE_TUTORIAL_COMBAT_ACKNOWLEDGED,
    PE_TUTORIAL_HERBS_DEPLETED,
    PE_TUTORIAL_TREE_DEPLETED,
    PE_TUTORIAL_SCRIPT_LEARNED,
    PE_TUTORIAL_SHOVEL_ACQUIRED,
    PE_TUTORIAL_DIRT_ACQUIRED,
    PE_TUTORIAL_BUILD_ENTERED,
    PE_TUTORIAL_BUILD_ACKNOWLEDGED,
    PE_TUTORIAL_RECEIVERS_BLOCKED
} PeTutorialEvent;
typedef struct {
    uint32_t mission_id;
    uint32_t camera_objectives;
    uint32_t movement_objectives;
    uint32_t stage_objectives; /* Current storage/return/combat lesson only. */
} PeTutorial;
typedef struct {
    int target_id, text_id;
    PeTutorialEvent event;
    uint32_t bit;
} PeTutorialObjective;
typedef enum {
    PE_TUTORIAL_IGNORED,
    PE_TUTORIAL_PROGRESS,
    PE_TUTORIAL_COMPLETED
} PeTutorialResult;
typedef struct {
    int completed_mission_id, next_mission_id;
} PeTutorialTransition;

void pe_tutorial_init(PeTutorial *tutorial);
bool pe_tutorial_valid(const PeTutorial *tutorial);
bool pe_tutorial_supported(const PeTutorial *tutorial);
const PeTutorialObjective *pe_tutorial_objectives(int mission_id, size_t *count);
bool pe_tutorial_objective_done(const PeTutorial *tutorial, int target_id);
/* Events describe successful gameplay actions, never raw key presses.
 * Inactive/duplicate events have no effect. Transition is zeroed on entry;
 * COMPLETED emits it once. Item transactions belong to the training controller.
 * Camera tasks require distinct modes, repairing the old callback counter. */
PeTutorialResult pe_tutorial_event(PeTutorial *tutorial, PeTutorialEvent event,
                                    PeTutorialTransition *transition);

/* Standalone snapshot, not yet embedded in PECS game saves. Fixed byte layout:
 * PETT magic, u32 version=1, mission, camera mask, movement mask, FNV1a checksum
 * of the preceding 20 bytes. All integers little-endian; no struct padding.
 * Decode validates a temporary state and leaves the destination unchanged on
 * any error. Exact length required. Encode also leaves output untouched on
 * failure. This v1 format supports only 739/747/718; extended missions are
 * rejected because their dialogue, storage and inventory need a new schema.
 * Replaying a completed event after restore cannot complete twice. */
#define PE_TUTORIAL_SNAPSHOT_SIZE 24u
bool pe_tutorial_encode(const PeTutorial *tutorial, uint8_t *bytes, size_t size);
bool pe_tutorial_decode(PeTutorial *tutorial, const uint8_t *bytes, size_t size);
#endif
