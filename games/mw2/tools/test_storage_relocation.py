"""Run the relocated retail client scan against four isolated records."""
import sys
from pathlib import Path
from struct import pack

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_PROT_EXEC, UC_PROT_READ
from unicorn.x86_const import UC_X86_REG_EFLAGS, UC_X86_REG_RAX, UC_X86_REG_RIP, UC_X86_REG_RSP

from storage_relocation import (CLIENT_BASE, CLIENT_END, CLIENT_STRIDE,
                                CONNECTION_BASE, CONNECTION_END, CONNECTION_STRIDE,
                                relocate)


def check(executable):
    image, bases, counts = relocate(executable)
    original = pefile.PE(str(executable), fast_load=True).get_memory_mapped_image()
    assert bases['clients'] % 0x1000 == bases['connections'] % 0x1000 == 0
    assert len(image) >= bases['connections'] + 4 * CONNECTION_STRIDE
    assert image[CLIENT_END:CLIENT_END + 32] == original[CLIENT_END:CLIENT_END + 32]
    assert image[CONNECTION_END:CONNECTION_END + 32] == original[CONNECTION_END:CONNECTION_END + 32]
    assert image[bases['clients']:bases['clients'] + 2 * CLIENT_STRIDE] == original[CLIENT_BASE:CLIENT_END]
    assert image[bases['connections']:bases['connections'] + 2 * CONNECTION_STRIDE] == original[CONNECTION_BASE:CONNECTION_END]
    assert not any(image[bases['clients'] + 2 * CLIENT_STRIDE:bases['clients'] + 4 * CLIENT_STRIDE])
    assert not any(image[bases['connections'] + 2 * CONNECTION_STRIDE:bases['connections'] + 4 * CONNECTION_STRIDE])
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
    print('PASS relocated retail scan recognizes slots 0-3 and ignores old storage')
    print('PASS relocated connection lookup ignores old storage')
    print(f'PASS {sum(counts.values())} verified direct instruction relocations')


if __name__ == '__main__':
    check(Path(sys.argv[1]))
