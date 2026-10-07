#ifndef PE_ADVENTURE_MISSION_EVENTS_H
#define PE_ADVENTURE_MISSION_EVENTS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Disconnected9077..9084 rules. No PeGame, NPC/geometry inference, inventory,
 * quest-host enabling or save layout. Actual world owners qualify input events. */
#define PE_MISSION_EVENTS_COUNT 8
#define PE_MISSION_EVENTS_EFFECT_CAPACITY 16
typedef struct {int item_id,count;} PeMissionEventItem;
typedef struct {
    int mission,prerequisite,mutex[2],choices[2],player_talk[2];
    int opening[4],opening_count,ending,targets[3],target_count;
    bool any_target,auto_reply,repeatable,increase_chain;
    int accept_plot,complete_plot,next;
    unsigned grant_count,remove_count,reward_count;
    PeMissionEventItem grants[3],removes[1],rewards[7];
} PeAdventureMissionEventSource;
const PeAdventureMissionEventSource *pe_adventure_mission_events_source(int mission);
typedef struct {
    bool prior9127_complete,plot5_pending;
    uint8_t offered,active,completed,acceptance_granted,ending_pending,queued_next,chain_advanced;
    uint8_t opening_index[PE_MISSION_EVENTS_COUNT],ready_targets[PE_MISSION_EVENTS_COUNT];
} PeAdventureMissionEvents;
typedef enum {
    PE_MISSION_EVENT_OFFER,
    PE_MISSION_EVENT_OPENING_ACK,
    PE_MISSION_EVENT_TARGET_READY,
    PE_MISSION_EVENT_TURN_IN,
    PE_MISSION_EVENT_PROCESS_PLOT,
    PE_MISSION_EVENT_ENDING_ACK
} PeAdventureMissionInputKind;
typedef struct {
    PeAdventureMissionInputKind kind;
    int mission,value; /* Talk ID, target ID, or plot5; otherwise0. */
    bool qualified; /* Fresh event after source mission init; caller verified NPC/choice/target conditions. */
    bool ready; /* TARGET_READY can clear current possession/readiness. */
} PeAdventureMissionInput;
typedef enum {
    PE_MISSION_EFFECT_ACCEPT,
    PE_MISSION_EFFECT_ACCEPT_PLOT,
    PE_MISSION_EFFECT_GRANT_ACCEPT,
    PE_MISSION_EFFECT_ADVANCE_CHAIN,
    PE_MISSION_EFFECT_REMOVE_COMPLETE,
    PE_MISSION_EFFECT_GRANT_COMPLETE,
    PE_MISSION_EFFECT_COMPLETE_PLOT,
    PE_MISSION_EFFECT_ENDING_DIALOGUE,
    PE_MISSION_EFFECT_NEXT_MISSION,
    PE_MISSION_EFFECT_COMPLETE
} PeAdventureMissionEffectKind;
typedef struct {PeAdventureMissionEffectKind kind;int mission,value,count;} PeAdventureMissionEffect;
typedef struct {unsigned count;PeAdventureMissionEffect values[PE_MISSION_EVENTS_EFFECT_CAPACITY];} PeAdventureMissionEffects;
/* Callback must preflight/commit the WHOLE batch atomically, returnfalse with
 * no side effects, and never mutate this reducer/reenter it. Item owners must
 * validate catalog, gender/components, capacity and current possessions. This
 * module describes source item IDs without establishing runtime support. The
 * callback must also validate every requested NPC/chain effect. No callback
 * means only effect-free routing may proceed. */
typedef bool (*PeAdventureMissionCommit)(void *context,const PeAdventureMissionEffects *effects);
void pe_adventure_mission_events_init(PeAdventureMissionEvents *state,bool completed9127);
bool pe_adventure_mission_events_valid(const PeAdventureMissionEvents *state);
/* Restore exact latches/queues without emitting callbacks or replaying grants.
 * This is a value snapshot contract, not a new PECS/Unity archive decoder. */
bool pe_adventure_mission_events_restore(PeAdventureMissionEvents *state,const PeAdventureMissionEvents *snapshot);
/* Checks retained prerequisite/mutex state, not source NPC/chain availability.
 * Repeatable zero-target choice shells9081/83 remain metadata-only. */
bool pe_adventure_mission_events_eligible(const PeAdventureMissionEvents *state,int mission);
/* Ordered TalkOP acknowledgments accept only on the final page; no-opening
 * offers require ACK with value0. PROCESS_PLOT(9079,5) may directly accept9080
 * before9079 completes. TARGET_READY respects source auto-reply/ComMission.
 * Failed transactions preserve state AND output; caller can retry. Output is
 * an observation of the already committed batch; never execute it a second time. */
bool pe_adventure_mission_events_apply(PeAdventureMissionEvents *state,PeAdventureMissionInput input,
    PeAdventureMissionCommit commit,void *context,PeAdventureMissionEffects *out);
#endif
