"""Run the relocated retail client scan against four isolated records."""
import sys
from pathlib import Path
from struct import pack

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_HOOK_CODE, UC_MODE_64, UC_PROT_EXEC, UC_PROT_READ
from unicorn.x86_const import (UC_X86_REG_EFLAGS, UC_X86_REG_R8, UC_X86_REG_RAX,
                               UC_X86_REG_RBP, UC_X86_REG_RBX, UC_X86_REG_RCX,
                               UC_X86_REG_RDI, UC_X86_REG_RIP, UC_X86_REG_RSI,
                               UC_X86_REG_RSP)

from storage_relocation import (AUX_BASE, AUX_END, AUX_STRIDE,
                                CLIENT_BASE, CLIENT_END, CLIENT_STRIDE,
                                CONNECTION_BASE, CONNECTION_END, CONNECTION_STRIDE,
                                PLAYER_BASE, PLAYER_END, PLAYER_STRIDE,
                                relocate)


def check(executable):
    image, bases, counts = relocate(executable)
    original = pefile.PE(str(executable), fast_load=True).get_memory_mapped_image()
    assert all(base % 0x1000 == 0 for base in bases.values())
    assert len(image) >= bases['aux'] + 4 * AUX_STRIDE
    assert image[CLIENT_END:CLIENT_END + 32] == original[CLIENT_END:CLIENT_END + 32]
    assert image[CONNECTION_END:CONNECTION_END + 32] == original[CONNECTION_END:CONNECTION_END + 32]
    assert image[PLAYER_END:PLAYER_END + 32] == original[PLAYER_END:PLAYER_END + 32]
    assert image[AUX_END:AUX_END + 32] == original[AUX_END:AUX_END + 32]
    assert image[bases['clients']:bases['clients'] + 2 * CLIENT_STRIDE] == original[CLIENT_BASE:CLIENT_END]
    assert image[bases['connections']:bases['connections'] + 2 * CONNECTION_STRIDE] == original[CONNECTION_BASE:CONNECTION_END]
    assert image[bases['players']:bases['players'] + 2 * PLAYER_STRIDE] == original[PLAYER_BASE:PLAYER_END]
    assert image[bases['aux']:bases['aux'] + 2 * AUX_STRIDE] == original[AUX_BASE:AUX_END]
    assert not any(image[bases['clients'] + 2 * CLIENT_STRIDE:bases['clients'] + 4 * CLIENT_STRIDE])
    assert not any(image[bases['connections'] + 2 * CONNECTION_STRIDE:bases['connections'] + 4 * CONNECTION_STRIDE])
    assert not any(image[bases['players'] + 2 * PLAYER_STRIDE:bases['players'] + 4 * PLAYER_STRIDE])
    assert not any(image[bases['aux'] + 2 * AUX_STRIDE:bases['aux'] + 4 * AUX_STRIDE])
    vm = Uc(UC_ARCH_X86, UC_MODE_64)
    code_page = 0x22E000
    vm.mem_map(code_page, 0x2000, UC_PROT_READ | UC_PROT_EXEC)
    vm.mem_write(code_page, bytes(image[code_page:code_page + 0x2000]))
    new_base = bases['clients']
    vm.mem_map(new_base, 0x6000)
    vm.mem_write(new_base, bytes(image[new_base:new_base + 0x6000]))
    vm.mem_map(CLIENT_BASE & ~0xfff, 0x4000)
    vm.mem_write(CLIENT_BASE & ~0xfff, bytes(image[CLIENT_BASE & ~0xfff:(CLIENT_BASE & ~0xfff) + 0x4000]))
    vm.mem_map(0x4000000, 0x1000)
    vm.mem_write(0x4000800, pack('<Q', 0x4000900))
    for active in range(4):
        vm.mem_write(new_base, bytes(4 * CLIENT_STRIDE))
        vm.mem_write(new_base + active * CLIENT_STRIDE, pack('<I', 2))
        # A stale state at the original location must not influence the scan.
        vm.mem_write(CLIENT_BASE, pack('<I', 2 if active == 3 else 0))
        vm.reg_write(UC_X86_REG_RSP, 0x4000800)
        vm.emu_start(0x22ED40, 0x4000900, count=200)
        assert vm.reg_read(UC_X86_REG_RIP) == 0x4000900
        assert vm.reg_read(UC_X86_REG_RAX) & 0xff == 1
    vm.mem_write(new_base, bytes(4 * CLIENT_STRIDE))
    vm.mem_write(CLIENT_BASE, pack('<I', 2))
    vm.reg_write(UC_X86_REG_RSP, 0x4000800)
    vm.emu_start(0x22ED40, 0x4000900, count=200)
    assert vm.reg_read(UC_X86_REG_RAX) & 0xff == 0
    # A direct connection lookup must read the copied, relocated second slot.
    vm.mem_map(0xA6000, 0x1000, UC_PROT_READ | UC_PROT_EXEC)
    vm.mem_write(0xA6000, bytes(image[0xA6000:0xA7000]))
    vm.mem_map(bases['connections'], 0xA000)
    vm.mem_write(bases['connections'], bytes(image[bases['connections']:bases['connections'] + 0xA000]))
    vm.mem_map(CONNECTION_BASE & ~0xfff, 0x6000)
    vm.mem_write(CONNECTION_BASE & ~0xfff, bytes(image[CONNECTION_BASE & ~0xfff:(CONNECTION_BASE & ~0xfff) + 0x6000]))
    vm.mem_write(bases['connections'] + CONNECTION_STRIDE + 0x10, pack('<I', 2))
    vm.mem_write(CONNECTION_BASE + CONNECTION_STRIDE + 0x10, pack('<I', 0))
    vm.emu_start(0xA67BB, 0xA67C2, count=1)
    assert vm.reg_read(UC_X86_REG_EFLAGS) & 0x40
    vm.mem_write(bases['connections'], bytes(4 * CONNECTION_STRIDE))
    vm.mem_map(0x22D000, 0x1000, UC_PROT_READ | UC_PROT_EXEC)
    vm.mem_write(0x22D000, bytes(image[0x22D000:0x22E000]))
    vm.mem_map(0x52F000, 0x1000)
    vm.mem_write(0x52FB5C, b'\x01')  # setup takes its no-client branch
    vm.reg_write(UC_X86_REG_RSP, 0x4000800)
    vm.emu_start(0x22D055, 0x22D064, count=5)
    assert vm.reg_read(UC_X86_REG_RCX) == bases['clients']
    assert vm.reg_read(UC_X86_REG_R8) == 4 * CLIENT_STRIDE
    visited = []

    def setup_slot(machine, address, size, _):
        if address == 0x22D107:
            visited.append((machine.reg_read(UC_X86_REG_RBP),
                            machine.reg_read(UC_X86_REG_RSI)))

    hook = vm.hook_add(UC_HOOK_CODE, setup_slot, begin=0x22D107, end=0x22D107)
    vm.emu_start(0x22D069, 0x22D127, count=200)
    vm.hook_del(hook)
    assert visited == [(bases['connections'] + 0x10 + slot * CONNECTION_STRIDE,
                        bases['clients'] + 0x1210 + slot * CLIENT_STRIDE)
                       for slot in range(4)]
    vm.mem_map(0x231000, 0x1000, UC_PROT_READ | UC_PROT_EXEC)
    vm.mem_write(0x231000, bytes(image[0x231000:0x232000]))
    for slot in range(4):
        vm.mem_write(bases['clients'] + slot * CLIENT_STRIDE + 0x1670,
                     pack('<Q', 0x12345678))
    adjacent = vm.mem_read(CLIENT_END, 32)
    vm.reg_write(UC_X86_REG_RBP, 0)
    vm.emu_start(0x231AE1, 0x231AF8, count=5)
    assert vm.reg_read(UC_X86_REG_RSI) == 4
    assert vm.reg_read(UC_X86_REG_RBX) == bases['clients']
    assert vm.reg_read(UC_X86_REG_RDI) == bases['clients'] + 0x1670
    for slot in range(4):
        vm.emu_start(0x231AFD, 0x231AF8 if slot < 3 else 0x231B11, count=10)
        if slot < 3:
            assert vm.reg_read(UC_X86_REG_RBX) == bases['clients'] + (slot + 1) * CLIENT_STRIDE
    assert all(vm.mem_read(bases['clients'] + slot * CLIENT_STRIDE + 0x1670, 8)
               == bytes(8) for slot in range(4))
    assert vm.mem_read(CLIENT_END, 32) == adjacent
    vm.mem_map(0x171000, 0x1000, UC_PROT_READ | UC_PROT_EXEC)
    vm.mem_write(0x171000, bytes(image[0x171000:0x172000]))
    for page in (0xD96000, 0xEED000, 0xD9E000):
        vm.mem_map(page, 0x1000)
    vm.emu_start(0x1716AF, 0x1716D5, count=6)
    assert vm.reg_read(UC_X86_REG_RCX) == bases['players']
    assert vm.reg_read(UC_X86_REG_R8) == 4 * PLAYER_STRIDE
    assert vm.mem_read(0xD96EC0, 4) == pack('<I', 4)
    vm.emu_start(0x1716DA, 0x1716E8, count=2)
    assert vm.mem_read(0xD96E70, 8) == pack('<Q', bases['players'])
    vm.emu_start(0x1716EF, 0x1716FD, count=2)
    assert vm.mem_read(0xD96EA0, 8) == pack('<Q', bases['players'] + 4 * PLAYER_STRIDE)
    vm.mem_write(0xD9E000, bytes([0xA5]) * 0x1000)
    vm.emu_start(0x171707, 0x17173C, count=50)
    assert all(vm.mem_read(0xD9E070 + slot * 0x2A0 + 0x108, 8)
               == pack('<Q', bases['players'] + slot * PLAYER_STRIDE)
               for slot in range(4))
    assert vm.mem_read(0xD9E070 + 4 * 0x2A0 + 0x108, 8) == bytes([0xA5]) * 8
    vm.mem_map(0x22C000, 0x1000, UC_PROT_READ | UC_PROT_EXEC)
    vm.mem_write(0x22C000, bytes(image[0x22C000:0x22D000]))
    vm.mem_map(0x19C3000, 0x1000)
    vm.mem_write(0x19C3848, pack('<Q', 0xD9E070))
    for slot in range(4):
        record = bases['clients'] + slot * CLIENT_STRIDE
        vm.reg_write(UC_X86_REG_RCX, record)
        vm.emu_start(0x22CF9A, 0x22CFCB, count=20)
        assert vm.reg_read(UC_X86_REG_RCX) == slot
        vm.emu_start(0x22CFDB, 0x22CFDE, count=1)
        vm.emu_start(0x22CFE3, 0x22CFF8, count=3)
        assert vm.mem_read(record + 0x1670, 8) == pack('<Q', 0xD9E070 + slot * 0x2A0)
    vm.mem_map(0x15E000, 0x1000, UC_PROT_READ | UC_PROT_EXEC)
    vm.mem_write(0x15E000, bytes(image[0x15E000:0x15F000]))
    vm.mem_map(bases['aux'], 0x1000)
    vm.mem_write(bases['aux'], bytes(image[bases['aux']:bases['aux'] + 4 * AUX_STRIDE]))
    old_aux_neighbor = vm.mem_read(AUX_END, 32)
    for slot in (2, 3):
        record = bases['aux'] + slot * AUX_STRIDE
        saved = 0x12340000 + slot
        vm.mem_write(record + 0x178, pack('<Q', saved))
        vm.reg_write(UC_X86_REG_RSI, slot)
        vm.emu_start(0x15E39C, 0x15E3BF, count=8)
        assert vm.reg_read(UC_X86_REG_RDI) == record
        assert vm.reg_read(UC_X86_REG_RCX) == record
        assert vm.reg_read(UC_X86_REG_RBX) == saved
        vm.mem_write(record, bytes(AUX_STRIDE))  # model only the retail clear call
        vm.emu_start(0x15E3C4, 0x15E3D4, count=3)
        assert vm.mem_read(record + 0x178, 8) == pack('<Q', saved)
        assert vm.mem_read(record + 4, 4) == pack('<I', slot)
        assert vm.mem_read(record, 4) == pack('<I', 1)
    assert vm.mem_read(AUX_END, 32) == old_aux_neighbor
    print('PASS relocated retail scan recognizes slots 0-3 and ignores old storage')
    print('PASS relocated connection lookup ignores old storage')
    print('PASS four-slot setup and reset walks leave neighboring storage intact')
    print('PASS four player-state bindings with a fifth entity left untouched')
    print('PASS record-to-entity callback addresses four distinct slots')
    print('PASS third and fourth auxiliary records initialize without adjacent writes')
    print(f'PASS {sum(counts.values())} verified instruction modifications')


if __name__ == '__main__':
    check(Path(sys.argv[1]))
