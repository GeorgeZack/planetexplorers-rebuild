#ifndef PE_TRAINING_TEXT_H
#define PE_TRAINING_TEXT_H

typedef struct {
    int dialogue_id;       /* Scenario.talk_id, also used by Quest_List.TalkOP. */
    int localization_id;   /* First component of Scenario.talk_content. */
    int speaker_npc_id;    /* Preserve source sentinel IDs; do not infer names. */
    const char *english;
} PeTrainingDialogue;

/* Borrowed immutable entries. Unknown IDs return NULL; no allocation or I/O. */
const PeTrainingDialogue *pe_training_dialogue_entry(int dialogue_id);
const char *pe_training_dialogue_text(int dialogue_id);
#endif
