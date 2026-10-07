#ifndef PE_CONVERSATION_HISTORY_H
#define PE_CONVERSATION_HISTORY_H
#include <stdbool.h>
#define PE_CONVERSATION_HISTORY_CAPACITY 20u
/* Native UTF-8 byte bounds include NUL. Source strings are unbounded. Reject
 * oversized input without truncating or changing history. */
#define PE_CONVERSATION_SPEAKER_CAPACITY 128u
#define PE_CONVERSATION_CONTENT_CAPACITY 4096u
typedef struct {
    char speaker[PE_CONVERSATION_SPEAKER_CAPACITY];
    char content[PE_CONVERSATION_CONTENT_CAPACITY];
} PeConversationEntry;
typedef struct {
    unsigned count;
    PeConversationEntry entries[PE_CONVERSATION_HISTORY_CAPACITY];
} PeConversationHistory;
bool pe_conversation_history_valid(const PeConversationHistory *history);
/* Records fully resolved displayed text. Content-only duplicate returns true
 * with added=false; speaker does not change the dedupe key. Failed calls leave
 * history and optional added output untouched. Inputs may alias live entries. */
bool pe_conversation_history_add(PeConversationHistory *history,const char *speaker,
    const char *content,bool *added);
void pe_conversation_history_clear(PeConversationHistory *history);
#endif
