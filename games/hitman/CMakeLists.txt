cmake_minimum_required(VERSION 3.20)

# Builds the HitmanCoop mod against a prebuilt ZHMModSDK developer package
# (DevPkg-ZHMModSDK.zip from the SDK's GitHub releases). See BUILDING.md.
project(HitmanCoop CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The SDK and its mods use the static runtime. Only release-type builds are supported, because
# the developer package's release libraries are what the shipped ZHMModSDK.dll was built from.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif ()

set(ZHMMODSDK_DEVPKG_DIR "" CACHE PATH "Folder where DevPkg-ZHMModSDK.zip was extracted")

if (NOT EXISTS "${ZHMMODSDK_DEVPKG_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set ZHMMODSDK_DEVPKG_DIR to the extracted DevPkg-ZHMModSDK.zip (tools/fetch-sdk.ps1 does this).")
endif ()

set(DIRECTX_HEADERS_DIR "${CMAKE_SOURCE_DIR}/_deps/directx-headers/DirectX-Headers-1.614.0" CACHE PATH
    "Extracted DirectX-Headers (tools/fetch-sdk.ps1 does this); the SDK's headers include <directx/d3d12.h>")

if (NOT EXISTS "${DIRECTX_HEADERS_DIR}/include/directx/d3d12.h")
    message(FATAL_ERROR "DirectX-Headers not found at ${DIRECTX_HEADERS_DIR}. Run tools/fetch-sdk.ps1.")
endif ()

add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)

# fmt (bundled in the SDK headers) requires it.
add_compile_options(/utf-8)

add_subdirectory("${ZHMMODSDK_DEVPKG_DIR}" "${CMAKE_BINARY_DIR}/zhmmodsdk")
add_subdirectory(core)
add_subdirectory(plugin)

install(TARGETS HitmanCoop RUNTIME DESTINATION mods)
install(TARGETS coop_nettest RUNTIME DESTINATION .)

if (COOP_BUILD_TESTS)
    # Action completion observation uses the same SDK-free native fixture target.
    set(COOP_ACTION_PROBE tools/TestNpcActionProbe.cpp plugin/Src/NpcActionProbe.cpp)
    add_executable(coop_npc_native_tests core/tests/Main.cpp tools/TestNpcReasoningFence.cpp tools/TestNpcMutationGate.cpp tools/TestNpcLifeReceipts.cpp plugin/Src/NpcReasoningFence.cpp plugin/Src/CrashGuards.cpp plugin/Src/NpcMutationGate.cpp plugin/Src/NpcLifeReceipts.cpp)
    target_include_directories(coop_npc_native_tests PRIVATE plugin/Src core/tests)
    target_compile_definitions(coop_npc_native_tests PRIVATE COOP_GUARD_TESTS)
    target_sources(coop_npc_native_tests PRIVATE ${COOP_ACTION_PROBE})
    target_sources(coop_npc_native_tests PRIVATE tools/TestNpcActorBirths.cpp plugin/Src/NpcActorBirths.cpp)
    target_sources(coop_npc_native_tests PRIVATE tools/TestNpcBehaviorReadback.cpp plugin/Src/NpcBehaviorReadback.cpp)
    add_executable(coop_npc_phase_tests core/tests/Main.cpp tools/TestNpcActionPhase.cpp plugin/Src/NpcActionPhase.cpp plugin/Src/NpcActionProbe.cpp plugin/Src/NpcMutationGate.cpp)
    target_sources(coop_npc_phase_tests PRIVATE plugin/Src/NpcBehaviorReadback.cpp)
    target_include_directories(coop_npc_phase_tests PRIVATE plugin/Src core/tests)
    add_executable(coop_guard_tests core/tests/Main.cpp tools/TestPlayerGuards.cpp plugin/Src/CrashGuards.cpp plugin/Src/NpcReasoningFence.cpp plugin/Src/NpcMutationGate.cpp)
    target_include_directories(coop_guard_tests PRIVATE plugin/Src core/tests)
    target_compile_definitions(coop_guard_tests PRIVATE COOP_GUARD_TESTS)
    add_executable(coop_pose_tests core/tests/Main.cpp tools/TestPlayerPoses.cpp plugin/Src/PoseHook.cpp)
    target_include_directories(coop_pose_tests PRIVATE plugin/Src core/tests core/include)
    add_executable(coop_launcher_bridge_harness tools/LauncherBridgeHarness.cpp plugin/Src/LauncherBridge.cpp)
    target_include_directories(coop_launcher_bridge_harness PRIVATE plugin/Src core/include)
    target_link_libraries(coop_launcher_bridge_harness PRIVATE advapi32)
endif ()
