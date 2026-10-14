#ifndef PE_TRAINING_H
#define PE_TRAINING_H
#include "pe_scene_collision.h"
#include "pe_tutorial.h"
#include "pe_training_gathering.h"
#include "pe_training_dig.h"
#include "pe_training_receiver.h"

#define PE_TRAINING_RADIUS 0.30f
#define PE_TRAINING_HEIGHT 1.80f
#define PE_TRAINING_DT (1.0f/60.0f)
typedef struct {
    PeVec3 position, velocity; /* Feet position in original Unity world space. */
    bool ready, grounded, sprinting, jumping;
    /* The capsule's ground contact at the start of the last step, any slope;
     * a presentation input standing in for HumanPhyCtrl's contact normals. */
    bool ground_contact; PeVec3 ground_normal;
    float stamina, stamina_idle, jump_elapsed;
    int camera_mode; /* 0 third person, 1 control, 2 first person. */
    PeTutorial tutorial;
    PeTrainingDig dig;
    PeTrainingShovel shovels[2];
    int equipped_shovel; /* -1 or stable instance index0/1, sharing slot16. */
    bool shovel_held;
    bool build_mode;
    PeTrainingGathering gathering; /* Source herb/axe lesson ledger; not persisted. */
    unsigned dialogue_index;
    uint64_t combat_kill_entity_id; /* One authoritative prototype13 death receipt. */
    bool kit_open, kit_transferred, kit_intro_seen;
    bool andy_reached, andy_dialogue_open;
    int juice_count; /* Fixed training inventory: juice916 and one knife33. */
    bool knife_owned, knife_equipped, equipment_lesson_accepted;
    bool axe_equipped, axe_held; /* Axe and knife share source equipment slot16. */
    bool replicator_open, replicator_intro_seen, replicator_intro_pending;
    unsigned craft_elapsed_ticks; /* Fixed recipe1 time, bounded0..180. */
    float hp, max_hp;
    float juice_cooldown; /* Derived seconds for display; ticks are authoritative. */
    uint32_t juice_cooldown_ticks, juice_buff_age_ticks, juice_buff_tick;
    bool juice_buff_active;
    bool inventory_lesson_accepted, inventory_open, character_open;
    int quickslot_item_id; /* Implemented slot1 only; zero is empty. */
} PeTraining;
typedef struct {
    const PeSceneCollision *terrain_scene; /* Optional current MC collision snapshot. */
    const PeSceneCollision *build_scene; /* Optional current Block45 collision snapshot. */
    float move_x, move_z; /* World-space direction; normalized when length >1. */
    bool walk, sprint, jump, autorun;
    bool cut_held, tree_raycast_hit, animator_do_nothing;
    bool stamina_action; /* Host reports an active source stamina-consuming action. */
} PeTrainingInput;

/* Source spawn (27,2,11), projected to a nearby walkable scene surface with a
 * non-overlapping capsule. Failure leaves the destination unchanged. Scene
 * ownership stays with the caller. No synthetic floor/room is supplied. */
bool pe_training_init(PeTraining *training, const PeSceneCollision *scene);
/* Actual changed modes emit source tutorial events. Invalid/same modes do not. */
bool pe_training_camera(PeTraining *training, int mode);
/* Exactly one fixed 1/60s tick. Source speed/stamina rules with provisional
 * 7m/s jump impulse and 20m/s² gravity; capsule slide/step is a new controller.
 * The source jump action expires after 3 seconds even if still airborne;
 * this releases its stamina recovery lock, not the physical falling motion.
 * Tutorial events come from accepted actions, never raw jump/sprint requests.
 * Autorun means the frontend has enabled it, and supplies its world direction.
 * Lessons reach751 after combat, herbs, tree cutting, script use and crafting. Extended
 * dialogue and fixed first-lesson inventory are not persisted. */
void pe_training_step(PeTraining *training, const PeSceneCollision *scene,
                      PeTrainingInput input);
/* Startup739 presents Andy300215 then Plot351 Tips300258 before camera input.
 * Scenario TalkID, not localization ID; zero means no active dialogue.
 * Acknowledgement must match the visible ID, so duplicate/stale input cannot
 * skip another line. Source walking is allowed; a view may pause its modal UI. */
int pe_training_dialogue(const PeTraining *training);
bool pe_training_advance_dialogue(PeTraining *training, int expected_talk_id);
/* Presentation routing: response1 selects the sole visible Continue; zero or
 * absent rows do nothing. Returns modal ownership even on the closing frame,
 * so this input must never also use a quickslot/build/action. Host gates focus. */
bool pe_training_dialogue_response(PeTraining *training,unsigned response,bool advance);
/* Kit at (7,1.5,12), inclusive 10m. Transfer the authored whole 916x5 stack
 * once, only after opening and acknowledging its introduction. */
bool pe_training_interact_kit(PeTraining *training);
void pe_training_close_kit(PeTraining *training);
bool pe_training_transfer_juice(PeTraining *training);
int pe_training_inventory_count(const PeTraining *training, int item_id);
/* Return target (32,4,12), inclusive 3m, then explicit Andy interaction. */
bool pe_training_interact_andy(PeTraining *training);
/* Accept743 once: its authored extra916x5 grant precedes300221/300222.
 * Only a fresh inventory-open edge after both acknowledgements completes743.
 * Open returns true for an actual window state change, even in earlier lessons;
 * it never counts a premature open toward a later mission. */
bool pe_training_accept_inventory_lesson(PeTraining *training);
bool pe_training_open_inventory(PeTraining *training);
void pe_training_close_inventory(PeTraining *training);
/* Source character window is independent of the inventory. Opening it emits
 * no inventory lesson event. Unequip accepts either panel; equip needs the bag.
 * Character UI is modal for world tool actions; paid crafting/timers continue. */
bool pe_training_open_character(PeTraining *training);
void pe_training_close_character(PeTraining *training);
/* Frontend calls assignment only after a real drag/drop into slot1. It changes
 * the slot without moving/duplicating inventory. Valid use consumes one916,
 * applies source HP-only buff30200009 and its ten-second cooldown; later uses
 * cannot replay lesson completion. Dead players cannot consume or revive. */
bool pe_training_assign_quickslot(PeTraining *training, int item_id);
bool pe_training_use_quickslot(PeTraining *training);
/* Explicit745 acceptance grants one unique knife once; inventory_count(33)
 * excludes it while equipped. Equip/unequip transfer that same ownership and
 * require a living player and no active dialogue. Equip needs the inventory;
 * unequip accepts inventory or character panel. */
bool pe_training_accept_equipment_lesson(PeTraining *training);
bool pe_training_equip_knife(PeTraining *training);
bool pe_training_unequip_knife(PeTraining *training);
/* Host calls only for an actual positive-to-zero HP death, never an attack
 * request or animation event. Source target1025 matches prototype13, count1,
 * without requiring a particular spawn or player killer. Accepted only after
 * both746 intro lines; duplicate/wrong/inactive receipts fail atomically.
 * Marks the objective but retains746 until300230 is acknowledged, then719
 * presents4072/300231. This API neither kills an entity nor grants items. */
bool pe_training_monster_died(PeTraining *training,uint32_t prototype_id,uint64_t entity_id);
/* Lesson719: living player, cleared intro and closed inventory; strict source
 * dist²<7, no equipment restriction. Multiple distinct nearby herbs can run
 * concurrently. Accepted gathering locks requested movement until the1.1s
 * depletion; physics still runs. Death cancels pending actions, not depleted
 * herbs. Fifth depletion enters748 and grants safety axe1527 once. */
bool pe_training_begin_herb(PeTraining *training,unsigned index);
/* Source shared slot16: equip swaps the other weapon back to inventory.
 * Ledger counts are ownership, inventory_count excludes the equipped axe.
 * Draw/stow is an immediate host state until source animation is integrated. */
bool pe_training_equip_axe(PeTraining *training);
bool pe_training_unequip_axe(PeTraining *training);
bool pe_training_set_axe_held(PeTraining *training,bool held);
/* Explicit successful script use learns formula1 and consumes406 once. */
bool pe_training_learn_script(PeTraining *training);
/* First open during721 queues300241; opening alone never produces a shovel.
 * Recipe1 debits10 wood at start and creates1 shovel after3 fixed seconds.
 * Completion enters751, granting the source's second shovel; no cancellation
 * or mining is exposed here. Closing the window does not cancel paid work. */
bool pe_training_open_replicator(PeTraining *training);
void pe_training_close_replicator(PeTraining *training);
bool pe_training_start_shovel_craft(PeTraining *training);
bool pe_training_equip_shovel(PeTraining *training,unsigned index);
bool pe_training_unequip_shovel(PeTraining *training);
bool pe_training_set_shovel_held(PeTraining *training,bool held);
/* Call once per fixed tick before pe_training_step. Outside751 this is a
 * successful no-op/cancellation. Mound is externally owned; queries use room
 * coordinates. After a changed world revision rebuild its rendered MC mesh
 * and collision snapshot before the following physics tick. False rejects an
 * invalid active input without mutation. Host reports actual movement intent.
 * Five-or-more real dirt receipts clear the authored mound and accept752 once. */
bool pe_training_mine_step(PeTraining *training,PeWorld *mound,
    PeVec3 ray_origin,PeVec3 ray_direction,bool held,bool move_requested,
    PeTrainingDigUpdate *update);
/* Bounded native recovery when the authored mound appears around the player.
 * Keeps x/z; searches at most4m upward, never crosses a room ceiling, then
 * settles on the earliest walkable room/terrain surface. Clear inputs remain
 * byte-identical; failure leaves all state untouched. No source parity claim. */
/* Same recovery with additional receiver/build collision. At least one
 * dynamic scene is required; pre-existing room penetration is not repaired.
 * If the first downward contact is steep, the first clear raised position is
 * accepted airborne and ordinary gravity/slide resolves the edge afterward. */
bool pe_training_resolve_obstacles(PeTraining *training,const PeSceneCollision *room,
                                   const PeSceneCollision *terrain,const PeSceneCollision *build);
bool pe_training_resolve_terrain(PeTraining *training,const PeSceneCollision *room,
                                 const PeSceneCollision *terrain);
/* An actual successful build-mode open completes752 only after its intro.
 * Its three completion lines must be acknowledged before receiver lesson753.
 * Reopening cannot replay the event/grant. Closing is always allowed. */
bool pe_training_set_build_mode(PeTraining *training,bool enabled);
/* Runs the real50-ray evaluator; no caller-supplied completion bool. Success
 * advances directly to unsupported756, not obsolete754/755. The frontend must
 * remove authored anchors and lesson-written blocks when that transition occurs.
 * Failure leaves training/output unchanged (query must itself be read-only). */
bool pe_training_check_receivers(PeTraining *training,PeTrainingReceiverQuery query,
                                  void *context,PeTrainingReceiverResult *result);
#endif
