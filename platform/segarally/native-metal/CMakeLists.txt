cmake_minimum_required(VERSION 3.25)
project(SegaRallyNativeMetal LANGUAGES CXX)
if(NOT APPLE)
    message(FATAL_ERROR "Native Metal requires the Apple SDK")
endif()
enable_language(OBJCXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_EXTENSIONS OFF)
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(METAL_FRAMEWORK Metal REQUIRED)

# Direct API implementations plus the common frame packet. No SDL, game code,
# ROM input or platform presentation is needed to compile this library.
add_library(segarally_native_metal STATIC
    "${ROOT}/src/app/native/metal/model2_metal.mm"
    "${ROOT}/src/app/native/metal/metal_crt.mm"
    "${ROOT}/src/app/gpu/model2_frame.cpp")
target_include_directories(segarally_native_metal PUBLIC "${ROOT}/src")
target_link_libraries(segarally_native_metal PUBLIC
    "${FOUNDATION_FRAMEWORK}" "${METAL_FRAMEWORK}")

# Link both fixture clients so unresolved implementation symbols cannot hide in
# the static archive. CI deliberately does not execute them: a hosted runner's
# Apple SDK availability is not evidence that a Metal GPU is available.
add_executable(test_native_metal_model2
    "${ROOT}/tests/test_native_metal_model2.mm"
    "${ROOT}/src/runtime/video.cpp"
    "${ROOT}/src/runtime/raster.cpp")
target_compile_definitions(test_native_metal_model2 PRIVATE NATIVE_METAL_MODEL2=1)
target_link_libraries(test_native_metal_model2 PRIVATE segarally_native_metal)
add_executable(test_native_metal_crt "${ROOT}/tests/test_native_metal_crt.mm")
target_link_libraries(test_native_metal_crt PRIVATE segarally_native_metal)

foreach(target segarally_native_metal test_native_metal_model2 test_native_metal_crt)
    target_compile_options(${target} PRIVATE
        -Wall -Wextra -Werror -Wno-unused-private-field -Wno-misleading-indentation
        -fno-fast-math -ffp-contract=off
        "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
endforeach()
