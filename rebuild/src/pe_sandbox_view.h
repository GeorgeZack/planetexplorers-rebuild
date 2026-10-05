#ifndef PE_SANDBOX_VIEW_H
#define PE_SANDBOX_VIEW_H
#include "pe_game.h"
/* Borrowed simulation survives graphics contexts and visits to the tutorial.
 * Return 2 for menu, 1 for error, 0 for quit/successful bounded smoke. */
int pe_sandbox_view(PeGame *game,const char *assets,const char *save_path,
                    int smoke_frames,const char *smoke_panel,const char *screenshot);
#endif
