CC ?= cc
PKG_CONFIG ?= pkg-config
CPPFLAGS += -Isrc
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
ASAN_OPTIONS ?= detect_leaks=1
CORE = src/pe_debug.c src/pe_hunt_loot.c src/pe_hunt_loot_combat.c src/pe_preferences.c src/pe_melee_facing.c src/pe_hunt_targeting.c src/pe_camera_query.c src/pe_camera_projection.c src/pe_conversation_history.c src/pe_ui_input.c src/pe_camera_geometry.c src/pe_camp_repair.c src/pe_camp_repair_interaction.c src/pe_hare_pounce.c src/pe_inventory.c src/pe_harvest.c src/pe_vegetation.c src/pe_build_history.c src/pe_town_trade.c src/pe_town_wallet.c src/pe_sword_root_motion.c src/pe_sword_variant.c src/pe_sword_motion.c src/pe_revival.c src/pe_attack_playback.c src/pe_shield_action.c src/pe_capsule_history.c src/pe_stamina_recovery.c src/pe_passive_recovery.c src/pe_sword.c src/pe_shield.c src/pe_outfit.c src/pe_running_attack_motion.c src/pe_root_motion.c src/pe_sword_targeting.c src/pe_town_defense_clock.c src/pe_hunt_combat.c src/pe_hare_skill.c src/pe_hunt_encounter.c src/pe_hare_perception.c src/pe_hare_damage.c src/pe_sandbox_melee.c src/pe_adventure_hunt.c src/pe_deployable.c src/pe_adventure_medicine.c src/pe_town_interaction.c src/pe_settlement.c src/pe_terrain_material.c src/pe_adventure_dialogue.c src/pe_adventure_npc.c src/pe_sandbox_camera.c src/pe_tree_collision.c src/pe_sandbox_actions.c src/pe_sandbox_collision.c src/pe_world.c src/pe_mesh.c src/pe_legacy.c src/pe_block.c src/pe_game.c src/pe_save.c src/pe_tutorial.c src/pe_scene_collision.c src/pe_training.c src/pe_player_locomotion.c src/pe_training_melee.c src/pe_training_tree_hit.c src/pe_training_dig.c src/pe_training_terrain.c src/pe_training_build.c src/pe_training_receiver.c src/pe_training_npc.c src/pe_training_ui_layout.c src/pe_training_gathering.c src/pe_training_text.c src/pe_actor.c src/pe_hare.c src/pe_combat.c src/pe_knife_contact.c src/pe_hare_attack.c src/pe_hitbox.c src/pe_actor_scene_manifest.c generated/pe_content.c
CORE += src/pe_repulsed.c
CORE += src/pe_human_phy.c
CORE += src/pe_whacked.c
CORE += src/pe_sword_motor.c
CORE += src/pe_halt.c
CORE += src/pe_hit_ik.c
CORE += src/pe_ragdoll_action.c
CORE += src/pe_animfollow.c
UI_SHARED = src/pe_ui_history.c src/pe_ui_draw.c src/pe_ui_hud.c
FRONTEND = src/pe_ui_settings.c $(UI_SHARED) src/pe_vegetation_view.c src/pe_deployable_view.c src/pe_terrain_material_view.c src/pe_adventure_npc_view.c src/pe_sandbox_world_view.c src/pe_app_menu.c src/pe_sandbox_view.c src/pe_sandbox_actor.c src/pe_sandbox_ui.c src/pe_sandbox_ui_input.c src/main.c src/pe_reference.c src/pe_training_view.c src/pe_training_terrain_view.c src/pe_training_npc_view.c src/pe_training_ui.c src/pe_training_ui_minimap.c src/pe_training_ui_shader.c src/pe_training_ui_replicator.c src/pe_training_ui_mission.c src/pe_ui_assets.c src/pe_actor_view.c src/pe_actor_scene.c src/pe_actor_scene_view.c
TESTS = tests/test_main.c tests/test_block.c $(shell sed -n 's|^PE_TEST(\(.*\))$$|tests/test_\1.c|p' tests/test_core_list.h)
BUILD = build
RAYLIB_CFLAGS := $(shell $(PKG_CONFIG) --cflags raylib)
LZ4_CFLAGS := $(shell $(PKG_CONFIG) --cflags liblz4)
LZ4_LIBS := $(shell $(PKG_CONFIG) --libs liblz4)
RAYLIB_LIBS := $(shell $(PKG_CONFIG) --libs raylib)

# Object build. CORE and the core tests compile once into per-profile object
# directories with compiler-generated header dependencies; every binary that
# previously recompiled $(CORE) from source now links $(CORE_LIB). Use
# `make -j$(JOBS)` (or `make check`) to compile objects in parallel.
.DEFAULT_GOAL := all
JOBS ?= $(shell nproc 2>/dev/null || echo 2)
OBJ = $(BUILD)/obj
CORE_OBJ_FLAGS = $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -ffunction-sections -fdata-sections
FRONTEND_OBJ_FLAGS = $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS)
SANITIZE_FLAGS = -std=c11 -g -O1 -Wall -Wextra -Wpedantic -fno-omit-frame-pointer -fsanitize=address,undefined
SANITIZE_OBJ_FLAGS = $(CPPFLAGS) $(LZ4_CFLAGS) $(SANITIZE_FLAGS)
CORE_OBJS = $(patsubst %.c,$(OBJ)/core/%.o,$(CORE))
TEST_OBJS = $(patsubst %.c,$(OBJ)/core/%.o,$(TESTS))
FRONTEND_OBJS = $(patsubst %.c,$(OBJ)/frontend/%.o,$(FRONTEND))
SANITIZE_OBJS = $(patsubst %.c,$(OBJ)/sanitize/%.o,$(TESTS) $(CORE))
CORE_LIB = $(OBJ)/core/libpe_core.a
# $(1) object directory, $(2) name of its flags variable. The .flags stamp is
# refreshed while parsing and changes only when the command line does, so a
# CFLAGS override rebuilds objects and a dry run reports only real staleness.
define OBJECT_RULES
$$(shell mkdir -p $(1); echo '$$(CC) $$($(2))' | cmp -s - $(1)/.flags || echo '$$(CC) $$($(2))' > $(1)/.flags)
$(1)/%.o: %.c $(1)/.flags
	@mkdir -p $$(@D)
	$$(CC) $$($(2)) -MMD -MP -c $$< -o $$@
endef
$(eval $(call OBJECT_RULES,$(OBJ)/core,CORE_OBJ_FLAGS))
$(eval $(call OBJECT_RULES,$(OBJ)/frontend,FRONTEND_OBJ_FLAGS))
$(eval $(call OBJECT_RULES,$(OBJ)/sanitize,SANITIZE_OBJ_FLAGS))
$(CORE_LIB): $(CORE_OBJS)
	@rm -f $@
	$(AR) rcs $@ $(CORE_OBJS)
-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
# Binaries that compile their own sources in one command record their header
# dependencies here before compiling, so a header edit rebuilds only the
# binaries that include it. $(1) is the preprocessor flags and sources.
DEPS = $(OBJ)/deps
DEPGEN = @mkdir -p $(DEPS) && $(CC) $(1) -MM -MP -MT $@ > $(DEPS)/$(@F).d
$(DEPS)/%.d: ;
.PRECIOUS: $(DEPS)/%.d
# Asset-backed checks need UnityPy. `make unity-tools` creates the pinned
# environment under build/, which both variables then prefer; without it they
# fall back to python3 and UnityPy-dependent tests skip (skips are not passes).
UNITY_PYTHON ?= $(if $(wildcard build/unity-tools/bin/python),build/unity-tools/bin/python,python3)
ASSET_PYTHON ?= $(UNITY_PYTHON)
.PHONY: unity-tools adventure-npc-collider-check
unity-tools:
	python3 tools/setup_unity_tools.py
	build/unity-tools/bin/python -m pip install -r tools/requirements-unity.txt
	python3 tools/setup_unity_tools.py --repair
adventure-npc-collider-check:
	$(UNITY_PYTHON) tools/export_adventure_npc_collider.py --check
	$(UNITY_PYTHON) tests/test_adventure_npc_collider.py
.PHONY: check check-sanitize check-sources compile-commands
# Every UnityPy-backed regeneration check. `check-content` cannot run these
# with the ordinary interpreter, so a shared source list can drift unnoticed.
# Exporter --check modes that had no target. Each is read-only and compares
# regenerated output with the installed or generated files. The capture and
# save inputs are the local reference profiles the installed groups came from;
# see docs/audit/EXPORTER_CHECKS.md.
RUNNING_ROOT_CAPTURE ?= $(HOME)/.local/share/planet-explorers-strafe-reference/pose-captures/female-running-root-06.jsonl
SETTLEMENT_CAPTURE ?= $(HOME)/.local/share/planet-explorers-settlement-reference/terrain-captures/starttown.pevx
ROUTE_CAPTURE ?= $(HOME)/.local/share/planet-explorers-route-reference/terrain-captures/annot3.pevx
ROUTE_SAVE ?= $(HOME)/.local/share/planet-explorers-route-reference/PlanetExplorers/GameSave/save2
.PHONY: exporter-check
exporter-check:
	$(UNITY_PYTHON) tools/export_actor_materials.py --check
	$(UNITY_PYTHON) tools/export_reference_materials.py --check
	$(UNITY_PYTHON) tools/export_adventure_population.py --check
	$(UNITY_PYTHON) tools/export_adventure_settlement.py --check
	$(UNITY_PYTHON) tools/export_adventure_settlement_assets.py --check
	$(UNITY_PYTHON) tools/export_adventure_terrain.py --map-type 1 --check
	$(UNITY_PYTHON) tools/export_adventure_terrain.py --capture '$(SETTLEMENT_CAPTURE)' --check
	$(UNITY_PYTHON) tools/export_adventure_route.py --capture '$(ROUTE_CAPTURE)' --check
	$(UNITY_PYTHON) tools/export_adventure_route_assets.py --save-dir '$(ROUTE_SAVE)' --check
	$(UNITY_PYTHON) tools/export_hare_pounce_assets.py --check
	$(UNITY_PYTHON) tools/export_sandbox_world_assets.py --check
	$(UNITY_PYTHON) tools/export_torch_assets.py --check
	$(UNITY_PYTHON) tools/export_tree_collision.py --check
	$(UNITY_PYTHON) tools/export_sword_root_motion.py --output build/reference-assets/player-sword-motion --check
	$(UNITY_PYTHON) tools/export_sprint_root_motion.py --output build/sprint-source-motion-01 --check
	$(UNITY_PYTHON) tools/export_running_knife_motion.py '$(RUNNING_ROOT_CAPTURE)' --plan build/root-motion-bake/root-06-provenance/running-knife-plan.json --probe-source build/root-motion-bake/root-06-provenance/ReferenceRootMotionBake.cs --probe-assembly build/root-motion-bake/root-06-provenance/ReferenceRootMotionBake.dll --output build/running-motion-20261006 --check
check-sources: exporter-check adventure-npc-collider-check attack-playback-source-check camera-query-source-check camp-repair-source-check capsule-history-source-check hare-perception-assets-check hit-reaction-source-check hunt-loot-source-check hunt-targeting-source-check starter-equipment-check sword-source-check terrain-assets-check
# Parallel build and run of everything `make test` covers; continues past
# failures and totals skips. See tools/run_tests.py --help.
check:
	python3 tools/run_tests.py --jobs $(JOBS) $(CHECK_ARGS)
check-sanitize:
	python3 tools/run_tests.py --jobs $(JOBS) $(CHECK_ARGS) sanitize
compile-commands:
	python3 tools/run_tests.py --compile-commands

.PHONY: all test sanitize content check-content run clean smoke training-smoke training-npcs-smoke training-sequence-smoke training-combat-smoke training-gathering-smoke training-crafting-smoke training-mining-smoke training-building-smoke actor-smoke cli-test asset-test ui-asset-test
all: $(BUILD)/planet_explorers

.PHONY: index index-test
index:
	python3 tools/index_project.py build
index-test:
	python3 tests/test_project_index.py

$(BUILD):
	mkdir -p $@
$(BUILD)/planet_explorers: $(FRONTEND_OBJS) $(CORE_LIB)
	$(CC) $(CFLAGS) $(FRONTEND_OBJS) $(CORE_LIB) -o $@ $(RAYLIB_LIBS) $(LZ4_LIBS) -lm
$(BUILD)/test_core: $(TEST_OBJS) $(CORE_LIB)
	$(CC) $(CFLAGS) $(TEST_OBJS) $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
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
	python3 tests/test_sprint_root_export.py
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
	python3 tests/test_source_paths.py
	python3 tests/test_runtime_paths.py
$(BUILD)/test_sanitize: $(SANITIZE_OBJS)
	$(CC) $(SANITIZE_FLAGS) $(SANITIZE_OBJS) -o $@ $(LZ4_LIBS) -lm
sanitize: $(BUILD)/test_sanitize
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
# Remove only what this Makefile can rebuild: objects, dependency files, test
# logs and the binaries it defines. build/ also holds recovered assets, staged
# groups, fixtures and captured evidence that cannot be regenerated from this
# repository, so nothing else under it is touched.
BUILT_BINARIES := $(addprefix $(BUILD)/,$(shell sed -n 's/^$$(BUILD)\/\([A-Za-z0-9_]*\):.*/\1/p' $(firstword $(MAKEFILE_LIST)) | sort -u))
clean:
	rm -rf $(OBJ) $(BUILD)/test-logs
	rm -f $(BUILT_BINARIES)

$(BUILD)/smoke_fixture: tests/smoke_fixture.c $(CORE_LIB) $(DEPS)/smoke_fixture.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/smoke_fixture.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/smoke_fixture.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
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
$(BUILD)/test_sandbox_equipment_host: tests/test_sandbox_equipment_host.c src/pe_sandbox_view.c src/pe_revival.c src/pe_inventory.c src/pe_game.c src/pe_sandbox_actions.c src/pe_stamina_recovery.c src/pe_outfit.c src/pe_world.c src/pe_combat.c src/pe_sword.c generated/pe_content.c $(DEPS)/test_sandbox_equipment_host.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_equipment_host.c src/pe_revival.c src/pe_inventory.c src/pe_game.c src/pe_sandbox_actions.c src/pe_stamina_recovery.c src/pe_outfit.c src/pe_world.c src/pe_combat.c src/pe_sword.c generated/pe_content.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_equipment_host.c src/pe_revival.c src/pe_inventory.c src/pe_game.c src/pe_sandbox_actions.c src/pe_stamina_recovery.c src/pe_outfit.c src/pe_world.c src/pe_combat.c src/pe_sword.c generated/pe_content.c -Wl,--gc-sections -o $@ -lm
sandbox-equipment-host-test: $(BUILD)/test_sandbox_equipment_host
	./$(BUILD)/test_sandbox_equipment_host

$(BUILD)/test_training_asset_clocks: tests/test_training_asset_clocks.c src/pe_training_view.c $(DEPS)/test_training_asset_clocks.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_training_asset_clocks.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_training_asset_clocks.c -Wl,--gc-sections -o $@ -lm
training-asset-clocks-test: $(BUILD)/test_training_asset_clocks
	./$(BUILD)/test_training_asset_clocks

$(BUILD)/test_actor_blend_view: tests/test_actor_blend_view.c src/pe_actor.c src/pe_actor_view.c src/pe_actor_scene_view.c $(DEPS)/test_actor_blend_view.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_actor_blend_view.c src/pe_actor.c src/pe_actor_view.c src/pe_actor_scene_view.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_actor_blend_view.c src/pe_actor.c src/pe_actor_view.c src/pe_actor_scene_view.c -Wl,--gc-sections -o $@ -lm
actor-blend-test: $(BUILD)/test_actor_blend_view
	./$(BUILD)/test_actor_blend_view
$(BUILD)/test_sandbox_actor: tests/test_sandbox_actor.c src/pe_inventory.c generated/pe_content.c src/pe_sandbox_actor.c src/pe_player_locomotion.c src/pe_combat.c $(DEPS)/test_sandbox_actor.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_actor.c src/pe_inventory.c generated/pe_content.c src/pe_sandbox_actor.c src/pe_player_locomotion.c src/pe_combat.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_actor.c src/pe_inventory.c generated/pe_content.c src/pe_sandbox_actor.c src/pe_player_locomotion.c src/pe_combat.c -o $@ -lm
sandbox-actor-test: $(BUILD)/test_sandbox_actor
	./$(BUILD)/test_sandbox_actor
$(BUILD)/test_sandbox_ui: tests/test_sandbox_ui.c src/pe_sandbox_ui_input.c $(CORE_LIB) $(DEPS)/test_sandbox_ui.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_ui.c src/pe_sandbox_ui_input.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_ui.c src/pe_sandbox_ui_input.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
sandbox-ui-test: $(BUILD)/test_sandbox_ui
	./$(BUILD)/test_sandbox_ui
menu-smoke: all
	./$(BUILD)/planet_explorers --main-menu --smoke 30 --screenshot $(BUILD)/smoke-main-menu.png

.PHONY: adventure-smoke
$(BUILD)/adventure_fixture: tests/adventure_fixture.c $(CORE_LIB) $(DEPS)/adventure_fixture.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_fixture.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_fixture.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
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

$(BUILD)/test_reference_validation: tests/test_reference_validation.c src/pe_reference.c $(DEPS)/test_reference_validation.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_reference_validation.c src/pe_reference.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_reference_validation.c src/pe_reference.c -o $@ $(RAYLIB_LIBS) -lm
asset-test: $(BUILD)/test_reference_validation
	./$(BUILD)/test_reference_validation

# Raylib types only: test_ui_assets supplies headless GPU/decoder stubs.
$(BUILD)/test_ui_assets: tests/test_ui_assets.c src/pe_ui_assets.c $(DEPS)/test_ui_assets.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_ui_assets.c src/pe_ui_assets.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_ui_assets.c src/pe_ui_assets.c -o $@ -lm
ui-asset-test: $(BUILD)/test_ui_assets
	./$(BUILD)/test_ui_assets $(BUILD)/reference-assets/ui

actor-smoke: all
	./$(BUILD)/planet_explorers --reference-actor $(BUILD)/reference-assets/hare.peac --smoke 630 --screenshot $(BUILD)/smoke-hare.png

$(BUILD)/reference_melee_check: tests/reference_melee_check.c src/pe_actor.c src/pe_hitbox.c src/pe_combat.c src/pe_capsule_history.c src/pe_knife_contact.c $(DEPS)/reference_melee_check.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/reference_melee_check.c src/pe_actor.c src/pe_hitbox.c src/pe_combat.c src/pe_capsule_history.c src/pe_knife_contact.c)
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
$(BUILD)/test_sandbox_world_view: tests/test_sandbox_world_view.c src/pe_sandbox_world_view.c $(DEPS)/test_sandbox_world_view.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_world_view.c src/pe_sandbox_world_view.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_world_view.c src/pe_sandbox_world_view.c -o $@ -lm
sandbox-world-test: $(BUILD)/test_sandbox_world_view
	./$(BUILD)/test_sandbox_world_view

$(BUILD)/sandbox_action_fixture: tests/sandbox_action_fixture.c $(CORE_LIB) $(DEPS)/sandbox_action_fixture.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/sandbox_action_fixture.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/sandbox_action_fixture.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
.PHONY: sandbox-action-fixtures
sandbox-action-fixtures: $(BUILD)/sandbox_action_fixture
	./$(BUILD)/sandbox_action_fixture $(BUILD)/sandbox-dig-action.pesave $(BUILD)/sandbox-fell-action.pesave

.PHONY: sandbox-camera-test
$(BUILD)/test_sandbox_camera: tests/test_sandbox_camera.c src/pe_sandbox_camera.c $(DEPS)/test_sandbox_camera.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/test_sandbox_camera.c src/pe_sandbox_camera.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_sandbox_camera.c src/pe_sandbox_camera.c -o $@ -lm
sandbox-camera-test: $(BUILD)/test_sandbox_camera
	./$(BUILD)/test_sandbox_camera

# Test-only funded saves for physical town UI and Annot completion checks.
$(BUILD)/adventure_town_fixture: tests/adventure_town_fixture.c $(CORE_LIB) $(DEPS)/adventure_town_fixture.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_town_fixture.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_town_fixture.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
.PHONY: adventure-town-fixtures adventure-shop-fixture adventure-annot-fixture
adventure-shop-fixture: $(BUILD)/adventure_town_fixture
	./$(BUILD)/adventure_town_fixture --shop $(BUILD)/adventure-shop-fixture.pesave
adventure-annot-fixture: $(BUILD)/adventure_town_fixture
	./$(BUILD)/adventure_town_fixture --annot $(BUILD)/adventure-annot-fixture.pesave
adventure-town-fixtures: adventure-shop-fixture adventure-annot-fixture

$(BUILD)/adventure_supplier_fixture: tests/adventure_supplier_fixture.c $(CORE_LIB) $(DEPS)/adventure_supplier_fixture.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_supplier_fixture.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_supplier_fixture.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm

.PHONY: terrain-material-test
$(BUILD)/test_terrain_material: tests/test_terrain_material.c src/pe_terrain_material.c $(DEPS)/test_terrain_material.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/test_terrain_material.c src/pe_terrain_material.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_terrain_material.c src/pe_terrain_material.c -o $@ -lm
$(BUILD)/test_terrain_material_view: tests/test_terrain_material_view.c src/pe_terrain_material_view.c $(DEPS)/test_terrain_material_view.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_terrain_material_view.c src/pe_terrain_material_view.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_terrain_material_view.c src/pe_terrain_material_view.c -o $@ -lm
terrain-material-test: $(BUILD)/test_terrain_material $(BUILD)/test_terrain_material_view
	./$(BUILD)/test_terrain_material
	./$(BUILD)/test_terrain_material_view

# Optional offline UnityPy/Pillow environment; never needed by the C runtime.

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
$(BUILD)/terrain_material_fixture: tests/terrain_material_fixture.c $(CORE_LIB) $(DEPS)/terrain_material_fixture.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/terrain_material_fixture.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/terrain_material_fixture.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
$(BUILD)/adventure_route_fixture: tests/adventure_route_fixture.c $(CORE_LIB) $(DEPS)/adventure_route_fixture.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_route_fixture.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_route_fixture.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm

# Existing physically reached checkpoint; no fixture grants or position edits.
$(BUILD)/adventure_medicine_fixture: tests/adventure_medicine_fixture.c $(CORE_LIB) $(DEPS)/adventure_medicine_fixture.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_medicine_fixture.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/adventure_medicine_fixture.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
.PHONY: adventure-medicine-fixture
adventure-medicine-fixture: $(BUILD)/adventure_medicine_fixture
	./$(BUILD)/adventure_medicine_fixture --prepare $(BUILD)/annot-route-graphical-final.pesave $(BUILD)/medicine-ready.pesave > $(BUILD)/medicine-headless.json

.PHONY: hare-perception-assets-check
hare-perception-assets-check:
	$(ASSET_PYTHON) tools/export_hare_perception.py --check
	$(ASSET_PYTHON) tests/test_hare_perception_export.py

.PHONY: town-defense-clock-test
$(BUILD)/test_town_defense_clock: tests/test_town_defense_clock.c src/pe_town_defense_clock.c $(DEPS)/test_town_defense_clock.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/test_town_defense_clock.c src/pe_town_defense_clock.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_town_defense_clock.c src/pe_town_defense_clock.c -o $@ -lm
town-defense-clock-test: $(BUILD)/test_town_defense_clock
	./$(BUILD)/test_town_defense_clock

.PHONY: hunt-consume-test
$(BUILD)/hunt_consume_host.inc: tests/test_hunt_consume_host.py src/pe_sandbox_view.c | $(BUILD)
	python3 tests/test_hunt_consume_host.py --output $@
$(BUILD)/test_hunt_consume: tests/test_hunt_consume.c $(BUILD)/hunt_consume_host.inc src/pe_sandbox_ui_input.c $(CORE_LIB) $(DEPS)/test_hunt_consume.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) -I$(BUILD) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_hunt_consume.c src/pe_sandbox_ui_input.c)
	$(CC) $(CPPFLAGS) -I$(BUILD) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_hunt_consume.c src/pe_sandbox_ui_input.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
hunt-consume-test: $(BUILD)/test_hunt_consume
	@./$(BUILD)/test_hunt_consume; result=$$?; if [ $$result -eq 77 ]; then echo "hunt consume: asset-dependent regression SKIPPED (not passed)"; elif [ $$result -ne 0 ]; then exit $$result; fi

.PHONY: sandbox-health-host-test
$(BUILD)/test_sandbox_health_host: tests/test_sandbox_health_host.c src/pe_sandbox_view.c $(CORE_LIB) $(DEPS)/test_sandbox_health_host.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_health_host.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_health_host.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-health-host-test: $(BUILD)/test_sandbox_health_host
	./$(BUILD)/test_sandbox_health_host

.PHONY: sandbox-stamina-host-test
$(BUILD)/test_sandbox_stamina_host: tests/test_sandbox_stamina_host.c src/pe_sandbox_view.c $(CORE_LIB) $(DEPS)/test_sandbox_stamina_host.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_stamina_host.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_stamina_host.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
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
$(BUILD)/test_human_rotation_math: tests/test_human_rotation_math.c tools/human_rotation_math.c $(DEPS)/test_human_rotation_math.d | $(BUILD)
	$(call DEPGEN,$(CFLAGS) tests/test_human_rotation_math.c tools/human_rotation_math.c)
	$(CC) $(CFLAGS) tests/test_human_rotation_math.c tools/human_rotation_math.c -o $@ -lm
humanoid-rotation-test: $(BUILD)/test_human_rotation_math
	./$(BUILD)/test_human_rotation_math
	python3 tests/test_humanoid_rotation_probe.py

.PHONY: sandbox-revival-host-test
$(BUILD)/test_sandbox_revival_host: tests/test_sandbox_revival_host.c src/pe_sandbox_view.c src/pe_sandbox_ui_input.c $(CORE_LIB) $(DEPS)/test_sandbox_revival_host.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_revival_host.c src/pe_sandbox_ui_input.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_revival_host.c src/pe_sandbox_ui_input.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-revival-host-test: $(BUILD)/test_sandbox_revival_host
	./$(BUILD)/test_sandbox_revival_host

.PHONY: sword-policy-test
$(BUILD)/test_sword_policy: tests/test_sword_policy_main.c tests/test_sword_variant.c tests/test_sword_motion.c tests/test_running_attack_motion.c tests/test_sandbox_melee.c tests/test_running_attack_pipeline.c $(CORE_LIB) $(DEPS)/test_sword_policy.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_sword_policy_main.c tests/test_sword_variant.c tests/test_sword_motion.c tests/test_running_attack_motion.c tests/test_sandbox_melee.c tests/test_running_attack_pipeline.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_sword_policy_main.c tests/test_sword_variant.c tests/test_sword_motion.c tests/test_running_attack_motion.c tests/test_sandbox_melee.c tests/test_running_attack_pipeline.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
sword-policy-test: $(BUILD)/test_sword_policy
	./$(BUILD)/test_sword_policy

# These prefab checks require UnityPy, like the existing animation exporters.
.PHONY: sword-source-check
sword-source-check:
	$(UNITY_PYTHON) tools/export_sword_variant.py --check
	$(UNITY_PYTHON) tests/test_sword_variant_export.py

.PHONY: sword-walking-test sandbox-sword-motion-host-test
$(BUILD)/test_sword_walking: tests/test_sword_walking_main.c tests/test_sword_root_motion.c tests/test_sword_walking.c $(CORE_LIB) $(DEPS)/test_sword_walking.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_sword_walking_main.c tests/test_sword_root_motion.c tests/test_sword_walking.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_sword_walking_main.c tests/test_sword_root_motion.c tests/test_sword_walking.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
sword-walking-test: $(BUILD)/test_sword_walking
	./$(BUILD)/test_sword_walking
	python3 tests/test_player_root_channels.py
	python3 tests/test_sword_root_export.py
$(BUILD)/test_sandbox_sword_motion_host: tests/test_sandbox_sword_motion_host.c src/pe_sandbox_view.c $(CORE_LIB) $(DEPS)/test_sandbox_sword_motion_host.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_sword_motion_host.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_sword_motion_host.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-sword-motion-host-test: $(BUILD)/test_sandbox_sword_motion_host
	./$(BUILD)/test_sandbox_sword_motion_host

.PHONY: town-wallet-test sandbox-wallet-host-test
$(BUILD)/test_town_wallet: tests/test_town_wallet_main.c tests/test_town_wallet.c tests/test_town_wallet_save.c $(CORE_LIB) $(DEPS)/test_town_wallet.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_town_wallet_main.c tests/test_town_wallet.c tests/test_town_wallet_save.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_town_wallet_main.c tests/test_town_wallet.c tests/test_town_wallet_save.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
town-wallet-test: $(BUILD)/test_town_wallet
	./$(BUILD)/test_town_wallet
	python3 tests/test_town_wallet_export.py
$(BUILD)/test_sandbox_wallet_host: tests/test_sandbox_wallet_host.c src/pe_sandbox_view.c $(CORE_LIB) $(DEPS)/test_sandbox_wallet_host.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_wallet_host.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_wallet_host.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-wallet-host-test: $(BUILD)/test_sandbox_wallet_host
	./$(BUILD)/test_sandbox_wallet_host

.PHONY: hunt-save-inspector hunt-save-inspector-test native-private-input-test
$(BUILD)/inspect_hunt_save: tools/inspect_hunt_save.c $(CORE_LIB) $(DEPS)/inspect_hunt_save.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tools/inspect_hunt_save.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tools/inspect_hunt_save.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
hunt-save-inspector: $(BUILD)/inspect_hunt_save

hunt-save-inspector-test:
	python3 tests/test_hunt_save_inspector.py

native-private-input-test:
	python3 tests/test_native_private_input.py

# Pure/subprocess tool checks only; ordinary graphical acceptance is opt-in.
test: hunt-save-inspector-test native-private-input-test

.PHONY: town-trade-test
$(BUILD)/test_town_trade: tests/test_town_trade_main.c tests/test_town_trade.c tests/test_town_trade_save.c $(CORE_LIB) $(DEPS)/test_town_trade.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_town_trade_main.c tests/test_town_trade.c tests/test_town_trade_save.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_town_trade_main.c tests/test_town_trade.c tests/test_town_trade_save.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
town-trade-test: $(BUILD)/test_town_trade
	./$(BUILD)/test_town_trade
	python3 tests/test_town_trade_export.py

.PHONY: sandbox-shop-trade-ui-test
$(BUILD)/test_sandbox_shop_trade_ui: tests/test_sandbox_shop_trade_ui.c src/pe_sandbox_ui_input.c $(CORE_LIB) $(DEPS)/test_sandbox_shop_trade_ui.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_shop_trade_ui.c src/pe_sandbox_ui_input.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_shop_trade_ui.c src/pe_sandbox_ui_input.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
sandbox-shop-trade-ui-test: $(BUILD)/test_sandbox_shop_trade_ui
	./$(BUILD)/test_sandbox_shop_trade_ui

.PHONY: build-history-test sandbox-build-history-host-test
$(BUILD)/test_build_history: tests/test_build_history_main.c tests/test_build_history.c tests/test_build_history_save.c $(CORE_LIB) $(DEPS)/test_build_history.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_build_history_main.c tests/test_build_history.c tests/test_build_history_save.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_build_history_main.c tests/test_build_history.c tests/test_build_history_save.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
build-history-test: $(BUILD)/test_build_history
	./$(BUILD)/test_build_history
	python3 tests/test_build_history_export.py
$(BUILD)/test_sandbox_build_history_host: tests/test_sandbox_build_history_host.c src/pe_sandbox_view.c src/pe_sandbox_ui_input.c $(CORE_LIB) $(DEPS)/test_sandbox_build_history_host.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_build_history_host.c src/pe_sandbox_ui_input.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_build_history_host.c src/pe_sandbox_ui_input.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-build-history-host-test: $(BUILD)/test_sandbox_build_history_host
	./$(BUILD)/test_sandbox_build_history_host

.PHONY: vegetation-test sandbox-vegetation-host-test
$(BUILD)/test_vegetation: tests/test_vegetation_main.c tests/test_vegetation.c tests/test_vegetation_save.c $(CORE_LIB) $(DEPS)/test_vegetation.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_vegetation_main.c tests/test_vegetation.c tests/test_vegetation_save.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_vegetation_main.c tests/test_vegetation.c tests/test_vegetation_save.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
vegetation-test: $(BUILD)/test_vegetation
	./$(BUILD)/test_vegetation
$(BUILD)/vegetation_fixture: tests/vegetation_fixture.c $(CORE_LIB) $(DEPS)/vegetation_fixture.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/vegetation_fixture.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/vegetation_fixture.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
$(BUILD)/test_sandbox_vegetation_host: tests/test_sandbox_vegetation_host.c src/pe_sandbox_view.c src/pe_sandbox_ui_input.c $(CORE_LIB) $(DEPS)/test_sandbox_vegetation_host.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_vegetation_host.c src/pe_sandbox_ui_input.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_vegetation_host.c src/pe_sandbox_ui_input.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-vegetation-host-test: $(BUILD)/test_sandbox_vegetation_host
	./$(BUILD)/test_sandbox_vegetation_host
.PHONY: vegetation-view-test
$(BUILD)/test_vegetation_view: tests/test_vegetation_view.c src/pe_vegetation_view.c src/pe_reference.c $(CORE_LIB) $(DEPS)/test_vegetation_view.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_vegetation_view.c src/pe_vegetation_view.c src/pe_reference.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_vegetation_view.c src/pe_vegetation_view.c src/pe_reference.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(RAYLIB_LIBS) $(LZ4_LIBS) -lm
vegetation-view-test: $(BUILD)/test_vegetation_view
	./$(BUILD)/test_vegetation_view; result=$$?; test $$result -eq 0 -o $$result -eq 77

.PHONY: sandbox-inventory-ui-test inventory-test
$(BUILD)/test_inventory_ui: tests/test_inventory_ui.c src/pe_sandbox_ui_input.c $(CORE_LIB) $(DEPS)/test_inventory_ui.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_inventory_ui.c src/pe_sandbox_ui_input.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_inventory_ui.c src/pe_sandbox_ui_input.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
sandbox-inventory-ui-test: $(BUILD)/test_inventory_ui
	./$(BUILD)/test_inventory_ui
$(BUILD)/test_inventory: tests/test_inventory_main.c tests/test_inventory.c tests/test_inventory_save.c tests/test_harvest.c $(CORE_LIB) $(DEPS)/test_inventory.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_inventory_main.c tests/test_inventory.c tests/test_inventory_save.c tests/test_harvest.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_inventory_main.c tests/test_inventory.c tests/test_inventory_save.c tests/test_harvest.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
inventory-test: $(BUILD)/test_inventory
	./$(BUILD)/test_inventory

.PHONY: camera-environment-test sandbox-camera-environment-host-test town-defense-spawn-test
$(BUILD)/test_camera_environment: tests/test_camera_environment.c src/pe_sandbox_camera.c $(DEPS)/test_camera_environment.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/test_camera_environment.c src/pe_sandbox_camera.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_camera_environment.c src/pe_sandbox_camera.c -o $@ -lm
camera-environment-test: $(BUILD)/test_camera_environment
	./$(BUILD)/test_camera_environment
$(BUILD)/test_sandbox_camera_environment_host: tests/test_sandbox_camera_environment_host.c src/pe_sandbox_view.c $(CORE_LIB) $(DEPS)/test_sandbox_camera_environment_host.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_camera_environment_host.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_camera_environment_host.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-camera-environment-host-test: $(BUILD)/test_sandbox_camera_environment_host
	./$(BUILD)/test_sandbox_camera_environment_host
$(BUILD)/test_town_defense_spawn: tests/test_town_defense_spawn.c src/pe_town_defense_spawn.c src/pe_town_defense_clock.c $(DEPS)/test_town_defense_spawn.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/test_town_defense_spawn.c src/pe_town_defense_spawn.c src/pe_town_defense_clock.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_town_defense_spawn.c src/pe_town_defense_spawn.c src/pe_town_defense_clock.c -o $@ -lm
town-defense-spawn-test: $(BUILD)/test_town_defense_spawn
	./$(BUILD)/test_town_defense_spawn

.PHONY: hare-pounce-assets-test
$(BUILD)/test_hare_pounce_assets: tests/test_hare_pounce_assets.c src/pe_actor.c src/pe_hitbox.c src/pe_hare_pounce.c $(DEPS)/test_hare_pounce_assets.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/test_hare_pounce_assets.c src/pe_actor.c src/pe_hitbox.c src/pe_hare_pounce.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_hare_pounce_assets.c src/pe_actor.c src/pe_hitbox.c src/pe_hare_pounce.c -o $@ -lm
hare-pounce-assets-test: $(BUILD)/test_hare_pounce_assets
	./$(BUILD)/test_hare_pounce_assets $(BUILD)/reference-assets/hare.peac $(BUILD)/reference-assets/hare-pounce-staged-20261007/hare.peac $(BUILD)/reference-assets/hare.pehb

.PHONY: adventure-mission-events-test
$(BUILD)/test_adventure_mission_events: tests/test_adventure_mission_events.c src/pe_adventure_mission_events.c $(DEPS)/test_adventure_mission_events.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/test_adventure_mission_events.c src/pe_adventure_mission_events.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_adventure_mission_events.c src/pe_adventure_mission_events.c -o $@
adventure-mission-events-test: $(BUILD)/test_adventure_mission_events
	./$(BUILD)/test_adventure_mission_events

.PHONY: inventory-discard-test
$(BUILD)/test_inventory_discard: tests/test_inventory_discard.c src/pe_sandbox_ui_input.c $(CORE_LIB) $(DEPS)/test_inventory_discard.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_inventory_discard.c src/pe_sandbox_ui_input.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_inventory_discard.c src/pe_sandbox_ui_input.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
inventory-discard-test: $(BUILD)/test_inventory_discard
	./$(BUILD)/test_inventory_discard

.PHONY: camp-repair-test camp-repair-source-check
$(BUILD)/test_camp_repair: tests/test_camp_repair.c src/pe_camp_repair.c src/pe_inventory.c generated/pe_content.c $(DEPS)/test_camp_repair.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) -DPE_CAMP_REPAIR_TEST_MAIN tests/test_camp_repair.c src/pe_camp_repair.c src/pe_inventory.c generated/pe_content.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DPE_CAMP_REPAIR_TEST_MAIN tests/test_camp_repair.c src/pe_camp_repair.c src/pe_inventory.c generated/pe_content.c -o $@ -lm
camp-repair-test: $(BUILD)/test_camp_repair
	./$(BUILD)/test_camp_repair
camp-repair-source-check:
	$(UNITY_PYTHON) tools/export_camp_repair.py --check
	$(UNITY_PYTHON) tests/test_camp_repair_export.py

.PHONY: camp-repair-save-test camp-repair-interaction-test sandbox-repair-ui-test sandbox-repair-host-test
$(BUILD)/test_camp_repair_save: tests/test_camp_repair_save.c $(CORE_LIB) $(DEPS)/test_camp_repair_save.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_CAMP_REPAIR_SAVE_TEST_MAIN tests/test_camp_repair_save.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_CAMP_REPAIR_SAVE_TEST_MAIN tests/test_camp_repair_save.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
camp-repair-save-test: $(BUILD)/test_camp_repair_save
	./$(BUILD)/test_camp_repair_save
$(BUILD)/test_camp_repair_interaction: tests/test_camp_repair_interaction.c $(CORE_LIB) $(DEPS)/test_camp_repair_interaction.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_camp_repair_interaction.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_camp_repair_interaction.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
camp-repair-interaction-test: $(BUILD)/test_camp_repair_interaction
	cd .. && rebuild/$(BUILD)/test_camp_repair_interaction --installed
$(BUILD)/test_sandbox_repair_host: tests/test_sandbox_repair_host.c src/pe_sandbox_view.c src/pe_sandbox_ui_input.c $(CORE_LIB) $(DEPS)/test_sandbox_repair_host.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_repair_host.c src/pe_sandbox_ui_input.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_repair_host.c src/pe_sandbox_ui_input.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-repair-host-test: $(BUILD)/test_sandbox_repair_host
	./$(BUILD)/test_sandbox_repair_host

$(BUILD)/test_sandbox_repair_ui: tests/test_sandbox_repair_ui.c src/pe_sandbox_ui_input.c $(CORE_LIB) $(DEPS)/test_sandbox_repair_ui.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_repair_ui.c src/pe_sandbox_ui_input.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_sandbox_repair_ui.c src/pe_sandbox_ui_input.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
sandbox-repair-ui-test: $(BUILD)/test_sandbox_repair_ui
	./$(BUILD)/test_sandbox_repair_ui

.PHONY: shop-quantity-details-ui-test
$(BUILD)/test_shop_quantity_details_ui: tests/test_shop_quantity_details_ui.c src/pe_sandbox_ui_input.c $(CORE_LIB) $(DEPS)/test_shop_quantity_details_ui.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_shop_quantity_details_ui.c src/pe_sandbox_ui_input.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_shop_quantity_details_ui.c src/pe_sandbox_ui_input.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
shop-quantity-details-ui-test: $(BUILD)/test_shop_quantity_details_ui
	./$(BUILD)/test_shop_quantity_details_ui

.PHONY: bandage-test camera-geometry-test camera-geometry-source-check
$(BUILD)/test_bandage: tests/test_bandage.c $(CORE_LIB) $(DEPS)/test_bandage.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_BANDAGE_TEST_MAIN tests/test_bandage.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_BANDAGE_TEST_MAIN tests/test_bandage.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
bandage-test: $(BUILD)/test_bandage
	./$(BUILD)/test_bandage
$(BUILD)/test_camera_geometry: tests/test_camera_geometry.c src/pe_camera_geometry.c $(DEPS)/test_camera_geometry.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/test_camera_geometry.c src/pe_camera_geometry.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_camera_geometry.c src/pe_camera_geometry.c -o $@ -lm
camera-geometry-test: $(BUILD)/test_camera_geometry
	./$(BUILD)/test_camera_geometry
camera-geometry-source-check:
	python3 tools/export_camera_geometry.py --check
	python3 tests/test_camera_geometry_export.py

# Optional staged rendering on a private Xvfb display; never part of make test.
# Requires the locally extracted Xvfb described in SHOP_QUANTITY_AND_ITEM_DETAILS.
.PHONY: shop-details-render-capture
$(BUILD)/render_shop_details_ui: tests/render_shop_details_ui.c $(UI_SHARED) src/pe_sandbox_ui.c src/pe_sandbox_ui_input.c src/pe_ui_assets.c src/pe_training_ui_shader.c $(CORE_LIB) $(DEPS)/render_shop_details_ui.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LZ4_CFLAGS) tests/render_shop_details_ui.c $(UI_SHARED) src/pe_sandbox_ui.c src/pe_sandbox_ui_input.c src/pe_ui_assets.c src/pe_training_ui_shader.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LZ4_CFLAGS) tests/render_shop_details_ui.c $(UI_SHARED) src/pe_sandbox_ui.c src/pe_sandbox_ui_input.c src/pe_ui_assets.c src/pe_training_ui_shader.c $(CORE_LIB) -o $@ $(RAYLIB_LIBS) $(LZ4_LIBS) -lm
shop-details-render-capture: $(BUILD)/render_shop_details_ui
	python3 tests/run_private_ui_capture.py

.PHONY: ui-shared-test
$(BUILD)/test_ui_hud: tests/test_ui_hud.c src/pe_ui_draw.c src/pe_ui_hud.c src/pe_training_ui_layout.c generated/pe_content.c $(DEPS)/test_ui_hud.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_ui_hud.c src/pe_ui_draw.c src/pe_ui_hud.c src/pe_training_ui_layout.c generated/pe_content.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_ui_hud.c src/pe_ui_draw.c src/pe_ui_hud.c src/pe_training_ui_layout.c generated/pe_content.c -o $@ -lm
ui-shared-test: $(BUILD)/test_ui_hud
	./$(BUILD)/test_ui_hud
	python3 tests/test_ui_unification.py
test: ui-shared-test

.PHONY: ui-shared-render-capture
UI_RENDER_HOSTS = src/pe_ui_settings.c src/pe_training_ui.c src/pe_training_ui_minimap.c src/pe_training_ui_replicator.c src/pe_training_ui_mission.c src/pe_sandbox_ui.c src/pe_sandbox_ui_input.c src/pe_ui_assets.c src/pe_training_ui_shader.c
$(BUILD)/render_shared_ui: tests/render_shared_ui.c $(UI_SHARED) $(UI_RENDER_HOSTS) $(CORE_LIB) $(DEPS)/render_shared_ui.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LZ4_CFLAGS) tests/render_shared_ui.c $(UI_SHARED) $(UI_RENDER_HOSTS))
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LZ4_CFLAGS) tests/render_shared_ui.c $(UI_SHARED) $(UI_RENDER_HOSTS) $(CORE_LIB) -o $@ $(RAYLIB_LIBS) $(LZ4_LIBS) -lm
ui-shared-render-capture: $(BUILD)/render_shared_ui
	python3 tests/run_private_ui_capture.py --shared

.PHONY: conversation-ui-test
$(BUILD)/test_conversation_ui: tests/test_conversation_ui.c src/pe_sandbox_ui_input.c $(CORE_LIB) $(DEPS)/test_conversation_ui.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LZ4_CFLAGS) tests/test_conversation_ui.c src/pe_sandbox_ui_input.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LZ4_CFLAGS) tests/test_conversation_ui.c src/pe_sandbox_ui_input.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
conversation-ui-test: $(BUILD)/test_conversation_ui
	./$(BUILD)/test_conversation_ui
test: conversation-ui-test

.PHONY: conversation-render-test
$(BUILD)/test_ui_history: tests/test_ui_history.c src/pe_ui_history.c src/pe_ui_draw.c src/pe_training_ui_layout.c src/pe_conversation_history.c generated/pe_content.c $(DEPS)/test_ui_history.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_ui_history.c src/pe_ui_history.c src/pe_ui_draw.c src/pe_training_ui_layout.c src/pe_conversation_history.c generated/pe_content.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_ui_history.c src/pe_ui_history.c src/pe_ui_draw.c src/pe_training_ui_layout.c src/pe_conversation_history.c generated/pe_content.c -o $@ -lm
conversation-render-test: $(BUILD)/test_ui_history
	./$(BUILD)/test_ui_history
	python3 tests/test_conversation_history_source.py
test: conversation-render-test

.PHONY: camera-projection-test
$(BUILD)/test_camera_projection: tests/test_camera_projection.c src/pe_camera_projection.c $(DEPS)/test_camera_projection.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/test_camera_projection.c src/pe_camera_projection.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_camera_projection.c src/pe_camera_projection.c -o $@ -lm
camera-projection-test: $(BUILD)/test_camera_projection
test: camera-projection-test

$(BUILD)/test_camera_projection_raylib: tests/test_camera_projection_raylib.c src/pe_camera_projection.c $(DEPS)/test_camera_projection_raylib.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_camera_projection_raylib.c src/pe_camera_projection.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/test_camera_projection_raylib.c src/pe_camera_projection.c -o $@ $(RAYLIB_LIBS) -lm
camera-projection-test: $(BUILD)/test_camera_projection_raylib
	./$(BUILD)/test_camera_projection
	./$(BUILD)/test_camera_projection_raylib

.PHONY: camera-query-test camera-query-source-check
CAMERA_QUERY_TEST_CORE = src/pe_camera_query.c src/pe_camera_geometry.c src/pe_sandbox_collision.c src/pe_scene_collision.c src/pe_world.c src/pe_mesh.c src/pe_block.c src/pe_tree_collision.c generated/pe_content.c
$(BUILD)/test_camera_query: tests/test_camera_query.c $(CAMERA_QUERY_TEST_CORE) $(DEPS)/test_camera_query.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/test_camera_query.c $(CAMERA_QUERY_TEST_CORE))
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_camera_query.c $(CAMERA_QUERY_TEST_CORE) -o $@ -lm
camera-query-test: $(BUILD)/test_camera_query
	./$(BUILD)/test_camera_query
camera-query-source-check:
	$(UNITY_PYTHON) tools/export_camera_query.py --check
	$(UNITY_PYTHON) tests/test_camera_query_export.py
test: camera-query-test
check-content: camera-query-source-check

.PHONY: sandbox-blade-facing-test
$(BUILD)/test_sandbox_blade_facing: tests/test_sandbox_blade_facing.c src/pe_sandbox_view.c $(CORE_LIB) $(DEPS)/test_sandbox_blade_facing.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_blade_facing.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_blade_facing.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-blade-facing-test: $(BUILD)/test_sandbox_blade_facing
	./$(BUILD)/test_sandbox_blade_facing
test: sandbox-blade-facing-test

.PHONY: hit-reaction-test hit-reaction-source-check hunt-targeting-source-check
$(BUILD)/test_hit_reaction: tests/test_hit_reaction.c src/pe_hit_reaction.c $(DEPS)/test_hit_reaction.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/test_hit_reaction.c src/pe_hit_reaction.c)
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
$(BUILD)/test_preferences: tests/test_preferences.c src/pe_preferences.c $(DEPS)/test_preferences.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) tests/test_preferences.c src/pe_preferences.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_preferences.c src/pe_preferences.c -o $@
preferences-test: $(BUILD)/test_preferences
	./$(BUILD)/test_preferences
test: preferences-test

.PHONY: app-menu-settings-test
$(BUILD)/test_app_menu_settings: tests/test_app_menu_settings.c src/pe_app_menu.c src/pe_ui_settings.c src/pe_preferences.c src/pe_ui_draw.c $(DEPS)/test_app_menu_settings.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_app_menu_settings.c src/pe_app_menu.c src/pe_ui_settings.c src/pe_preferences.c src/pe_ui_draw.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_app_menu_settings.c src/pe_app_menu.c src/pe_ui_settings.c src/pe_preferences.c src/pe_ui_draw.c -Wl,--gc-sections -o $@ -lm
app-menu-settings-test: $(BUILD)/test_app_menu_settings
	./$(BUILD)/test_app_menu_settings
test: app-menu-settings-test

.PHONY: settings-render-capture
$(BUILD)/render_app_settings: tests/render_app_settings.c src/pe_app_menu.c src/pe_ui_settings.c src/pe_preferences.c src/pe_ui_assets.c src/pe_ui_draw.c $(DEPS)/render_app_settings.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/render_app_settings.c src/pe_preferences.c src/pe_ui_assets.c src/pe_ui_draw.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/render_app_settings.c src/pe_preferences.c src/pe_ui_assets.c src/pe_ui_draw.c -Wl,--gc-sections -o $@ $(RAYLIB_LIBS) -lm
settings-render-capture: $(BUILD)/render_app_settings
	python3 tests/run_private_ui_capture.py --settings

.PHONY: hunt-loot-test hunt-loot-combat-test hunt-loot-source-check
$(BUILD)/test_hunt_loot: tests/test_hunt_loot.c $(CORE_LIB) $(DEPS)/test_hunt_loot.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_HUNT_LOOT_TEST_MAIN tests/test_hunt_loot.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_HUNT_LOOT_TEST_MAIN tests/test_hunt_loot.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
$(BUILD)/test_hunt_loot_save: tests/test_hunt_loot_save.c $(CORE_LIB) $(DEPS)/test_hunt_loot_save.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_HUNT_LOOT_SAVE_TEST_MAIN tests/test_hunt_loot_save.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_HUNT_LOOT_SAVE_TEST_MAIN tests/test_hunt_loot_save.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
$(BUILD)/test_hunt_loot_combat: tests/test_hunt_loot_combat.c $(CORE_LIB) $(DEPS)/test_hunt_loot_combat.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_HUNT_LOOT_COMBAT_TEST_MAIN tests/test_hunt_loot_combat.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) -DPE_HUNT_LOOT_COMBAT_TEST_MAIN tests/test_hunt_loot_combat.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
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
$(BUILD)/render_hunt_loot: tests/render_hunt_loot.c src/pe_sandbox_view.c $(CORE_LIB) src/pe_ui_draw.c src/pe_ui_assets.c $(DEPS)/render_hunt_loot.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/render_hunt_loot.c src/pe_ui_draw.c src/pe_ui_assets.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/render_hunt_loot.c src/pe_ui_draw.c src/pe_ui_assets.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(RAYLIB_LIBS) $(LZ4_LIBS) -lm
hunt-loot-render-capture: $(BUILD)/render_hunt_loot
	python3 tests/run_private_ui_capture.py --loot

.PHONY: hunt-loot-markers-test
$(BUILD)/test_hunt_loot_markers: tests/test_hunt_loot_markers.c src/pe_sandbox_view.c $(CORE_LIB) $(DEPS)/test_hunt_loot_markers.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_hunt_loot_markers.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_hunt_loot_markers.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
hunt-loot-markers-test: $(BUILD)/test_hunt_loot_markers
	./$(BUILD)/test_hunt_loot_markers
test: hunt-loot-markers-test

.PHONY: debug-test sandbox-debug-host-test
$(BUILD)/test_debug: tests/test_debug.c $(CORE_LIB) $(DEPS)/test_debug.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_debug.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) tests/test_debug.c $(CORE_LIB) -o $@ $(LZ4_LIBS) -lm
debug-test: $(BUILD)/test_debug
	./$(BUILD)/test_debug
$(BUILD)/test_sandbox_debug_host: tests/test_sandbox_debug_host.c src/pe_sandbox_view.c $(CORE_LIB) $(DEPS)/test_sandbox_debug_host.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_debug_host.c)
	$(CC) $(CPPFLAGS) $(LZ4_CFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/test_sandbox_debug_host.c $(CORE_LIB) -Wl,--gc-sections -o $@ $(LZ4_LIBS) -lm
sandbox-debug-host-test: $(BUILD)/test_sandbox_debug_host
	./$(BUILD)/test_sandbox_debug_host
test: debug-test sandbox-debug-host-test

.PHONY: actor-material-test actor-material-render-capture
actor-material-test:
	python3 -m unittest discover -s tests -p 'test_actor_material_export.py'
$(BUILD)/render_actor_materials: tests/render_actor_materials.c src/pe_actor_scene_view.c src/pe_actor_view.c src/pe_actor_scene_manifest.c src/pe_actor.c $(DEPS)/render_actor_materials.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/render_actor_materials.c src/pe_actor_scene_view.c src/pe_actor_view.c src/pe_actor_scene_manifest.c src/pe_actor.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) -ffunction-sections -fdata-sections tests/render_actor_materials.c src/pe_actor_scene_view.c src/pe_actor_view.c src/pe_actor_scene_manifest.c src/pe_actor.c -Wl,--gc-sections -o $@ $(RAYLIB_LIBS) -lm
actor-material-render-capture: $(BUILD)/render_actor_materials
	python3 tests/run_private_ui_capture.py --materials
test: actor-material-test

.PHONY: reference-material-test reference-material-sanitize-test reference-material-render-capture
reference-material-test: asset-test
	python3 tests/test_reference_material_export.py
$(BUILD)/test_reference_material_sanitize: tests/test_reference_validation.c src/pe_reference.c $(DEPS)/test_reference_material_sanitize.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(SANITIZE_FLAGS) $(RAYLIB_CFLAGS) tests/test_reference_validation.c src/pe_reference.c)
	$(CC) $(CPPFLAGS) $(SANITIZE_FLAGS) $(RAYLIB_CFLAGS) tests/test_reference_validation.c src/pe_reference.c -o $@ $(RAYLIB_LIBS) -lm
reference-material-sanitize-test: $(BUILD)/test_reference_material_sanitize
	ASAN_OPTIONS=$(ASAN_OPTIONS) ./$(BUILD)/test_reference_material_sanitize
sanitize: reference-material-sanitize-test
test: reference-material-test
$(BUILD)/render_reference_materials: tests/render_reference_materials.c src/pe_reference.c $(CORE_LIB) $(DEPS)/render_reference_materials.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/render_reference_materials.c src/pe_reference.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/render_reference_materials.c src/pe_reference.c $(CORE_LIB) -o $@ $(RAYLIB_LIBS) $(LZ4_LIBS) -lm
reference-material-render-capture: $(BUILD)/render_reference_materials
	python3 tests/run_private_ui_capture.py --reference-materials

.PHONY: repulsed-source-test repulsed-source-check repulsed-arithmetic-test
repulsed-source-test:
	python3 tests/test_repulsed_export.py
repulsed-source-check:
	$(UNITY_PYTHON) tools/export_repulsed_action.py --check
	$(UNITY_PYTHON) tests/test_repulsed_export.py
repulsed-arithmetic-test: $(CORE_LIB)
	python3 tests/test_repulsed_arithmetic.py
test: repulsed-source-test repulsed-arithmetic-test
check-sources: repulsed-source-check
.PHONY: human-phy-source-test human-phy-source-check human-phy-arithmetic-test
human-phy-source-test:
	python3 tests/test_human_phy_export.py
human-phy-source-check:
	$(UNITY_PYTHON) tools/export_human_phy.py --check
	$(UNITY_PYTHON) tests/test_human_phy_export.py
human-phy-arithmetic-test: $(CORE_LIB)
	python3 tests/test_human_phy_arithmetic.py
test: human-phy-source-test human-phy-arithmetic-test
check-sources: human-phy-source-check
.PHONY: reaction-presentation-source-test reaction-presentation-source-check
reaction-presentation-source-test:
	python3 tests/test_reaction_presentation_export.py
reaction-presentation-source-check:
	$(UNITY_PYTHON) tools/export_reaction_presentation.py --check
	$(UNITY_PYTHON) tests/test_reaction_presentation_export.py
test: reaction-presentation-source-test
check-sources: reaction-presentation-source-check
.PHONY: halt-source-test halt-source-check
halt-source-test:
	python3 tests/test_halt_export.py
halt-source-check:
	$(UNITY_PYTHON) tools/export_halt_effect.py --check
	$(UNITY_PYTHON) tests/test_halt_export.py
test: halt-source-test
check-sources: halt-source-check

.PHONY: hit-ik-source-test hit-ik-source-check
hit-ik-source-test:
	python3 tests/test_hit_ik_export.py
hit-ik-source-check:
	$(UNITY_PYTHON) tools/export_hit_ik.py --check
	$(UNITY_PYTHON) tests/test_hit_ik_export.py
test: hit-ik-source-test
check-sources: hit-ik-source-check

.PHONY: ragdoll-action-source-test ragdoll-action-source-check
ragdoll-action-source-test:
	python3 tests/test_ragdoll_action_export.py
ragdoll-action-source-check:
	$(UNITY_PYTHON) tools/export_ragdoll_actions.py --check
	$(UNITY_PYTHON) tests/test_ragdoll_action_export.py
test: ragdoll-action-source-test
check-sources: ragdoll-action-source-check

.PHONY: animfollow-source-test animfollow-source-check
animfollow-source-test:
	python3 tests/test_animfollow_export.py
animfollow-source-check:
	$(UNITY_PYTHON) tools/export_animfollow.py --check
	$(UNITY_PYTHON) tests/test_animfollow_export.py
test: animfollow-source-test
check-sources: animfollow-source-check

.PHONY: reaction-render-capture
REACTION_CAPTURE_OUTPUT ?= build/reaction-captures-01
REACTION_RENDER_OBJS = $(addprefix $(OBJ)/frontend/src/,pe_sandbox_actor.o pe_actor_scene_view.o pe_actor_view.o)
$(BUILD)/render_reaction_group: tests/render_reaction_group.c $(REACTION_RENDER_OBJS) $(CORE_LIB) $(DEPS)/render_reaction_group.d | $(BUILD)
	$(call DEPGEN,$(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/render_reaction_group.c)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) tests/render_reaction_group.c $(REACTION_RENDER_OBJS) $(CORE_LIB) -o $@ $(RAYLIB_LIBS) $(LZ4_LIBS) -lm
reaction-render-capture: $(BUILD)/render_reaction_group
	python3 tests/run_private_ui_capture.py --reactions --output $(REACTION_CAPTURE_OUTPUT)

.PHONY: knife-reaction-render-capture
knife-reaction-render-capture: $(BUILD)/render_reaction_group
	python3 tests/run_private_ui_capture.py --reactions --knife-reactions --output $(REACTION_CAPTURE_OUTPUT)
