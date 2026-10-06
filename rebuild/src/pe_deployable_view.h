#ifndef PE_DEPLOYABLE_VIEW_H
#define PE_DEPLOYABLE_VIEW_H
#include "pe_deployable.h"
#include "pe_reference.h"
typedef struct { PeReferenceModel torch; Texture2D flame; bool available; } PeDeployableView;
/* Complete absence of the optional directory is tolerated. A partial directory fails
 * closed. Initialize to zero and call only with a live graphics context. */
bool pe_deployable_view_load(PeDeployableView *view,const char *asset_root,char *error,size_t capacity);
void pe_deployable_view_unload(PeDeployableView *view);
bool pe_deployable_view_draw(const PeDeployableView *view,const PeGame *game,Camera3D camera,double seconds);
void pe_deployable_view_preview(const PeDeployableView *view,PeVec3 position,Camera3D camera,double seconds,bool allowed);
/* Shared original flame texture/emitter for an actual held-item tip. It is a
 * bounded renderer for source flame emission/size/alpha; smoke/rubble and full
 * Unity random/world-space trails remain unported. No synthetic model fallback. */
void pe_deployable_view_flame(const PeDeployableView *view,PeVec3 position,Camera3D camera,double seconds,uint32_t identity);
#endif
