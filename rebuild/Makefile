CC ?= cc
PKG_CONFIG ?= pkg-config
CPPFLAGS += -Isrc -DPE_SOURCE_ROOT=\"$(abspath ..)\"
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
ASAN_OPTIONS ?= detect_leaks=1
CORE = src/pe_sword_variant.c src/pe_sword_motion.c src/pe_revival.c src/pe_attack_playback.c src/pe_shield_action.c src/pe_capsule_history.c src/pe_stamina_recovery.c src/pe_passive_recovery.c src/pe_sword.c src/pe_shield.c src/pe_outfit.c src/pe_running_attack_motion.c src/pe_root_motion.c src/pe_sword_targeting.c src/pe_town_defense_clock.c src/pe_hunt_combat.c src/pe_hare_skill.c src/pe_hunt_encounter.c src/pe_hare_perception.c src/pe_hare_damage.c src/pe_sandbox_melee.c src/pe_adventure_hunt.c src/pe_deployable.c src/pe_adventure_medicine.c src/pe_town_interaction.c src/pe_settlement.c src/pe_terrain_material.c src/pe_adventure_dialogue.c src/pe_adventure_npc.c src/pe_sandbox_camera.c src/pe_tree_collision.c src/pe_sandbox_actions.c src/pe_sandbox_collision.c src/pe_world.c src/pe_mesh.c src/pe_legacy.c src/pe_block.c src/pe_game.c src/pe_save.c src/pe_tutorial.c src/pe_scene_collision.c src/pe_training.c src/pe_player_locomotion.c src/pe_training_melee.c src/pe_training_tree_hit.c src/pe_training_dig.c src/pe_training_terrain.c src/pe_training_build.c src/pe_training_receiver.c src/pe_training_npc.c src/pe_training_ui_layout.c src/pe_training_gathering.c src/pe_training_text.c src/pe_actor.c src/pe_hare.c src/pe_combat.c src/pe_knife_contact.c src/pe_hare_attack.c src/pe_hitbox.c src/pe_actor_scene_manifest.c generated/pe_content.c
HEADERS = src/pe_stamina_recovery.h generated/pe_stamina_recovery_source.h src/pe_passive_recovery.h generated/pe_passive_recovery_source.h src/pe_root_motion.h src/pe_running_attack_motion.h src/pe_hare_skill.h generated/pe_hare_skill_source.h generated/pe_adventure_population.h src/pe_town_interaction.h generated/pe_town_offers.h src/pe_settlement.h generated/pe_adventure_settlement.h src/pe_terrain_material.h src/pe_terrain_material_view.h generated/pe_terrain_materials.h src/pe_adventure_dialogue.h generated/pe_adventure_dialogue_text.h generated/pe_adventure_npc_collider.h src/pe_adventure_npc.h src/pe_adventure_npc_view.h src/pe_sandbox_camera.h src/pe_tree_collision.h generated/pe_tree_collider.h src/pe_sandbox_actions.h src/pe_sandbox_world_view.h src/pe_sandbox_collision.h src/pe_sandbox_ui.h src/pe_app_menu.h src/pe_sandbox_actor.h src/pe_sandbox_view.h src/pe_world.h src/pe_game.h src/pe_content.h generated/pe_mc_tables.h src/pe_legacy.h src/pe_block.h src/pe_tutorial.h src/pe_scene_collision.h src/pe_training.h src/pe_player_locomotion.h src/pe_training_melee.h src/pe_training_tree_hit.h src/pe_training_dig.h src/pe_training_terrain.h src/pe_training_build.h src/pe_training_receiver.h src/pe_training_npc.h src/pe_training_ui_layout.h generated/pe_training_mound.h src/pe_training_gathering.h src/pe_training_text.h src/pe_actor.h src/pe_hare.h src/pe_combat.h src/pe_knife_contact.h src/pe_hare_attack.h src/pe_hitbox.h src/pe_actor_scene.h
FRONTEND = src/pe_deployable_view.c src/pe_terrain_material_view.c src/pe_adventure_npc_view.c src/pe_sandbox_world_view.c src/pe_app_menu.c src/pe_sandbox_view.c src/pe_sandbox_actor.c src/pe_sandbox_ui.c src/pe_sandbox_ui_input.c src/main.c src/pe_reference.c src/pe_training_view.c src/pe_training_terrain_view.c src/pe_training_npc_view.c src/pe_training_ui.c src/pe_training_ui_minimap.c src/pe_training_ui_shader.c src/pe_training_ui_replicator.c src/pe_training_ui_mission.c src/pe_ui_assets.c src/pe_actor_view.c src/pe_actor_scene.c src/pe_actor_scene_view.c
HEADERS += $(wildcard src/*.h generated/*.h)
TESTS = tests/test_sword_variant.c tests/test_sword_motion.c tests/test_revival.c tests/test_attack_playback.c tests/test_shield_action.c tests/test_capsule_history.c tests/test_stamina_recovery.c tests/test_stamina_save.c tests/test_passive_recovery.c tests/test_shield.c tests/test_shield_save.c tests/test_outfit.c tests/test_outfit_save.c tests/test_hunt_lifecycle.c tests/test_running_attack_pipeline.c tests/test_running_attack_motion.c tests/test_root_motion.c tests/test_sword_targeting.c tests/test_v13_save.c tests/test_hunt_return.c tests/test_v12_save.c tests/test_hunt_combat.c tests/test_hare_skill.c tests/test_hunt_encounter.c tests/test_hare_perception.c tests/test_v11_save.c tests/test_hare_route.c tests/test_hare_damage.c tests/test_sandbox_melee.c tests/test_v10_save.c tests/test_adventure_hunt.c tests/test_deployable.c tests/test_medicine_save.c tests/test_adventure_medicine.c tests/test_adventure_population.c tests/test_town_interaction.c tests/test_settlement.c tests/test_world_depth.c tests/test_adventure_welcome.c tests/test_adventure_dialogue.c tests/test_adventure_npc.c tests/test_sandbox_actions.c tests/test_sandbox_collision.c tests/test_main.c tests/test_world.c tests/test_mesh.c tests/test_legacy.c tests/test_block.c tests/test_game.c tests/test_tutorial.c tests/test_scene_collision.c tests/test_training.c tests/test_player_locomotion.c tests/test_training_melee.c tests/test_training_tree_hit.c tests/test_training_dig.c tests/test_training_terrain.c tests/test_training_build.c tests/test_training_receiver.c tests/test_training_npc.c tests/test_training_ui_layout.c tests/test_training_gathering.c tests/test_actor.c tests/test_hare.c tests/test_combat.c tests/test_knife_contact.c tests/test_hare_attack.c tests/test_hitbox.c tests/test_actor_scene.c
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
test: sandbox-revival-host-test humanoid-rotation-test sandbox-stamina-host-test sandbox-health-host-test sandbox-equipment-host-test hunt-consume-test training-asset-clocks-test actor-blend-test town-defense-clock-test terrain-material-test $(BUILD)/test_core ui-asset-test sandbox-ui-test sandbox-actor-test sandbox-world-test sandbox-camera-test
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
	python3 tools/export_revival.py --check
	python3 tools/export_stamina_recovery.py --check
	python3 tools/export_passive_recovery.py --check
	python3 tools/export_hare_skill.py --check
	python3 tools/export_adventure_hunt_encounter.py --check
	python3 tools/export_adventure_hunt.py --check
	python3 tools/export_starter_outfit.py --check
	python3 tools/export_adventure_town_defense.py --check
	python3 tools/export_adventure_medicine.py --check
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
$(BUILD)/test_sandbox_equipment_host: tests/test_sandbox_equipment_host.c src/pe_sandbox_view.c src/pe_revival.c src/pe_game.c src/pe_sandbox_actions.c src/pe_stamina_recovery.c src/pe_outfit.c src/pe_world.c src/pe_combat.c src/pe_sword.c generated/pe_content.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_equipment_host.c src/pe_revival.c src/pe_game.c src/pe_sandbox_actions.c src/pe_stamina_recovery.c src/pe_outfit.c src/pe_world.c src/pe_combat.c src/pe_sword.c generated/pe_content.c -Wl,--gc-sections -o $@ -lm
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
$(BUILD)/test_sandbox_actor: tests/test_sandbox_actor.c src/pe_sandbox_actor.c src/pe_sandbox_actor.h src/pe_player_locomotion.c src/pe_combat.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_actor.c src/pe_sandbox_actor.c src/pe_player_locomotion.c src/pe_combat.c -o $@ -lm
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
