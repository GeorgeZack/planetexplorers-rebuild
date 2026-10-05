#ifndef PE_TRAINING_DIG_H
#define PE_TRAINING_DIG_H
#include "pe_training_gathering.h"

typedef enum { PE_DIG_IDLE, PE_DIG_ENTRY, PE_DIG_LOOP, PE_DIG_EXIT } PeTrainingDigStage;
typedef struct { uint64_t instance_id; float durability; } PeTrainingShovel;
typedef struct {
    PeTrainingDigStage stage;
    bool male, ending, first, target_valid, stamina_lock;
    double seconds, exit_seconds;
    unsigned change_count, hit_count;
    uint64_t swings;
    float speed;
    int target_x,target_y,target_z;
} PeTrainingDig;
typedef struct {
    PeVec3 ray_origin,ray_direction,player; /* World coordinates, not crop-local. */
    bool enabled,held; /* Authoritative ownership, held tool and action permissions. */
} PeTrainingDigInput;
typedef struct {
    bool started,swung,removed,capacity_blocked,finished;
    int x,y,z; /* Removed/damaged crop-local sample when swung. */
    unsigned dirt_added;
} PeTrainingDigUpdate;
void pe_training_dig_init(PeTrainingDig *action,bool male);
void pe_training_dig_cancel(PeTrainingDig *action);
const char *pe_training_dig_clip(const PeTrainingDig *action);
double pe_training_dig_clip_seconds(const PeTrainingDig *action);
/* One1/60 scaled-game-second tick. Source entry exit(.6 at speed2), loop clip
 * events and release(.8), then exit state are evaluated as discrete stages.
 * Incoming/outgoing pose blends, IK and original entry-transition delay are
 * deliberately NOT reproduced. Female/male authored loops differ.
 * Valid disabled calls cancel and succeed without needing a world/tool.
 * Invalid inputs leave action, world, inventory, tool and stamina untouched.
 * Capacity rejection consumes the animation event but preserves edit/wear and
 * follow-up stamina charge. This is a conservation repair, not source Add loss. */
bool pe_training_dig_step(PeTrainingDig *action,PeWorld *world,
    PeTrainingGathering *inventory,PeTrainingShovel *tool,float *stamina,
    PeTrainingDigInput input,PeTrainingDigUpdate *update);
#endif
