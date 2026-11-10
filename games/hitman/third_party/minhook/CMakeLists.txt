# Pinned upstream source; see PROVENANCE.md and LICENSE.txt.
enable_language(C)
add_library(coop_minhook STATIC src/buffer.c src/hook.c src/trampoline.c src/hde/hde64.c)
target_sources(coop_minhook PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../native/MinHookDiagnostics.cpp")
target_include_directories(coop_minhook PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../native")
# Visual Studio cannot represent target-wide C/C++ language-specific flags for
# mixed-language targets. Set them on the vendor C sources explicitly.
set_source_files_properties(src/buffer.c src/hook.c src/trampoline.c src/hde/hde64.c PROPERTIES
    COMPILE_DEFINITIONS COOP_MINHOOK_WRAP_ALLOCATIONS
    COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/../../native/MinHookAllocationCalls.h")
target_include_directories(coop_minhook PUBLIC include)
set_target_properties(coop_minhook PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded)
