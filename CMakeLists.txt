cmake_minimum_required(VERSION 3.20)
project(sega_rally_recomp LANGUAGES C CXX)
if(IOS)
    enable_language(OBJCXX)
endif()

# Mobile frontends use the same runtime/app code as desktop. Android needs
# SDL's shared JNI library; iOS uses the static library in an app bundle.
set(M2_MOBILE OFF)
if(ANDROID OR IOS)
    set(M2_MOBILE ON)
endif()
set(M2_GEN_ROOT "${CMAKE_BINARY_DIR}/gen" CACHE PATH
    "Directory containing recompiled daytona93/daytona93_tgp/daytona93_snd sources")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # /bigobj: the generated game code has many labels per object file
    add_compile_options(/W4 /fp:strict /bigobj /utf-8)
else()
    # No -ffast-math, ever (rules.md): exact rounding is the point. No FP
    # contraction either: a fused multiply-add (ARM64 always has one) rounds
    # once where the TGP's code rounds twice.
    add_compile_options(-Wall -Wextra -fno-fast-math -ffp-contract=off)
endif()

# Berkeley SoftFloat 3e (scripts/fetch_softfloat.sh): the extF80 reference
# model for i960 FP. 8086-SSE specialisation, FAST_INT64 build.
set(SOFTFLOAT_DIR ${CMAKE_SOURCE_DIR}/extern/softfloat)
include(cmake/softfloat_sources.cmake)
add_library(softfloat STATIC ${SOFTFLOAT_SOURCES})
# SoftFloat's GCC platform header needs __int128, __builtin_clz and GNU inline
# rules, which MSVC rejects (and clang-cl's are not GCC's). Those compilers get
# our portable one (cmake/softfloat-portable): same results, plain functions.
# M2_SOFTFLOAT_PORTABLE forces it anywhere, to test that path off Windows.
option(M2_SOFTFLOAT_PORTABLE "Build SoftFloat without GCC extensions" OFF)
if(MSVC OR M2_SOFTFLOAT_PORTABLE)
    set(SOFTFLOAT_PLATFORM ${CMAKE_SOURCE_DIR}/cmake/softfloat-portable)
    set(SOFTFLOAT_INLINE)
else()
    set(SOFTFLOAT_PLATFORM ${SOFTFLOAT_DIR}/build/Linux-x86_64-GCC)
    set(SOFTFLOAT_INLINE INLINE_LEVEL=5)
endif()
target_include_directories(softfloat
    PUBLIC ${SOFTFLOAT_DIR}/source/include
    PRIVATE ${SOFTFLOAT_PLATFORM} ${SOFTFLOAT_DIR}/source/8086-SSE)
target_compile_definitions(softfloat PRIVATE
    SOFTFLOAT_ROUND_ODD ${SOFTFLOAT_INLINE} SOFTFLOAT_FAST_DIV32TO16 SOFTFLOAT_FAST_DIV64TO32)
# FAST_INT64 selects the by-value extF80 API and LITTLEENDIAN the field order of
# extFloat80_t in the public headers, so users must see both (a mismatch silently
# swaps signExp and signif across the call boundary). Little-endian hosts only.
# Rounding mode and flags are globals in SoftFloat: make them per thread.
# C++ users get __thread, not thread_local: on macOS an extern thread_local is
# reached through a C++ wrapper function the C definitions never emit. MSVC and
# clang-cl (Windows TLS has no such wrapper) use __declspec(thread) in C, which
# both accept where MSVC's C may not take _Thread_local, and thread_local in C++.
if(MSVC)
    set(SOFTFLOAT_TLS_C "__declspec(thread)")
    set(SOFTFLOAT_TLS_CXX thread_local)
else()
    set(SOFTFLOAT_TLS_C _Thread_local)
    set(SOFTFLOAT_TLS_CXX __thread)
endif()
target_compile_definitions(softfloat PUBLIC SOFTFLOAT_FAST_INT64 LITTLEENDIAN=1
    $<$<COMPILE_LANGUAGE:C>:THREAD_LOCAL=${SOFTFLOAT_TLS_C}>
    $<$<COMPILE_LANGUAGE:CXX>:THREAD_LOCAL=${SOFTFLOAT_TLS_CXX}>)
set_target_properties(softfloat PROPERTIES C_STANDARD 11)
if(NOT MSVC)
    target_compile_options(softfloat PRIVATE -w) # upstream code, not ours
endif()

add_library(i960 STATIC
    src/i960/isa.cpp
    src/i960/decode.cpp
    src/i960/reach.cpp
    src/i960/timing.cpp
    src/i960/fp.cpp
)
target_include_directories(i960 PUBLIC src)
target_link_libraries(i960 PUBLIC softfloat)

add_library(trace STATIC src/trace/trace.cpp)
target_include_directories(trace PUBLIC src)

# Runtime the recompiled game runs on: i960 context and services (call/ret,
# interrupts, memory; MAME's semantics, transplanted, BSD-3), replay bus and
# lockstep. No instruction interpreter.
# The ROM set the game is recompiled from: daytona93 (Daytona USA Deluxe '93,
# the default) or daytona (Revision A, 1994). One set per build directory:
# its generated code, seeds and saves are its own.
set(M2_ROMSET daytona93 CACHE STRING "MAME ROM set to recompile: daytona93 or daytona")
set_property(CACHE M2_ROMSET PROPERTY STRINGS daytona93 daytona)
if(NOT M2_ROMSET MATCHES "^(daytona93|daytona)$")
    message(FATAL_ERROR "M2_ROMSET must be daytona93 or daytona, not ${M2_ROMSET}")
endif()

add_library(runtime STATIC
    src/runtime/cpu.cpp
    src/runtime/tgp.cpp
    src/runtime/m2_tgp_board.cpp
    src/runtime/geo.cpp
    src/runtime/raster.cpp
    src/runtime/video.cpp
    src/runtime/m2_board.cpp
    src/runtime/model2a_bus.cpp
    src/runtime/model2_io.cpp
    src/runtime/model2a_io.cpp
    src/runtime/model2_eeprom.cpp
    src/runtime/model2a_comm.cpp
    src/runtime/model2a_control.cpp
    src/runtime/model2a_clock.cpp
    src/runtime/model2a_video.cpp
    src/runtime/model2_uart.cpp
    src/runtime/model2a_uart.cpp
    src/runtime/game_loop.cpp
    src/runtime/zip.cpp
    src/runtime/rom_import.cpp
    src/runtime/archive.cpp
    src/runtime/m2_replay_bus.cpp
    src/runtime/lockstep.cpp
    src/runtime/snd_cpu.cpp
    src/runtime/multipcm.cpp
    src/runtime/sound_board.cpp
    src/runtime/native_sample_mixer.cpp
    src/runtime/native_sound_sequencer.cpp
    src/runtime/native_sound_engine.cpp
    src/runtime/enhance.cpp
    src/runtime/comm_board.cpp
)
target_include_directories(runtime PUBLIC src)
target_compile_definitions(runtime PUBLIC M2_ROMSET="${M2_ROMSET}")
# 7z ROM sets: the 7-Zip LZMA SDK's decoder (public-domain files only),
# fetched by scripts/setup.py into extern/lzma.
set(LZMA_DIR ${CMAKE_SOURCE_DIR}/extern/lzma/C)
if(EXISTS ${LZMA_DIR}/7zArcIn.c)
    add_library(lzma7z STATIC
        ${LZMA_DIR}/7zArcIn.c ${LZMA_DIR}/7zDec.c ${LZMA_DIR}/7zBuf.c ${LZMA_DIR}/7zCrc.c ${LZMA_DIR}/7zCrcOpt.c
        ${LZMA_DIR}/7zStream.c ${LZMA_DIR}/7zAlloc.c ${LZMA_DIR}/Alloc.c ${LZMA_DIR}/LzmaDec.c ${LZMA_DIR}/Lzma2Dec.c
        ${LZMA_DIR}/Bra.c ${LZMA_DIR}/Bra86.c ${LZMA_DIR}/BraIA64.c ${LZMA_DIR}/Bcj2.c ${LZMA_DIR}/Delta.c
        ${LZMA_DIR}/CpuArch.c ${LZMA_DIR}/Ppmd7.c ${LZMA_DIR}/Ppmd7Dec.c)
    target_include_directories(lzma7z SYSTEM PUBLIC ${LZMA_DIR})
    target_compile_definitions(lzma7z PRIVATE Z7_PPMD_SUPPORT) # 7z's PPMd method too, not only LZMA/LZMA2
    if(NOT MSVC)
        target_compile_options(lzma7z PRIVATE -w)
    endif()
    target_link_libraries(runtime PUBLIC lzma7z)
    target_compile_definitions(runtime PRIVATE M2_HAVE_7Z)
    # Reuse the pinned public-domain SDK for capture identities. The wrapper
    # selects its software implementation through the public per-context API.
    add_library(sha256_digest STATIC src/runtime/sha256_digest.cpp
        ${LZMA_DIR}/Sha256.c ${LZMA_DIR}/Sha256Opt.c ${LZMA_DIR}/CpuArch.c)
    target_include_directories(sha256_digest PUBLIC src)
    target_include_directories(sha256_digest SYSTEM PUBLIC ${LZMA_DIR})
    set_target_properties(sha256_digest PROPERTIES C_STANDARD 11)
else()
    message(STATUS "extern/lzma not found: 7z ROM sets unsupported (run scripts/setup.py)")
endif()
target_link_libraries(runtime PUBLIC trace i960)

# YM3438 (the sound board's FM chip): ymfm, BSD-3, fetched by
# scripts/setup.py into extern/ymfm.
set(YMFM_DIR ${CMAKE_SOURCE_DIR}/extern/ymfm/src)
if(NOT EXISTS ${YMFM_DIR}/ymfm_opn.cpp)
    message(FATAL_ERROR "extern/ymfm not found: run scripts/setup.py")
endif()
add_library(ymfm STATIC ${YMFM_DIR}/ymfm_opn.cpp ${YMFM_DIR}/ymfm_adpcm.cpp ${YMFM_DIR}/ymfm_ssg.cpp)
target_include_directories(ymfm SYSTEM PUBLIC ${YMFM_DIR})
if(NOT MSVC)
    target_compile_options(ymfm PRIVATE -w)
endif()
target_link_libraries(runtime PUBLIC ymfm)

# Reference interpreter (MAME's i960 executor, transplanted, BSD-3). Test
# oracle only: m2replay links it; the game never does.
add_library(refcore STATIC src/refcore/i960_ref.cpp)
target_link_libraries(refcore PUBLIC runtime)
if(NOT MSVC)
    # Upstream MAME code: keep its own warnings out of ours.
    set_source_files_properties(src/runtime/cpu.cpp src/refcore/i960_ref.cpp src/runtime/geo.cpp PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-sign-compare;-Wno-unused-but-set-variable;-Wno-attributes")
endif()

add_executable(m2replay tools/m2replay/main.cpp)
target_link_libraries(m2replay PRIVATE refcore)

# The recompiler (ours; committed) and, when it has been run, its output.
add_executable(m2recomp tools/m2recomp/main.cpp)
target_link_libraries(m2recomp PRIVATE i960)

# The sound CPU (68000) decoder and static recompiler.
add_library(m68k STATIC src/m68k/decode.cpp src/m68k/reach.cpp src/m68k/timing.cpp)
target_include_directories(m68k PUBLIC src)
add_executable(m2sndrecomp tools/m2sndrecomp/main.cpp)
target_link_libraries(m2sndrecomp PRIVATE m68k)

# The windowed app: SDL3 (window, input, SDL_GPU: Vulkan, Direct3D 12, Metal),
# fetched by scripts/setup.py into extern/sdl3, Dear ImGui for the launcher,
# and the app's own code. Built whenever SDL is there, ROM set or not, so CI
# compiles all of it; the daytona executable itself also needs the generated
# game code (below).
option(M2_BUILD_WINDOWED_APP "Build the inherited Daytona SDL3 launcher and windowed app" ON)
option(M2_METAL_TESTS "Build and register tests that require a working macOS Metal device" OFF)
if((M2_BUILD_WINDOWED_APP OR M2_METAL_TESTS) AND EXISTS ${CMAKE_SOURCE_DIR}/extern/sdl3/CMakeLists.txt)
    if(ANDROID)
        # SDLActivity loads libSDL3.so before the game's libmain.so.
        set(M2_SDL_TARGET SDL3::SDL3-shared)
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    else()
        set(M2_SDL_TARGET SDL3::SDL3-static)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
    endif()
    if(NOT TARGET ${M2_SDL_TARGET})
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        # SDL builds with its own flags, not ours: MSVC's /fp:strict (our FP
        # rule, for our code) rejects SDL's float constant tables (C2099).
        get_directory_property(M2_COMPILE_OPTIONS COMPILE_OPTIONS)
        set_directory_properties(PROPERTIES COMPILE_OPTIONS "")
        add_subdirectory(${CMAKE_SOURCE_DIR}/extern/sdl3 ${CMAKE_BINARY_DIR}/sdl3 EXCLUDE_FROM_ALL SYSTEM)
        set_directory_properties(PROPERTIES COMPILE_OPTIONS "${M2_COMPILE_OPTIONS}")
    endif()
endif()

if(M2_BUILD_WINDOWED_APP AND EXISTS ${CMAKE_SOURCE_DIR}/extern/sdl3/CMakeLists.txt)
    # Dear ImGui (launcher UI), with its SDL3 and SDL_GPU backends
    if(NOT ANDROID AND NOT IOS)
        add_executable(test_gpu_startup tests/test_gpu_startup.cpp)
        target_include_directories(test_gpu_startup PRIVATE src ${CMAKE_SOURCE_DIR}/extern/sdl3/include)
        add_test(NAME gpu_startup COMMAND test_gpu_startup)
    endif()
    set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/extern/imgui)
    add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp)
    target_include_directories(imgui SYSTEM PUBLIC ${IMGUI_DIR})
    target_link_libraries(imgui PUBLIC ${M2_SDL_TARGET})
    if(NOT MSVC)
        target_compile_options(imgui PRIVATE -w)
    endif()
    # The hardware renderer (SDL_GPU), shared by the app and m2gpushot.
    add_library(gpurender STATIC src/app/gpu/gpu_renderer.cpp)
    target_include_directories(gpurender PUBLIC src)
    target_link_libraries(gpurender PUBLIC runtime ${M2_SDL_TARGET})
    add_library(daytona_app OBJECT src/app/main.cpp src/app/launcher.cpp src/app/config.cpp src/app/controls.cpp src/app/ffb.cpp src/app/link_socket.cpp src/app/pacing.cpp)
    target_link_libraries(daytona_app PRIVATE runtime imgui gpurender ${M2_SDL_TARGET})
    if(WIN32)
        target_link_libraries(daytona_app PUBLIC ws2_32) # link play's sockets
    endif()
    if(IOS)
        target_sources(daytona_app PRIVATE platform/mobile/ios/rom_picker.mm)
        set_source_files_properties(platform/mobile/ios/rom_picker.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
        target_include_directories(daytona_app PRIVATE platform/mobile/ios)
    endif()
    if(M2_MOBILE)
        target_compile_definitions(daytona_app PRIVATE M2_MOBILE=1)
    endif()
endif()

# Recompiled game code lives in build/gen/<set>/ (derived from the game: never
# committed). scripts/recompile.sh generates it and re-runs CMake.
set(GEN_DIR ${M2_GEN_ROOT}/${M2_ROMSET})
set(TGP_GEN ${M2_GEN_ROOT}/${M2_ROMSET}_tgp/tgp_gen.cpp)
set(SND_GEN ${M2_GEN_ROOT}/${M2_ROMSET}_snd/snd_gen.cpp)
if(EXISTS ${GEN_DIR}/gen_table.cpp AND EXISTS ${TGP_GEN} AND EXISTS ${SND_GEN})
    file(GLOB GEN_SOURCES ${GEN_DIR}/*.cpp)
    add_executable(m2native tools/m2native/main.cpp ${GEN_SOURCES} ${TGP_GEN} ${SND_GEN})
    target_link_libraries(m2native PRIVATE runtime)
    # The game on its own: native board runtime, no trace.
    add_executable(m2run tools/m2run/main.cpp src/app/link_socket.cpp ${GEN_SOURCES} ${TGP_GEN} ${SND_GEN})
    target_link_libraries(m2run PRIVATE runtime)
    if(WIN32)
        target_link_libraries(m2run PRIVATE ws2_32) # link play's sockets
    endif()

    # The game in a window (needs the app objects below).
    if(TARGET daytona_app)
        if(ANDROID)
            # SDLActivity loads this as libmain.so and calls SDL_main from it.
            add_library(daytona SHARED $<TARGET_OBJECTS:daytona_app> ${GEN_SOURCES} ${TGP_GEN} ${SND_GEN})
            set_target_properties(daytona PROPERTIES OUTPUT_NAME main)
        elseif(IOS)
            add_executable(daytona MACOSX_BUNDLE $<TARGET_OBJECTS:daytona_app> ${GEN_SOURCES} ${TGP_GEN} ${SND_GEN})
            set_target_properties(daytona PROPERTIES
                MACOSX_BUNDLE_BUNDLE_NAME "Daytona USA"
                MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/platform/mobile/ios/Info.plist.in"
                MACOSX_BUNDLE_GUI_IDENTIFIER "com.boucydesigns.daytona"
                MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
                MACOSX_BUNDLE_BUNDLE_VERSION "1"
                XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.boucydesigns.daytona"
                XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
                XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
                XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
        else()
            add_executable(daytona WIN32 $<TARGET_OBJECTS:daytona_app> ${GEN_SOURCES} ${TGP_GEN} ${SND_GEN})
        endif()
        target_link_libraries(daytona PRIVATE runtime imgui gpurender ${M2_SDL_TARGET})
        if(WIN32)
            target_link_libraries(daytona PRIVATE ws2_32) # link play's sockets
        endif()
        # Headless frames through the hardware renderer, to compare with m2run's.
        add_executable(m2gpushot tools/m2gpushot/main.cpp ${GEN_SOURCES} ${TGP_GEN} ${SND_GEN})
        target_link_libraries(m2gpushot PRIVATE runtime gpurender)
    else()
        message(STATUS "extern/sdl3 not found: skipping the windowed game (run scripts/setup.py)")
    endif()
    if(NOT MSVC)
        set_source_files_properties(${GEN_SOURCES} ${TGP_GEN} ${SND_GEN} PROPERTIES COMPILE_OPTIONS "-Wno-unused-label")
    endif()
endif()

# The TGP recompiler and, when it has been run, the lockstep check of its
# output against MAME's TGP log.
# The ROM set importer (zip or 7z) for the build.
add_executable(m2import tools/m2import/main.cpp)
target_link_libraries(m2import PRIVATE runtime)

add_executable(m2tgprecomp tools/m2tgprecomp/main.cpp)
target_include_directories(m2tgprecomp PRIVATE src)
if(EXISTS ${TGP_GEN})
    add_executable(m2tgpcheck tools/m2tgpcheck/main.cpp ${TGP_GEN})
    target_link_libraries(m2tgpcheck PRIVATE runtime)
    # The per-instruction hook exists only in this test build.
    target_compile_definitions(m2tgpcheck PRIVATE M2TGP_WITH_HOOK)
    if(NOT MSVC)
        set_source_files_properties(${TGP_GEN} PROPERTIES COMPILE_OPTIONS "-Wno-unused-label")
    endif()
endif()

# The lockstep check of the recompiled sound 68000 against MAME's log
# (M2TRACE_SNDLOG, patches/mame/0003).
if(EXISTS ${SND_GEN})
    add_executable(m2sndcheck tools/m2sndcheck/main.cpp ${SND_GEN})
    target_link_libraries(m2sndcheck PRIVATE runtime)
    if(NOT MSVC)
        set_source_files_properties(${SND_GEN} PROPERTIES COMPILE_OPTIONS "-Wno-unused-label")
    endif()
endif()

add_executable(i960dis tools/i960dis/main.cpp)
target_link_libraries(i960dis PRIVATE i960)

add_executable(tracediff tools/tracediff/main.cpp)
target_link_libraries(tracediff PRIVATE trace)

add_executable(tracedev tools/tracedev/main.cpp)
target_link_libraries(tracedev PRIVATE trace)

add_executable(traceinfo tools/traceinfo/main.cpp)
target_link_libraries(traceinfo PRIVATE trace)

enable_testing()

add_executable(test_scenery tests/test_scenery.cpp src/runtime/enhance.cpp)
target_include_directories(test_scenery PRIVATE src)
add_test(NAME scenery COMMAND test_scenery)

# Geometrizer clipping and display-list streaming (shared runtime code).
add_executable(test_geo_clip tests/test_geo_clip.cpp)
target_include_directories(test_geo_clip PRIVATE src)
add_test(NAME geo_clip COMMAND test_geo_clip)

add_executable(test_geo_stream tests/test_geo_stream.cpp)
target_include_directories(test_geo_stream PRIVATE src)
add_test(NAME geo_stream COMMAND test_geo_stream)

add_executable(test_model2_priority tests/test_model2_priority.c)
target_include_directories(test_model2_priority PRIVATE src)
add_test(NAME model2_priority COMMAND test_model2_priority)

add_executable(test_model2a_bus tests/test_model2a_bus.cpp src/runtime/model2a_bus.cpp src/runtime/cpu.cpp)
target_include_directories(test_model2a_bus PRIVATE src)
add_test(NAME model2a_bus COMMAND test_model2a_bus)

add_executable(test_model2_io tests/test_model2_io.cpp src/runtime/model2_io.cpp)
target_include_directories(test_model2_io PRIVATE src)
add_test(NAME model2_io COMMAND test_model2_io)

add_executable(test_model2_eeprom tests/test_model2_eeprom.cpp src/runtime/model2_eeprom.cpp)
target_include_directories(test_model2_eeprom PRIVATE src)
add_test(NAME model2_eeprom COMMAND test_model2_eeprom)

add_executable(test_model2a_io tests/test_model2a_io.cpp
    src/runtime/model2_io.cpp src/runtime/model2a_io.cpp src/runtime/model2_eeprom.cpp
    src/runtime/model2a_bus.cpp src/runtime/cpu.cpp)
target_include_directories(test_model2a_io PRIVATE src)
add_test(NAME model2a_io COMMAND test_model2a_io)

add_executable(test_i960_timing tests/test_i960_timing.cpp)
target_link_libraries(test_i960_timing PRIVATE refcore)
add_test(NAME i960_timing COMMAND test_i960_timing)

add_executable(test_model2a_comm tests/test_model2a_comm.cpp
    src/runtime/model2a_comm.cpp src/runtime/comm_board.cpp
    src/runtime/model2a_bus.cpp src/runtime/cpu.cpp)
target_include_directories(test_model2a_comm PRIVATE src)
add_test(NAME model2a_comm COMMAND test_model2a_comm)

add_executable(test_model2a_control tests/test_model2a_control.cpp
    src/runtime/model2a_control.cpp src/runtime/model2a_clock.cpp src/runtime/model2a_bus.cpp src/runtime/cpu.cpp)
target_include_directories(test_model2a_control PRIVATE src)
add_test(NAME model2a_control COMMAND test_model2a_control)

add_executable(test_model2a_clock tests/test_model2a_clock.cpp
    src/runtime/model2a_clock.cpp src/runtime/model2a_control.cpp)
target_include_directories(test_model2a_clock PRIVATE src)
add_test(NAME model2a_clock COMMAND test_model2a_clock)

add_executable(test_model2a_bus_clock tests/test_model2a_bus_clock.cpp
    src/runtime/model2a_clock.cpp src/runtime/model2a_control.cpp
    src/runtime/model2a_bus.cpp src/runtime/cpu.cpp)
target_include_directories(test_model2a_bus_clock PRIVATE src)
add_test(NAME model2a_bus_clock COMMAND test_model2a_bus_clock)

add_executable(test_model2a_video tests/test_model2a_video.cpp
    src/runtime/model2a_video.cpp src/runtime/model2a_bus.cpp src/runtime/cpu.cpp
    src/runtime/video.cpp src/runtime/raster.cpp src/runtime/geo.cpp)
target_include_directories(test_model2a_video PRIVATE src)
add_test(NAME model2a_video COMMAND test_model2a_video)

add_executable(test_model2_uart tests/test_model2_uart.cpp src/runtime/model2_uart.cpp)
target_include_directories(test_model2_uart PRIVATE src)
add_test(NAME model2_uart COMMAND test_model2_uart)

add_executable(test_model2a_uart tests/test_model2a_uart.cpp src/runtime/model2a_uart.cpp
    src/runtime/model2_uart.cpp src/runtime/model2a_bus.cpp src/runtime/cpu.cpp)
target_include_directories(test_model2a_uart PRIVATE src)
add_test(NAME model2a_uart COMMAND test_model2a_uart)

add_executable(test_model2a_machine tests/test_model2a_machine.cpp
    src/runtime/model2a_machine.cpp src/runtime/model2a_bus.cpp src/runtime/model2a_clock.cpp
    src/runtime/model2a_control.cpp src/runtime/model2_io.cpp src/runtime/model2a_io.cpp
    src/runtime/model2_eeprom.cpp src/runtime/cpu.cpp src/runtime/lockstep.cpp)
target_include_directories(test_model2a_machine PRIVATE src)
target_compile_definitions(test_model2a_machine PRIVATE M2_SRALLY_IO M2_SRALLY_CONTROL)
add_test(NAME model2a_machine COMMAND test_model2a_machine)

add_executable(test_lockstep_boundary_hook tests/test_lockstep_boundary_hook.cpp
    src/runtime/cpu.cpp src/runtime/lockstep.cpp)
target_include_directories(test_lockstep_boundary_hook PRIVATE src)
target_compile_definitions(test_lockstep_boundary_hook PRIVATE M2_INSTRUCTION_BOUNDARY_HOOK)
target_compile_options(test_lockstep_boundary_hook PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
add_test(NAME lockstep_boundary_hook COMMAND test_lockstep_boundary_hook "${CMAKE_CURRENT_BINARY_DIR}/boundary-hook-irq.txt")

add_executable(test_model2a_boundary_hook tests/test_model2a_boundary_hook.cpp
    src/runtime/model2a_machine.cpp src/runtime/model2a_bus.cpp src/runtime/model2a_clock.cpp
    src/runtime/model2a_control.cpp src/runtime/cpu.cpp src/runtime/lockstep.cpp
    src/runtime/tgp.cpp src/runtime/m2_tgp_board.cpp src/runtime/model2a_video.cpp
    src/runtime/video.cpp src/runtime/raster.cpp src/runtime/geo.cpp)
target_include_directories(test_model2a_boundary_hook PRIVATE src)
target_compile_definitions(test_model2a_boundary_hook PRIVATE M2_INSTRUCTION_BOUNDARY_HOOK
    M2_SRALLY_CONTROL M2_SRALLY_SCREEN M2_SRALLY_VIDEO M2_SRALLY_TGP)
target_compile_options(test_model2a_boundary_hook PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
add_test(NAME model2a_boundary_hook COMMAND test_model2a_boundary_hook)

add_executable(test_model2a_machine_audio tests/test_model2a_machine_audio.cpp
    src/runtime/model2a_machine.cpp src/runtime/model2a_bus.cpp src/runtime/model2a_clock.cpp
    src/runtime/model2a_control.cpp src/runtime/model2_io.cpp src/runtime/model2a_io.cpp
    src/runtime/model2_eeprom.cpp src/runtime/cpu.cpp src/runtime/lockstep.cpp
    src/runtime/model2_uart.cpp src/runtime/model2a_uart.cpp src/runtime/model2a_sound.cpp
    src/runtime/model2a_sound_bus.cpp src/runtime/snd_cpu.cpp
    src/runtime/model2_scsp.cpp src/runtime/model2_scsp_dsp.cpp)
target_include_directories(test_model2a_machine_audio PRIVATE src)
target_compile_definitions(test_model2a_machine_audio PRIVATE
    M2_SRALLY_IO M2_SRALLY_CONTROL M2_SRALLY_UART M2_SRALLY_SOUND)
target_compile_options(test_model2a_machine_audio PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
add_test(NAME model2a_machine_audio COMMAND test_model2a_machine_audio)

add_executable(test_segarally_input_replay tests/test_segarally_input_replay.cpp)
target_include_directories(test_segarally_input_replay PRIVATE src .)
add_test(NAME segarally_input_replay COMMAND test_segarally_input_replay)

add_executable(test_segarally_host_input_transcript tests/test_segarally_host_input_transcript.cpp)
target_include_directories(test_segarally_host_input_transcript PRIVATE src .)
add_test(NAME segarally_host_input_transcript COMMAND test_segarally_host_input_transcript)

# Shared ROM-free Machine fixture sources; no generated game code.
set(SEGARALLY_INPUT_MACHINE_SOURCES
    src/runtime/model2a_machine.cpp src/runtime/model2a_bus.cpp src/runtime/model2a_clock.cpp
    src/runtime/model2a_control.cpp src/runtime/cpu.cpp src/runtime/lockstep.cpp
    src/runtime/model2_io.cpp src/runtime/model2a_io.cpp src/runtime/model2_eeprom.cpp
    src/runtime/tgp.cpp src/runtime/m2_tgp_board.cpp src/runtime/model2a_video.cpp
    src/runtime/video.cpp src/runtime/raster.cpp src/runtime/geo.cpp)

# Independent interpreter resume phases; ROM-free, no production host linkage.
add_executable(test_segarally_reference_resume tests/test_segarally_reference_resume.cpp
    src/refcore/i960_ref.cpp src/i960/decode.cpp src/i960/isa.cpp
    ${SEGARALLY_INPUT_MACHINE_SOURCES})
target_include_directories(test_segarally_reference_resume PRIVATE src .)
target_compile_definitions(test_segarally_reference_resume PRIVATE M2_INSTRUCTION_BOUNDARY_HOOK
    M2_SRALLY_CONTROL M2_SRALLY_SCREEN M2_SRALLY_VIDEO M2_SRALLY_TGP M2_SRALLY_IO)
target_compile_options(test_segarally_reference_resume PRIVATE
    $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-fast-math;-ffp-contract=off>)
add_test(NAME segarally_reference_resume COMMAND test_segarally_reference_resume)

if(TARGET sha256_digest)
    add_executable(test_sha256 tests/test_sha256.cpp)
    target_link_libraries(test_sha256 PRIVATE sha256_digest)
    add_test(NAME sha256 COMMAND test_sha256)
    add_executable(test_segarally_recording_bundle tests/test_segarally_recording_bundle.cpp tools/srally_boot/recording_bundle.cpp
        src/runtime/model2a_machine.cpp src/runtime/model2a_bus.cpp src/runtime/model2a_clock.cpp
        src/runtime/model2a_control.cpp src/runtime/cpu.cpp src/runtime/lockstep.cpp
        src/runtime/model2_io.cpp src/runtime/model2a_io.cpp src/runtime/model2_eeprom.cpp
        src/runtime/tgp.cpp src/runtime/m2_tgp_board.cpp src/runtime/model2a_video.cpp src/runtime/video.cpp src/runtime/raster.cpp src/runtime/geo.cpp
        src/runtime/model2_uart.cpp src/runtime/model2a_uart.cpp src/runtime/model2a_sound.cpp src/runtime/model2a_sound_bus.cpp
        src/runtime/snd_cpu.cpp src/runtime/model2_scsp.cpp src/runtime/model2_scsp_dsp.cpp)
    target_include_directories(test_segarally_recording_bundle PRIVATE src .)
    target_compile_definitions(test_segarally_recording_bundle PRIVATE M2_INSTRUCTION_BOUNDARY_HOOK M2_SRALLY_CONTROL M2_SRALLY_SCREEN
        M2_SRALLY_VIDEO M2_SRALLY_TGP M2_SRALLY_IO M2_SRALLY_UART M2_SRALLY_SOUND)
    target_link_libraries(test_segarally_recording_bundle PRIVATE sha256_digest)
    add_test(NAME segarally_recording_bundle COMMAND test_segarally_recording_bundle)
    add_executable(test_segarally_recording_reference tests/test_segarally_recording_reference.cpp
        tools/srally_boot/recording_bundle.cpp src/refcore/i960_ref.cpp src/i960/decode.cpp src/i960/isa.cpp
        ${SEGARALLY_INPUT_MACHINE_SOURCES} src/runtime/model2a_comm.cpp src/runtime/comm_board.cpp
        src/runtime/model2_uart.cpp src/runtime/model2a_uart.cpp)
    target_include_directories(test_segarally_recording_reference PRIVATE src .)
    target_compile_definitions(test_segarally_recording_reference PRIVATE M2_INSTRUCTION_BOUNDARY_HOOK
        M2_SRALLY_CONTROL M2_SRALLY_SCREEN M2_SRALLY_VIDEO M2_SRALLY_TGP M2_SRALLY_IO M2_SRALLY_COMM M2_SRALLY_UART)
    target_link_libraries(test_segarally_recording_reference PRIVATE sha256_digest)
    target_compile_options(test_segarally_recording_reference PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
    add_test(NAME segarally_recording_reference COMMAND test_segarally_recording_reference)
endif()

add_executable(test_segarally_host_input_machine tests/test_segarally_host_input_machine.cpp
    ${SEGARALLY_INPUT_MACHINE_SOURCES})
target_include_directories(test_segarally_host_input_machine PRIVATE src .)
target_compile_definitions(test_segarally_host_input_machine PRIVATE M2_INSTRUCTION_BOUNDARY_HOOK
    M2_SRALLY_CONTROL M2_SRALLY_SCREEN M2_SRALLY_VIDEO M2_SRALLY_TGP M2_SRALLY_IO)
add_test(NAME segarally_host_input_machine COMMAND test_segarally_host_input_machine)

add_executable(test_model2_scsp tests/test_model2_scsp.cpp
    src/runtime/model2_scsp.cpp src/runtime/model2_scsp_dsp.cpp src/runtime/model2a_clock.cpp)
target_include_directories(test_model2_scsp PRIVATE src)
add_test(NAME model2_scsp COMMAND test_model2_scsp)

add_executable(test_model2a_audio_link tests/test_model2a_audio_link.cpp
    src/runtime/model2_scsp.cpp src/runtime/model2_scsp_dsp.cpp src/runtime/model2a_clock.cpp
    src/runtime/model2_uart.cpp src/runtime/model2a_uart.cpp)
target_include_directories(test_model2a_audio_link PRIVATE src)
add_test(NAME model2a_audio_link COMMAND test_model2a_audio_link)

add_executable(test_model2a_sound tests/test_model2a_sound.cpp
    src/runtime/model2a_sound.cpp src/runtime/model2a_sound_bus.cpp src/runtime/snd_cpu.cpp
    src/runtime/model2_scsp.cpp src/runtime/model2_scsp_dsp.cpp src/runtime/model2a_clock.cpp)
target_include_directories(test_model2a_sound PRIVATE src)
add_test(NAME model2a_sound COMMAND test_model2a_sound)

add_executable(test_snd_timing tests/test_snd_timing.cpp src/runtime/snd_cpu.cpp)
target_include_directories(test_snd_timing PRIVATE src)
add_test(NAME snd_timing COMMAND test_snd_timing)

add_executable(test_decode tests/test_decode.cpp)
target_link_libraries(test_decode PRIVATE i960)
add_test(NAME decode COMMAND test_decode)

add_executable(test_m68k tests/test_m68k.cpp)
target_link_libraries(test_m68k PRIVATE m68k runtime)
add_test(NAME m68k COMMAND test_m68k)

add_executable(test_fp tests/test_fp.cpp)
target_link_libraries(test_fp PRIVATE i960)
find_package(Threads REQUIRED)
target_link_libraries(test_fp PRIVATE Threads::Threads)
add_test(NAME fp COMMAND test_fp)

add_executable(test_native_sample_mixer tests/test_native_sample_mixer.cpp src/runtime/native_sample_mixer.cpp)
target_include_directories(test_native_sample_mixer PRIVATE src/runtime)
add_test(NAME native_sample_mixer COMMAND test_native_sample_mixer)

add_executable(test_multipcm_volumes tests/test_multipcm_volumes.cpp src/runtime/multipcm.cpp)
target_include_directories(test_multipcm_volumes PRIVATE src)
add_test(NAME multipcm_volumes COMMAND test_multipcm_volumes)

add_executable(test_app_pacing tests/test_app_pacing.cpp src/app/pacing.cpp)
target_include_directories(test_app_pacing PRIVATE src)
add_test(NAME app_pacing COMMAND test_app_pacing)

add_executable(test_test_hold tests/test_test_hold.cpp)
target_include_directories(test_test_hold PRIVATE src)
add_test(NAME test_hold COMMAND test_test_hold)

add_executable(test_native_sound_sequencer tests/test_native_sound_sequencer.cpp src/runtime/native_sound_sequencer.cpp)
target_include_directories(test_native_sound_sequencer PRIVATE src)
add_test(NAME native_sound_sequencer COMMAND test_native_sound_sequencer)

# Link play's communication board, two boards wired in memory (no ROM, no network).
add_executable(test_comm_board tests/test_comm_board.cpp src/runtime/comm_board.cpp)
target_include_directories(test_comm_board PRIVATE src)
add_test(NAME comm_board COMMAND test_comm_board)

# Wheel and pedal bindings against SDL virtual joysticks.
if(M2_METAL_TESTS)
    if(NOT APPLE OR IOS OR NOT TARGET SDL3::SDL3-static)
        message(FATAL_ERROR "M2_METAL_TESTS requires macOS and the pinned SDL3 source")
    endif()
    add_executable(test_gpu_polygon_scanlines tests/test_gpu_polygon_scanlines.cpp
        src/app/gpu/gpu_renderer.cpp src/runtime/video.cpp src/runtime/raster.cpp)
    target_include_directories(test_gpu_polygon_scanlines PRIVATE src)
    target_compile_definitions(test_gpu_polygon_scanlines PRIVATE SDL_MAIN_HANDLED)
    target_link_libraries(test_gpu_polygon_scanlines PRIVATE SDL3::SDL3-static "-framework Metal" "-framework QuartzCore")
    add_test(NAME gpu_polygon_scanlines COMMAND test_gpu_polygon_scanlines)
    set_tests_properties(gpu_polygon_scanlines PROPERTIES TIMEOUT 60 LABELS "metal")
    add_executable(test_gpu_crt tests/test_gpu_crt.cpp src/app/gpu/crt_renderer.cpp)
    target_include_directories(test_gpu_crt PRIVATE src)
    target_compile_definitions(test_gpu_crt PRIVATE SDL_MAIN_HANDLED)
    target_link_libraries(test_gpu_crt PRIVATE SDL3::SDL3-static "-framework Metal" "-framework QuartzCore")
    add_test(NAME gpu_crt COMMAND test_gpu_crt)
    set_tests_properties(gpu_crt PROPERTIES TIMEOUT 30 LABELS "metal")
endif()

if(TARGET SDL3::SDL3-static)
    add_executable(test_segarally_audio tests/test_segarally_audio.cpp)
    target_include_directories(test_segarally_audio PRIVATE src)
    target_link_libraries(test_segarally_audio PRIVATE SDL3::SDL3-static)
    target_compile_options(test_segarally_audio PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
    add_test(NAME segarally_audio COMMAND test_segarally_audio "${CMAKE_CURRENT_BINARY_DIR}/audio-fixture.wav")
    add_executable(test_segarally_controls tests/test_segarally_controls.cpp src/app/segarally_controls.cpp)
    target_include_directories(test_segarally_controls PRIVATE src)
    target_link_libraries(test_segarally_controls PRIVATE SDL3::SDL3-static)
    target_compile_options(test_segarally_controls PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/UNDEBUG,-UNDEBUG>)
    add_test(NAME segarally_controls COMMAND test_segarally_controls)
    add_executable(test_app_config tests/test_app_config.cpp src/app/config.cpp src/app/controls.cpp)
    target_include_directories(test_app_config PRIVATE src)
    target_link_libraries(test_app_config PRIVATE runtime SDL3::SDL3-static)
    add_test(NAME app_config COMMAND test_app_config)
    add_executable(test_app_controls tests/test_app_controls.cpp src/app/controls.cpp src/app/ffb.cpp)
    target_include_directories(test_app_controls PRIVATE src)
    target_link_libraries(test_app_controls PRIVATE runtime SDL3::SDL3-static)
    add_test(NAME app_controls COMMAND test_app_controls)
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/extern/sdl3/include/SDL3/SDL_audio.h)
    add_executable(test_app_native_audio tests/test_app_native_audio.cpp)
    target_include_directories(test_app_native_audio PRIVATE extern/sdl3/include)
    target_link_libraries(test_app_native_audio PRIVATE Threads::Threads)
    add_test(NAME app_native_audio COMMAND test_app_native_audio)
endif()

# PS Vita frontend tests: platform/vita code against narrow host mocks of the
# Vita SDK. ROM-free; they check CPU data and lifetimes, not GXM rendering or
# device frame rates. Off by default: not part of the desktop build.
option(M2_VITA_TESTS "Build the PS Vita frontend's host tests" OFF)
if(M2_VITA_TESTS)
    add_executable(test_vita_imgui tests/test_vita_imgui.cpp
        extern/imgui/imgui.cpp extern/imgui/imgui_draw.cpp
        extern/imgui/imgui_tables.cpp extern/imgui/imgui_widgets.cpp)
    target_include_directories(test_vita_imgui PRIVATE . tests/vita_gpu_shim extern/imgui)
    add_test(NAME vita_imgui COMMAND test_vita_imgui)
    # The same menu through the vitaGL backend (GPU_GL builds), gpu_gl.h mocked.
    add_executable(test_vita_imgui_gl tests/test_vita_imgui_gl.cpp
        extern/imgui/imgui.cpp extern/imgui/imgui_draw.cpp
        extern/imgui/imgui_tables.cpp extern/imgui/imgui_widgets.cpp)
    target_include_directories(test_vita_imgui_gl PRIVATE . tests/vita_gl_shim tests/vita_gpu_shim src platform/vita extern/imgui)
    add_test(NAME vita_imgui_gl COMMAND test_vita_imgui_gl)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_executable(test_vita_link tests/test_vita_link.cpp src/app/link_socket.cpp src/runtime/comm_board.cpp)
        target_include_directories(test_vita_link PRIVATE tests/vita_link_shim src)
        add_test(NAME vita_link COMMAND test_vita_link)
    endif()
    add_executable(test_vita_controls tests/test_vita_controls.cpp)
    add_executable(test_vita_core_policy tests/test_vita_core_policy.cpp)
    add_test(NAME vita_core_policy COMMAND test_vita_core_policy)
    add_test(NAME vita_controls COMMAND test_vita_controls)
    add_executable(test_vita_gpu_memory tests/test_vita_gpu_memory.cpp)
    target_include_directories(test_vita_gpu_memory PRIVATE tests/vita_gpu_shim platform/vita)
    add_test(NAME vita_gpu_memory COMMAND test_vita_gpu_memory)

    add_executable(test_vita_gpu_lifetime tests/test_vita_gpu_lifetime.cpp
        src/runtime/video.cpp src/runtime/raster.cpp)
    target_include_directories(test_vita_gpu_lifetime PRIVATE tests/vita_gpu_shim src)
    target_compile_definitions(test_vita_gpu_lifetime PRIVATE M2_VITA_RENDER_OPT=1)
    add_test(NAME vita_gpu_lifetime COMMAND test_vita_gpu_lifetime)

    add_executable(test_vita_tessellation tests/test_vita_tessellation.cpp)
    target_include_directories(test_vita_tessellation PRIVATE tests/vita_gpu_shim platform/vita)
    add_test(NAME vita_tessellation COMMAND test_vita_tessellation)

    add_executable(test_vita_polygon_order tests/test_vita_polygon_order.cpp)
    target_include_directories(test_vita_polygon_order PRIVATE platform/vita src)
    add_test(NAME vita_polygon_order COMMAND test_vita_polygon_order)

    # The multicore/vitaGL helpers: texel index, flat index, reference audio pacing.
    add_executable(test_vita_texel_index tests/test_vita_texel_index.cpp)
    target_include_directories(test_vita_texel_index PRIVATE src)
    add_test(NAME vita_texel_index COMMAND test_vita_texel_index)
    add_executable(test_vita_flat_index tests/test_vita_flat_index.cpp)
    add_test(NAME vita_flat_index COMMAND test_vita_flat_index)
    add_executable(test_vita_audio_rate tests/test_vita_audio_rate.cpp)
    add_test(NAME vita_audio_rate COMMAND test_vita_audio_rate)

    add_executable(test_vita_native_audio tests/test_vita_native_audio.cpp)
    target_include_directories(test_vita_native_audio PRIVATE tests/vita_audio_shim)
    target_link_libraries(test_vita_native_audio PRIVATE Threads::Threads)
    add_test(NAME vita_native_audio COMMAND test_vita_native_audio)

    add_executable(test_vita_sound_worker tests/test_vita_sound_worker.cpp)
    target_include_directories(test_vita_sound_worker PRIVATE tests/vita_audio_shim)
    target_link_libraries(test_vita_sound_worker PRIVATE Threads::Threads)
    add_test(NAME vita_sound_worker COMMAND test_vita_sound_worker)

    add_executable(test_vita_audio_worker tests/test_vita_audio_worker.cpp)
    target_include_directories(test_vita_audio_worker PRIVATE tests/vita_audio_shim)
    target_link_libraries(test_vita_audio_worker PRIVATE Threads::Threads)
    add_test(NAME vita_audio_worker COMMAND test_vita_audio_worker)

    add_executable(test_vita_async_log tests/test_vita_async_log.cpp)
    target_include_directories(test_vita_async_log PRIVATE tests/vita_log_shim)
    target_link_libraries(test_vita_async_log PRIVATE Threads::Threads)
    add_test(NAME vita_async_log COMMAND test_vita_async_log)
    set_tests_properties(vita_async_log PROPERTIES TIMEOUT 20)

    add_executable(test_vita_log tests/test_vita_log.cpp)
    target_include_directories(test_vita_log PRIVATE tests/vita_log_shim)
    add_test(NAME vita_log COMMAND test_vita_log)
endif()

# Measurement, not a test: where MAME's double FP disagrees with the model.
add_executable(fp_vs_mame tests/fp_vs_mame.cpp)
target_link_libraries(fp_vs_mame PRIVATE i960 Threads::Threads)

add_executable(test_reach tests/test_reach.cpp)
target_link_libraries(test_reach PRIVATE i960)
add_test(NAME reach COMMAND test_reach)

add_executable(test_trace tests/test_trace.cpp)
target_link_libraries(test_trace PRIVATE trace)
add_test(NAME trace COMMAND test_trace ${CMAKE_CURRENT_BINARY_DIR})

# The MAME plugin's Lua core against the C++ trace library (needs lupa).
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME segarally_recording_reference_build
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/test_segarally_recording_reference_build.py)
endif()
if(Python3_FOUND)
    add_test(NAME segarally_mac_launcher
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/test_segarally_mac_launcher.py)
    add_test(NAME segarally_mac_pipeline
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/test_segarally_mac_pipeline.py)
    add_test(NAME segarally_package_licenses
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/test_segarally_package_licenses.py)
    add_test(NAME segarally_host_assets
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/test_segarally_host_assets.py)
    add_test(NAME segarally_recording_launcher
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/test_segarally_recording_launcher.py)
    add_test(NAME segarally_staging_provenance
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/test_segarally_staging_provenance.py)
    add_test(NAME segarally_boundary_contract
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/test_segarally_boundary_contract.py)
    add_test(NAME segarally_boundary_audit
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/test_segarally_boundary_audit.py)
    add_test(NAME i960_runtime_timing
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/test_i960_runtime_timing.py
                --compiler ${CMAKE_CXX_COMPILER})
    add_test(NAME lua_core
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/lua_core_test.py
                ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/lua_core)
    set_tests_properties(lua_core PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME lua_plugin_mock
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/lua_plugin_mock_test.py
                ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/lua_plugin_mock)
    set_tests_properties(lua_plugin_mock PROPERTIES SKIP_RETURN_CODE 77)
    # Second decode reference: Ghidra SLEIGH (third-party i960 module via
    # pypcode). Needs scripts/fetch_ghidra_i960.sh and pip install pypcode.
    if(EXISTS ${CMAKE_SOURCE_DIR}/extern/ghidra_i960/data/languages/i960.slaspec)
        add_test(NAME ghidra_oracle
            COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tests/ghidra_oracle.py
                    ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/extern/ghidra_i960 --random 200000)
        set_tests_properties(ghidra_oracle PROPERTIES SKIP_RETURN_CODE 77)
    endif()
endif()

# Differential test against MAME's disassembler. Needs scripts/fetch_mame.sh.
set(MAME_DIR ${CMAKE_SOURCE_DIR}/extern/mame CACHE PATH "Pinned MAME checkout")
if(EXISTS ${MAME_DIR}/src/devices/cpu/i960/i960dis.cpp)
    add_executable(mame_oracle
        tests/mame_oracle.cpp
        ${MAME_DIR}/src/devices/cpu/i960/i960dis.cpp
    )
    target_include_directories(mame_oracle PRIVATE
        tests/mame_shim
        ${MAME_DIR}/src/devices
        ${MAME_DIR}/src/devices/cpu/i960
    )
    target_link_libraries(mame_oracle PRIVATE i960)
    find_package(Threads REQUIRED)
    target_link_libraries(mame_oracle PRIVATE Threads::Threads)
    # MAME's source, not ours: don't drown our warnings in its.
    set_source_files_properties(${MAME_DIR}/src/devices/cpu/i960/i960dis.cpp
        PROPERTIES COMPILE_OPTIONS "-w")
    add_test(NAME mame_oracle COMMAND mame_oracle)
else()
    message(STATUS "extern/mame not found: skipping mame_oracle (run scripts/fetch_mame.sh)")
endif()
