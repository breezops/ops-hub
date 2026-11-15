"""Relocate direct retail co-op storage references in a private mapped image.

This is a source-side prototype. It never writes to the installed executable or a
running process. Indirect references and session limits need separate validation.
"""
import hashlib
import struct
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_64, Cs
from capstone.x86 import X86_OP_MEM, X86_REG_RIP

BUILD = '2f8c9cfda1b8c0fb308fd9719577154e8222fc7654f5e8f1bf54579bca69d805'
CLIENT_BASE, CLIENT_STRIDE = 0x19ADA98, 0x1678
CONNECTION_BASE, CONNECTION_STRIDE = 0x540BA0, 0x2650
CLIENT_END = CLIENT_BASE + 2 * CLIENT_STRIDE
CONNECTION_END = CONNECTION_BASE + 2 * CONNECTION_STRIDE
# Verified loop bounds. The third client-end reference writes adjacent state.
CLIENT_END_LOOP_BOUNDS = {0x22ED47, 0x22F003}


def instructions(pe):
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    decoder.detail = True
    decoder.skipdata = True
    for section in pe.sections:
        if section.Characteristics & 0x20000000:
            yield from decoder.disasm(section.get_data(), section.VirtualAddress)


def relocate(executable: Path):
    source = executable.read_bytes()
    if hashlib.sha256(source).hexdigest() != BUILD:
        raise ValueError('Unsupported executable')
    pe = pefile.PE(data=source, fast_load=True)
    original = pe.get_memory_mapped_image()
    image = bytearray(original)
    first_free = (len(image) + 0xfff) & ~0xfff
    client_new = first_free
    connection_new = (client_new + 4 * CLIENT_STRIDE + 0xfff) & ~0xfff
    image.extend(bytes(connection_new + 4 * CONNECTION_STRIDE - len(image)))
    image[client_new:client_new + 2 * CLIENT_STRIDE] = original[CLIENT_BASE:CLIENT_END]
    image[connection_new:connection_new + 2 * CONNECTION_STRIDE] = original[CONNECTION_BASE:CONNECTION_END]
    counts = {'server_clients': 0, 'coop_connections': 0, 'client_loop_end': 0}
    sites = []

    for instruction in instructions(pe):
        if instruction.id == 0:
            continue
        for operand in instruction.operands:
            if operand.type != X86_OP_MEM or operand.mem.base != X86_REG_RIP:
                continue
            old_target = instruction.address + instruction.size + operand.mem.disp
            if CLIENT_BASE <= old_target < CLIENT_END:
                new_target = client_new + old_target - CLIENT_BASE
                group = 'server_clients'
            elif CONNECTION_BASE <= old_target < CONNECTION_END:
                new_target = connection_new + old_target - CONNECTION_BASE
                group = 'coop_connections'
            elif old_target == CLIENT_END and instruction.address in CLIENT_END_LOOP_BOUNDS:
                new_target = client_new + 4 * CLIENT_STRIDE
                group = 'client_loop_end'
            else:
                continue
            # RIP addressing uses a signed disp32. Capstone 5 reports the
            # operand's word width as disp_size for some 0x66-prefixed stores.
            if (not instruction.disp_offset or
                    instruction.disp_offset + 4 > instruction.size or
                    struct.unpack_from('<i', instruction.bytes, instruction.disp_offset)[0]
                    != operand.mem.disp):
                raise ValueError(f'Unsupported displacement at {instruction.address:#x}')
            at = instruction.address + instruction.disp_offset
            expected = original[instruction.address:instruction.address + instruction.size]
            if image[instruction.address:instruction.address + instruction.size] != expected:
                raise ValueError(f'Overlapping patch at {instruction.address:#x}')
            displacement = new_target - (instruction.address + instruction.size)
            if not -(2 ** 31) <= displacement < 2 ** 31:
                raise ValueError(f'Relocated target out of RIP range at {instruction.address:#x}')
            image[at:at + 4] = struct.pack('<i', displacement)
            counts[group] += 1
            sites.append(instruction.address)

    if counts != {'server_clients': 28, 'coop_connections': 138, 'client_loop_end': 2}:
        raise ValueError(f'Incomplete direct relocation: {counts}')
    if len(sites) != len(set(sites)):
        raise ValueError('Duplicate relocation site')
    if image[CLIENT_END:CLIENT_END + 32] != original[CLIENT_END:CLIENT_END + 32]:
        raise ValueError('Adjacent server data changed')
    if image[CONNECTION_END:CONNECTION_END + 32] != original[CONNECTION_END:CONNECTION_END + 32]:
        raise ValueError('Adjacent connection data changed')
    return image, {'clients': client_new, 'connections': connection_new}, counts


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    options = parser.parse_args()
    _, bases, changed = relocate(options.exe)
    print(f'Private image bases: {bases}')
    print(f'Validated direct relocations: {changed}')
