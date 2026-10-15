cmake_minimum_required(VERSION 3.25)
project(SegaRallyNative LANGUAGES C CXX OBJC OBJCXX)
if(NOT APPLE)
    message(FATAL_ERROR "This dedicated frontend requires the native Metal backend on macOS or iOS")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
get_filename_component(ROOT "${ROOT}" ABSOLUTE)
set(SEGARALLY_NATIVE_BUILD "${ROOT}/build/segarally-timed" CACHE PATH "Audited native generation directory")
option(SEGARALLY_SOUND "Build optional diagnostic native SCSP sound support" OFF)
option(SEGARALLY_INPUT_RECORDING "Build opt-in Mac input recording and offline same-executable replay" ON)
option(SEGARALLY_INSTRUCTION_BOUNDARY_HOOK "Build audited exact per-instruction hook support (runtime opt-in)" OFF)
set(SEGARALLY_SOUND_BUILD "${ROOT}/build/validation/segarally-pipeline" CACHE PATH "Audited timed sound generation directory")
set(SEGARALLY_SDL_SOURCE "${ROOT}/extern/sdl3" CACHE PATH "Pinned SDL3 3.4.16 source")
execute_process(COMMAND git -C "${SEGARALLY_SDL_SOURCE}" rev-parse HEAD
    OUTPUT_VARIABLE SDL_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE SDL_REVISION_STATUS)
if(NOT SDL_REVISION_STATUS EQUAL 0 OR NOT SDL_REVISION STREQUAL "fa2c02bb6e21974a89ea9824bc53c9932abe5f9c")
    message(FATAL_ERROR "Use the pinned Daytona SDL3 revision fa2c02bb6e21974a89ea9824bc53c9932abe5f9c")
endif()
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${SEGARALLY_SDL_SOURCE}" "${CMAKE_BINARY_DIR}/sdl3" EXCLUDE_FROM_ALL)
# Reuse Daytona's pinned, MIT-licensed display interface and GPU backend.
set(SEGARALLY_IMGUI_SOURCE "${ROOT}/extern/imgui" CACHE PATH "Pinned Dear ImGui source")
execute_process(COMMAND git -C "${SEGARALLY_IMGUI_SOURCE}" rev-parse HEAD
    OUTPUT_VARIABLE IMGUI_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE IMGUI_REVISION_STATUS)
if(NOT IMGUI_REVISION_STATUS EQUAL 0 OR NOT IMGUI_REVISION STREQUAL "f1cc2ae15e53a861a874c3034aae6798fde194ab")
    message(FATAL_ERROR "Use the pinned Daytona Dear ImGui revision f1cc2ae15e53a861a874c3034aae6798fde194ab (scripts/setup.py)")
endif()
add_library(segarally_imgui STATIC
    "${SEGARALLY_IMGUI_SOURCE}/imgui.cpp" "${SEGARALLY_IMGUI_SOURCE}/imgui_draw.cpp"
    "${SEGARALLY_IMGUI_SOURCE}/imgui_tables.cpp" "${SEGARALLY_IMGUI_SOURCE}/imgui_widgets.cpp"
    "${SEGARALLY_IMGUI_SOURCE}/backends/imgui_impl_sdl3.cpp"
    "${SEGARALLY_IMGUI_SOURCE}/backends/imgui_impl_sdlgpu3.cpp")
target_include_directories(segarally_imgui SYSTEM PUBLIC "${SEGARALLY_IMGUI_SOURCE}")
target_link_libraries(segarally_imgui PUBLIC SDL3::SDL3-static)
file(GLOB GENERATED CONFIGURE_DEPENDS "${SEGARALLY_NATIVE_BUILD}/gen/*.cpp")
if(NOT GENERATED OR NOT EXISTS "${SEGARALLY_NATIVE_BUILD}/tgp/program.cpp")
    message(FATAL_ERROR "Generate the audited native main/TGP code before configuring the host")
endif()
set(RUNTIME cpu lockstep model2a_bus model2a_clock model2a_machine model2a_control model2_io model2_eeprom
    model2a_io model2a_comm comm_board model2_uart model2a_uart tgp m2_tgp_board model2a_video video raster geo)
set(SOURCES "${ROOT}/src/app/segarally_main.cpp" "${ROOT}/src/app/gpu/gpu_renderer.cpp"
    "${ROOT}/src/app/gpu/crt_renderer.cpp" "${ROOT}/src/app/segarally_display.cpp"
    "${ROOT}/src/app/segarally_controls.cpp"
    ${GENERATED} "${SEGARALLY_NATIVE_BUILD}/tgp/program.cpp")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(STAGE_ARGS)
if(SEGARALLY_INSTRUCTION_BOUNDARY_HOOK)
    list(APPEND STAGE_ARGS --instruction-boundary-hook)
endif()
if(SEGARALLY_SOUND)
    list(APPEND STAGE_ARGS --sound-build "${SEGARALLY_SOUND_BUILD}")
    list(APPEND RUNTIME model2a_sound model2a_sound_bus snd_cpu model2_scsp model2_scsp_dsp)
    list(APPEND SOURCES "${SEGARALLY_SOUND_BUILD}/sound/native-timed/snd_gen.cpp")
endif()
# Refuse capability/macro mismatches before compiling. This also checks the
# generated source/header inventory before the host rebuilds all native chunks.
file(READ "${SEGARALLY_NATIVE_BUILD}/coverage.json" NATIVE_AUDIT)
string(JSON GENERATED_BOUNDARY_HOOK ERROR_VARIABLE BOUNDARY_JSON_ERROR GET "${NATIVE_AUDIT}" reachability instruction_boundary_hook)
if(BOUNDARY_JSON_ERROR)
    set(GENERATED_BOUNDARY_HOOK OFF)
endif()
if((GENERATED_BOUNDARY_HOOK AND NOT SEGARALLY_INSTRUCTION_BOUNDARY_HOOK) OR
   (SEGARALLY_INSTRUCTION_BOUNDARY_HOOK AND NOT GENERATED_BOUNDARY_HOOK))
    message(FATAL_ERROR "SEGARALLY_INSTRUCTION_BOUNDARY_HOOK must match the audited native generation")
endif()
if(SEGARALLY_SOUND OR SEGARALLY_INSTRUCTION_BOUNDARY_HOOK)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/stage_data.py"
        --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${CMAKE_BINARY_DIR}/assets" ${STAGE_ARGS}
        RESULT_VARIABLE OPTIONAL_VERIFY_RESULT)
    if(NOT OPTIONAL_VERIFY_RESULT EQUAL 0)
        message(FATAL_ERROR "Verified optional native generation/assets required")
    endif()
endif()
foreach(NAME IN LISTS RUNTIME)
    list(APPEND SOURCES "${ROOT}/src/runtime/${NAME}.cpp")
endforeach()
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # Actual Xcode resource inputs establish copy/sign dependencies, including
    # incremental builds where imported data changes without a relink.
    set(ASSET_STAGE "${CMAKE_BINARY_DIR}/assets")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/stage_data.py"
        --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${ASSET_STAGE}" ${STAGE_ARGS}
        RESULT_VARIABLE STAGE_RESULT)
    if(NOT STAGE_RESULT EQUAL 0)
        message(FATAL_ERROR "Verified iOS asset staging failed")
    endif()
    set(ASSETS)
    foreach(NAME program main_data tgp_tables copro_data polygons textures)
        list(APPEND ASSETS "${ASSET_STAGE}/${NAME}.bin")
    endforeach()
    if(SEGARALLY_SOUND)
        list(APPEND ASSETS "${ASSET_STAGE}/sound_program.bin" "${ASSET_STAGE}/sound_samples.bin")
    endif()
    list(APPEND ASSETS "${ASSET_STAGE}/assets.json")
    set_source_files_properties(${ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION assets)
    list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ios/bootstrap.mm")
    add_executable(segarally_native MACOSX_BUNDLE ${SOURCES} ${ASSETS})
    add_custom_target(segarally_assets
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/stage_data.py"
            --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${ASSET_STAGE}" ${STAGE_ARGS}
        BYPRODUCTS ${ASSETS} VERBATIM COMMENT "Verify personally imported Sega Rally assets")
    add_dependencies(segarally_native segarally_assets)
    target_compile_definitions(segarally_native PRIVATE SEGARALLY_IOS=1)
    set_target_properties(segarally_native PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.lgarciacarro.segarally.native"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "3VJB3669V5"
        XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES")
    target_link_libraries(segarally_native PRIVATE "-framework UIKit" "-framework Foundation")
else()
    add_executable(segarally_native ${SOURCES})
    set(ASSET_DEST "$<TARGET_FILE_DIR:segarally_native>/assets")
endif()
target_include_directories(segarally_native PRIVATE "${ROOT}/src" "${ROOT}/src/app")
target_compile_definitions(segarally_native PRIVATE M2_SRALLY_TGP=1 M2_SRALLY_IO=1
    M2_SRALLY_COMM=1 M2_SRALLY_CONTROL=1 M2_SRALLY_VIDEO=1 M2_SRALLY_SCREEN=1 M2_SRALLY_UART=1 SDL_MAIN_HANDLED)
if(SEGARALLY_SOUND)
    target_compile_definitions(segarally_native PRIVATE M2_SRALLY_SOUND=1)
endif()
if(SEGARALLY_INSTRUCTION_BOUNDARY_HOOK)
    # One target owns runtime AND generated C++: no stale prebuilt main objects.
    target_compile_definitions(segarally_native PRIVATE M2_INSTRUCTION_BOUNDARY_HOOK=1)
endif()
if(SEGARALLY_SOUND OR SEGARALLY_INSTRUCTION_BOUNDARY_HOOK)
    # Repeat provenance checks for incremental builds, before linking changed
    # native code. A successful older configure is insufficient evidence.
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        add_custom_target(segarally_native_verify
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/stage_data.py"
                --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${CMAKE_BINARY_DIR}/assets" ${STAGE_ARGS}
            VERBATIM)
        add_dependencies(segarally_native segarally_native_verify)
    endif()
endif()
target_compile_options(segarally_native PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-fast-math;-ffp-contract=off>
    $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:-O3>)
target_link_libraries(segarally_native PRIVATE SDL3::SDL3-static segarally_imgui "-framework Metal" "-framework QuartzCore")
if(SEGARALLY_INPUT_RECORDING AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(SEGARALLY_LZMA_SOURCE "${ROOT}/extern/lzma" CACHE PATH "Pinned public-domain 7-Zip SHA-256 sources")
    execute_process(COMMAND git -C "${SEGARALLY_LZMA_SOURCE}" rev-parse HEAD
        OUTPUT_VARIABLE SHA_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE SHA_REVISION_STATUS)
    if(NOT SHA_REVISION_STATUS EQUAL 0 OR NOT SHA_REVISION STREQUAL "0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17")
        message(FATAL_ERROR "Input recording requires the pinned public-domain 7-Zip revision")
    endif()
    add_library(segarally_recording STATIC "${ROOT}/tools/srally_boot/recording_bundle.cpp" "${ROOT}/src/runtime/sha256_digest.cpp"
        "${SEGARALLY_LZMA_SOURCE}/C/Sha256.c" "${SEGARALLY_LZMA_SOURCE}/C/Sha256Opt.c" "${SEGARALLY_LZMA_SOURCE}/C/CpuArch.c")
    target_include_directories(segarally_recording PUBLIC "${ROOT}/src")
    target_include_directories(segarally_recording SYSTEM PUBLIC "${SEGARALLY_LZMA_SOURCE}/C")
    target_compile_options(segarally_recording PRIVATE -fno-fast-math -ffp-contract=off)
    set_target_properties(segarally_recording PROPERTIES C_STANDARD 11)
    if(SEGARALLY_INSTRUCTION_BOUNDARY_HOOK)
        target_compile_definitions(segarally_recording PRIVATE M2_INSTRUCTION_BOUNDARY_HOOK=1)
    endif()
    target_compile_definitions(segarally_native PRIVATE SEGARALLY_INPUT_RECORDING=1)
    target_link_libraries(segarally_native PRIVATE segarally_recording)
endif()
# Retain actual existing notices, including optional recording's public-domain
# SHA sources. Refuse packaging when an expected dependency notice is absent.
set(LICENSE_ARGS --output "${CMAKE_BINARY_DIR}/licenses"
    --sdl-source "${SEGARALLY_SDL_SOURCE}" --imgui-source "${SEGARALLY_IMGUI_SOURCE}")
if(SEGARALLY_INPUT_RECORDING AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    list(APPEND LICENSE_ARGS --lzma-source "${SEGARALLY_LZMA_SOURCE}")
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package_licenses.py" ${LICENSE_ARGS}
    RESULT_VARIABLE LICENSE_RESULT)
if(NOT LICENSE_RESULT EQUAL 0)
    message(FATAL_ERROR "Required distribution notices could not be retained")
endif()
add_custom_target(segarally_licenses
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package_licenses.py" ${LICENSE_ARGS}
    VERBATIM COMMENT "Retain project and dependency distribution notices")
add_dependencies(segarally_native segarally_licenses)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_testing()
    foreach(TEST_NAME mac_launcher recording_launcher host_assets staging_provenance package_licenses mac_pipeline)
        add_test(NAME segarally_${TEST_NAME}
            COMMAND "${Python3_EXECUTABLE}" "${ROOT}/tests/test_segarally_${TEST_NAME}.py")
    endforeach()
    add_executable(test_segarally_audio EXCLUDE_FROM_ALL "${ROOT}/tests/test_segarally_audio.cpp")
    target_include_directories(test_segarally_audio PRIVATE "${ROOT}/src")
    target_link_libraries(test_segarally_audio PRIVATE SDL3::SDL3-static)
    # Keep assert-based fixtures active even when the actual host is Release.
    target_compile_options(test_segarally_audio PRIVATE -UNDEBUG)
    add_test(NAME segarally_audio COMMAND test_segarally_audio "${CMAKE_CURRENT_BINARY_DIR}/audio-fixture.wav")
    add_executable(test_segarally_controls EXCLUDE_FROM_ALL
        "${ROOT}/tests/test_segarally_controls.cpp" "${ROOT}/src/app/segarally_controls.cpp")
    target_include_directories(test_segarally_controls PRIVATE "${ROOT}/src")
    target_link_libraries(test_segarally_controls PRIVATE SDL3::SDL3-static)
    target_compile_options(test_segarally_controls PRIVATE -UNDEBUG)
    add_test(NAME segarally_controls COMMAND test_segarally_controls)
    add_executable(test_segarally_display_preferences EXCLUDE_FROM_ALL "${ROOT}/tests/test_segarally_display_preferences.cpp")
    target_include_directories(test_segarally_display_preferences PRIVATE "${ROOT}/src")
    target_link_libraries(test_segarally_display_preferences PRIVATE SDL3::SDL3-static)
    add_test(NAME segarally_display_preferences COMMAND test_segarally_display_preferences)
    add_custom_command(TARGET segarally_native POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/stage_data.py"
            --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${ASSET_DEST}" ${STAGE_ARGS}
        VERBATIM COMMENT "Verify and package personally imported Sega Rally assets")
endif()
