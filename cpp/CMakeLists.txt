cmake_minimum_required(VERSION 3.24)
project(tumon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The source DLL is loaded into Windows service processes (Frame Server), so it links the C runtime
# statically: no dependency on a Visual C++ redistributable being installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
add_compile_options(/W4 /permissive- /EHsc /Zc:__cplusplus)

# --- the media source DLL (what the Frame Server loads) ---------------------------------------------
add_library(tumon_source SHARED
    src/source/activator.cpp
    src/source/dllmain.cpp
    src/source/frame_generator.cpp
    src/source/media_source.cpp
    src/source/media_stream.cpp
    src/source/tumon_source.def)
target_link_libraries(tumon_source PRIVATE mfplat mf mfuuid mfsensorgroup propsys ole32 uuid advapi32)

# --- tools ------------------------------------------------------------------------------------------
add_executable(tumon_vcam_host tools/vcam_host.cpp)
target_link_libraries(tumon_vcam_host PRIVATE mfplat mf mfsensorgroup ole32)

add_executable(tumon_vcam_ctl tools/vcam_ctl.cpp)
target_link_libraries(tumon_vcam_ctl PRIVATE mfplat mf mfuuid mfsensorgroup ole32)

add_executable(tumon_mfinfo tools/mf_info.cpp)
target_link_libraries(tumon_mfinfo PRIVATE mfplat mf mfuuid mfreadwrite ole32)

add_executable(tumon_selftest tools/selftest.cpp src/source/frame_generator.cpp)
target_link_libraries(tumon_selftest PRIVATE mfplat mf mfuuid mfreadwrite ole32)
add_dependencies(tumon_selftest tumon_source)

# --- tests ------------------------------------------------------------------------------------------
enable_testing()
add_executable(test_frame_generator tests/test_frame_generator.cpp src/source/frame_generator.cpp)
add_test(NAME frame_generator COMMAND test_frame_generator)
# Loads the DLL straight from the build folder: no registration or administrator rights needed.
add_test(NAME source_selftest COMMAND tumon_selftest $<TARGET_FILE:tumon_source>)
