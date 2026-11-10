# Pinned upstream source; see PROVENANCE.md and LICENSE.txt.
enable_language(C)
option(COOP_MINHOOK_ABSOLUTE_ENTRIES "Private x64 absolute entry backend for verified prefixes" OFF)
set(COOP_MINHOOK_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}")
if (COOP_MINHOOK_ABSOLUTE_ENTRIES)
    if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Absolute entry backend requires x64")
    endif ()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(COOP_MINHOOK_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/absolute")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/prepare-absolute-minhook.py"
        --source "${CMAKE_CURRENT_SOURCE_DIR}" --output "${COOP_MINHOOK_SOURCE}" RESULT_VARIABLE patch_result)
    if (NOT patch_result EQUAL 0)
        message(FATAL_ERROR "Pinned absolute entry source preparation failed")
    endif ()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/prepare-absolute-minhook.py")
endif ()
set(COOP_MINHOOK_C_SOURCES "${COOP_MINHOOK_SOURCE}/src/buffer.c" "${COOP_MINHOOK_SOURCE}/src/hook.c" "${COOP_MINHOOK_SOURCE}/src/trampoline.c" "${COOP_MINHOOK_SOURCE}/src/hde/hde64.c")
add_library(coop_minhook STATIC ${COOP_MINHOOK_C_SOURCES})
if (COOP_MINHOOK_ABSOLUTE_ENTRIES)
    target_compile_definitions(coop_minhook PRIVATE COOP_MINHOOK_ABSOLUTE_ENTRIES)
endif ()
target_sources(coop_minhook PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../native/MinHookDiagnostics.cpp")
target_include_directories(coop_minhook PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../native")
# Visual Studio cannot represent target-wide C/C++ language-specific flags for
# mixed-language targets. Set them on the vendor C sources explicitly.
set_source_files_properties(${COOP_MINHOOK_C_SOURCES} PROPERTIES
    COMPILE_DEFINITIONS COOP_MINHOOK_WRAP_ALLOCATIONS
    COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/../../native/MinHookAllocationCalls.h")
target_include_directories(coop_minhook PUBLIC include)
set_target_properties(coop_minhook PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded)
