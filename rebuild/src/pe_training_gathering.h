#ifndef PE_TRAINING_GATHERING_H
#define PE_TRAINING_GATHERING_H
#include "pe_world.h"
#include <stdbool.h>
#include <stdint.h>
#define PE_GATHERING_HERBS 5
#define PE_GATHERING_ITEM_LIMIT 1000000u
/* Host owns the real inventory/equipment and event permission boundary. These
 * counts are a standalone lesson ledger, not a replacement inventory system. */
typedef enum { PE_TREE_IDLE, PE_TREE_WAIT, PE_TREE_ENTRY, PE_TREE_CUT, PE_TREE_EXIT } PeGatheringTreeStage;
typedef struct {
    int mission;
    uint32_t accepted, wood, axe, script, shovel, dirt;
    bool complete, formula1, crafting;
    bool herb_dead[PE_GATHERING_HERBS], herb_active[PE_GATHERING_HERBS];
    double herb_time[PE_GATHERING_HERBS], craft_time, tree_time;
    unsigned herbs_depleted, tree_hp;
    PeGatheringTreeStage tree_stage;
    bool tree_hit, tree_release; /* Fell=false stays latched until the cut exits. */
    unsigned removed_count;
    int64_t removed[3][3]; /* Lesson2032 needs three2-dirt samples; replay guard. */
} PeTrainingGathering;
typedef struct {
    bool cut_held, axe_present, equipment_held;
    /* Actual source player-forward RaycastAll(2m) contains HoloTree collider;
     * this is not camera aim or an invented nearest-obstacle restriction. */
    bool tree_raycast_hit, animator_do_nothing;
} PeGatheringInput;
void pe_training_gathering_init(PeTrainingGathering *state);
/* Host starts719 only after real746 completion/dialogue. This module has no
 * combat/death event or automatic mission entry. Later entry requires source
 * successor + completed objective; duplicate entry cannot duplicate grants. */
bool pe_training_gathering_enter(PeTrainingGathering *state,int mission);
int pe_training_gathering_next(const PeTrainingGathering *state);
uint32_t pe_training_gathering_count(const PeTrainingGathering *state,int item);
/* Host-confirmed external acquisition only; known IDs, positive bounded count.
 * Updates possession objectives. Source lesson grants do not use this API. */
bool pe_training_gathering_acquire(PeTrainingGathering *state,int item,uint32_t count);
PeVec3 pe_training_gathering_herb_position(unsigned index);
bool pe_training_gathering_begin_herb(PeTrainingGathering *state,unsigned index,PeVec3 player);
/* dt finite [0,60]. Processes crossed timer boundaries without dropping hits;
 * invalid state/input leaves state untouched. Capacity failure on final tree
 * reward blocks that hit so wood cannot be lost. No graphics or physics. */
bool pe_training_gathering_step(PeTrainingGathering *state,double dt,PeGatheringInput input);
bool pe_training_gathering_learn_script(PeTrainingGathering *state);
bool pe_training_gathering_start_craft(PeTrainingGathering *state);
/* Read-only snapshots of an actual committed edit, local voxel coordinates.
 * Same world identity/origin, changed revision, material1 nonair->air required.
 * Rejects coordinate replays, unchanged edits and post-completion receipts.
 * Host remains responsible for snapshot authenticity. No terrain mutation. */
bool pe_training_gathering_removed_voxel(PeTrainingGathering *state,
    const PeWorld *before,const PeWorld *after,int x,int y,int z);
bool pe_training_gathering_enter_build_mode(PeTrainingGathering *state);
bool pe_training_gathering_movement_locked(const PeTrainingGathering *state);
#endif
