// =============================================================================
// TestList.h  --  모든 테스트 함수의 선언
// -----------------------------------------------------------------------------
// 각 함수의 정의는 tests/cases/<suite>/NN_이름.cpp 에 있고, tests/main.cpp 의 switch 가 직접 호출한다.
// 반환 값: 0 = PASS, 1 = FAIL, 77 = SKIP.   (자세한 규약은 common/Check.h)
// 새 테스트를 추가하면 여기에도 선언을 한 줄 추가한다.
// =============================================================================
#pragma once

// ---- unit ----
int Test_unit_01_triton_runtime_param_structs_are_packed();
int Test_unit_02_hrtf_structs_layout();
int Test_unit_03_preprocessor_structs_layout();
int Test_unit_04_param_flags_and_failure_code();
int Test_unit_05_coordinate_unreal_to_triton_position();
int Test_unit_06_coordinate_position_roundtrip();
int Test_unit_07_coordinate_direction_has_no_scale();
int Test_unit_08_coordinate_triton_up_becomes_hrtf_up();
int Test_unit_09_bridge_maps_wet_and_dry_fields();
int Test_unit_10_bridge_clamps_outdoorness_and_near_distance();
int Test_unit_11_bridge_detects_triton_failure_code();
int Test_unit_12_procedural_box_is_valid_watertight_mesh();
int Test_unit_13_procedural_quad_is_open_surface();
int Test_unit_14_quote_argument_rules();
int Test_unit_15_build_command_line_minimal();
int Test_unit_16_build_command_line_all_options();
int Test_unit_17_validate_rejects_bad_options();
int Test_unit_18_find_ace_and_cleanup_enc();
int Test_unit_19_run_reports_failure_for_missing_processor();

// ---- hrtf ----
int Test_hrtf_01_binaural_engine_has_two_channels_left_right();
int Test_hrtf_02_source_on_right_is_louder_in_right_ear();
int Test_hrtf_03_mirrored_source_swaps_ears();
int Test_hrtf_04_process_rejects_invalid_input();
int Test_hrtf_05_set_parameters_out_of_range_index_fails();
int Test_hrtf_06_panner_process_honors_output_format();
int Test_hrtf_07_panner_channel_count_query_is_not_refreshed();
int Test_hrtf_08_reverb_tail_query_is_optional_export();
int Test_hrtf_09_create_fails_with_clear_error_for_missing_dll();

// ---- preprocessor ----
int Test_preprocessor_01_loads_and_resolves_all_exports();
int Test_preprocessor_02_load_fails_clearly_for_missing_dll();
int Test_preprocessor_03_material_library_roundtrip();
int Test_preprocessor_04_material_lookup_by_name_matches_known_codes();
int Test_preprocessor_05_mesh_accepts_geometry_and_navigation();
int Test_preprocessor_06_error_paths_return_failure_not_crash();
int Test_preprocessor_07_handle_keeps_dll_alive_after_client_destroyed();
int Test_preprocessor_08_config_failure_no_overlap_with_navigation_is_survivable();
int Test_preprocessor_09_config_success_then_unload_is_survivable();
int Test_preprocessor_10_config_failure_zero_dense_probes_is_survivable();

// ---- runtime ----
int Test_runtime_01_init_and_teardown_can_be_repeated();
int Test_runtime_02_log_hook_is_accepted();
int Test_runtime_03_load_missing_ace_fails();
int Test_runtime_04_query_without_loaded_data_fails();
int Test_runtime_05_dynamic_opening_unknown_id_fails();
int Test_runtime_06_reverb_send_weights_blend_adjacent_buses();
int Test_runtime_07_reverb_send_weights_exact_match_selects_single_bus();
int Test_runtime_08_reverb_send_weights_empty_input_fails();

// ---- e2e ----
int Test_e2e_01_mesh_to_bake_to_runtime_query_to_hrtf();
