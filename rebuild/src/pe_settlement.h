#ifndef PE_SETTLEMENT_H
#define PE_SETTLEMENT_H
#include "pe_game.h"
#include "pe_scene_collision.h"

/* One archived reference settlement, not a procedural Adventure generator.
 * Configure before creating/loading games. Collision ownership lasts for the
 * process so existing immutable snapshots can safely borrow it. */
bool pe_settlement_configure(const char *asset_root,char *error,size_t capacity);
bool pe_settlement_matches(const PeWorld *world);
bool pe_settlement_is_route(const PeWorld *world);
const PeSceneCollision *pe_settlement_collision_for_world(const PeWorld *world,char *error,size_t capacity);
PeVec3 pe_settlement_npc_anchor_for_world(const PeWorld *world,int role);
const PeSceneCollision *pe_settlement_collision(char *error,size_t capacity);
bool pe_settlement_load_world(const char *path,PeWorld *world,char *error,size_t capacity);
bool pe_settlement_start(PeGame *game,char *error,size_t capacity);
PeVec3 pe_settlement_npc_anchor(int role);
/* Unity Y rotation in degrees for the actor renderer. */
float pe_settlement_npc_yaw(int role);
#endif
