"""Generate the explicit x64 absolute-entry backend from pinned MinHook sources.

Upstream files stay unchanged. Only verified, relocation-free entry prefixes
are admitted by MinHookDiagnostics in this opt-in build. Keep upstream thread
suspension/IP mapping, instruction decoding and hook bookkeeping.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
source, output = a.source.resolve(), a.output.resolve()
if source == output or source in output.parents or output in source.parents:
    raise SystemExit('Generated output must be separate from upstream source')
for name, expected in json.loads((source/'UPSTREAM-HASHES.json').read_text())['files'].items():
    raw = (source/name).read_bytes()
    lf = raw.replace(b'\r\n', b'\n')
    # Git checkout line endings may differ; no other content change is allowed.
    hashes = {hashlib.sha256(data).hexdigest() for data in (raw, lf, lf.replace(b'\n', b'\r\n'))}
    if expected not in hashes:
        raise SystemExit('Pinned upstream fingerprint mismatch: '+name)
for folder in ('src', 'include'):
    shutil.copytree(source/folder, output/folder, dirs_exist_ok=True)

def read(name):
    return (output/name).read_text(encoding='utf-8')

def replace(text, old, new):
    if text.count(old) != 1:
        raise SystemExit('Expected one pinned patch anchor: '+old[:80])
    return text.replace(old, new)

buffer = read('src/buffer.c')
start = buffer.index('#if defined(_M_X64)', buffer.index('static PMEMORY_BLOCK GetMemoryBlock('))
end = buffer.index('    if (pBlock != NULL)\n', start)
# The first matching line is in the allocation search, not slot initialization.
end = buffer.index('    if (pBlock != NULL)\n    {\n        // Build a linked list', end)
buffer = buffer[:start]+'''    // Absolute entries need no near relay. Reuse only this instance's pages.
    for (pBlock = g_pMemoryBlocks; pBlock != NULL; pBlock = pBlock->pNext)
        if (pBlock->pFree != NULL)
            return pBlock;
    pBlock = (PMEMORY_BLOCK)VirtualAlloc(
        NULL, MEMORY_BLOCK_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

'''+buffer[end:]

trampoline = read('src/trampoline.c')
trampoline = replace(trampoline, 'if (oldPos >= sizeof(JMP_REL))', 'if (oldPos >= sizeof(JMP_ABS))')
trampoline = replace(trampoline, '''        if (hs.flags & F_ERROR)
            return FALSE;
''', '''        if (hs.flags & F_ERROR)
            return FALSE;
        // No relocated RIP operands or relative branches in stolen instructions.
        if (oldPos < sizeof(JMP_ABS) &&
            ((hs.flags & F_RELATIVE) || ((hs.modrm & 0xC7) == 0x05)))
            return FALSE;
''')
start = trampoline.index('    // Is there enough place for a long jump?')
end = trampoline.index('#if defined(_M_X64)', start)
trampoline = trampoline[:start]+'''    // No short functions or hot-patch-above path in this explicit backend.
    if (oldPos < sizeof(JMP_ABS))
        return FALSE;

'''+trampoline[end:]

hook = read('src/hook.c')
hook = replace(hook, 'UINT8  backup[8];', 'UINT8  backup[16];')
hook = replace(hook, 'pHook->pDetour     = ct.pRelay;', 'pHook->pDetour     = ct.pDetour;')
start = hook.index('    SIZE_T patchSize    = sizeof(JMP_REL);', hook.index('static MH_STATUS EnableHookLL('))
end = hook.index('    VirtualProtect(pPatchTarget, patchSize, oldProtect, &oldProtect);', start)
hook = hook[:start]+'''    SIZE_T patchSize = sizeof(JMP_ABS);
    LPBYTE pPatchTarget = (LPBYTE)pHook->pTarget;
    if (pHook->patchAbove)
        return MH_ERROR_UNSUPPORTED_FUNCTION;
    if (!VirtualProtect(pPatchTarget, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect))
        return MH_ERROR_MEMORY_PROTECT;
    if (enable)
    {
        JMP_ABS jump = { 0xFF, 0x25, 0, (UINT64)pHook->pDetour };
        memcpy(pPatchTarget, &jump, sizeof(jump));
    }
    else
        memcpy(pPatchTarget, pHook->backup, sizeof(JMP_ABS));

'''+hook[end:]
start = hook.index('                            // Back up the target function.')
end = hook.index('                            if (ppOriginal != NULL)', start)
hook = hook[:start]+'''                            // Only the fourteen overwritten bytes need restoration.
                            memcpy(pHook->backup, pTarget, sizeof(JMP_ABS));

'''+hook[end:]
for name, text in [('src/buffer.c', buffer), ('src/trampoline.c', trampoline), ('src/hook.c', hook)]:
    (output/name).write_bytes(text.encode('utf-8'))
(output/'absolute-patch-audit.json').write_text(json.dumps({
    'upstream_commit': 'c3fcafdc10146beb5919319d0683e44e3c30d537',
    'mode': 'x64-absolute-14-verified-prefixes-only',
    'generated': {name: hashlib.sha256((output/name).read_bytes()).hexdigest()
                  for name in ('src/buffer.c','src/trampoline.c','src/hook.c')}
}, indent=2)+'\n', encoding='utf-8')
