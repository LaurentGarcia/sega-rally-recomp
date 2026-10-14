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
file(GLOB GENERATED CONFIGURE_DEPENDS "${SEGARALLY_NATIVE_BUILD}/gen/*.cpp")
if(NOT GENERATED OR NOT EXISTS "${SEGARALLY_NATIVE_BUILD}/tgp/program.cpp")
    message(FATAL_ERROR "Generate the audited native main/TGP code before configuring the host")
endif()
set(RUNTIME cpu lockstep model2a_bus model2a_clock model2a_machine model2a_control model2_io model2_eeprom
    model2a_io model2a_comm comm_board model2_uart model2a_uart tgp m2_tgp_board model2a_video video raster geo)
set(SOURCES "${ROOT}/src/app/segarally_main.cpp" "${ROOT}/src/app/gpu/gpu_renderer.cpp"
    ${GENERATED} "${SEGARALLY_NATIVE_BUILD}/tgp/program.cpp")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(SOUND_STAGE_ARGS)
if(SEGARALLY_SOUND)
    list(APPEND SOUND_STAGE_ARGS --sound-build "${SEGARALLY_SOUND_BUILD}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/stage_data.py"
        --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${CMAKE_BINARY_DIR}/assets" ${SOUND_STAGE_ARGS}
        RESULT_VARIABLE SOUND_VERIFY_RESULT)
    if(NOT SOUND_VERIFY_RESULT EQUAL 0)
        message(FATAL_ERROR "Verified native sound generation/assets required")
    endif()
    list(APPEND RUNTIME model2a_sound model2a_sound_bus snd_cpu model2_scsp model2_scsp_dsp)
    list(APPEND SOURCES "${SEGARALLY_SOUND_BUILD}/sound/native-timed/snd_gen.cpp")
endif()
foreach(NAME IN LISTS RUNTIME)
    list(APPEND SOURCES "${ROOT}/src/runtime/${NAME}.cpp")
endforeach()
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # Actual Xcode resource inputs establish copy/sign dependencies, including
    # incremental builds where imported data changes without a relink.
    set(ASSET_STAGE "${CMAKE_BINARY_DIR}/assets")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/stage_data.py"
        --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${ASSET_STAGE}" ${SOUND_STAGE_ARGS}
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
            --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${ASSET_STAGE}" ${SOUND_STAGE_ARGS}
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
    # Repeat provenance checks for incremental builds, before linking changed
    # native code. A successful older configure is insufficient evidence.
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        add_custom_target(segarally_sound_verify
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/stage_data.py"
                --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${CMAKE_BINARY_DIR}/assets" ${SOUND_STAGE_ARGS}
            VERBATIM)
        add_dependencies(segarally_native segarally_sound_verify)
    endif()
endif()
target_compile_options(segarally_native PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-fast-math;-ffp-contract=off>
    $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:-O3>)
target_link_libraries(segarally_native PRIVATE SDL3::SDL3-static "-framework Metal" "-framework QuartzCore")
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_testing()
    add_executable(test_segarally_audio EXCLUDE_FROM_ALL "${ROOT}/tests/test_segarally_audio.cpp")
    target_include_directories(test_segarally_audio PRIVATE "${ROOT}/src")
    target_link_libraries(test_segarally_audio PRIVATE SDL3::SDL3-static)
    # Keep assert-based fixtures active even when the actual host is Release.
    target_compile_options(test_segarally_audio PRIVATE -UNDEBUG)
    add_test(NAME segarally_audio COMMAND test_segarally_audio "${CMAKE_CURRENT_BINARY_DIR}/audio-fixture.wav")
    add_custom_command(TARGET segarally_native POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/stage_data.py"
            --native-build "${SEGARALLY_NATIVE_BUILD}" --output "${ASSET_DEST}" ${SOUND_STAGE_ARGS}
        VERBATIM COMMENT "Verify and package personally imported Sega Rally assets")
endif()
