# MinHook

Unmodified `include/`, `src/` and `LICENSE.txt` from
https://github.com/TsudaKageyu/minhook/tree/v1.3.4,
commit `c3fcafdc10146beb5919319d0683e44e3c30d537`.
The local CMakeLists builds the x64 sources statically with the mod's runtime.

ZHMModSDK also uses MinHook. This is a separate private static instance, used
for verified job/action observation entries and the explicitly enabled private
native hold-setter diagnostic. It never targets SDK hook entries or
uses MH_ALL_HOOKS. The observer retains its trampoline and pins its containing
module until process exit. Stop detaches the client callback; native execution
then passes straight through. We never remove/uninitialize this instance while
the process can still execute native continuations. Thread-aware entry patching
is not proof that native jobs drained or that NPC state can safely be mutated.

Ship LICENSE.txt with binary packages, in addition to including these sources.

The local CMake build force-includes native/MinHookAllocationCalls.h in vendor
C translation units. It redirects only this instance's VirtualAlloc,
VirtualQuery, HeapAlloc and HeapReAlloc calls through native/MinHookDiagnostics.
Vendor source files remain unchanged. Synchronous hook creation captures fixed
counters and immediate VirtualAlloc/VirtualQuery errors without allocating or
changing the allocator policy; native yielding jobs are never traced this way.
Allocation fault injection is compiled only into native fixtures, not player
builds. No process addresses are logged by this tracing.
