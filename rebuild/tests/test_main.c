#include <stdio.h>
int pe_test_adventure_welcome(void);
int pe_test_adventure_dialogue(void);
int pe_test_adventure_npc(void);
int pe_test_world(void);
int pe_test_world_depth(void);
int pe_test_settlement(void);
int pe_test_sandbox_collision(void);
int pe_test_sandbox_actions(void);
int pe_test_game(void);
int pe_test_mesh(void);
int pe_test_legacy(void);
int pe_test_tutorial(void);
int pe_test_scene_collision(void);
int pe_test_training(void);
int pe_test_player_locomotion(void);
int pe_test_training_melee(void);
int pe_test_training_tree_hit(void);
int pe_test_training_dig(void);
int pe_test_training_terrain(void);
int pe_test_training_build(void);
int pe_test_training_receiver(void);
int pe_test_training_npc(void);
int pe_test_training_ui_layout(void);
int pe_test_training_gathering(void);
int pe_test_actor(void);
int pe_test_hare(void);
int pe_test_combat(void);
int pe_test_knife_contact(void);
int pe_test_hare_attack(void);
int pe_test_hitbox(void);
int pe_test_actor_scene(void);
void test_block(void);
int main(void) {
    test_block();
    int failures = pe_test_world_depth() + pe_test_settlement() + pe_test_adventure_welcome() + pe_test_adventure_dialogue() + pe_test_adventure_npc() + pe_test_sandbox_actions() + pe_test_sandbox_collision() + pe_test_world() + pe_test_mesh() + pe_test_legacy() + pe_test_game() + pe_test_tutorial() + pe_test_scene_collision() + pe_test_training() + pe_test_player_locomotion() + pe_test_training_melee() + pe_test_training_tree_hit() + pe_test_training_dig() + pe_test_training_terrain() + pe_test_training_build() + pe_test_training_receiver() + pe_test_training_npc() + pe_test_training_ui_layout() + pe_test_training_gathering() + pe_test_actor() + pe_test_hare() + pe_test_combat() + pe_test_knife_contact() + pe_test_hare_attack() + pe_test_hitbox() + pe_test_actor_scene();
    printf("Core tests: %s (%d failures)\n", failures ? "FAILED" : "PASS", failures);
    return failures ? 1 : 0;
}
