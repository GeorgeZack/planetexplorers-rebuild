CC ?= cc
PKG_CONFIG ?= pkg-config
CPPFLAGS += -Isrc -DPE_SOURCE_ROOT=\"$(abspath ..)\"
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
ASAN_OPTIONS ?= detect_leaks=1
CORE = src/pe_debug.c src/pe_hunt_loot.c src/pe_hunt_loot_combat.c src/pe_preferences.c src/pe_melee_facing.c src/pe_hunt_targeting.c src/pe_camera_query.c src/pe_camera_projection.c src/pe_conversation_history.c src/pe_ui_input.c src/pe_camera_geometry.c src/pe_camp_repair.c src/pe_camp_repair_interaction.c src/pe_hare_pounce.c src/pe_inventory.c src/pe_harvest.c src/pe_vegetation.c src/pe_build_history.c src/pe_town_trade.c src/pe_town_wallet.c src/pe_sword_root_motion.c src/pe_sword_variant.c src/pe_sword_motion.c src/pe_revival.c src/pe_attack_playback.c src/pe_shield_action.c src/pe_capsule_history.c src/pe_stamina_recovery.c src/pe_passive_recovery.c src/pe_sword.c src/pe_shield.c src/pe_outfit.c src/pe_running_attack_motion.c src/pe_root_motion.c src/pe_sword_targeting.c src/pe_town_defense_clock.c src/pe_hunt_combat.c src/pe_hare_skill.c src/pe_hunt_encounter.c src/pe_hare_perception.c src/pe_hare_damage.c src/pe_sandbox_melee.c src/pe_adventure_hunt.c src/pe_deployable.c src/pe_adventure_medicine.c src/pe_town_interaction.c src/pe_settlement.c src/pe_terrain_material.c src/pe_adventure_dialogue.c src/pe_adventure_npc.c src/pe_sandbox_camera.c src/pe_tree_collision.c src/pe_sandbox_actions.c src/pe_sandbox_collision.c src/pe_world.c src/pe_mesh.c src/pe_legacy.c src/pe_block.c src/pe_game.c src/pe_save.c src/pe_tutorial.c src/pe_scene_collision.c src/pe_training.c src/pe_player_locomotion.c src/pe_training_melee.c src/pe_training_tree_hit.c src/pe_training_dig.c src/pe_training_terrain.c src/pe_training_build.c src/pe_training_receiver.c src/pe_training_npc.c src/pe_training_ui_layout.c src/pe_training_gathering.c src/pe_training_text.c src/pe_actor.c src/pe_hare.c src/pe_combat.c src/pe_knife_contact.c src/pe_hare_attack.c src/pe_hitbox.c src/pe_actor_scene_manifest.c generated/pe_content.c
HEADERS = src/pe_camera_query.h generated/pe_camera_query_source.h src/pe_camera_projection.h src/pe_ui_history_state.h src/pe_conversation_history.h src/pe_ui_history.h src/pe_ui_draw.h src/pe_ui_hud.h src/pe_ui_input.h src/pe_harvest.h generated/pe_harvest_source.h generated/pe_inventory_source.h src/pe_vegetation.h src/pe_vegetation_view.h generated/pe_vegetation_source.h generated/pe_vegetation_visual_source.h src/pe_stamina_recovery.h generated/pe_stamina_recovery_source.h src/pe_passive_recovery.h generated/pe_passive_recovery_source.h src/pe_root_motion.h src/pe_running_attack_motion.h src/pe_hare_skill.h generated/pe_hare_skill_source.h generated/pe_adventure_population.h src/pe_town_interaction.h generated/pe_town_offers.h src/pe_settlement.h generated/pe_adventure_settlement.h src/pe_terrain_material.h src/pe_terrain_material_view.h generated/pe_terrain_materials.h src/pe_adventure_dialogue.h generated/pe_adventure_dialogue_text.h generated/pe_adventure_npc_collider.h src/pe_adventure_npc.h src/pe_adventure_npc_view.h src/pe_sandbox_camera.h src/pe_tree_collision.h generated/pe_tree_collider.h src/pe_sandbox_actions.h src/pe_sandbox_world_view.h src/pe_sandbox_collision.h src/pe_sandbox_ui.h src/pe_app_menu.h src/pe_sandbox_actor.h src/pe_sandbox_view.h src/pe_world.h src/pe_game.h src/pe_content.h generated/pe_mc_tables.h src/pe_legacy.h src/pe_block.h src/pe_tutorial.h src/pe_scene_collision.h src/pe_training.h src/pe_player_locomotion.h src/pe_training_melee.h src/pe_training_tree_hit.h src/pe_training_dig.h src/pe_training_terrain.h src/pe_training_build.h src/pe_training_receiver.h src/pe_training_npc.h src/pe_training_ui_layout.h generated/pe_training_mound.h src/pe_training_gathering.h src/pe_training_text.h src/pe_actor.h src/pe_hare.h src/pe_combat.h src/pe_knife_contact.h src/pe_hare_attack.h src/pe_hitbox.h src/pe_actor_scene.h
UI_SHARED = src/pe_ui_history.c src/pe_ui_draw.c src/pe_ui_hud.c
FRONTEND = src/pe_ui_settings.c $(UI_SHARED) src/pe_vegetation_view.c src/pe_deployable_view.c src/pe_terrain_material_view.c src/pe_adventure_npc_view.c src/pe_sandbox_world_view.c src/pe_app_menu.c src/pe_sandbox_view.c src/pe_sandbox_actor.c src/pe_sandbox_ui.c src/pe_sandbox_ui_input.c src/main.c src/pe_reference.c src/pe_training_view.c src/pe_training_terrain_view.c src/pe_training_npc_view.c src/pe_training_ui.c src/pe_training_ui_minimap.c src/pe_training_ui_shader.c src/pe_training_ui_replicator.c src/pe_training_ui_mission.c src/pe_ui_assets.c src/pe_actor_view.c src/pe_actor_scene.c src/pe_actor_scene_view.c
HEADERS += $(wildcard src/*.h generated/*.h)
TESTS = tests/test_hunt_loot.c tests/test_hunt_loot_save.c tests/test_hunt_targeting.c tests/test_melee_facing.c tests/test_conversation_history.c tests/test_conversation_history_save.c tests/test_ui_input.c tests/test_bandage.c tests/test_camp_repair_save.c tests/test_followup_catalog.c tests/test_hare_pounce.c tests/test_inventory.c tests/test_inventory_save.c tests/test_harvest.c tests/test_vegetation.c tests/test_vegetation_save.c tests/test_build_history.c tests/test_build_history_save.c tests/test_town_trade.c tests/test_town_trade_save.c tests/test_town_wallet.c tests/test_town_wallet_save.c tests/test_sword_root_motion.c tests/test_sword_walking.c tests/test_sword_variant.c tests/test_sword_motion.c tests/test_revival.c tests/test_attack_playback.c tests/test_shield_action.c tests/test_capsule_history.c tests/test_stamina_recovery.c tests/test_stamina_save.c tests/test_passive_recovery.c tests/test_shield.c tests/test_shield_save.c tests/test_outfit.c tests/test_outfit_save.c tests/test_hunt_lifecycle.c tests/test_running_attack_pipeline.c tests/test_running_attack_motion.c tests/test_root_motion.c tests/test_sword_targeting.c tests/test_v13_save.c tests/test_hunt_return.c tests/test_v12_save.c tests/test_hunt_combat.c tests/test_hare_skill.c tests/test_hunt_encounter.c tests/test_hare_perception.c tests/test_v11_save.c tests/test_hare_route.c tests/test_hare_damage.c tests/test_sandbox_melee.c tests/test_v10_save.c tests/test_adventure_hunt.c tests/test_deployable.c tests/test_medicine_save.c tests/test_adventure_medicine.c tests/test_adventure_population.c tests/test_town_interaction.c tests/test_settlement.c tests/test_world_depth.c tests/test_adventure_welcome.c tests/test_adventure_dialogue.c tests/test_adventure_npc.c tests/test_sandbox_actions.c tests/test_sandbox_collision.c tests/test_main.c tests/test_world.c tests/test_mesh.c tests/test_legacy.c tests/test_block.c tests/test_game.c tests/test_tutorial.c tests/test_scene_collision.c tests/test_training.c tests/test_player_locomotion.c tests/test_training_melee.c tests/test_training_tree_hit.c tests/test_training_dig.c tests/test_training_terrain.c tests/test_training_build.c tests/test_training_receiver.c tests/test_training_npc.c tests/test_training_ui_layout.c tests/test_training_gathering.c tests/test_actor.c tests/test_hare.c tests/test_combat.c tests/test_knife_contact.c tests/test_hare_attack.c tests/test_hitbox.c tests/test_actor_scene.c
BUILD = build
RAYLIB_CFLAGS = $(shell $(PKG_CONFIG) --cflags raylib)
LZ4_CFLAGS = $(shell $(PKG_CONFIG) --cflags liblz4)
LZ4_LIBS = $(shell $(PKG_CONFIG) --libs liblz4)
RAYLIB_LIBS = $(shell $(PKG_CONFIG) --libs raylib)
.PHONY: all test sanitize content check-content run clean smoke training-smoke training-npcs-smoke training-sequence-smoke training-combat-smoke training-gathering-smoke training-crafting-smoke training-mining-smoke training-building-smoke actor-smoke cli-test asset-test ui-asset-test
all: $(BUILD)/planet_explorers

.PHONY: index index-test
index:
	python3 tools/index_project.py build
index-test:
	python3 tests/test_project_index.py

$(BUILD):
	mkdir -p $@
$(BUILD)/planet_explorers: $(FRONTEND) src/pe_reference.h src/pe_training_view.h src/pe_training_terrain_view.h src/pe_training_npc_view.h src/pe_training_ui.h src/pe_training_ui_minimap.h src/pe_training_ui_shader.h src/pe_training_ui_replicator.h src/pe_training_ui_mission.h generated/pe_training_ui_text.h src/pe_ui_assets.h src/pe_actor_view.h src/pe_actor_scene_view.h $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(FRONTEND) $(CORE) -o $@ $(RAYLIB_LIBS) $(LZ4_LIBS) -lm
$(BUILD)/test_core: tests/adventure_test_helpers.h $(TESTS) $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(TESTS) $(CORE) -o $@ $(LZ4_LIBS) -lm
test: camera-geometry-test shop-quantity-details-ui-test camp-repair-test camp-repair-interaction-test sandbox-repair-ui-test sandbox-repair-host-test inventory-discard-test adventure-mission-events-test camera-environment-test sandbox-camera-environment-host-test town-defense-spawn-test sandbox-inventory-ui-test sandbox-vegetation-host-test vegetation-view-test sandbox-build-history-host-test sandbox-shop-trade-ui-test sandbox-wallet-host-test sandbox-revival-host-test humanoid-rotation-test sandbox-stamina-host-test sandbox-health-host-test sandbox-equipment-host-test hunt-consume-test training-asset-clocks-test actor-blend-test town-defense-clock-test terrain-material-test $(BUILD)/test_core ui-asset-test sandbox-ui-test sandbox-actor-test sandbox-world-test sandbox-camera-test
	./$(BUILD)/test_core
	python3 tests/test_adventure_settlement_export.py
	python3 tests/test_adventure_settlement_assets.py
	python3 tests/test_adventure_terrain_export.py
	python3 tests/test_adventure_route_export.py
	python3 tests/test_adventure_route_assets.py
	python3 tests/test_adventure_population.py
	python3 tests/test_hare_skill_export.py
	python3 tests/test_hare_perception_export.py
	python3 tests/test_adventure_hunt_export.py
	python3 tests/test_attack_playback_export.py
	python3 tests/test_capsule_history_export.py
	python3 tests/test_player_collider_center.py
	python3 tests/test_hare_history_center_export.py
	python3 tests/test_sword_variant_export.py
	python3 tests/test_player_root_channels.py
	python3 tests/test_sword_root_export.py
	python3 tests/test_revival_export.py
	python3 tests/test_stamina_recovery_export.py
	python3 tests/test_passive_recovery_export.py
	python3 tests/test_starter_outfit_export.py
	python3 tests/test_starter_sword_shield_export.py
	python3 tests/test_starter_pair_assets.py
	python3 tests/test_starter_pair_unified.py
	python3 tests/test_sandbox_actor_assets.py
	python3 tests/test_shield_layer_inventory.py
	python3 tests/test_shield_pose_analysis.py
	python3 tests/test_shield_controller_export.py
	python3 tests/test_shield_composition.py
	python3 tests/test_shield_capture_numbers.py
	python3 tests/test_shield_qualification.py
	python3 tests/test_shield_qualification_preparation.py
	python3 tests/test_adventure_town_defense_export.py
	python3 tests/test_reference_hunt_encounter.py
	python3 tests/test_reference_town_defense.py
	python3 tests/test_creature_sampler_inventory.py
	python3 tests/test_reference_hunt_combat.py
	python3 tests/test_reference_hunt_return.py
	python3 tests/test_adventure_hunt_encounter_export.py
	python3 tests/test_baked_torch.py
	python3 tests/test_torch_assets.py
	python3 tests/test_adventure_medicine_export.py
	python3 tests/test_town_wallet_export.py
	python3 tests/test_adventure_mission_events_export.py
	python3 tests/test_hare_pounce_export.py
	python3 tests/test_camera_environment_export.py
	python3 tests/test_export_inventory.py
	python3 tests/test_harvest_export.py
	python3 tests/test_vegetation_export.py
	python3 tests/test_build_history_export.py
	python3 tests/test_town_trade_export.py
	python3 tests/test_town_interaction_export.py
	python3 tests/test_adventure_dialogue_export.py
	python3 tests/test_adventure_supplier.py
	python3 tests/test_adventure_welcome_assets.py
	python3 tests/test_terrain_assets.py
	python3 tests/test_content.py
	python3 tests/test_reference_launcher.py
	python3 tests/test_reference_input.py
	python3 tests/test_rebuild_input.py
	python3 tests/test_reference_assets.py
	python3 tests/test_reference_actor.py
	python3 tests/test_reference_hitboxes.py
	python3 tests/test_reference_knife_collision.py
	python3 tests/test_reference_player.py
	python3 tests/test_reference_pose.py
	python3 tests/test_reference_animator_bake.py
	python3 tests/test_directional_player.py
	python3 tests/test_running_player.py
	python3 tests/test_reference_root_motion.py
	python3 tests/test_reference_running_attack.py
	python3 tests/test_running_knife_motion_export.py
	python3 tests/test_reference_bake_analysis.py
	python3 tests/test_baked_knife.py
	python3 tests/test_sandbox_world_assets.py
	python3 tests/test_baked_axe.py
	python3 tests/test_baked_shovel.py
	python3 tests/test_baked_pickaxe.py
	python3 tests/test_player_defence.py
	python3 tests/test_reference_pose_analysis.py
	python3 tests/test_training_gathering_export.py
	python3 tests/test_training_gathering_assets.py
	python3 tests/test_training_terrain_export.py
	python3 tests/test_training_build_assets.py
	python3 tests/test_training_receiver_source.py
	python3 tests/test_training_npcs.py
	python3 tests/test_training_ui_assets.py
	python3 tests/test_training_ui_effects.py
	python3 tests/test_training_ui_text.py
	python3 tests/test_training_ui_input.py
	python3 tests/test_training_npc_analysis.py
	python3 tests/test_reference_npc_bake.py
sanitize: | $(BUILD)
	$(CC) $(CPPFLAGS) -std=c11 -g -O1 -Wall -Wextra -Wpedantic -fno-omit-frame-pointer -fsanitize=address,undefined $(TESTS) $(CORE) -o $(BUILD)/test_sanitize $(LZ4_LIBS) -lm
	ASAN_OPTIONS='$(ASAN_OPTIONS)' ./$(BUILD)/test_sanitize
content:
	python3 tools/export_content.py
	python3 tools/export_mc_tables.py
check-content:
	python3 tests/test_shop_details_source.py
	python3 tools/export_camera_geometry.py --check
	python3 tools/export_adventure_mission_events.py --check
	python3 tools/export_hare_pounce.py --check
	python3 tools/export_camera_environment.py --check
	python3 tools/export_inventory.py --check
	python3 tools/export_harvest.py --check
	python3 tools/export_vegetation_source.py --check
	python3 tools/export_build_history.py --check
	python3 tools/export_town_trade_prices.py --check
	python3 tools/export_revival.py --check
	python3 tools/export_stamina_recovery.py --check
	python3 tools/export_passive_recovery.py --check
	python3 tools/export_hare_skill.py --check
	python3 tools/export_adventure_hunt_encounter.py --check
	python3 tools/export_adventure_hunt.py --check
	python3 tools/export_starter_outfit.py --check
	python3 tools/export_adventure_town_defense.py --check
	python3 tools/export_adventure_medicine.py --check
	python3 tools/export_town_wallets.py --check
	python3 tools/export_town_interaction.py --check
	python3 tools/export_adventure_dialogue.py --check
	python3 tools/export_content.py --check
	python3 tools/export_mc_tables.py --check
	python3 tools/export_training_terrain.py --check
	python3 tools/export_training_ui_text.py --check
run: all
	./$(BUILD)/planet_explorers
clean:
	rm -rf $(BUILD)

$(BUILD)/smoke_fixture: tests/smoke_fixture.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/smoke_fixture.c $(CORE) -o $@ $(LZ4_LIBS) -lm
smoke: all $(BUILD)/smoke_fixture
	./$(BUILD)/smoke_fixture $(BUILD)/smoke-fixture.pesave
	./$(BUILD)/planet_explorers --load $(BUILD)/smoke-fixture.pesave --smoke 90 --smoke-panel inventory --screenshot $(BUILD)/smoke-inventory.png
	./$(BUILD)/planet_explorers --load $(BUILD)/smoke-fixture.pesave --terrain '../CustomGames/Dragon Desert/Worlds/Dragon Desert/map_x0_y0.voxelform' --smoke 90 --screenshot $(BUILD)/smoke-original.png

cli-test: all
	python3 tests/test_cli.py

.PHONY: sandbox-ui-test sandbox-actor-test menu-smoke
.PHONY: actor-blend-test
.PHONY: training-asset-clocks-test
.PHONY: sandbox-equipment-host-test
$(BUILD)/test_sandbox_equipment_host: tests/test_sandbox_equipment_host.c src/pe_sandbox_view.c src/pe_revival.c src/pe_inventory.c src/pe_game.c src/pe_sandbox_actions.c src/pe_stamina_recovery.c src/pe_outfit.c src/pe_world.c src/pe_combat.c src/pe_sword.c generated/pe_content.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_equipment_host.c src/pe_revival.c src/pe_inventory.c src/pe_game.c src/pe_sandbox_actions.c src/pe_stamina_recovery.c src/pe_outfit.c src/pe_world.c src/pe_combat.c src/pe_sword.c generated/pe_content.c -Wl,--gc-sections -o $@ -lm
sandbox-equipment-host-test: $(BUILD)/test_sandbox_equipment_host
	./$(BUILD)/test_sandbox_equipment_host

$(BUILD)/test_training_asset_clocks: tests/test_training_asset_clocks.c src/pe_training_view.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_training_asset_clocks.c -Wl,--gc-sections -o $@ -lm
training-asset-clocks-test: $(BUILD)/test_training_asset_clocks
	./$(BUILD)/test_training_asset_clocks

$(BUILD)/test_actor_blend_view: tests/test_actor_blend_view.c src/pe_actor.c src/pe_actor_view.c src/pe_actor_scene_view.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_actor_blend_view.c src/pe_actor.c src/pe_actor_view.c src/pe_actor_scene_view.c -Wl,--gc-sections -o $@ -lm
actor-blend-test: $(BUILD)/test_actor_blend_view
	./$(BUILD)/test_actor_blend_view
$(BUILD)/test_sandbox_actor: tests/test_sandbox_actor.c src/pe_inventory.c generated/pe_content.c src/pe_sandbox_actor.c src/pe_sandbox_actor.h src/pe_player_locomotion.c src/pe_combat.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_actor.c src/pe_inventory.c generated/pe_content.c src/pe_sandbox_actor.c src/pe_player_locomotion.c src/pe_combat.c -o $@ -lm
sandbox-actor-test: $(BUILD)/test_sandbox_actor
	./$(BUILD)/test_sandbox_actor
$(BUILD)/test_sandbox_ui: tests/test_sandbox_ui.c src/pe_sandbox_ui_input.c src/pe_sandbox_ui.h tests/adventure_test_helpers.h $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_ui.c src/pe_sandbox_ui_input.c $(CORE) -o $@ $(LZ4_LIBS) -lm
sandbox-ui-test: $(BUILD)/test_sandbox_ui
	./$(BUILD)/test_sandbox_ui
menu-smoke: all
	./$(BUILD)/planet_explorers --main-menu --smoke 30 --screenshot $(BUILD)/smoke-main-menu.png

.PHONY: adventure-smoke
$(BUILD)/adventure_fixture: tests/adventure_fixture.c tests/adventure_test_helpers.h $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_fixture.c $(CORE) -o $@ $(LZ4_LIBS) -lm
adventure-smoke: all $(BUILD)/adventure_fixture
	./$(BUILD)/adventure_fixture $(BUILD)/adventure-fixture.pesave $(BUILD)/adventure-airborne.pesave
	./$(BUILD)/planet_explorers --load $(BUILD)/adventure-fixture.pesave --smoke 90 --smoke-panel character --screenshot $(BUILD)/smoke-adventure-character.png
	./$(BUILD)/planet_explorers --load $(BUILD)/adventure-fixture.pesave --smoke 90 --smoke-panel crafting --screenshot $(BUILD)/smoke-adventure-crafting.png
	./$(BUILD)/planet_explorers --load $(BUILD)/adventure-airborne.pesave --smoke 90 --screenshot $(BUILD)/smoke-adventure-airborne.png

training-smoke: all
	./$(BUILD)/planet_explorers --training-room $(BUILD)/reference-assets --smoke 90 --screenshot $(BUILD)/smoke-training.png

training-sequence-smoke: all
	./$(BUILD)/planet_explorers --training-room $(BUILD)/reference-assets --smoke 1500 --screenshot $(BUILD)/smoke-training-sequence.png

training-npcs-smoke: all
	./$(BUILD)/planet_explorers --training-room $(BUILD)/reference-assets --smoke 1750 --screenshot $(BUILD)/smoke-training-npcs.png

$(BUILD)/test_reference_validation: tests/test_reference_validation.c src/pe_reference.c src/pe_reference.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_reference_validation.c src/pe_reference.c -o $@ $(RAYLIB_LIBS) -lm
asset-test: $(BUILD)/test_reference_validation
	./$(BUILD)/test_reference_validation

# Raylib types only: test_ui_assets supplies headless GPU/decoder stubs.
$(BUILD)/test_ui_assets: tests/test_ui_assets.c src/pe_ui_assets.c src/pe_ui_assets.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_ui_assets.c src/pe_ui_assets.c -o $@ -lm
ui-asset-test: $(BUILD)/test_ui_assets
	./$(BUILD)/test_ui_assets $(BUILD)/reference-assets/ui

actor-smoke: all
	./$(BUILD)/planet_explorers --reference-actor $(BUILD)/reference-assets/hare.peac --smoke 630 --screenshot $(BUILD)/smoke-hare.png

$(BUILD)/reference_melee_check: tests/reference_melee_check.c src/pe_actor.c src/pe_hitbox.c src/pe_combat.c src/pe_capsule_history.c src/pe_knife_contact.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/reference_melee_check.c src/pe_actor.c src/pe_hitbox.c src/pe_combat.c src/pe_capsule_history.c src/pe_knife_contact.c -o $@ -lm

training-combat-smoke: all
	./$(BUILD)/planet_explorers --training-room $(BUILD)/reference-assets --smoke 3600 --screenshot $(BUILD)/smoke-training-combat.png

training-gathering-smoke: all
	./$(BUILD)/planet_explorers --training-room $(BUILD)/reference-assets --smoke 6000 --screenshot $(BUILD)/smoke-training-gathering.png

training-crafting-smoke: all
	./$(BUILD)/planet_explorers --training-room $(BUILD)/reference-assets --smoke 9000 --screenshot $(BUILD)/smoke-training-crafting.png

training-mining-smoke: all
	./$(BUILD)/planet_explorers --training-room $(BUILD)/reference-assets --smoke 12000 --screenshot $(BUILD)/smoke-training-mining.png

training-building-smoke: all
	./$(BUILD)/planet_explorers --training-room $(BUILD)/reference-assets --smoke 15000 --screenshot $(BUILD)/smoke-training-building.png

.PHONY: sandbox-world-test
$(BUILD)/test_sandbox_world_view: tests/test_sandbox_world_view.c src/pe_sandbox_world_view.c src/pe_sandbox_world_view.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_world_view.c src/pe_sandbox_world_view.c -o $@ -lm
sandbox-world-test: $(BUILD)/test_sandbox_world_view
	./$(BUILD)/test_sandbox_world_view

$(BUILD)/sandbox_action_fixture: tests/sandbox_action_fixture.c tests/adventure_test_helpers.h $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/sandbox_action_fixture.c $(CORE) -o $@ $(LZ4_LIBS) -lm
.PHONY: sandbox-action-fixtures
sandbox-action-fixtures: $(BUILD)/sandbox_action_fixture
	./$(BUILD)/sandbox_action_fixture $(BUILD)/sandbox-dig-action.pesave $(BUILD)/sandbox-fell-action.pesave

.PHONY: sandbox-camera-test
$(BUILD)/test_sandbox_camera: tests/test_sandbox_camera.c src/pe_sandbox_camera.c src/pe_sandbox_camera.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_sandbox_camera.c src/pe_sandbox_camera.c -o $@ -lm
sandbox-camera-test: $(BUILD)/test_sandbox_camera
	./$(BUILD)/test_sandbox_camera

# Test-only funded saves for physical town UI and Annot completion checks.
$(BUILD)/adventure_town_fixture: tests/adventure_town_fixture.c tests/adventure_test_helpers.h $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_town_fixture.c $(CORE) -o $@ $(LZ4_LIBS) -lm
.PHONY: adventure-town-fixtures adventure-shop-fixture adventure-annot-fixture
adventure-shop-fixture: $(BUILD)/adventure_town_fixture
	./$(BUILD)/adventure_town_fixture --shop $(BUILD)/adventure-shop-fixture.pesave
adventure-annot-fixture: $(BUILD)/adventure_town_fixture
	./$(BUILD)/adventure_town_fixture --annot $(BUILD)/adventure-annot-fixture.pesave
adventure-town-fixtures: adventure-shop-fixture adventure-annot-fixture

$(BUILD)/adventure_supplier_fixture: tests/adventure_supplier_fixture.c tests/adventure_test_helpers.h $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_supplier_fixture.c $(CORE) -o $@ $(LZ4_LIBS) -lm

.PHONY: terrain-material-test
$(BUILD)/test_terrain_material: tests/test_terrain_material.c src/pe_terrain_material.c src/pe_terrain_material.h generated/pe_terrain_materials.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_terrain_material.c src/pe_terrain_material.c -o $@ -lm
$(BUILD)/test_terrain_material_view: tests/test_terrain_material_view.c src/pe_terrain_material_view.c src/pe_terrain_material_view.h generated/pe_terrain_materials.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_terrain_material_view.c src/pe_terrain_material_view.c -o $@ -lm
terrain-material-test: $(BUILD)/test_terrain_material $(BUILD)/test_terrain_material_view
	./$(BUILD)/test_terrain_material
	./$(BUILD)/test_terrain_material_view

# Optional offline UnityPy/Pillow environment; never needed by the C runtime.
ASSET_PYTHON ?= python3

# Modern sword/shield metadata is a foundation, not enabled runtime equipment.
# Binary source-prefab validation requires the optional UnityPy environment.
.PHONY: starter-equipment-check
starter-equipment-check:
	$(ASSET_PYTHON) tools/export_starter_sword_shield.py --check
	$(ASSET_PYTHON) tests/test_starter_sword_shield_export.py
.PHONY: terrain-asset-test terrain-assets-check
terrain-asset-test:
	$(ASSET_PYTHON) tests/test_terrain_assets.py
terrain-assets-check:
	$(ASSET_PYTHON) tools/export_terrain_assets.py --check
$(BUILD)/terrain_material_fixture: tests/terrain_material_fixture.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/terrain_material_fixture.c $(CORE) -o $@ $(LZ4_LIBS) -lm
$(BUILD)/adventure_route_fixture: tests/adventure_route_fixture.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_route_fixture.c $(CORE) -o $@ $(LZ4_LIBS) -lm

# Existing physically reached checkpoint; no fixture grants or position edits.
$(BUILD)/adventure_medicine_fixture: tests/adventure_medicine_fixture.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_medicine_fixture.c $(CORE) -o $@ $(LZ4_LIBS) -lm
.PHONY: adventure-medicine-fixture
adventure-medicine-fixture: $(BUILD)/adventure_medicine_fixture
	./$(BUILD)/adventure_medicine_fixture --prepare $(BUILD)/annot-route-graphical-final.pesave $(BUILD)/medicine-ready.pesave > $(BUILD)/medicine-headless.json

.PHONY: hare-perception-assets-check
hare-perception-assets-check:
	$(ASSET_PYTHON) tools/export_hare_perception.py --check
	$(ASSET_PYTHON) tests/test_hare_perception_export.py

.PHONY: town-defense-clock-test
$(BUILD)/test_town_defense_clock: tests/test_town_defense_clock.c src/pe_town_defense_clock.c src/pe_town_defense_clock.h generated/pe_adventure_town_defense_source.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_town_defense_clock.c src/pe_town_defense_clock.c -o $@ -lm
town-defense-clock-test: $(BUILD)/test_town_defense_clock
	./$(BUILD)/test_town_defense_clock

.PHONY: hunt-consume-test
$(BUILD)/hunt_consume_host.inc: tests/test_hunt_consume_host.py src/pe_sandbox_view.c | $(BUILD)
	python3 tests/test_hunt_consume_host.py --output $@
$(BUILD)/test_hunt_consume: tests/test_hunt_consume.c $(BUILD)/hunt_consume_host.inc src/pe_sandbox_ui_input.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) -I$(BUILD) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_hunt_consume.c src/pe_sandbox_ui_input.c $(CORE) -o $@ $(LZ4_LIBS) -lm
hunt-consume-test: $(BUILD)/test_hunt_consume
	@./$(BUILD)/test_hunt_consume; result=$$?; if [ $$result -eq 77 ]; then echo "hunt consume: asset-dependent regression SKIPPED (not passed)"; elif [ $$result -ne 0 ]; then exit $$result; fi

.PHONY: sandbox-health-host-test
$(BUILD)/test_sandbox_health_host: tests/test_sandbox_health_host.c src/pe_sandbox_view.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_health_host.c $(CORE) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-health-host-test: $(BUILD)/test_sandbox_health_host
	./$(BUILD)/test_sandbox_health_host

.PHONY: sandbox-stamina-host-test
$(BUILD)/test_sandbox_stamina_host: tests/test_sandbox_stamina_host.c src/pe_sandbox_view.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_stamina_host.c $(CORE) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-stamina-host-test: $(BUILD)/test_sandbox_stamina_host
	./$(BUILD)/test_sandbox_stamina_host

# Source collider centers require offline UnityPy, never an engine launch.
.PHONY: capsule-history-source-check
capsule-history-source-check:
	python3 tools/export_capsule_history.py --check
	python3 tests/test_capsule_history_export.py
	$(ASSET_PYTHON) tools/export_player_collider_center.py --check
	$(ASSET_PYTHON) tools/export_hare_history_center.py --check
	$(ASSET_PYTHON) tests/test_player_collider_center.py
	$(ASSET_PYTHON) tests/test_hare_history_center_export.py

# Offline prefab/curve contracts; no engine initialization or desktop input.
.PHONY: attack-playback-source-check
attack-playback-source-check:
	$(ASSET_PYTHON) tools/export_attack_playback.py --check
	$(ASSET_PYTHON) tests/test_attack_playback_export.py

# Experimental offline retargeting arithmetic; not linked into runtime CORE.
.PHONY: humanoid-rotation-test
$(BUILD)/test_human_rotation_math: tests/test_human_rotation_math.c tools/human_rotation_math.c tools/human_rotation_math.h | $(BUILD)
	$(CC) $(CFLAGS) tests/test_human_rotation_math.c tools/human_rotation_math.c -o $@ -lm
humanoid-rotation-test: $(BUILD)/test_human_rotation_math
	./$(BUILD)/test_human_rotation_math
	python3 tests/test_humanoid_rotation_probe.py

.PHONY: sandbox-revival-host-test
$(BUILD)/test_sandbox_revival_host: tests/test_sandbox_revival_host.c src/pe_sandbox_view.c src/pe_sandbox_ui_input.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_revival_host.c src/pe_sandbox_ui_input.c $(CORE) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-revival-host-test: $(BUILD)/test_sandbox_revival_host
	./$(BUILD)/test_sandbox_revival_host

.PHONY: sword-policy-test
$(BUILD)/test_sword_policy: tests/test_sword_policy_main.c tests/test_sword_variant.c tests/test_sword_motion.c tests/test_running_attack_motion.c tests/test_sandbox_melee.c tests/test_running_attack_pipeline.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_sword_policy_main.c tests/test_sword_variant.c tests/test_sword_motion.c tests/test_running_attack_motion.c tests/test_sandbox_melee.c tests/test_running_attack_pipeline.c $(CORE) -o $@ $(LZ4_LIBS) -lm
sword-policy-test: $(BUILD)/test_sword_policy
	./$(BUILD)/test_sword_policy

# These prefab checks require UnityPy, like the existing animation exporters.
UNITY_PYTHON ?= $(if $(wildcard build/unity-tools/bin/python),build/unity-tools/bin/python,python3)
.PHONY: sword-source-check
sword-source-check:
	$(UNITY_PYTHON) tools/export_sword_variant.py --check
	$(UNITY_PYTHON) tests/test_sword_variant_export.py

.PHONY: sword-walking-test sandbox-sword-motion-host-test
$(BUILD)/test_sword_walking: tests/test_sword_walking_main.c tests/test_sword_root_motion.c tests/test_sword_walking.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_sword_walking_main.c tests/test_sword_root_motion.c tests/test_sword_walking.c $(CORE) -o $@ $(LZ4_LIBS) -lm
sword-walking-test: $(BUILD)/test_sword_walking
	./$(BUILD)/test_sword_walking
	python3 tests/test_player_root_channels.py
	python3 tests/test_sword_root_export.py
$(BUILD)/test_sandbox_sword_motion_host: tests/test_sandbox_sword_motion_host.c src/pe_sandbox_view.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_sword_motion_host.c $(CORE) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-sword-motion-host-test: $(BUILD)/test_sandbox_sword_motion_host
	./$(BUILD)/test_sandbox_sword_motion_host

.PHONY: town-wallet-test sandbox-wallet-host-test
$(BUILD)/test_town_wallet: tests/test_town_wallet_main.c tests/test_town_wallet.c tests/test_town_wallet_save.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_town_wallet_main.c tests/test_town_wallet.c tests/test_town_wallet_save.c $(CORE) -o $@ $(LZ4_LIBS) -lm
town-wallet-test: $(BUILD)/test_town_wallet
	./$(BUILD)/test_town_wallet
	python3 tests/test_town_wallet_export.py
$(BUILD)/test_sandbox_wallet_host: tests/test_sandbox_wallet_host.c src/pe_sandbox_view.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_wallet_host.c $(CORE) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-wallet-host-test: $(BUILD)/test_sandbox_wallet_host
	./$(BUILD)/test_sandbox_wallet_host

.PHONY: hunt-save-inspector hunt-save-inspector-test native-private-input-test
$(BUILD)/inspect_hunt_save: tools/inspect_hunt_save.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tools/inspect_hunt_save.c $(CORE) -o $@ $(LZ4_LIBS) -lm
hunt-save-inspector: $(BUILD)/inspect_hunt_save

hunt-save-inspector-test:
	python3 tests/test_hunt_save_inspector.py

native-private-input-test:
	python3 tests/test_native_private_input.py

# Pure/subprocess tool checks only; ordinary graphical acceptance is opt-in.
test: hunt-save-inspector-test native-private-input-test

.PHONY: town-trade-test
$(BUILD)/test_town_trade: tests/test_town_trade_main.c tests/test_town_trade.c tests/test_town_trade_save.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_town_trade_main.c tests/test_town_trade.c tests/test_town_trade_save.c $(CORE) -o $@ $(LZ4_LIBS) -lm
town-trade-test: $(BUILD)/test_town_trade
	./$(BUILD)/test_town_trade
	python3 tests/test_town_trade_export.py

.PHONY: sandbox-shop-trade-ui-test
$(BUILD)/test_sandbox_shop_trade_ui: tests/test_sandbox_shop_trade_ui.c src/pe_sandbox_ui_input.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_shop_trade_ui.c src/pe_sandbox_ui_input.c $(CORE) -o $@ $(LZ4_LIBS) -lm
sandbox-shop-trade-ui-test: $(BUILD)/test_sandbox_shop_trade_ui
	./$(BUILD)/test_sandbox_shop_trade_ui

.PHONY: build-history-test sandbox-build-history-host-test
$(BUILD)/test_build_history: tests/test_build_history_main.c tests/test_build_history.c tests/test_build_history_save.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_build_history_main.c tests/test_build_history.c tests/test_build_history_save.c $(CORE) -o $@ $(LZ4_LIBS) -lm
build-history-test: $(BUILD)/test_build_history
	./$(BUILD)/test_build_history
	python3 tests/test_build_history_export.py
$(BUILD)/test_sandbox_build_history_host: tests/test_sandbox_build_history_host.c src/pe_sandbox_view.c src/pe_sandbox_ui_input.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_build_history_host.c src/pe_sandbox_ui_input.c $(CORE) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-build-history-host-test: $(BUILD)/test_sandbox_build_history_host
	./$(BUILD)/test_sandbox_build_history_host

.PHONY: vegetation-test sandbox-vegetation-host-test
$(BUILD)/test_vegetation: tests/test_vegetation_main.c tests/test_vegetation.c tests/test_vegetation_save.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_vegetation_main.c tests/test_vegetation.c tests/test_vegetation_save.c $(CORE) -o $@ $(LZ4_LIBS) -lm
vegetation-test: $(BUILD)/test_vegetation
	./$(BUILD)/test_vegetation
$(BUILD)/vegetation_fixture: tests/vegetation_fixture.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/vegetation_fixture.c $(CORE) -o $@ $(LZ4_LIBS) -lm
$(BUILD)/test_sandbox_vegetation_host: tests/test_sandbox_vegetation_host.c src/pe_sandbox_view.c src/pe_sandbox_ui_input.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_vegetation_host.c src/pe_sandbox_ui_input.c $(CORE) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-vegetation-host-test: $(BUILD)/test_sandbox_vegetation_host
	./$(BUILD)/test_sandbox_vegetation_host
.PHONY: vegetation-view-test
$(BUILD)/test_vegetation_view: tests/test_vegetation_view.c src/pe_vegetation_view.c src/pe_reference.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_vegetation_view.c src/pe_vegetation_view.c src/pe_reference.c $(CORE) -Wl,--gc-sections -o $@ $(RAYLIB_LIBS) $(LZ4_LIBS) -lm
vegetation-view-test: $(BUILD)/test_vegetation_view
	./$(BUILD)/test_vegetation_view; result=$$?; test $$result -eq 0 -o $$result -eq 77

.PHONY: sandbox-inventory-ui-test inventory-test
$(BUILD)/test_inventory_ui: tests/test_inventory_ui.c src/pe_sandbox_ui_input.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_inventory_ui.c src/pe_sandbox_ui_input.c $(CORE) -o $@ $(LZ4_LIBS) -lm
sandbox-inventory-ui-test: $(BUILD)/test_inventory_ui
	./$(BUILD)/test_inventory_ui
$(BUILD)/test_inventory: tests/test_inventory_main.c tests/test_inventory.c tests/test_inventory_save.c tests/test_harvest.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_inventory_main.c tests/test_inventory.c tests/test_inventory_save.c tests/test_harvest.c $(CORE) -o $@ $(LZ4_LIBS) -lm
inventory-test: $(BUILD)/test_inventory
	./$(BUILD)/test_inventory

.PHONY: camera-environment-test sandbox-camera-environment-host-test town-defense-spawn-test
$(BUILD)/test_camera_environment: tests/test_camera_environment.c src/pe_sandbox_camera.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_camera_environment.c src/pe_sandbox_camera.c -o $@ -lm
camera-environment-test: $(BUILD)/test_camera_environment
	./$(BUILD)/test_camera_environment
$(BUILD)/test_sandbox_camera_environment_host: tests/test_sandbox_camera_environment_host.c src/pe_sandbox_view.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_camera_environment_host.c $(CORE) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-camera-environment-host-test: $(BUILD)/test_sandbox_camera_environment_host
	./$(BUILD)/test_sandbox_camera_environment_host
$(BUILD)/test_town_defense_spawn: tests/test_town_defense_spawn.c src/pe_town_defense_spawn.c src/pe_town_defense_spawn.h src/pe_town_defense_clock.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_town_defense_spawn.c src/pe_town_defense_spawn.c src/pe_town_defense_clock.c -o $@ -lm
town-defense-spawn-test: $(BUILD)/test_town_defense_spawn
	./$(BUILD)/test_town_defense_spawn

.PHONY: hare-pounce-assets-test
$(BUILD)/test_hare_pounce_assets: tests/test_hare_pounce_assets.c src/pe_actor.c src/pe_hitbox.c src/pe_hare_pounce.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_hare_pounce_assets.c src/pe_actor.c src/pe_hitbox.c src/pe_hare_pounce.c -o $@ -lm
hare-pounce-assets-test: $(BUILD)/test_hare_pounce_assets
	./$(BUILD)/test_hare_pounce_assets $(BUILD)/reference-assets/hare.peac $(BUILD)/reference-assets/hare-pounce-staged-20261007/hare.peac $(BUILD)/reference-assets/hare.pehb

.PHONY: adventure-mission-events-test
$(BUILD)/test_adventure_mission_events: tests/test_adventure_mission_events.c src/pe_adventure_mission_events.c src/pe_adventure_mission_events.h generated/pe_adventure_mission_events_source.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_adventure_mission_events.c src/pe_adventure_mission_events.c -o $@
adventure-mission-events-test: $(BUILD)/test_adventure_mission_events
	./$(BUILD)/test_adventure_mission_events

.PHONY: inventory-discard-test
$(BUILD)/test_inventory_discard: tests/test_inventory_discard.c src/pe_sandbox_ui_input.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_inventory_discard.c src/pe_sandbox_ui_input.c $(CORE) -o $@ $(LZ4_LIBS) -lm
inventory-discard-test: $(BUILD)/test_inventory_discard
	./$(BUILD)/test_inventory_discard

.PHONY: camp-repair-test camp-repair-source-check
$(BUILD)/test_camp_repair: tests/test_camp_repair.c src/pe_camp_repair.c src/pe_camp_repair.h src/pe_inventory.c generated/pe_content.c generated/pe_camp_repair_source.h $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DPE_CAMP_REPAIR_TEST_MAIN tests/test_camp_repair.c src/pe_camp_repair.c src/pe_inventory.c generated/pe_content.c -o $@ -lm
camp-repair-test: $(BUILD)/test_camp_repair
	./$(BUILD)/test_camp_repair
camp-repair-source-check:
	$(UNITY_PYTHON) tools/export_camp_repair.py --check
	$(UNITY_PYTHON) tests/test_camp_repair_export.py

.PHONY: camp-repair-save-test camp-repair-interaction-test sandbox-repair-ui-test sandbox-repair-host-test
$(BUILD)/test_camp_repair_save: tests/test_camp_repair_save.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_CAMP_REPAIR_SAVE_TEST_MAIN tests/test_camp_repair_save.c $(CORE) -o $@ $(LZ4_LIBS) -lm
camp-repair-save-test: $(BUILD)/test_camp_repair_save
	./$(BUILD)/test_camp_repair_save
$(BUILD)/test_camp_repair_interaction: tests/test_camp_repair_interaction.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_camp_repair_interaction.c $(CORE) -o $@ $(LZ4_LIBS) -lm
camp-repair-interaction-test: $(BUILD)/test_camp_repair_interaction
	cd .. && rebuild/$(BUILD)/test_camp_repair_interaction --installed
$(BUILD)/test_sandbox_repair_host: tests/test_sandbox_repair_host.c src/pe_sandbox_view.c src/pe_sandbox_ui_input.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_repair_host.c src/pe_sandbox_ui_input.c $(CORE) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-repair-host-test: $(BUILD)/test_sandbox_repair_host
	./$(BUILD)/test_sandbox_repair_host

$(BUILD)/test_sandbox_repair_ui: tests/test_sandbox_repair_ui.c src/pe_sandbox_ui_input.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_repair_ui.c src/pe_sandbox_ui_input.c $(CORE) -o $@ $(LZ4_LIBS) -lm
sandbox-repair-ui-test: $(BUILD)/test_sandbox_repair_ui
	./$(BUILD)/test_sandbox_repair_ui

.PHONY: shop-quantity-details-ui-test
$(BUILD)/test_shop_quantity_details_ui: tests/test_shop_quantity_details_ui.c src/pe_sandbox_ui_input.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_shop_quantity_details_ui.c src/pe_sandbox_ui_input.c $(CORE) -o $@ $(LZ4_LIBS) -lm
shop-quantity-details-ui-test: $(BUILD)/test_shop_quantity_details_ui
	./$(BUILD)/test_shop_quantity_details_ui

.PHONY: bandage-test camera-geometry-test camera-geometry-source-check
$(BUILD)/test_bandage: tests/test_bandage.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_BANDAGE_TEST_MAIN tests/test_bandage.c $(CORE) -o $@ $(LZ4_LIBS) -lm
bandage-test: $(BUILD)/test_bandage
	./$(BUILD)/test_bandage
$(BUILD)/test_camera_geometry: tests/test_camera_geometry.c src/pe_camera_geometry.c src/pe_camera_geometry.h generated/pe_camera_geometry_source.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_camera_geometry.c src/pe_camera_geometry.c -o $@ -lm
camera-geometry-test: $(BUILD)/test_camera_geometry
	./$(BUILD)/test_camera_geometry
camera-geometry-source-check:
	python3 tools/export_camera_geometry.py --check
	python3 tests/test_camera_geometry_export.py

# Optional staged rendering on a private Xvfb display; never part of make test.
# Requires the locally extracted Xvfb described in SHOP_QUANTITY_AND_ITEM_DETAILS.
.PHONY: shop-details-render-capture
$(BUILD)/render_shop_details_ui: tests/render_shop_details_ui.c $(UI_SHARED) src/pe_sandbox_ui.c src/pe_sandbox_ui_input.c src/pe_ui_assets.c src/pe_training_ui_shader.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LZ4_CFLAGS) tests/render_shop_details_ui.c $(UI_SHARED) src/pe_sandbox_ui.c src/pe_sandbox_ui_input.c src/pe_ui_assets.c src/pe_training_ui_shader.c $(CORE) -o $@ $(RAYLIB_LIBS) $(LZ4_LIBS) -lm
shop-details-render-capture: $(BUILD)/render_shop_details_ui
	python3 tests/run_private_ui_capture.py

.PHONY: ui-shared-test
$(BUILD)/test_ui_hud: tests/test_ui_hud.c src/pe_ui_draw.c src/pe_ui_hud.c src/pe_training_ui_layout.c generated/pe_content.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_ui_hud.c src/pe_ui_draw.c src/pe_ui_hud.c src/pe_training_ui_layout.c generated/pe_content.c -o $@ -lm
ui-shared-test: $(BUILD)/test_ui_hud
	./$(BUILD)/test_ui_hud
	python3 tests/test_ui_unification.py
test: ui-shared-test

.PHONY: ui-shared-render-capture
UI_RENDER_HOSTS = src/pe_ui_settings.c src/pe_training_ui.c src/pe_training_ui_minimap.c src/pe_training_ui_replicator.c src/pe_training_ui_mission.c src/pe_sandbox_ui.c src/pe_sandbox_ui_input.c src/pe_ui_assets.c src/pe_training_ui_shader.c
$(BUILD)/render_shared_ui: tests/render_shared_ui.c $(UI_SHARED) $(UI_RENDER_HOSTS) $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LZ4_CFLAGS) tests/render_shared_ui.c $(UI_SHARED) $(UI_RENDER_HOSTS) $(CORE) -o $@ $(RAYLIB_LIBS) $(LZ4_LIBS) -lm
ui-shared-render-capture: $(BUILD)/render_shared_ui
	python3 tests/run_private_ui_capture.py --shared

.PHONY: conversation-ui-test
$(BUILD)/test_conversation_ui: tests/test_conversation_ui.c src/pe_sandbox_ui_input.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LZ4_CFLAGS) tests/test_conversation_ui.c src/pe_sandbox_ui_input.c $(CORE) -o $@ $(LZ4_LIBS) -lm
conversation-ui-test: $(BUILD)/test_conversation_ui
	./$(BUILD)/test_conversation_ui
test: conversation-ui-test

.PHONY: conversation-render-test
$(BUILD)/test_ui_history: tests/test_ui_history.c src/pe_ui_history.c src/pe_ui_draw.c src/pe_training_ui_layout.c src/pe_conversation_history.c generated/pe_content.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_ui_history.c src/pe_ui_history.c src/pe_ui_draw.c src/pe_training_ui_layout.c src/pe_conversation_history.c generated/pe_content.c -o $@ -lm
conversation-render-test: $(BUILD)/test_ui_history
	./$(BUILD)/test_ui_history
	python3 tests/test_conversation_history_source.py
test: conversation-render-test

.PHONY: camera-projection-test
$(BUILD)/test_camera_projection: tests/test_camera_projection.c src/pe_camera_projection.c src/pe_camera_projection.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_camera_projection.c src/pe_camera_projection.c -o $@ -lm
camera-projection-test: $(BUILD)/test_camera_projection
test: camera-projection-test

$(BUILD)/test_camera_projection_raylib: tests/test_camera_projection_raylib.c src/pe_camera_projection.c src/pe_camera_projection.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_camera_projection_raylib.c src/pe_camera_projection.c -o $@ $(RAYLIB_LIBS) -lm
camera-projection-test: $(BUILD)/test_camera_projection_raylib
	./$(BUILD)/test_camera_projection
	./$(BUILD)/test_camera_projection_raylib

.PHONY: camera-query-test camera-query-source-check
CAMERA_QUERY_TEST_CORE = src/pe_camera_query.c src/pe_camera_geometry.c src/pe_sandbox_collision.c src/pe_scene_collision.c src/pe_world.c src/pe_mesh.c src/pe_block.c src/pe_tree_collision.c generated/pe_content.c
$(BUILD)/test_camera_query: tests/test_camera_query.c $(CAMERA_QUERY_TEST_CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_camera_query.c $(CAMERA_QUERY_TEST_CORE) -o $@ -lm
camera-query-test: $(BUILD)/test_camera_query
	./$(BUILD)/test_camera_query
camera-query-source-check:
	$(UNITY_PYTHON) tools/export_camera_query.py --check
	$(UNITY_PYTHON) tests/test_camera_query_export.py
test: camera-query-test
check-content: camera-query-source-check

.PHONY: sandbox-blade-facing-test
$(BUILD)/test_sandbox_blade_facing: tests/test_sandbox_blade_facing.c src/pe_sandbox_view.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_blade_facing.c $(CORE) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-blade-facing-test: $(BUILD)/test_sandbox_blade_facing
	./$(BUILD)/test_sandbox_blade_facing
test: sandbox-blade-facing-test

.PHONY: hit-reaction-test hit-reaction-source-check hunt-targeting-source-check
$(BUILD)/test_hit_reaction: tests/test_hit_reaction.c src/pe_hit_reaction.c src/pe_hit_reaction.h generated/pe_hit_reaction_source.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_hit_reaction.c src/pe_hit_reaction.c -o $@ -lm
hit-reaction-test: $(BUILD)/test_hit_reaction
	./$(BUILD)/test_hit_reaction
hit-reaction-source-check:
	$(UNITY_PYTHON) tools/export_hit_reaction.py --check
	$(UNITY_PYTHON) tests/test_hit_reaction_export.py
hunt-targeting-source-check:
	$(UNITY_PYTHON) tools/export_hunt_targeting.py --check
	$(UNITY_PYTHON) tests/test_hunt_targeting_export.py
test: hit-reaction-test
check-content: hit-reaction-source-check hunt-targeting-source-check

.PHONY: preferences-test
$(BUILD)/test_preferences: tests/test_preferences.c src/pe_preferences.c src/pe_preferences.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_preferences.c src/pe_preferences.c -o $@
preferences-test: $(BUILD)/test_preferences
	./$(BUILD)/test_preferences
test: preferences-test

.PHONY: app-menu-settings-test
$(BUILD)/test_app_menu_settings: tests/test_app_menu_settings.c src/pe_app_menu.c src/pe_ui_settings.c src/pe_preferences.c src/pe_ui_draw.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_app_menu_settings.c src/pe_app_menu.c src/pe_ui_settings.c src/pe_preferences.c src/pe_ui_draw.c -Wl,--gc-sections -o $@ -lm
app-menu-settings-test: $(BUILD)/test_app_menu_settings
	./$(BUILD)/test_app_menu_settings
test: app-menu-settings-test

.PHONY: settings-render-capture
$(BUILD)/render_app_settings: tests/render_app_settings.c src/pe_app_menu.c src/pe_ui_settings.c src/pe_preferences.c src/pe_ui_assets.c src/pe_ui_draw.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/render_app_settings.c src/pe_preferences.c src/pe_ui_assets.c src/pe_ui_draw.c -Wl,--gc-sections -o $@ $(RAYLIB_LIBS) -lm
settings-render-capture: $(BUILD)/render_app_settings
	python3 tests/run_private_ui_capture.py --settings

.PHONY: hunt-loot-test hunt-loot-combat-test hunt-loot-source-check
$(BUILD)/test_hunt_loot: tests/test_hunt_loot.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_HUNT_LOOT_TEST_MAIN tests/test_hunt_loot.c $(CORE) -o $@ $(LZ4_LIBS) -lm
$(BUILD)/test_hunt_loot_save: tests/test_hunt_loot_save.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_HUNT_LOOT_SAVE_TEST_MAIN tests/test_hunt_loot_save.c $(CORE) -o $@ $(LZ4_LIBS) -lm
$(BUILD)/test_hunt_loot_combat: tests/test_hunt_loot_combat.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_HUNT_LOOT_COMBAT_TEST_MAIN tests/test_hunt_loot_combat.c $(CORE) -o $@ $(LZ4_LIBS) -lm
hunt-loot-test: $(BUILD)/test_hunt_loot $(BUILD)/test_hunt_loot_save $(BUILD)/test_hunt_loot_combat
	./$(BUILD)/test_hunt_loot
	./$(BUILD)/test_hunt_loot_save
	./$(BUILD)/test_hunt_loot_combat
hunt-loot-combat-test: $(BUILD)/test_hunt_loot_combat
	./$(BUILD)/test_hunt_loot_combat
hunt-loot-source-check:
	$(UNITY_PYTHON) tools/export_hunt_loot.py --check --verify-prefab
	$(UNITY_PYTHON) tests/test_hunt_loot_export.py
test: hunt-loot-combat-test
check-content: hunt-loot-source-check

.PHONY: hunt-loot-render-capture
$(BUILD)/render_hunt_loot: tests/render_hunt_loot.c src/pe_sandbox_view.c $(CORE) $(HEADERS) src/pe_ui_draw.c src/pe_ui_assets.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/render_hunt_loot.c src/pe_ui_draw.c src/pe_ui_assets.c $(CORE) -Wl,--gc-sections -o $@ $(RAYLIB_LIBS) $(LZ4_LIBS) -lm
hunt-loot-render-capture: $(BUILD)/render_hunt_loot
	python3 tests/run_private_ui_capture.py --loot

.PHONY: hunt-loot-markers-test
$(BUILD)/test_hunt_loot_markers: tests/test_hunt_loot_markers.c src/pe_sandbox_view.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_hunt_loot_markers.c $(CORE) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
hunt-loot-markers-test: $(BUILD)/test_hunt_loot_markers
	./$(BUILD)/test_hunt_loot_markers
test: hunt-loot-markers-test

.PHONY: debug-test sandbox-debug-host-test
$(BUILD)/test_debug: tests/test_debug.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_debug.c $(CORE) -o $@ $(LZ4_LIBS) -lm
debug-test: $(BUILD)/test_debug
	./$(BUILD)/test_debug
$(BUILD)/test_sandbox_debug_host: tests/test_sandbox_debug_host.c src/pe_sandbox_view.c $(CORE) $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_debug_host.c $(CORE) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-debug-host-test: $(BUILD)/test_sandbox_debug_host
	./$(BUILD)/test_sandbox_debug_host
test: debug-test sandbox-debug-host-test

.PHONY: actor-material-test actor-material-render-capture
actor-material-test:
	python3 -m unittest discover -s tests -p 'test_actor_material_export.py'
$(BUILD)/render_actor_materials: tests/render_actor_materials.c src/pe_actor_scene_view.c src/pe_actor_view.c src/pe_actor_scene_manifest.c src/pe_actor.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/render_actor_materials.c src/pe_actor_scene_view.c src/pe_actor_view.c src/pe_actor_scene_manifest.c src/pe_actor.c -Wl,--gc-sections -o $@ $(RAYLIB_LIBS) -lm
actor-material-render-capture: $(BUILD)/render_actor_materials
	python3 tests/run_private_ui_capture.py --materials
test: actor-material-test
