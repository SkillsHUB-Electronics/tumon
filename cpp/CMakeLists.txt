cmake_minimum_required(VERSION 3.24)
project(tumon VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The source DLL is loaded into Windows service processes (Frame Server), so it links the C runtime
# statically: no dependency on a Visual C++ redistributable being installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/W4> $<$<COMPILE_LANGUAGE:CXX>:/permissive-> $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
                    $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus> $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)

# --- the media source DLL (what the Frame Server loads) ---------------------------------------------
add_library(tumon_source SHARED
    src/channel/frame_channel.cpp
    src/source/activator.cpp
    src/source/dllmain.cpp
    src/source/frame_generator.cpp
    src/source/idle_loader.cpp
    src/source/idle_screen.cpp
    src/source/media_source.cpp
    src/source/media_stream.cpp
    src/source/yuy2_scaler.cpp
    src/source/tumon_source.def
    src/source/tumon_source.rc)
target_link_libraries(tumon_source PRIVATE mfplat mf mfuuid mfsensorgroup propsys ole32 uuid advapi32 windowscodecs)

# --- tools ------------------------------------------------------------------------------------------
add_executable(tumon_vcam_host tools/vcam_host.cpp)
target_link_libraries(tumon_vcam_host PRIVATE mfplat mf mfsensorgroup ole32)

add_executable(tumon_vcam_ctl tools/vcam_ctl.cpp)
target_link_libraries(tumon_vcam_ctl PRIVATE mfplat mf mfuuid mfsensorgroup ole32)

# The feeder engine, shared by the app and the console tool.
add_library(tumon_feeder_core STATIC src/feeder/camera_feeder.cpp src/channel/frame_channel.cpp)
target_link_libraries(tumon_feeder_core PUBLIC mfplat mf mfuuid mfreadwrite ole32 advapi32)

add_executable(tumon_feeder tools/feeder.cpp)
target_link_libraries(tumon_feeder PRIVATE tumon_feeder_core)

# --- the app ----------------------------------------------------------------------------------------
add_executable(tumon WIN32 app/main.cpp app/settings.cpp app/tumon.rc)
target_link_libraries(tumon PRIVATE tumon_feeder_core comctl32 shell32 uiautomationcore oleaut32 user32)
# The manifest is embedded by tumon.rc, so the linker must not add its own.
set_target_properties(tumon PROPERTIES OUTPUT_NAME "Tumon" LINK_FLAGS "/MANIFEST:NO")

add_executable(tumon_mfinfo tools/mf_info.cpp)
target_link_libraries(tumon_mfinfo PRIVATE mfplat mf mfuuid mfreadwrite ole32)

add_executable(tumon_selftest tools/selftest.cpp src/source/frame_generator.cpp src/channel/frame_channel.cpp)
target_link_libraries(tumon_selftest PRIVATE mfplat mf mfuuid mfreadwrite ole32 advapi32)
add_dependencies(tumon_selftest tumon_source)

# --- tests ------------------------------------------------------------------------------------------
enable_testing()
add_executable(test_frame_generator tests/test_frame_generator.cpp src/source/frame_generator.cpp)
add_test(NAME frame_generator COMMAND test_frame_generator)

add_executable(test_idle_screen tests/test_idle_screen.cpp src/source/idle_screen.cpp)
add_test(NAME idle_screen COMMAND test_idle_screen)

add_executable(test_yuy2_scaler tests/test_yuy2_scaler.cpp src/source/yuy2_scaler.cpp)
add_test(NAME yuy2_scaler COMMAND test_yuy2_scaler)

add_executable(test_frame_channel tests/test_frame_channel.cpp src/channel/frame_channel.cpp)
target_link_libraries(test_frame_channel PRIVATE advapi32)
add_test(NAME frame_channel COMMAND test_frame_channel)

# Loads the DLL straight from the build folder: no registration or administrator rights needed.
add_test(NAME source_selftest COMMAND tumon_selftest $<TARGET_FILE:tumon_source>)
