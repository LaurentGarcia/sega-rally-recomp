cmake_minimum_required(VERSION 3.25)
project(SegaRallyNative LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()
option(SEGARALLY_FRONTEND_COMPILE_ONLY "ROM-free real-host compile/link fixture; NEVER a playable build" OFF)
if(SEGARALLY_FRONTEND_COMPILE_ONLY AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "The ROM-free frontend compile fixture is desktop-only")
endif()
set(HOST_TARGET segarally_native)
if(SEGARALLY_FRONTEND_COMPILE_ONLY)
    set(HOST_TARGET segarally_frontend_compile)
    message(STATUS "ROM-free frontend compile/link check: no guest program, assets, or playable executable")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(NATIVE_METAL_DEFAULT OFF)
if(APPLE)
    set(NATIVE_METAL_DEFAULT ON)
endif()
option(SEGARALLY_NATIVE_METAL "Build direct Metal player rendering on macOS and iOS" ${NATIVE_METAL_DEFAULT})
if(SEGARALLY_NATIVE_METAL AND NOT APPLE)
    message(FATAL_ERROR "Direct Metal player integration requires an Apple platform")
endif()
option(SEGARALLY_NATIVE_VULKAN "Build opt-in direct Vulkan player rendering on Windows/Linux" OFF)
if(SEGARALLY_NATIVE_VULKAN)
    if(APPLE)
        message(FATAL_ERROR "Direct Vulkan player integration targets Windows/Linux; Apple uses Metal")
    endif()
    find_package(Vulkan 1.1 REQUIRED)
endif()
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
if(SEGARALLY_NATIVE_METAL)
    target_sources(segarally_imgui PRIVATE "${SEGARALLY_IMGUI_SOURCE}/backends/imgui_impl_metal.mm")
    target_compile_options(segarally_imgui PRIVATE "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
    target_link_libraries(segarally_imgui PUBLIC "-framework Metal" "-framework Foundation")
endif()
if(SEGARALLY_NATIVE_VULKAN)
    target_sources(segarally_imgui PRIVATE "${SEGARALLY_IMGUI_SOURCE}/backends/imgui_impl_vulkan.cpp")
    target_link_libraries(segarally_imgui PUBLIC Vulkan::Vulkan)
endif()
file(GLOB GENERATED CONFIGURE_DEPENDS "${SEGARALLY_NATIVE_BUILD}/gen/*.cpp")
if(NOT SEGARALLY_FRONTEND_COMPILE_ONLY AND (NOT GENERATED OR NOT EXISTS "${SEGARALLY_NATIVE_BUILD}/tgp/program.cpp"))
    message(FATAL_ERROR "Generate the audited native main/TGP code before configuring the host")
endif()
set(RUNTIME cpu lockstep model2a_bus model2a_clock model2a_machine model2a_control model2_io model2_eeprom
    model2a_io model2a_comm comm_board model2_uart model2a_uart tgp m2_tgp_board model2a_video video raster geo)
set(SOURCES "${ROOT}/src/app/segarally_main.cpp" "${ROOT}/src/app/gpu/gpu_renderer.cpp" "${ROOT}/src/app/gpu/model2_frame.cpp"
    "${ROOT}/src/app/gpu/crt_renderer.cpp" "${ROOT}/src/app/segarally_display.cpp"
    "${ROOT}/src/app/segarally_controls.cpp"
    "${ROOT}/src/app/segarally_startup.cpp" "${ROOT}/src/app/segarally_rom_import.cpp"
    "${ROOT}/src/runtime/zip.cpp"
)
if(SEGARALLY_NATIVE_METAL)
    list(APPEND SOURCES "${ROOT}/src/app/native/metal/model2_metal.mm"
        "${ROOT}/src/app/native/metal/metal_crt.mm" "${ROOT}/src/app/native/metal/metal_presenter.mm"
        "${ROOT}/src/app/segarally_display_metal.mm")
endif()
if(SEGARALLY_NATIVE_VULKAN)
    list(APPEND SOURCES "${ROOT}/src/app/native/vulkan/model2_vulkan.cpp"
        "${ROOT}/src/app/native/vulkan/vulkan_presenter.cpp" "${ROOT}/src/app/segarally_display_vulkan.cpp")
endif()
if(SEGARALLY_FRONTEND_COMPILE_ONLY)
    list(APPEND SOURCES "${ROOT}/tests/fixtures/segarally_frontend_link.cpp")
else()
    list(APPEND SOURCES ${GENERATED} "${SEGARALLY_NATIVE_BUILD}/tgp/program.cpp")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(STAGE_ARGS)
if(SEGARALLY_INSTRUCTION_BOUNDARY_HOOK)
    list(APPEND STAGE_ARGS --instruction-boundary-hook)
endif()
if(SEGARALLY_SOUND)
    list(APPEND STAGE_ARGS --sound-build "${SEGARALLY_SOUND_BUILD}")
    list(APPEND RUNTIME model2a_sound model2a_sound_bus snd_cpu model2_scsp model2_scsp_dsp)
    if(NOT SEGARALLY_FRONTEND_COMPILE_ONLY)
        list(APPEND SOURCES "${SEGARALLY_SOUND_BUILD}/sound/native-timed/snd_gen.cpp")
    endif()
endif()
# Refuse capability/macro mismatches before compiling. This also checks the
# generated source/header inventory before the host rebuilds all native chunks.
if(NOT SEGARALLY_FRONTEND_COMPILE_ONLY)
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
endif()
foreach(NAME IN LISTS RUNTIME)
    list(APPEND SOURCES "${ROOT}/src/runtime/${NAME}.cpp")
endforeach()
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(SEGARALLY_IOS_TEAM "" CACHE STRING "Personal Apple development team identifier")
    set(SEGARALLY_IOS_BUNDLE_ID "com.lgarciacarro.segarally.native" CACHE STRING "Personal iOS bundle identifier")
    set(SEGARALLY_IOS_VERSION "0.1.0" CACHE STRING "iOS marketing version")
    set(SEGARALLY_IOS_BUILD_NUMBER "1" CACHE STRING "iOS build number")
    option(SEGARALLY_IOS_SIGNING "Sign the personal iOS app with Xcode" OFF)
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
    add_executable(${HOST_TARGET} MACOSX_BUNDLE ${SOURCES} ${ASSETS})
    add_custom_target(segarally_assets
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/stage_data.py"
            --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${ASSET_STAGE}" ${STAGE_ARGS}
        BYPRODUCTS ${ASSETS} VERBATIM COMMENT "Verify personally imported Sega Rally assets")
    add_dependencies(${HOST_TARGET} segarally_assets)
    target_compile_definitions(${HOST_TARGET} PRIVATE SEGARALLY_IOS=1)
    # The shared original PNG is the only committed icon. Xcode compiles the
    # derived asset catalog before signing; never inject resources after signing.
    set(ICON_SOURCE "${ROOT}/assets/branding/app-icon.png")
    set(ICON_BUILDER "${CMAKE_CURRENT_SOURCE_DIR}/build_icons.py")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ICON_SOURCE}" "${ICON_BUILDER}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${ICON_BUILDER}"
        --source "${ICON_SOURCE}" --output "${CMAKE_BINARY_DIR}/branding" --platform ios
        RESULT_VARIABLE ICON_RESULT OUTPUT_VARIABLE ICON_CATALOG OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT ICON_RESULT EQUAL 0 OR NOT EXISTS "${ICON_CATALOG}/AppIcon.appiconset/Contents.json")
        message(FATAL_ERROR "A valid shared 1024x1024 opaque app icon is required")
    endif()
    set_source_files_properties("${ICON_CATALOG}" PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources XCODE_EXPLICIT_FILE_TYPE folder.assetcatalog)
    target_sources(${HOST_TARGET} PRIVATE "${ICON_CATALOG}")
    add_custom_target(segarally_icons
        COMMAND "${Python3_EXECUTABLE}" "${ICON_BUILDER}"
            --source "${ICON_SOURCE}" --output "${CMAKE_BINARY_DIR}/branding" --platform ios
        VERBATIM COMMENT "Verify derived app icon resources")
    add_dependencies(${HOST_TARGET} segarally_icons)
    set_target_properties(${HOST_TARGET} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${SEGARALLY_IOS_BUNDLE_ID}"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
        XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
        XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES")
    if(SEGARALLY_IOS_SIGNING)
        if(NOT SEGARALLY_IOS_TEAM)
            message(FATAL_ERROR "Set SEGARALLY_IOS_TEAM for a signed personal iOS build")
        endif()
        set_target_properties(${HOST_TARGET} PROPERTIES
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${SEGARALLY_IOS_TEAM}")
    else()
        set_target_properties(${HOST_TARGET} PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
            XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
    endif()
    target_link_libraries(${HOST_TARGET} PRIVATE "-framework UIKit" "-framework Foundation")
else()
    add_executable(${HOST_TARGET} ${SOURCES})
    set(ASSET_DEST "$<TARGET_FILE_DIR:${HOST_TARGET}>/assets")
endif()
target_include_directories(${HOST_TARGET} PRIVATE "${ROOT}/src" "${ROOT}/src/app")
if(SEGARALLY_NATIVE_METAL)
    target_compile_definitions(${HOST_TARGET} PRIVATE SEGARALLY_NATIVE_METAL=1)
    target_compile_options(${HOST_TARGET} PRIVATE
        "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc;-fno-fast-math;-ffp-contract=off>")
endif()
if(SEGARALLY_NATIVE_VULKAN)
    target_compile_definitions(${HOST_TARGET} PRIVATE SEGARALLY_NATIVE_VULKAN=1)
    target_link_libraries(${HOST_TARGET} PRIVATE Vulkan::Vulkan)
endif()
target_compile_definitions(${HOST_TARGET} PRIVATE M2_SRALLY_TGP=1 M2_SRALLY_IO=1
    M2_SRALLY_COMM=1 M2_SRALLY_CONTROL=1 M2_SRALLY_VIDEO=1 M2_SRALLY_SCREEN=1 M2_SRALLY_UART=1 SDL_MAIN_HANDLED)
if(SEGARALLY_SOUND)
    target_compile_definitions(${HOST_TARGET} PRIVATE M2_SRALLY_SOUND=1)
endif()
if(SEGARALLY_INSTRUCTION_BOUNDARY_HOOK)
    # One target owns runtime AND generated C++: no stale prebuilt main objects.
    target_compile_definitions(${HOST_TARGET} PRIVATE M2_INSTRUCTION_BOUNDARY_HOOK=1)
endif()
if(NOT SEGARALLY_FRONTEND_COMPILE_ONLY AND (SEGARALLY_SOUND OR SEGARALLY_INSTRUCTION_BOUNDARY_HOOK))
    # Repeat provenance checks for incremental builds, before linking changed
    # native code. A successful older configure is insufficient evidence.
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        add_custom_target(segarally_native_verify
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/stage_data.py"
                --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${CMAKE_BINARY_DIR}/assets" ${STAGE_ARGS}
            VERBATIM)
        add_dependencies(${HOST_TARGET} segarally_native_verify)
    endif()
endif()
if(MSVC)
    target_compile_options(${HOST_TARGET} PRIVATE /fp:strict /bigobj /utf-8)
    target_compile_definitions(${HOST_TARGET} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(${HOST_TARGET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-fast-math;-ffp-contract=off>
        $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:-O3>)
endif()
target_link_libraries(${HOST_TARGET} PRIVATE SDL3::SDL3-static segarally_imgui)
if(APPLE)
    target_link_libraries(${HOST_TARGET} PRIVATE "-framework Metal" "-framework QuartzCore")
endif()
if(SEGARALLY_FRONTEND_COMPILE_ONLY)
    target_compile_definitions(${HOST_TARGET} PRIVATE SEGARALLY_FRONTEND_COMPILE_ONLY=1)
endif()
    set(SEGARALLY_LZMA_SOURCE "${ROOT}/extern/lzma" CACHE PATH "Pinned public-domain 7-Zip digest sources")
    execute_process(COMMAND git -C "${SEGARALLY_LZMA_SOURCE}" rev-parse HEAD
        OUTPUT_VARIABLE SHA_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE SHA_REVISION_STATUS)
    if(NOT SHA_REVISION_STATUS EQUAL 0 OR NOT SHA_REVISION STREQUAL "0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17")
        message(FATAL_ERROR "ROM import requires the pinned public-domain 7-Zip revision")
    endif()
    add_library(segarally_digests STATIC "${ROOT}/src/runtime/sha256_digest.cpp"
        "${SEGARALLY_LZMA_SOURCE}/C/Sha256.c" "${SEGARALLY_LZMA_SOURCE}/C/Sha256Opt.c" "${SEGARALLY_LZMA_SOURCE}/C/CpuArch.c"
        "${SEGARALLY_LZMA_SOURCE}/C/Sha1.c" "${SEGARALLY_LZMA_SOURCE}/C/Sha1Opt.c")
    target_include_directories(segarally_digests PUBLIC "${ROOT}/src")
    target_include_directories(segarally_digests SYSTEM PUBLIC "${SEGARALLY_LZMA_SOURCE}/C")
    set_target_properties(segarally_digests PROPERTIES C_STANDARD 11)
    target_link_libraries(${HOST_TARGET} PRIVATE segarally_digests)
if(SEGARALLY_INPUT_RECORDING AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_library(segarally_recording STATIC "${ROOT}/tools/srally_boot/recording_bundle.cpp")
    target_link_libraries(segarally_recording PUBLIC segarally_digests)
    target_include_directories(segarally_recording PUBLIC "${ROOT}/src")
    target_include_directories(segarally_recording SYSTEM PUBLIC "${SEGARALLY_LZMA_SOURCE}/C")
    if(MSVC)
        target_compile_options(segarally_recording PRIVATE /fp:strict /utf-8)
    else()
        target_compile_options(segarally_recording PRIVATE -fno-fast-math -ffp-contract=off)
    endif()
    set_target_properties(segarally_recording PROPERTIES C_STANDARD 11)
    if(SEGARALLY_INSTRUCTION_BOUNDARY_HOOK)
        target_compile_definitions(segarally_recording PRIVATE M2_INSTRUCTION_BOUNDARY_HOOK=1)
    endif()
    target_compile_definitions(${HOST_TARGET} PRIVATE SEGARALLY_INPUT_RECORDING=1)
    target_link_libraries(${HOST_TARGET} PRIVATE segarally_recording)
endif()
# Retain actual existing notices, including optional recording's public-domain
# SHA sources. Refuse packaging when an expected dependency notice is absent.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # Retain the same eight reviewed notices as the personal Mac package.
    set(SEGARALLY_LZMA_SOURCE "${ROOT}/extern/lzma" CACHE PATH "Pinned public-domain 7-Zip notice sources")
endif()
set(LICENSE_ARGS --output "${CMAKE_BINARY_DIR}/licenses"
    --sdl-source "${SEGARALLY_SDL_SOURCE}" --imgui-source "${SEGARALLY_IMGUI_SOURCE}")
list(APPEND LICENSE_ARGS --lzma-source "${SEGARALLY_LZMA_SOURCE}")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package_licenses.py" ${LICENSE_ARGS}
    RESULT_VARIABLE LICENSE_RESULT)
if(NOT LICENSE_RESULT EQUAL 0)
    message(FATAL_ERROR "Required distribution notices could not be retained")
endif()
add_custom_target(segarally_licenses
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package_licenses.py" ${LICENSE_ARGS}
    VERBATIM COMMENT "Retain project and dependency distribution notices")
add_dependencies(${HOST_TARGET} segarally_licenses)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IOS_NOTICES)
    foreach(NAME Project-BSD-3-Clause.txt THIRD_PARTY.md SDL.txt ImGui.txt
                 ImGui-embedded-fonts.txt Source-notices.txt
                 7-Zip-License.txt 7-Zip-selected-sources.txt)
        list(APPEND IOS_NOTICES "${CMAKE_BINARY_DIR}/licenses/${NAME}")
    endforeach()
    # Copy resources as signed Xcode inputs; never modify the app after signing.
    set_source_files_properties(${IOS_NOTICES} PROPERTIES MACOSX_PACKAGE_LOCATION licenses)
    target_sources(${HOST_TARGET} PRIVATE ${IOS_NOTICES})
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_testing()
    if(SEGARALLY_NATIVE_VULKAN AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(X11 REQUIRED)
        add_executable(test_native_vulkan_blit EXCLUDE_FROM_ALL "${ROOT}/tests/test_native_vulkan_blit.cpp"
            "${ROOT}/src/app/native/vulkan/model2_vulkan.cpp" "${ROOT}/src/app/native/vulkan/vulkan_presenter.cpp"
            "${ROOT}/src/app/gpu/model2_frame.cpp" "${ROOT}/src/app/gpu/gpu_renderer.cpp"
            "${ROOT}/src/runtime/video.cpp" "${ROOT}/src/runtime/raster.cpp")
        target_include_directories(test_native_vulkan_blit PRIVATE "${ROOT}/src")
        target_compile_options(test_native_vulkan_blit PRIVATE -Wall -Wextra -Werror
            -Wno-missing-field-initializers -Wno-misleading-indentation -fno-fast-math -ffp-contract=off)
        target_link_libraries(test_native_vulkan_blit PRIVATE SDL3::SDL3-static Vulkan::Vulkan X11::X11)
        add_executable(test_segarally_display_vulkan EXCLUDE_FROM_ALL "${ROOT}/tests/test_segarally_display_vulkan.cpp"
            "${ROOT}/src/app/segarally_display.cpp" "${ROOT}/src/app/segarally_display_vulkan.cpp"
            "${ROOT}/src/app/segarally_controls.cpp" "${ROOT}/src/app/native/vulkan/model2_vulkan.cpp"
            "${ROOT}/src/app/native/vulkan/vulkan_presenter.cpp" "${ROOT}/src/app/gpu/model2_frame.cpp"
            "${ROOT}/src/runtime/video.cpp" "${ROOT}/src/runtime/raster.cpp")
        target_compile_definitions(test_segarally_display_vulkan PRIVATE SEGARALLY_NATIVE_VULKAN=1)
        target_include_directories(test_segarally_display_vulkan PRIVATE "${ROOT}/src")
        target_compile_options(test_segarally_display_vulkan PRIVATE -Wall -Wextra -Werror
            -Wno-missing-field-initializers -Wno-misleading-indentation -fno-fast-math -ffp-contract=off)
        target_link_libraries(test_segarally_display_vulkan PRIVATE SDL3::SDL3-static segarally_imgui Vulkan::Vulkan X11::X11)
    endif()
    add_executable(test_segarally_rom_import EXCLUDE_FROM_ALL "${ROOT}/tests/test_segarally_rom_import.cpp" "${ROOT}/src/runtime/zip.cpp")
    target_include_directories(test_segarally_rom_import PRIVATE "${ROOT}/src")
    target_link_libraries(test_segarally_rom_import PRIVATE segarally_digests)
    add_test(NAME segarally_rom_import COMMAND test_segarally_rom_import "${CMAKE_CURRENT_BINARY_DIR}/rom-import-fixtures")
    add_executable(test_segarally_platform_files EXCLUDE_FROM_ALL "${ROOT}/tests/test_segarally_platform_files.cpp")
    target_include_directories(test_segarally_platform_files PRIVATE "${ROOT}/src")
    add_test(NAME segarally_platform_files COMMAND test_segarally_platform_files "${CMAKE_CURRENT_BINARY_DIR}/platform-files-fixture")
    add_executable(test_segarally_input_replay EXCLUDE_FROM_ALL "${ROOT}/tests/test_segarally_input_replay.cpp")
    target_include_directories(test_segarally_input_replay PRIVATE "${ROOT}" "${ROOT}/src")
    add_test(NAME segarally_input_replay COMMAND test_segarally_input_replay)
    foreach(TEST_NAME mac_launcher recording_launcher host_assets staging_provenance package_licenses mac_pipeline)
        add_test(NAME segarally_${TEST_NAME}
            COMMAND "${Python3_EXECUTABLE}" "${ROOT}/tests/test_segarally_${TEST_NAME}.py")
    endforeach()
    add_executable(test_segarally_audio EXCLUDE_FROM_ALL "${ROOT}/tests/test_segarally_audio.cpp")
    target_include_directories(test_segarally_audio PRIVATE "${ROOT}/src")
    target_link_libraries(test_segarally_audio PRIVATE SDL3::SDL3-static)
    # Keep assert-based fixtures active even when the actual host is Release.
    if(MSVC)
        set(ASSERT_FLAGS /UNDEBUG)
    else()
        set(ASSERT_FLAGS -UNDEBUG)
    endif()
    target_compile_options(test_segarally_audio PRIVATE ${ASSERT_FLAGS})
    add_test(NAME segarally_audio COMMAND test_segarally_audio "${CMAKE_CURRENT_BINARY_DIR}/audio-fixture.wav")
    add_executable(test_segarally_audio_output_capture EXCLUDE_FROM_ALL "${ROOT}/tests/test_segarally_audio_output_capture.cpp")
    target_include_directories(test_segarally_audio_output_capture PRIVATE "${ROOT}/src")
    target_link_libraries(test_segarally_audio_output_capture PRIVATE SDL3::SDL3-static)
    target_compile_options(test_segarally_audio_output_capture PRIVATE ${ASSERT_FLAGS})
    add_test(NAME segarally_audio_output_capture COMMAND test_segarally_audio_output_capture "${CMAKE_CURRENT_BINARY_DIR}/audio-output-fixture")
    add_executable(test_segarally_controls EXCLUDE_FROM_ALL
        "${ROOT}/tests/test_segarally_controls.cpp" "${ROOT}/src/app/segarally_controls.cpp")
    target_include_directories(test_segarally_controls PRIVATE "${ROOT}/src")
    target_link_libraries(test_segarally_controls PRIVATE SDL3::SDL3-static)
    target_compile_options(test_segarally_controls PRIVATE ${ASSERT_FLAGS})
    add_test(NAME segarally_controls COMMAND test_segarally_controls)
    add_executable(test_segarally_display_preferences EXCLUDE_FROM_ALL "${ROOT}/tests/test_segarally_display_preferences.cpp")
    target_include_directories(test_segarally_display_preferences PRIVATE "${ROOT}/src")
    target_link_libraries(test_segarally_display_preferences PRIVATE SDL3::SDL3-static)
    add_test(NAME segarally_display_preferences COMMAND test_segarally_display_preferences)
    if(NOT SEGARALLY_FRONTEND_COMPILE_ONLY)
    add_custom_command(TARGET ${HOST_TARGET} POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/stage_data.py"
            --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${ASSET_DEST}" ${STAGE_ARGS}
        VERBATIM COMMENT "Verify and package personally imported Sega Rally assets")
    endif()
endif()
if(SEGARALLY_FRONTEND_COMPILE_ONLY)
    add_test(NAME segarally_frontend_fixture_refuses_game
        COMMAND "${Python3_EXECUTABLE}" "${ROOT}/tests/test_segarally_frontend_fixture.py" "$<TARGET_FILE:${HOST_TARGET}>")
endif()
