cmake_minimum_required(VERSION 3.20)
project(FreedomUnite VERSION 0.1.0 LANGUAGES CXX)

enable_testing()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/portablekit/host/profile.hpp")
    message(FATAL_ERROR "PortableKit is missing. Run git submodule update --init --recursive.")
endif()
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/portablekit/host/profile.hpp" framework_profile)
if(NOT framework_profile MATCHES "bool launcher_home = false;")
    message(FATAL_ERROR "PortableKit needs patches/portablekit-local.patch. Run scripts/build-windows.ps1 -Step Patch first.")
endif()

option(MHFU_NO_CORPUS "Build the installer without generated game code" OFF)
option(MHFU_REQUIRE_RENDERER "Fail if graphics dependencies are missing" ON)
option(MHFU_ACTION_TIMING_DIAGNOSTICS "Observe internal USA action routines in local generated copies" OFF)
option(MHFU_NATIVE_60_EXPERIMENT "Build guarded main-image native 60 FPS research hooks" OFF)

# PortableKit brings the recompiler, the runtime and the system layer, and
# defines portablekit_add_game(). Same shape as Tenkawa's CMakeLists.txt.
add_subdirectory(portablekit)

# Standalone save editor: no renderer, game launch or Python runtime needed.
add_library(fu_save_editor_core STATIC host/save_editor.cpp host/save_preview.cpp host/fu_text_table.cpp
    portablekit/host/kernel/iso_image.cpp portablekit/host/save_data/auto_backup.cpp
    portablekit/host/save_data/aes128.cpp portablekit/host/save_data/param_sfo.cpp
    portablekit/host/save_data/savedata_crypto.cpp portablekit/host/save_data/savedata_store.cpp
    portablekit/host/crypto_keys_builtin.cpp portablekit/third_party/tiny_aes/aes.c)
target_compile_features(fu_save_editor_core PUBLIC cxx_std_20)
target_include_directories(fu_save_editor_core PUBLIC host portablekit/host portablekit/third_party)
target_link_libraries(fu_save_editor_core PUBLIC psprecomp_core)

if(WIN32)
    enable_language(RC)
    configure_file(host/app_icon.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc" @ONLY)
    add_executable(FreedomUnboundSaveEditor WIN32 host/save_editor_main.cpp "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
    target_compile_definitions(FreedomUnboundSaveEditor PRIVATE UNICODE _UNICODE)
    target_link_libraries(FreedomUnboundSaveEditor PRIVATE fu_save_editor_core comdlg32 shell32 user32 gdi32)
    set_target_properties(FreedomUnboundSaveEditor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_executable(FreedomUnbound WIN32 host/launcher_entry.cpp)
    target_sources(FreedomUnbound PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
    target_compile_features(FreedomUnbound PRIVATE cxx_std_17)
    target_link_libraries(FreedomUnbound PRIVATE shell32 user32)
    set_target_properties(FreedomUnbound PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(corpus_option)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(launcher_art_header "${CMAKE_CURRENT_BINARY_DIR}/launcher_art/launcher_art_data.hpp")
add_custom_command(OUTPUT "${launcher_art_header}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed-launcher-art.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher/modern-landscape.png" "${launcher_art_header}"
    DEPENDS scripts/embed-launcher-art.py assets/launcher/modern-landscape.png VERBATIM)
set(launcher_logo_header "${CMAKE_CURRENT_BINARY_DIR}/launcher_art/launcher_logo_data.hpp")
add_custom_command(OUTPUT "${launcher_logo_header}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed-launcher-art.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/freedom-unbound-logo-v2.png"
        "${launcher_logo_header}" kLauncherLogo
    DEPENDS scripts/embed-launcher-art.py assets/branding/freedom-unbound-logo-v2.png VERBATIM)
set(credit_logo_header "${CMAKE_CURRENT_BINARY_DIR}/launcher_art/credit_logo_data.hpp")
add_custom_command(OUTPUT "${credit_logo_header}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed-launcher-art.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/wyvern-works-logo.png"
        "${credit_logo_header}" kCreditLogo
    DEPENDS scripts/embed-launcher-art.py assets/branding/wyvern-works-logo.png VERBATIM)
set(profile_sources host/freedom_unite_profile.cpp host/timing_probe.cpp host/native_60.cpp host/mods_page.cpp host/mod_manager.cpp host/updater.cpp host/launcher_ini.cpp host/bug_report.cpp host/release_notes.cpp host/whats_new.cpp
    host/cheats.cpp host/cheats_ui.cpp host/mod_overlay.cpp host/damage_events.cpp host/damage_numbers.cpp
    host/gathering_nodes.cpp host/gathering_nodes_ui.cpp
    host/monster_hp.cpp host/monster_hp_ui.cpp host/modern_camera.cpp host/modern_camera_mod.cpp
    host/shared_progression.cpp host/shared_progression_mod.cpp host/key_quests.cpp host/key_quests_ui.cpp
    host/launcher_art.cpp host/save_preview.cpp host/save_preview_ui.cpp host/equipment_preview.cpp host/save_portraits.cpp host/direct_load.cpp host/hunter_model.cpp host/model_materials.cpp host/fu_host_text.cpp host/fu_text_table.cpp host/fu_text_mod.cpp host/game_log.cpp host/mods_used.cpp
    "${launcher_art_header}" "${launcher_logo_header}" "${credit_logo_header}")
if(MHFU_NO_CORPUS)
    set(corpus_option NO_CORPUS)
    list(APPEND profile_sources "${CMAKE_CURRENT_SOURCE_DIR}/portablekit/host/generated_stub.cpp")
endif()
portablekit_add_game(FreedomUniteNative ${corpus_option}
    PROFILE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    SOURCES ${profile_sources})
target_include_directories(FreedomUniteNative PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/launcher_art")
if(WIN32)
    target_sources(FreedomUniteNative PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
endif()

if(MHFU_ACTION_TIMING_DIAGNOSTICS)
    if(MHFU_NO_CORPUS)
        message(FATAL_ERROR "Action timing diagnostics need the generated USA corpus.")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    get_target_property(timing_sources FreedomUniteNative_generated SOURCES)
    foreach(timing_unit IN ITEMS 0007 0013 0015 0018 0023 0024 0025 0026 0052 0060)
        set(timing_original "${CMAKE_CURRENT_SOURCE_DIR}/generated/generated_unit_${timing_unit}.cpp")
        set(timing_copy "${CMAKE_CURRENT_BINARY_DIR}/action_timing/generated_unit_${timing_unit}.cpp")
        if(timing_unit STREQUAL "0007")
            set(timing_points 0x08823A50)
        elseif(timing_unit STREQUAL "0013")
            set(timing_points 0x0883B72C)
        elseif(timing_unit STREQUAL "0018")
            set(timing_points 0x0884D348 0x0884D418 0x0884D754)
        elseif(timing_unit STREQUAL "0015")
            set(timing_points 0x08841B5C)
        elseif(timing_unit STREQUAL "0060")
            set(timing_points 0x088F6000)
        elseif(timing_unit STREQUAL "0023")
            set(timing_points 0x08863894 0x088638BC 0x088638C8 0x0886399C 0x08863D00)
        elseif(timing_unit STREQUAL "0024")
            set(timing_points 0x0886401C 0x08864370)
        elseif(timing_unit STREQUAL "0025")
            set(timing_points 0x088693D8)
        elseif(timing_unit STREQUAL "0026")
            set(timing_points 0x0886DFAC 0x0886DFF8 0x0886EAC8)
        else()
            set(timing_points 0x088D5E30 0x088D5ED4)
        endif()
        list(FIND timing_sources "${timing_original}" timing_index)
        if(timing_index EQUAL -1)
            message(FATAL_ERROR "Expected generated timing unit is absent: ${timing_unit}")
        endif()
        add_custom_command(OUTPUT "${timing_copy}"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-action-timing.py"
                "${timing_original}" "${timing_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/timing_probe.hpp" ${timing_points}
            DEPENDS "${timing_original}" scripts/instrument-action-timing.py host/timing_probe.hpp
            VERBATIM)
        list(REMOVE_ITEM timing_sources "${timing_original}")
        list(APPEND timing_sources "${timing_copy}")
    endforeach()
    set_property(TARGET FreedomUniteNative_generated PROPERTY SOURCES "${timing_sources}")
    # Observe only this content-identified USA gameplay overlay. Copies stay
    # in the ignored build tree; the generated corpus remains untouched.
    set(timing_overlay ovl09A5F300_game_task_36BD79C16BF70197)
    set(timing_overlay_target "overlay_${timing_overlay}")
    if(NOT TARGET ${timing_overlay_target})
        message(FATAL_ERROR "Missing supported gameplay overlay for timing diagnostics")
    endif()
    get_target_property(timing_overlay_sources ${timing_overlay_target} SOURCES)
    foreach(timing_overlay_unit IN ITEMS 0002 0011 0012 0014 0015)
        if(timing_overlay_unit STREQUAL "0002")
            set(timing_overlay_point 0x09A675AC)
        elseif(timing_overlay_unit STREQUAL "0011")
            set(timing_overlay_point 0x09A8CE54)
        elseif(timing_overlay_unit STREQUAL "0012")
            set(timing_overlay_point 0x09A905F0)
        elseif(timing_overlay_unit STREQUAL "0014")
            set(timing_overlay_point 0x09A9AA08)
        else()
            set(timing_overlay_point 0x09A9C530)
        endif()
        set(timing_overlay_original "${CMAKE_CURRENT_SOURCE_DIR}/overlays/${timing_overlay}/generated_unit_${timing_overlay_unit}.cpp")
        set(timing_overlay_copy "${CMAKE_CURRENT_BINARY_DIR}/action_timing/${timing_overlay}/generated_unit_${timing_overlay_unit}.cpp")
        list(FIND timing_overlay_sources "${timing_overlay_original}" timing_overlay_index)
        if(timing_overlay_index EQUAL -1)
            message(FATAL_ERROR "Missing supported overlay timing unit")
        endif()
        add_custom_command(OUTPUT "${timing_overlay_copy}"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-action-timing.py"
                "${timing_overlay_original}" "${timing_overlay_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/timing_probe.hpp" ${timing_overlay_point}
            DEPENDS "${timing_overlay_original}" scripts/instrument-action-timing.py host/timing_probe.hpp VERBATIM)
        list(REMOVE_ITEM timing_overlay_sources "${timing_overlay_original}")
        list(APPEND timing_overlay_sources "${timing_overlay_copy}")
    endforeach()
    set_property(TARGET ${timing_overlay_target} PROPERTY SOURCES "${timing_overlay_sources}")
endif()

enable_language(C)
add_library(mhfu_miniz STATIC third_party/miniz/miniz.c)
target_include_directories(mhfu_miniz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz)
target_link_libraries(FreedomUniteNative PRIVATE mhfu_miniz)
if(WIN32)
    target_link_libraries(FreedomUniteNative PRIVATE winhttp bcrypt)
endif()
set(MHFU_VERSION "0.1.2-alpha" CACHE STRING "Version shown in the launcher and used by the updater")
option(MHFU_TEST_DATA "Use the FreedomUnite-Test user data folder" OFF)
target_compile_definitions(FreedomUniteNative PRIVATE MHFU_VERSION="${MHFU_VERSION}" MHFU_TEST_DATA=$<BOOL:${MHFU_TEST_DATA}>)

if(MHFU_NATIVE_60_EXPERIMENT)
    if(MHFU_NO_CORPUS)
        message(FATAL_ERROR "Native 60 research mode needs the supported generated corpus.")
    endif()
    target_compile_definitions(FreedomUniteNative PRIVATE MHFU_NATIVE_60_EXPERIMENT=1)
    get_target_property(native_sources FreedomUniteNative_generated SOURCES)
    foreach(native_unit IN ITEMS 0013 0015 0018 0021 0023 0024 0025 0026 0060)
        set(native_original "${CMAKE_CURRENT_SOURCE_DIR}/generated/generated_unit_${native_unit}.cpp")
        set(native_input "${native_original}")
        if(MHFU_ACTION_TIMING_DIAGNOSTICS AND NOT native_unit STREQUAL "0021")
            set(native_input "${CMAKE_CURRENT_BINARY_DIR}/action_timing/generated_unit_${native_unit}.cpp")
        endif()
        list(FIND native_sources "${native_input}" native_index)
        if(native_index EQUAL -1)
            message(FATAL_ERROR "Missing generated native timing unit: ${native_unit}")
        endif()
        set(native_copy "${CMAKE_CURRENT_BINARY_DIR}/native_60/generated_unit_${native_unit}.cpp")
        add_custom_command(OUTPUT "${native_copy}"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-native-60.py"
                "${native_input}" "${native_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/native_60.hpp" "${native_unit}"
            DEPENDS "${native_input}" scripts/instrument-native-60.py host/native_60.hpp VERBATIM)
        list(REMOVE_ITEM native_sources "${native_input}")
        list(APPEND native_sources "${native_copy}")
    endforeach()
    set_property(TARGET FreedomUniteNative_generated PROPERTY SOURCES "${native_sources}")
endif()

if(NOT MHFU_NO_CORPUS)
    # Copies intercept direct intra-unit calls too. Runtime instruction writes
    # alone cannot change AOT execution. The original corpus is never edited.
    set(cheat_overlay ovl09A5F300_game_task_36BD79C16BF70197)
    foreach(cheat_kind IN ITEMS main18 main19 main22 main26 main52 game2 game9 game26 lobby17 lobby18 lobby31 lobby32)
        if(cheat_kind MATCHES "^main")
            set(cheat_target FreedomUniteNative_generated)
            string(REPLACE "main" "00" cheat_unit "${cheat_kind}")
            set(cheat_original "${CMAKE_CURRENT_SOURCE_DIR}/generated/generated_unit_${cheat_unit}.cpp")
            set(cheat_input "${cheat_original}")
            if(cheat_kind STREQUAL "main26" AND MHFU_NATIVE_60_EXPERIMENT)
                set(cheat_input "${CMAKE_CURRENT_BINARY_DIR}/native_60/generated_unit_0026.cpp")
            elseif(cheat_kind STREQUAL "main26" AND MHFU_ACTION_TIMING_DIAGNOSTICS)
                set(cheat_input "${CMAKE_CURRENT_BINARY_DIR}/action_timing/generated_unit_0026.cpp")
            elseif(MHFU_ACTION_TIMING_DIAGNOSTICS AND cheat_kind STREQUAL "main52")
                set(cheat_input "${CMAKE_CURRENT_BINARY_DIR}/action_timing/generated_unit_0052.cpp")
            endif()
        else()
            if(cheat_kind MATCHES "^lobby")
                set(cheat_overlay ovl09A5F300_lobby_task_F431CAA134157E88)
                string(REPLACE "lobby" "00" cheat_unit "${cheat_kind}")
            else()
                set(cheat_overlay ovl09A5F300_game_task_36BD79C16BF70197)
            endif()
            set(cheat_target "overlay_${cheat_overlay}")
            if(cheat_kind STREQUAL "game2")
                set(cheat_unit 0002)
            elseif(cheat_kind STREQUAL "game9")
                set(cheat_unit 0009)
            elseif(cheat_kind STREQUAL "game26")
                set(cheat_unit 0026)
            endif()
            set(cheat_original "${CMAKE_CURRENT_SOURCE_DIR}/overlays/${cheat_overlay}/generated_unit_${cheat_unit}.cpp")
            set(cheat_input "${cheat_original}")
            if(MHFU_ACTION_TIMING_DIAGNOSTICS AND cheat_kind STREQUAL "game2")
                set(cheat_input "${CMAKE_CURRENT_BINARY_DIR}/action_timing/${cheat_overlay}/generated_unit_0002.cpp")
            endif()
        endif()
        if(NOT TARGET ${cheat_target})
            message(FATAL_ERROR "Cheats require the supported USA gameplay overlay")
        endif()
        get_target_property(cheat_sources ${cheat_target} SOURCES)
        list(FIND cheat_sources "${cheat_input}" cheat_index)
        if(cheat_index EQUAL -1)
            message(FATAL_ERROR "Missing supported cheat unit: ${cheat_kind}")
        endif()
        set(cheat_copy "${CMAKE_CURRENT_BINARY_DIR}/cheats/${cheat_kind}.cpp")
        add_custom_command(OUTPUT "${cheat_copy}"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-cheats.py"
                "${cheat_input}" "${cheat_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/cheats.hpp" "${cheat_kind}"
            DEPENDS "${cheat_input}" scripts/instrument-cheats.py host/cheats.hpp host/damage_events.hpp VERBATIM)
        list(REMOVE_ITEM cheat_sources "${cheat_input}")
        list(APPEND cheat_sources "${cheat_copy}")
        set_property(TARGET ${cheat_target} PROPERTY SOURCES "${cheat_sources}")
    endforeach()
endif()

if(NOT MHFU_NO_CORPUS)
    # Text mods (host/fu_text_mod.hpp): same approach as the cheats above. Unit
    # 0018 is already the cheats copy, so the text hook goes on top of it.
    get_target_property(text_sources FreedomUniteNative_generated SOURCES)
    foreach(text_kind IN ITEMS main18 main04 main35)
        string(REPLACE "main" "00" text_unit "${text_kind}")
        set(text_input "${CMAKE_CURRENT_BINARY_DIR}/cheats/${text_kind}.cpp")
        list(FIND text_sources "${text_input}" text_index)
        if(text_index EQUAL -1)
            set(text_input "${CMAKE_CURRENT_SOURCE_DIR}/generated/generated_unit_${text_unit}.cpp")
            list(FIND text_sources "${text_input}" text_index)
        endif()
        if(text_index EQUAL -1)
            message(FATAL_ERROR "Text hook: missing unit ${text_unit}")
        endif()
        set(text_copy "${CMAKE_CURRENT_BINARY_DIR}/text_mod/${text_kind}.cpp")
        add_custom_command(OUTPUT "${text_copy}"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-text.py"
                "${text_input}" "${text_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/fu_text_mod.hpp" "${text_kind}"
            DEPENDS "${text_input}" scripts/instrument-text.py host/fu_text_mod.hpp VERBATIM)
        list(REMOVE_ITEM text_sources "${text_input}")
        list(APPEND text_sources "${text_copy}")
    endforeach()
    set(key_input "${CMAKE_CURRENT_BINARY_DIR}/text_mod/main35.cpp")
    set(key_copy "${CMAKE_CURRENT_BINARY_DIR}/key_quests/main35.cpp")
    add_custom_command(OUTPUT "${key_copy}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-key-quests.py"
            "${key_input}" "${key_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/key_quests.hpp"
        DEPENDS "${key_input}" scripts/instrument-key-quests.py host/key_quests.hpp VERBATIM)
    list(REMOVE_ITEM text_sources "${key_input}")
    list(APPEND text_sources "${key_copy}")
    set_property(TARGET FreedomUniteNative_generated PROPERTY SOURCES "${text_sources}")
endif()
if(NOT MHFU_NO_CORPUS)
    set(damage_target overlay_ovl09C19100_game_sub_662DDC5878706505)
    set(damage_original "${CMAKE_CURRENT_SOURCE_DIR}/overlays/ovl09C19100_game_sub_662DDC5878706505/generated_unit_0010.cpp")
    set(damage_copy "${CMAKE_CURRENT_BINARY_DIR}/damage_events/game_sub10.cpp")
    get_target_property(damage_sources ${damage_target} SOURCES)
    list(FIND damage_sources "${damage_original}" damage_index)
    if(damage_index EQUAL -1)
        message(FATAL_ERROR "Damage events require the supported USA game_sub unit")
    endif()
    add_custom_command(OUTPUT "${damage_copy}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-damage.py"
            "${damage_original}" "${damage_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/damage_events.hpp"
        DEPENDS "${damage_original}" scripts/instrument-damage.py host/damage_events.hpp VERBATIM)
    list(REMOVE_ITEM damage_sources "${damage_original}")
    list(APPEND damage_sources "${damage_copy}")
    set_property(TARGET ${damage_target} PROPERTY SOURCES "${damage_sources}")
    get_target_property(damage_main_sources FreedomUniteNative_generated SOURCES)
    set(damage_main_input "${CMAKE_CURRENT_SOURCE_DIR}/generated/generated_unit_0025.cpp")
    if(MHFU_NATIVE_60_EXPERIMENT)
        set(damage_main_input "${CMAKE_CURRENT_BINARY_DIR}/native_60/generated_unit_0025.cpp")
    elseif(MHFU_ACTION_TIMING_DIAGNOSTICS)
        set(damage_main_input "${CMAKE_CURRENT_BINARY_DIR}/action_timing/generated_unit_0025.cpp")
    endif()
    list(FIND damage_main_sources "${damage_main_input}" damage_main_index)
    if(damage_main_index EQUAL -1)
        message(FATAL_ERROR "Damage camera requires the supported USA main unit 25")
    endif()
    set(damage_main_copy "${CMAKE_CURRENT_BINARY_DIR}/damage_events/main25.cpp")
    add_custom_command(OUTPUT "${damage_main_copy}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-damage.py"
            "${damage_main_input}" "${damage_main_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/damage_events.hpp" --camera
        DEPENDS "${damage_main_input}" scripts/instrument-damage.py host/damage_events.hpp VERBATIM)
    list(REMOVE_ITEM damage_main_sources "${damage_main_input}")
    list(APPEND damage_main_sources "${damage_main_copy}")
    set_property(TARGET FreedomUniteNative_generated PROPERTY SOURCES "${damage_main_sources}")
    set(hp_target overlay_ovl09A5F300_game_task_36BD79C16BF70197)
    set(hp_input "${CMAKE_CURRENT_BINARY_DIR}/cheats/game26.cpp")
    set(hp_copy "${CMAKE_CURRENT_BINARY_DIR}/monster_hp/game26.cpp")
    get_target_property(hp_sources ${hp_target} SOURCES)
    list(FIND hp_sources "${hp_input}" hp_index)
    if(hp_index EQUAL -1)
        message(FATAL_ERROR "Monster HP requires the supported instrumented USA game26")
    endif()
    add_custom_command(OUTPUT "${hp_copy}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-monster-hp.py"
            "${hp_input}" "${hp_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/monster_hp.hpp"
        DEPENDS "${hp_input}" scripts/instrument-monster-hp.py host/monster_hp.hpp VERBATIM)
    list(REMOVE_ITEM hp_sources "${hp_input}")
    list(APPEND hp_sources "${hp_copy}")
    set_property(TARGET ${hp_target} PROPERTY SOURCES "${hp_sources}")
    set(hp_lifecycle_input "${CMAKE_CURRENT_BINARY_DIR}/cheats/main26.cpp")
    set(hp_lifecycle_copy "${CMAKE_CURRENT_BINARY_DIR}/monster_hp/main26.cpp")
    list(FIND damage_main_sources "${hp_lifecycle_input}" hp_lifecycle_index)
    if(hp_lifecycle_index EQUAL -1)
        message(FATAL_ERROR "Monster HP requires the supported instrumented USA main26")
    endif()
    add_custom_command(OUTPUT "${hp_lifecycle_copy}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-monster-hp.py"
            "${hp_lifecycle_input}" "${hp_lifecycle_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/monster_hp.hpp" --lifecycle
        DEPENDS "${hp_lifecycle_input}" scripts/instrument-monster-hp.py host/monster_hp.hpp VERBATIM)
    list(REMOVE_ITEM damage_main_sources "${hp_lifecycle_input}")
    list(APPEND damage_main_sources "${hp_lifecycle_copy}")
    set(camera_input "${CMAKE_CURRENT_SOURCE_DIR}/generated/generated_unit_0032.cpp")
    set(camera_copy "${CMAKE_CURRENT_BINARY_DIR}/modern_camera/main32.cpp")
    list(FIND damage_main_sources "${camera_input}" camera_index)
    if(camera_index EQUAL -1)
        message(FATAL_ERROR "Modern camera requires the supported USA main32")
    endif()
    add_custom_command(OUTPUT "${camera_copy}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-modern-camera.py"
            "${camera_input}" "${camera_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/modern_camera.hpp"
        DEPENDS "${camera_input}" scripts/instrument-modern-camera.py host/modern_camera.hpp VERBATIM)
    list(REMOVE_ITEM damage_main_sources "${camera_input}")
    list(APPEND damage_main_sources "${camera_copy}")
    set(gathering_input "${CMAKE_CURRENT_SOURCE_DIR}/generated/generated_unit_0016.cpp")
    if(MHFU_NATIVE_60_EXPERIMENT)
        set(gathering_input "${CMAKE_CURRENT_BINARY_DIR}/native_60/generated_unit_0016.cpp")
    elseif(MHFU_ACTION_TIMING_DIAGNOSTICS)
        set(gathering_input "${CMAKE_CURRENT_BINARY_DIR}/action_timing/generated_unit_0016.cpp")
    endif()
    set(gathering_pad_input "${CMAKE_CURRENT_BINARY_DIR}/cheats/main52.cpp")
    set(gathering_pad_copy "${CMAKE_CURRENT_BINARY_DIR}/gathering_nodes/main52.cpp")
    list(FIND damage_main_sources "${gathering_pad_input}" gathering_pad_index)
    if(gathering_pad_index EQUAL -1)
        message(FATAL_ERROR "Gathering repeat requires USA main unit 52")
    endif()
    add_custom_command(OUTPUT "${gathering_pad_copy}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-gathering.py"
            "${gathering_pad_input}" "${gathering_pad_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/gathering_nodes.hpp" --input
        DEPENDS "${gathering_pad_input}" scripts/instrument-gathering.py host/gathering_nodes.hpp VERBATIM)
    list(REMOVE_ITEM damage_main_sources "${gathering_pad_input}")
    list(APPEND damage_main_sources "${gathering_pad_copy}")
    set(gathering_copy "${CMAKE_CURRENT_BINARY_DIR}/gathering_nodes/main16.cpp")
    list(FIND damage_main_sources "${gathering_input}" gathering_index)
    if(gathering_index EQUAL -1)
        message(FATAL_ERROR "Gathering nodes require the supported USA main unit 16")
    endif()
    add_custom_command(OUTPUT "${gathering_copy}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-gathering.py"
            "${gathering_input}" "${gathering_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/gathering_nodes.hpp"
        DEPENDS "${gathering_input}" scripts/instrument-gathering.py host/gathering_nodes.hpp VERBATIM)
    list(REMOVE_ITEM damage_main_sources "${gathering_input}")
    list(APPEND damage_main_sources "${gathering_copy}")
    set(shared_input "${CMAKE_CURRENT_SOURCE_DIR}/generated/generated_unit_0028.cpp")
    set(shared_copy "${CMAKE_CURRENT_BINARY_DIR}/shared_progression/main28.cpp")
    list(FIND damage_main_sources "${shared_input}" shared_index)
    if(shared_index EQUAL -1)
        message(FATAL_ERROR "Shared progression requires supported USA main unit 28")
    endif()
    add_custom_command(OUTPUT "${shared_copy}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/instrument-shared-progression.py"
            "${shared_input}" "${shared_copy}" "${CMAKE_CURRENT_SOURCE_DIR}/host/shared_progression.hpp"
        DEPENDS "${shared_input}" scripts/instrument-shared-progression.py host/shared_progression.hpp VERBATIM)
    list(REMOVE_ITEM damage_main_sources "${shared_input}")
    list(APPEND damage_main_sources "${shared_copy}")
    set_property(TARGET FreedomUniteNative_generated PROPERTY SOURCES "${damage_main_sources}")
endif()
if(MHFU_REQUIRE_RENDERER AND NOT TARGET FreedomUniteNative_shaders)
    message(FATAL_ERROR "A playable build needs SDL3, Vulkan, glslangValidator and Python 3. See BUILDING.md. For a deliberate headless build use -DMHFU_REQUIRE_RENDERER=OFF.")
endif()

if(PSPRECOMP_BUILD_TESTS)
    add_executable(mhfu_key_quests_tests tests/key_quests_tests.cpp host/key_quests.cpp)
    target_compile_features(mhfu_key_quests_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_key_quests_tests PRIVATE host)
    target_link_libraries(mhfu_key_quests_tests PRIVATE psprecomp_core)
    add_test(NAME mhfu_key_quests_tests COMMAND mhfu_key_quests_tests)
    add_test(NAME mhfu_key_quests_instrumentation_tests COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_key_quests_instrumentation.py")
    add_executable(mhfu_shared_progression_tests tests/shared_progression_tests.cpp host/shared_progression.cpp)
    target_compile_features(mhfu_shared_progression_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_shared_progression_tests PRIVATE host)
    target_link_libraries(mhfu_shared_progression_tests PRIVATE psprecomp_core)
    add_test(NAME mhfu_shared_progression_tests COMMAND mhfu_shared_progression_tests)
    add_test(NAME mhfu_shared_progression_instrumentation_tests COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_shared_progression_instrumentation.py")
    add_executable(mhfu_modern_camera_tests tests/modern_camera_tests.cpp host/modern_camera.cpp
        portablekit/host/camera/camera_input.cpp)
    target_compile_features(mhfu_modern_camera_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_modern_camera_tests PRIVATE host portablekit/host)
    target_link_libraries(mhfu_modern_camera_tests PRIVATE psprecomp_core)
    add_test(NAME mhfu_modern_camera_tests COMMAND mhfu_modern_camera_tests)
    add_test(NAME mhfu_modern_camera_instrumentation_tests COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_modern_camera_instrumentation.py")
    add_executable(mhfu_monster_hp_tests tests/monster_hp_tests.cpp host/monster_hp.cpp)
    target_compile_features(mhfu_monster_hp_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_monster_hp_tests PRIVATE host)
    target_link_libraries(mhfu_monster_hp_tests PRIVATE psprecomp_core)
    add_test(NAME mhfu_monster_hp_tests COMMAND mhfu_monster_hp_tests)
    add_test(NAME mhfu_monster_hp_instrumentation_tests COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_monster_hp_instrumentation.py")
    add_executable(mhfu_gathering_nodes_tests tests/gathering_nodes_tests.cpp host/gathering_nodes.cpp)
    target_compile_features(mhfu_gathering_nodes_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_gathering_nodes_tests PRIVATE host)
    target_link_libraries(mhfu_gathering_nodes_tests PRIVATE psprecomp_core)
    add_test(NAME mhfu_gathering_nodes_tests COMMAND mhfu_gathering_nodes_tests)
    add_test(NAME mhfu_gathering_instrumentation_tests COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gathering_instrumentation.py")
    add_executable(mhfu_damage_projection_tests tests/damage_projection_tests.cpp)
    target_compile_features(mhfu_damage_projection_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_damage_projection_tests PRIVATE portablekit/host)
    target_link_libraries(mhfu_damage_projection_tests PRIVATE psprecomp_core)
    add_test(NAME mhfu_damage_projection_tests COMMAND mhfu_damage_projection_tests)
    add_test(NAME mhfu_damage_instrumentation_tests COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_damage_instrumentation.py")
    add_executable(mhfu_damage_number_layout_tests tests/damage_number_layout_tests.cpp)
    target_compile_features(mhfu_damage_number_layout_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_damage_number_layout_tests PRIVATE host)
    add_test(NAME mhfu_damage_number_layout_tests COMMAND mhfu_damage_number_layout_tests)
    add_executable(mhfu_mod_overlay_tests tests/mod_overlay_tests.cpp host/mod_overlay.cpp host/damage_events.cpp)
    target_compile_features(mhfu_mod_overlay_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_mod_overlay_tests PRIVATE host)
    target_link_libraries(mhfu_mod_overlay_tests PRIVATE psprecomp_core)
    add_test(NAME mhfu_mod_overlay_tests COMMAND mhfu_mod_overlay_tests)
    add_executable(mhfu_save_editor_tests tests/save_editor_tests.cpp)
    target_link_libraries(mhfu_save_editor_tests PRIVATE fu_save_editor_core)
    add_test(NAME mhfu_save_editor_tests COMMAND mhfu_save_editor_tests)
    add_executable(mhfu_hunter_model_tests tests/hunter_model_tests.cpp host/hunter_model.cpp)
    target_compile_features(mhfu_hunter_model_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_hunter_model_tests PRIVATE host)
    add_test(NAME mhfu_hunter_model_tests COMMAND mhfu_hunter_model_tests)
    add_executable(mhfu_direct_load_tests tests/direct_load_tests.cpp)
    target_compile_features(mhfu_direct_load_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_direct_load_tests PRIVATE host)
    add_test(NAME mhfu_direct_load_tests COMMAND mhfu_direct_load_tests)
    find_package(Threads REQUIRED)
    add_executable(mhfu_model_materials_tests tests/model_materials_tests.cpp host/model_materials.cpp host/hunter_model.cpp
        portablekit/tests/test_profile.cpp portablekit/host/profile.cpp portablekit/host/gpu/texture_decode.cpp
        portablekit/host/gpu/texture_pack.cpp portablekit/host/gpu/texture_pack_import.cpp)
    target_compile_features(mhfu_model_materials_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_model_materials_tests PRIVATE host portablekit/host portablekit/third_party)
    target_link_libraries(mhfu_model_materials_tests PRIVATE psprecomp_core Threads::Threads)
    add_test(NAME mhfu_model_materials_tests COMMAND mhfu_model_materials_tests)
    add_executable(mhfu_save_editor_probe tests/save_editor_probe.cpp)
    target_link_libraries(mhfu_save_editor_probe PRIVATE fu_save_editor_core)
    add_executable(mhfu_cheats_tests tests/cheats_tests.cpp host/cheats.cpp)
    target_compile_features(mhfu_cheats_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_cheats_tests PRIVATE host)
    target_link_libraries(mhfu_cheats_tests PRIVATE psprecomp_core)
    add_test(NAME mhfu_cheats_tests COMMAND mhfu_cheats_tests)
    add_executable(mhfu_text_table_tests tests/text_table_tests.cpp host/fu_text_table.cpp)
    target_compile_features(mhfu_text_table_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_text_table_tests PRIVATE host)
    add_test(NAME mhfu_text_table_tests COMMAND mhfu_text_table_tests)
    add_executable(mhfu_host_text_value_tests tests/host_text_value_tests.cpp)
    target_compile_features(mhfu_host_text_value_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_host_text_value_tests PRIVATE portablekit/host)
    add_test(NAME mhfu_host_text_value_tests COMMAND mhfu_host_text_value_tests)
    add_executable(mhfu_game_log_tests tests/game_log_tests.cpp host/game_log.cpp)
    target_compile_features(mhfu_game_log_tests PRIVATE cxx_std_20)
    target_compile_definitions(mhfu_game_log_tests PRIVATE MHFU_GAME_LOG_PURE=1)
    target_include_directories(mhfu_game_log_tests PRIVATE host)
    add_test(NAME mhfu_game_log_tests COMMAND mhfu_game_log_tests)
    add_test(NAME mhfu_text_instrumentation_tests
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_text_instrumentation.py")
    add_test(NAME mhfu_cheat_instrumentation_tests
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cheat_instrumentation.py")
    add_executable(mhfu_half_step_tests tests/half_step_tests.cpp)
    target_compile_features(mhfu_half_step_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_half_step_tests PRIVATE host)
    add_test(NAME mhfu_half_step_tests COMMAND mhfu_half_step_tests)
    # FU Saves: automatic save backups, rotation and restore.
    add_executable(mhfu_auto_backup_tests tests/auto_backup_tests.cpp portablekit/host/save_data/auto_backup.cpp)
    target_compile_features(mhfu_auto_backup_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_auto_backup_tests PRIVATE portablekit/host)
    add_test(NAME mhfu_auto_backup_tests COMMAND mhfu_auto_backup_tests)
    # FU Saves, item 11: the launcher's import before boot, and mods-used.txt.
    add_executable(mhfu_launcher_import_tests tests/launcher_import_tests.cpp
        portablekit/host/save_data/launcher_import.cpp portablekit/host/save_data/auto_backup.cpp
        portablekit/host/save_data/param_sfo.cpp)
    target_compile_features(mhfu_launcher_import_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_launcher_import_tests PRIVATE portablekit/host)
    add_test(NAME mhfu_launcher_import_tests COMMAND mhfu_launcher_import_tests)
    add_executable(mhfu_mods_used_tests tests/mods_used_tests.cpp host/mods_used.cpp)
    target_compile_features(mhfu_mods_used_tests PRIVATE cxx_std_20)
    target_include_directories(mhfu_mods_used_tests PRIVATE host)
    add_test(NAME mhfu_mods_used_tests COMMAND mhfu_mods_used_tests)
    add_test(NAME mhfu_native_instrumentation_tests
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_native_instrumentation.py")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(mhfu_save_preview_probe tests/save_preview_probe.cpp host/save_preview.cpp
        portablekit/host/save_data/aes128.cpp portablekit/host/save_data/param_sfo.cpp
        portablekit/host/save_data/savedata_crypto.cpp portablekit/host/save_data/savedata_store.cpp
        portablekit/host/crypto_keys_builtin.cpp portablekit/third_party/tiny_aes/aes.c)
    target_compile_features(mhfu_save_preview_probe PRIVATE cxx_std_20)
    target_link_libraries(mhfu_save_preview_probe PRIVATE psprecomp_core)
    target_include_directories(mhfu_save_preview_probe PRIVATE host portablekit/host
        portablekit/third_party portablekit/include)
    set_target_properties(mhfu_save_preview_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME mhfu_save_preview_tests
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_save_preview.py"
            "$<TARGET_FILE:mhfu_save_preview_probe>")
    add_test(NAME mhfu_texture_composition_tests
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_texture_composition.py")
endif()
