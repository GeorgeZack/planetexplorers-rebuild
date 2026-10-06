#ifndef PE_ADVENTURE_DIALOGUE_H
#define PE_ADVENTURE_DIALOGUE_H
#include "pe_game.h"

typedef enum { PE_ADVENTURE_DIALOGUE_IDLE, PE_ADVENTURE_DIALOGUE_MENU, PE_ADVENTURE_DIALOGUE_TALK, PE_ADVENTURE_DIALOGUE_CHOICE } PeAdventureDialogueStage;
typedef enum { PE_ADVENTURE_SPEAKER_PLAYER, PE_ADVENTURE_SPEAKER_WELCOME, PE_ADVENTURE_SPEAKER_SUPPLIER, PE_ADVENTURE_SPEAKER_ANNOT } PeAdventureSpeaker;
typedef struct { int talk_id,localization_id; PeAdventureSpeaker speaker; const char *text; } PeAdventureDialogueLine;
typedef struct {
    PeAdventureDialogueStage stage;
    uint32_t entity_id; /* Host's resolved Adventure role entity; never a prototype. */
    unsigned page;
    int role; /* Resolved Adventure welcome1, supplier2 or Annot3. */
    int mission_id; /* Transient role3 offer binding; reject stale progression. */
} PeAdventureDialogue;
typedef struct {
    const char *player_name,*welcome_name,*supplier_name,*town_name; /* Given names in source tokens. */
    const char *seed_name;
    const char *item_name,*item_count;
    const char *annot_name,*annot_full_name,*start_town_name;
    const char *player_full_name,*welcome_full_name,*supplier_full_name; /* Optional speaker labels. */
} PeAdventureDialogueBindings;
typedef enum {
    PE_ADVENTURE_DIALOGUE_REJECTED, PE_ADVENTURE_DIALOGUE_ADVANCED,
    PE_ADVENTURE_DIALOGUE_ACCEPTED, PE_ADVENTURE_DIALOGUE_NO_SPACE,
    PE_ADVENTURE_DIALOGUE_UNAVAILABLE
} PeAdventureDialogueResult;

void pe_adventure_dialogue_init(PeAdventureDialogue *dialogue);
/* Explicit host interruption: discard transient pages, no grant. Reopening
 * replays supplier pages; welcome and hunt return resume saved progress. Source normal
 * dialogue close/click instead ADVANCES. */
void pe_adventure_dialogue_cancel(PeAdventureDialogue *dialogue);
/* Source4502: Adventure XZ distance <5 from role2's integer-truncated starting
 * XZ target; below-world/dungeon playerY<0 rejected. Host passes actual resolved
 * supplier position, not an invented named prototype. No supply grant here. */
bool pe_adventure_arrival_step(PeGame *game,PeVec3 supplier_position);
/* Roles1/2; role3 exposes9071 after9032 and9074 after9072. Pending hunt return
 * opens persisted4105/4106 directly, with no additional inventory transaction. Welcome opens its
 * next persisted source page. Host eligibility includes the actual NPC
 * hover/range/scene/UI query. Opening never changes game/progression/items. */
bool pe_adventure_dialogue_open(PeAdventureDialogue *dialogue,const PeGame *game,
    int adventure_role,uint32_t entity_id,bool eligible);
/* Role2 supplies or role3 introduction / ready delivery question. */
bool pe_adventure_dialogue_choose_supply(PeAdventureDialogue *dialogue,const PeGame *game);
/* Dispatch the bound role3 choice:4097 medicine/4098 refusal or4103 hunt/4104
 * refusal. Acceptance uses the authoritative transaction. Capacity/unavailable
 * failures retain CHOICE for retry and the core explains the failure. */
PeAdventureDialogueResult pe_adventure_dialogue_choose_medicine(PeAdventureDialogue *dialogue,PeGame *game,bool accept);
const PeAdventureDialogueLine *pe_adventure_dialogue_choice(const PeAdventureDialogue *dialogue,const PeGame *game,unsigned index);
/* Welcome ACK persists each source page; final4088 starts Arrival.
 * Final supplier ACK4094 alone invokes authoritative pe_game_accept_supplies.
 * Annot4096/4102 ACK opens the respective choices;4100 ACK completes medicine.
 * Hunt return4105/4106 ACK only advances already-rewarded saved dialogue.
 * Capacity failure stays on4094 for retry; all grants remain atomic/idempotent. */
PeAdventureDialogueResult pe_adventure_dialogue_advance(PeAdventureDialogue *dialogue,PeGame *game);
/* Welcome TALK returns1949,4081..4088. Supplier MENU returns the player's
 * source option4089; supplier TALK returns4090..4094.
 * After medicine completion role3 offers9074; accepted hunt has no further
 * current line until the return transaction schedules4105/4106. After those
 * pages,9077 remains unsupported and has no invented offer.
 * After supplier acceptance MENU has no offer; host may render its
 * source close choice. Invalid/stale/dead state also returns NULL. Text is raw
 * source with role tokens. */
const PeAdventureDialogueLine *pe_adventure_dialogue_current(const PeAdventureDialogue *dialogue,const PeGame *game);
const PeAdventureDialogueLine *pe_adventure_dialogue_line(int talk_id);
const char *pe_adventure_mission_title(int mission_id);
const char *pe_adventure_mission_description(int mission_id);
const char *pe_adventure_mission_objective(int mission_id);
/* Format only source role/name tokens; missing names retain original tokens.
 * Names bounded128 bytes, result bounded2048. Failure leaves output unchanged. */
bool pe_adventure_dialogue_format(const char *text,const PeAdventureDialogueBindings *bindings,char *output,size_t capacity);
const char *pe_adventure_dialogue_speaker(const PeAdventureDialogueLine *line,const PeAdventureDialogueBindings *bindings);
#endif
