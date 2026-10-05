#ifndef PE_APP_MENU_H
#define PE_APP_MENU_H
#include <stdbool.h>
typedef enum { PE_APP_QUIT, PE_APP_NEW_SANDBOX, PE_APP_CONTINUE_SANDBOX,
               PE_APP_TUTORIAL, PE_APP_MENU_ERROR } PeAppChoice;
/* Opens/closes its own graphics context; all resources are released before return.
 * smoke_frames>0 captures the menu and returns QUIT, or ERROR on failure. */
PeAppChoice pe_app_menu(const char *asset_dir,bool can_continue,bool active_sandbox,
                       const char *notice,int smoke_frames,const char *screenshot);
#endif
