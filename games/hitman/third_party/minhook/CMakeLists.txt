# Pinned upstream source; see PROVENANCE.md and LICENSE.txt.
enable_language(C)
add_library(coop_minhook STATIC src/buffer.c src/hook.c src/trampoline.c src/hde/hde64.c)
target_include_directories(coop_minhook PUBLIC include)
set_target_properties(coop_minhook PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreaded)
