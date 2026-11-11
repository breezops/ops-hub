cmake_minimum_required(VERSION 3.12)

# Normally built from the repo root against the SDK developer package. This folder can also be
# dropped into a full ZHMModSDK checkout as Mods/HitmanCoop, with the core copied next to it.
if (NOT TARGET coop_core)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core/CMakeLists.txt")
        set(COOP_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core")
    else ()
        set(COOP_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../core")
    endif ()

    set(COOP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${COOP_CORE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/coop_core")
endif ()

if (NOT TARGET coop_minhook)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/minhook" "${CMAKE_CURRENT_BINARY_DIR}/coop_minhook")
endif ()

file(GLOB_RECURSE SRC_FILES
        CONFIGURE_DEPENDS
        Src/*.cpp
        Src/*.h
)

add_library(HitmanCoop SHARED
        ${SRC_FILES}
)

target_include_directories(HitmanCoop PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

set(COOP_NATIVE_BUILD_OVERRIDE "" CACHE STRING "Explicit native identity for a separately versioned test build")
if (NOT "${COOP_NATIVE_BUILD_OVERRIDE}" STREQUAL "")
    if (NOT COOP_NATIVE_BUILD_OVERRIDE MATCHES "^[0-9A-Za-z._+-]+$")
        message(FATAL_ERROR "Native build identity contains unsupported characters")
    endif ()
    target_compile_definitions(HitmanCoop PRIVATE COOP_NATIVE_BUILD_OVERRIDE="${COOP_NATIVE_BUILD_OVERRIDE}")
endif ()
option(COOP_NPC_IDENTITY_DIAGNOSTICS "Read-only Freeform actor identity audit logging" OFF)
if (COOP_NPC_IDENTITY_DIAGNOSTICS)
    target_compile_definitions(HitmanCoop PRIVATE COOP_NPC_IDENTITY_DIAGNOSTICS=1)
endif ()
option(COOP_NPC_ACTION_DIAGNOSTICS "Read-only observation inside the native NPC action loop" OFF)
option(COOP_NPC_LIFETIME_DIAGNOSTICS "Private read-only pre-clear observed-job evidence" OFF)
option(COOP_NPC_DESTRUCTION_DIAGNOSTICS "Private pointer-only actor destruction entry observation" OFF)
if (COOP_NPC_DESTRUCTION_DIAGNOSTICS)
    if (NOT COOP_NPC_LIFETIME_DIAGNOSTICS OR NOT COOP_NPC_IDENTITY_DIAGNOSTICS)
        message(FATAL_ERROR "Destruction diagnostics require lifetime and identity diagnostics")
    endif ()
    target_compile_definitions(HitmanCoop PRIVATE COOP_NPC_DESTRUCTION_DIAGNOSTICS=1)
endif ()
if (COOP_NPC_LIFETIME_DIAGNOSTICS)
    if (NOT COOP_NPC_ACTION_DIAGNOSTICS)
        message(FATAL_ERROR "Lifetime evidence requires action diagnostics")
    endif ()
    target_compile_definitions(HitmanCoop PRIVATE COOP_NPC_LIFETIME_DIAGNOSTICS=1)
endif ()
option(COOP_NPC_HOLD_DIAGNOSTICS "Private solo one-guard native hold/resume test" OFF)
option(COOP_NPC_POOL_DIAGNOSTICS "Private solo two-NPC hold/resume prototype" OFF)
if (COOP_NPC_POOL_DIAGNOSTICS)
    if (NOT COOP_NPC_HOLD_DIAGNOSTICS OR NOT COOP_NPC_DESTRUCTION_DIAGNOSTICS)
        message(FATAL_ERROR "Pool diagnostic requires hold and destruction diagnostics")
    endif ()
    target_compile_definitions(HitmanCoop PRIVATE COOP_NPC_POOL_DIAGNOSTICS=1)
endif ()
option(COOP_NPC_CATALOG "Private two-actor Freeform movement sync" OFF)
option(COOP_PARTY_NATIVE_RESTART "Private party lifecycle build with host-followed native restart" OFF)
option(COOP_PARTY_INITIAL_LOAD "Private party lobby and native contract launch trace" OFF)
option(COOP_PARTY_FREEFORM_PROBE "Solo test of the native Freeform contract-start entity" OFF)
option(COOP_PARTY_FREEFORM_MENU_START "Test the contract entity launch routine from a clean main menu" OFF)
option(COOP_PARTY_GUEST_START "Follow the host into Freeform Training" OFF)
if (COOP_PARTY_GUEST_START)
    if (NOT COOP_PARTY_INITIAL_LOAD OR COOP_PARTY_FREEFORM_PROBE)
        message(FATAL_ERROR "Guest startup requires the initial load build without the solo hotkey")
    endif ()
    target_compile_definitions(HitmanCoop PRIVATE COOP_PARTY_GUEST_START=1 COOP_PARTY_FREEFORM_MENU_START=1)
endif ()
if (COOP_PARTY_FREEFORM_MENU_START)
    if (NOT COOP_PARTY_FREEFORM_PROBE)
        message(FATAL_ERROR "Native main-menu start requires the solo Freeform launch probe")
    endif ()
    target_compile_definitions(HitmanCoop PRIVATE COOP_PARTY_FREEFORM_MENU_START=1)
endif ()
if (COOP_PARTY_FREEFORM_PROBE)
    if (NOT COOP_PARTY_INITIAL_LOAD OR COOP_NPC_HOLD_DIAGNOSTICS)
        message(FATAL_ERROR "The solo Freeform launch probe requires the initial load build and its own test hotkey")
    endif ()
    target_compile_definitions(HitmanCoop PRIVATE COOP_PARTY_FREEFORM_PROBE=1)
endif ()
option(COOP_PARTY_CONTRACT_TRACE "Observe the native Freeform contract-start wrapper" OFF)
if (COOP_PARTY_CONTRACT_TRACE)
    if (NOT COOP_PARTY_INITIAL_LOAD)
        message(FATAL_ERROR "Contract tracing requires the private initial load build")
    endif ()
    target_compile_definitions(HitmanCoop PRIVATE COOP_PARTY_CONTRACT_TRACE=1)
endif ()
if (COOP_PARTY_INITIAL_LOAD)
    if (NOT COOP_PARTY_NATIVE_RESTART)
        message(FATAL_ERROR "Initial load tests require the private native restart build")
    endif ()
    target_compile_definitions(HitmanCoop PRIVATE COOP_PARTY_INITIAL_LOAD=1)
endif ()
if (COOP_PARTY_NATIVE_RESTART)
    if (COOP_NPC_CATALOG OR COOP_NATIVE_BUILD_OVERRIDE STREQUAL "")
        message(FATAL_ERROR "Party restart tests require a private movement build identity")
    endif ()
    target_compile_definitions(HitmanCoop PRIVATE COOP_PARTY_NATIVE_RESTART=1)
endif ()
if (COOP_NPC_CATALOG)
    if (NOT COOP_NPC_POOL_DIAGNOSTICS OR COOP_NATIVE_BUILD_OVERRIDE STREQUAL "")
        message(FATAL_ERROR "The NPC catalog requires pool diagnostics and a private native version")
    endif ()
    string(SHA256 catalog_build "hitman-npc-catalog-v3/${COOP_NATIVE_BUILD_OVERRIDE}")
    string(SUBSTRING "${catalog_build}" 0 24 catalog_build)
    target_compile_definitions(HitmanCoop PRIVATE COOP_NPC_CATALOG=1 COOP_NPC_CATALOG_ROOM_BUILD="${catalog_build}")
endif ()
if (COOP_NPC_HOLD_DIAGNOSTICS)
    if (NOT COOP_NPC_ACTION_DIAGNOSTICS)
        message(FATAL_ERROR "Private hold test requires action diagnostics")
    endif ()
    target_compile_definitions(HitmanCoop PRIVATE COOP_NPC_HOLD_DIAGNOSTICS=1)
endif ()
if (COOP_NPC_ACTION_DIAGNOSTICS)
    target_compile_definitions(HitmanCoop PRIVATE COOP_NPC_ACTION_DIAGNOSTICS=1)
endif ()

# Which ZHMModSDK (and so which game version) this build is for. The normal build is 4.2.1 (game
# 3.290.1); separate builds for players who can't update use 4.1.1 (game 3.280.x) or 4.0.0-rc.1
# (game 3.240.x), each with that SDK's dev package (-DZHMMODSDK_DEVPKG_DIR) and its own package.
# The code sees COOP_SDK_VERSION (40201, 40101 or 40000) and leaves out what an older SDK lacks.
set(COOP_SDK "4.2.1" CACHE STRING "ZHMModSDK version built against: 4.2.1, 4.1.1 or 4.0.0-rc.1")
set_property(CACHE COOP_SDK PROPERTY STRINGS "4.2.1" "4.1.1" "4.0.0-rc.1")
if (COOP_NPC_ACTION_DIAGNOSTICS AND NOT COOP_SDK STREQUAL "4.2.1")
    message(FATAL_ERROR "Native action diagnostics require the verified 4.2.1 SDK / 3.290.1 layout")
endif ()

if (COOP_SDK STREQUAL "4.2.1")
    target_compile_definitions(HitmanCoop PRIVATE COOP_SDK_VERSION=40201 COOP_GAME_TARGET="3.290")
elseif (COOP_SDK STREQUAL "4.1.1")
    target_compile_definitions(HitmanCoop PRIVATE COOP_SDK_VERSION=40101 COOP_GAME_TARGET="3.280")
elseif (COOP_SDK STREQUAL "4.0.0-rc.1")
    target_compile_definitions(HitmanCoop PRIVATE COOP_SDK_VERSION=40000 COOP_GAME_TARGET="3.240" COOP_LEGACY_SDK=1)
    target_compile_options(HitmanCoop PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/Src/LegacySdk.h")
else ()
    message(FATAL_ERROR "COOP_SDK must be 4.2.1, 4.1.1 or 4.0.0-rc.1, not ${COOP_SDK}.")
endif ()

# The co-op server players get by default (set with -DCOOP_DEFAULT_RELAY=host[:port]).
target_compile_definitions(HitmanCoop PRIVATE COOP_DEFAULT_RELAY="${COOP_DEFAULT_RELAY}")

if (DIRECTX_HEADERS_DIR)
    target_include_directories(HitmanCoop PRIVATE ${DIRECTX_HEADERS_DIR}/include ${DIRECTX_HEADERS_DIR}/include/directx)
endif ()

if (MSVC)
    target_link_options(HitmanCoop PRIVATE $<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO /OPT:REF /OPT:ICF>)
    # Only the debug file's name goes into the DLL, not the folder it was built in.
    target_link_options(HitmanCoop PRIVATE "/PDBALTPATH:%_PDB%")
endif ()

target_link_libraries(HitmanCoop PRIVATE
        coop_minhook
        ZHMModSDK
        implot # IPluginInterface::SetupUI/CleanupUI use it; the dev package's target doesn't link it
        coop_core
        version
        advapi32
)
