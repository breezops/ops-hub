"""Relocate verified retail co-op storage references in a private mapped image.

This is a source-side prototype. It never writes to the installed executable or a
running process. Session admission and replication need separate validation.
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
PLAYER_BASE, PLAYER_STRIDE = 0xEEE070, 0xAFE0
AUX_BASE, AUX_STRIDE = 0xD96B00, 0x198
CLIENT_END = CLIENT_BASE + 2 * CLIENT_STRIDE
CONNECTION_END = CONNECTION_BASE + 2 * CONNECTION_STRIDE
PLAYER_END = PLAYER_BASE + 2 * PLAYER_STRIDE
AUX_END = AUX_BASE + 2 * AUX_STRIDE
# Verified loop bounds. The third client-end reference writes adjacent state.
CLIENT_END_LOOP_BOUNDS = {0x22ED47, 0x22F003}
INDIRECT_CONNECTION_SITES = {
    0x89E56, 0xA08D8, 0xA08F7, 0xC284A, 0x18005B, 0x180076,
    0x18DF27, 0x18DF57, 0x18E017, 0x18E047, 0x18E0FC,
    0x18E137, 0x18E29D, 0x18E36F, 0x22D9C1, 0x22D9C9,
    0x22DB7C, 0x22DB92, 0x22DBAD, 0x22F6D3, 0x22F6FA,
    0x22FDC8, 0x22FDD6,
}
INDIRECT_CLIENT_SITES = {0x22F4C4}


def instructions(pe):
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    decoder.detail = True
    decoder.skipdata = True
    for section in pe.sections:
        if section.Characteristics & 0x20000000:
            yield from decoder.disasm(section.get_data(), section.VirtualAddress)


def relocate(executable: Path, include_manifest=False):
    source = executable.read_bytes()
    if hashlib.sha256(source).hexdigest() != BUILD:
        raise ValueError('Unsupported executable')
    pe = pefile.PE(data=source, fast_load=True)
    original = pe.get_memory_mapped_image()
    image = bytearray(original)
    first_free = (len(image) + 0xfff) & ~0xfff
    client_new = first_free
    connection_new = (client_new + 4 * CLIENT_STRIDE + 0xfff) & ~0xfff
    player_new = (connection_new + 4 * CONNECTION_STRIDE + 0xfff) & ~0xfff
    aux_new = (player_new + 4 * PLAYER_STRIDE + 0xfff) & ~0xfff
    image.extend(bytes(aux_new + 4 * AUX_STRIDE - len(image)))
    image[client_new:client_new + 2 * CLIENT_STRIDE] = original[CLIENT_BASE:CLIENT_END]
    image[connection_new:connection_new + 2 * CONNECTION_STRIDE] = original[CONNECTION_BASE:CONNECTION_END]
    image[player_new:player_new + 2 * PLAYER_STRIDE] = original[PLAYER_BASE:PLAYER_END]
    image[aux_new:aux_new + 2 * AUX_STRIDE] = original[AUX_BASE:AUX_END]
    counts = {'server_clients': 0, 'coop_connections': 0, 'client_loop_end': 0,
              'player_states': 0, 'player_setup_limits': 0,
              'aux_records': 0, 'indirect_connections': 0, 'indirect_clients': 0,
              'server_setup_limits': 0, 'server_reset_limit': 0}
    sites = []
    manifest = []

    for instruction in instructions(pe):
        if instruction.id == 0:
            continue
        indirect_kind = ('connections' if instruction.address in INDIRECT_CONNECTION_SITES else
                         'clients' if instruction.address in INDIRECT_CLIENT_SITES else None)
        for operand in instruction.operands:
            if operand.type != X86_OP_MEM:
                continue
            indirect = indirect_kind is not None and operand.mem.base != X86_REG_RIP
            if not indirect and operand.mem.base != X86_REG_RIP:
                continue
            old_target = (operand.mem.disp if indirect else
                          instruction.address + instruction.size + operand.mem.disp)
            if indirect and not ((indirect_kind == 'connections' and
                                  CONNECTION_BASE <= old_target < CONNECTION_END) or
                                 (indirect_kind == 'clients' and
                                  CLIENT_BASE <= old_target < CLIENT_END)):
                raise ValueError(f'Unexpected indirect target at {instruction.address:#x}')
            if CLIENT_BASE <= old_target < CLIENT_END:
                new_target = client_new + old_target - CLIENT_BASE
                group = 'server_clients'
            elif CONNECTION_BASE <= old_target < CONNECTION_END:
                new_target = connection_new + old_target - CONNECTION_BASE
                group = 'coop_connections'
            elif old_target == CLIENT_END and instruction.address in CLIENT_END_LOOP_BOUNDS:
                new_target = client_new + 4 * CLIENT_STRIDE
                group = 'client_loop_end'
            elif PLAYER_BASE <= old_target < PLAYER_END:
                new_target = player_new + old_target - PLAYER_BASE
                group = 'player_states'
            elif AUX_BASE <= old_target < AUX_END:
                new_target = aux_new + old_target - AUX_BASE
                group = 'aux_records'
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
            displacement = (new_target if indirect else
                            new_target - (instruction.address + instruction.size))
            if not -(2 ** 31) <= displacement < 2 ** 31:
                raise ValueError(f'Relocated target out of disp32 range at {instruction.address:#x}')
            image[at:at + 4] = struct.pack('<i', displacement)
            counts['indirect_' + indirect_kind if indirect else group] += 1
            sites.append(instruction.address)
            kind = ('clients' if group in ('server_clients', 'client_loop_end') else
                    'connections' if group == 'coop_connections' else
                    'players' if group == 'player_states' else 'aux')
            target_base = {'clients': client_new, 'connections': connection_new,
                           'players': player_new, 'aux': aux_new}[kind]
            manifest.append((instruction.address, instruction.size, instruction.disp_offset,
                             'indirect_' + kind if indirect else kind,
                             new_target - target_base, expected.hex(), ''))

    if {key: counts[key] for key in ('server_clients', 'coop_connections', 'client_loop_end',
                                     'player_states', 'aux_records')} != {
            'server_clients': 28, 'coop_connections': 138, 'client_loop_end': 2,
            'player_states': 1, 'aux_records': 7}:
        raise ValueError(f'Incomplete direct relocation: {counts}')
    if len(sites) != len(set(sites)):
        raise ValueError('Duplicate relocation site')
    if (counts['indirect_connections'] != len(INDIRECT_CONNECTION_SITES) or
            counts['indirect_clients'] != len(INDIRECT_CLIENT_SITES)):
        raise ValueError(f'Incomplete indirect relocation: {counts}')
    # These retail immediates limit the clear, setup walk and reset walk to
    # two records. Check the entire instruction before changing its operand.
    limits = ((0x22D05E, '41b8f02c0000', '41b8e0590000', 'server_setup_limits'),
              (0x22D11E, '83fb02', '83fb04', 'server_setup_limits'),
              (0x231AEF, '8d7502', '8d7504', 'server_reset_limit'),
              (0x1716CF, '41b8c05f0100', '41b880bf0200', 'player_setup_limits'))
    for address, before, after, group in limits:
        old, new = bytes.fromhex(before), bytes.fromhex(after)
        if original[address:address + len(old)] != old or image[address:address + len(old)] != old:
            raise ValueError(f'Unsupported limit instruction at {address:#x}')
        image[address:address + len(old)] = new
        counts[group] += 1
        manifest.append((address, len(old), 0, 'immediate', 0, before, after))
    if image[CLIENT_END:CLIENT_END + 32] != original[CLIENT_END:CLIENT_END + 32]:
        raise ValueError('Adjacent server data changed')
    if image[CONNECTION_END:CONNECTION_END + 32] != original[CONNECTION_END:CONNECTION_END + 32]:
        raise ValueError('Adjacent connection data changed')
    if image[PLAYER_END:PLAYER_END + 32] != original[PLAYER_END:PLAYER_END + 32]:
        raise ValueError('Adjacent player data changed')
    if image[AUX_END:AUX_END + 32] != original[AUX_END:AUX_END + 32]:
        raise ValueError('Adjacent auxiliary data changed')
    bases = {'clients': client_new, 'connections': connection_new,
             'players': player_new, 'aux': aux_new}
    return (image, bases, counts, manifest) if include_manifest else (image, bases, counts)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    options = parser.parse_args()
    _, bases, changed = relocate(options.exe)
    print(f'Private image bases: {bases}')
    print(f'Validated direct relocations: {changed}')
