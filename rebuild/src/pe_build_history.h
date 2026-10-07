#ifndef PE_BUILD_HISTORY_H
#define PE_BUILD_HISTORY_H
#include "pe_game.h"
/* Validates the journal itself, not whether current world contents/reach permit
 * replay. Intervening edits can leave valid, temporarily blocked history. */
bool pe_build_history_state_valid(const PeGame *game);
void pe_build_history_clear(PeGame *game);
/* Availability only; replay can still fail current geometry/resources checks. */
bool pe_build_history_can_undo(const PeGame *game);
bool pe_build_history_can_redo(const PeGame *game);
/* Failure preserves owners, cells and cursor; only message may change.
 * Undo Add refunds floor(n/4), Undo Remove costs ceil(n/4), per material.
 * These repeat source operations, never restore a historical inventory image. */
bool pe_build_history_undo(PeGame *game);
bool pe_build_history_redo(PeGame *game);
/* Transaction-owner hooks. Record only after all normal edit checks pass and
 * before the infallible commit. Replay applies exact-cell content guards and
 * authoritative collision/resource checks without recursively recording. */
bool pe_build_history_record(PeGame *game,const PeB45Cell *cells,int count,bool addition);
bool pe_game_replay_build_cells(PeGame *game,const PeB45Cell *cells,int count,bool addition);
#endif
