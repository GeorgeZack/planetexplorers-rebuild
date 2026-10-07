#ifndef PE_UI_HISTORY_H
#define PE_UI_HISTORY_H
#include "pe_ui_draw.h"
#include "pe_conversation_history.h"
#include "pe_ui_history_state.h"
/* Graphics/font metrics required. Pure input routes can adjust scroll directly
 * using the last content_height and layout.history_content.h, then draw clamps. */
void pe_ui_history_update(const PeUiCanvas *canvas,const PeConversationHistory *history,PeUiHistoryState *state,float wheel);
void pe_ui_history_seek(const PeUiCanvas *canvas,const PeConversationHistory *history,PeUiHistoryState *state,float pointer_y);
void pe_ui_history_draw(const PeUiCanvas *canvas,const PeConversationHistory *history,PeUiHistoryState *state);
#endif
