"""Generate local C++ patch metadata for the exact supported retail image.

The generated header stays in runtime/bin; it is not committed or distributed.
"""
import argparse
from pathlib import Path

from storage_relocation import relocate

KIND = {'clients': 0, 'connections': 1, 'players': 2, 'aux': 3,
        'immediate': 4, 'indirect_clients': 5, 'indirect_connections': 6}


def emit(executable: Path, output: Path):
    _, _, counts, manifest = relocate(executable, include_manifest=True)
    if len(manifest) != 204 or len({site[0] for site in manifest}) != 204:
        raise ValueError('Patch plan has missing or duplicate sites')
    lines = ['#pragma once', '#include <cstdint>',
             'struct PatchSpec { std::uint32_t rva, target_offset; '
             'std::uint8_t size, disp_offset, kind; '
             'std::uint8_t expected[15], replacement[15]; };',
             'static constexpr PatchSpec PatchPlan[] = {']
    for rva, size, displacement, kind, target_offset, expected, replacement in sorted(manifest):
        old, new = bytes.fromhex(expected), bytes.fromhex(replacement)
        if size != len(old) or size > 15 or len(new) not in (0, size):
            raise ValueError(f'Invalid instruction at {rva:#x}')
        old_list = ','.join(str(value) for value in old.ljust(15, b'\0'))
        new_list = ','.join(str(value) for value in new.ljust(15, b'\0'))
        lines.append(f'{{0x{rva:x},0x{target_offset:x},{size},{displacement},'
                     f'{KIND[kind]},{{{old_list}}},{{{new_list}}}}},')
    lines += ['};', 'static constexpr unsigned PatchCount = 204;']
    output.write_text('\n'.join(lines) + '\n', encoding='ascii')
    print(f'Wrote {len(manifest)} validated patch sites to {output}')
    print(counts)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    emit(args.exe, args.output)
