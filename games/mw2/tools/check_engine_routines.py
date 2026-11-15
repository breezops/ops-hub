"""Exercise retail x64 engine routines in an isolated emulator using local game bytes."""
import argparse
import hashlib
from pathlib import Path
from struct import pack, unpack

from engine_lifecycle import check_lifecycle

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_PROT_READ, UC_PROT_EXEC
from unicorn.x86_const import (
    UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8,
    UC_X86_REG_RSP, UC_X86_REG_RIP,
)

BUILD = "2f8c9cfda1b8c0fb308fd9719577154e8222fc7654f5e8f1bf54579bca69d805"
WRITE_BITS, READ_BITS, SCAN = 0x201B30, 0xA2310, 0x22ED40
CLIENTS, STRIDE = 0x19ADA98, 0x1678
MESSAGE, BUFFER, STACK, STOP = 0x3000000, 0x3001000, 0x3002808, 0x3003000


def require(condition, description):
    if not condition:
        raise RuntimeError(description)


def check(path):
    data = path.read_bytes()
    require(hashlib.sha256(data).hexdigest() == BUILD, "Unsupported game build")
    pe = pefile.PE(data=data, fast_load=True)
    vm = Uc(UC_ARCH_X86, UC_MODE_64)
    for start in (WRITE_BITS, READ_BITS, SCAN, 0x22f020):
        page = start & ~4095
        vm.mem_map(page, 4096, UC_PROT_READ | UC_PROT_EXEC)
        vm.mem_write(page, pe.get_data(page, 4096))
    vm.mem_map(CLIENTS & ~4095, 0x7000)
    vm.mem_map(MESSAGE, 0x4000)

    def call(start, argument=0, width=0):
        vm.mem_write(STACK, pack("<Q", STOP))
        for reg, value in ((UC_X86_REG_RSP, STACK), (UC_X86_REG_RCX, MESSAGE),
                           (UC_X86_REG_RDX, argument), (UC_X86_REG_R8, width)):
            vm.reg_write(reg, value)
        vm.emu_start(start, STOP, count=5000)
        require(vm.reg_read(UC_X86_REG_RIP) == STOP, "Routine failed to return")
        return vm.reg_read(UC_X86_REG_RAX) & 0xffffffff

    def initialize():
        vm.mem_write(MESSAGE, bytes(0x30))
        vm.mem_write(MESSAGE + 8, pack("<Q", BUFFER))
        vm.mem_write(MESSAGE + 0x18, pack("<I", 64))
        vm.mem_write(BUFFER, bytes(64))

    # Cross every possible byte boundary and verify both bit order and cursor.
    for prefix in range(8):
        initialize()
        if prefix:
            call(WRITE_BITS, 0, prefix)
        for player in range(4):
            call(WRITE_BITS, player, 2)
        encoded = int.from_bytes(vm.mem_read(BUFFER, 2), "little")
        require(encoded == 0xe4 << prefix, "Incorrect packed player IDs")
        vm.mem_write(MESSAGE + 0x28, bytes(4))
        if prefix:
            require(call(READ_BITS, prefix) == 0, "Incorrect prefix")
        for player in range(4):
            require(call(READ_BITS, 2) == player, "Incorrect decoded ID")
        require(unpack("<I", vm.mem_read(MESSAGE + 0x28, 4))[0] == prefix + 8,
                "Incorrect read cursor")
    initialize()
    require(call(READ_BITS, 2) == 0xffffffff and vm.mem_read(MESSAGE, 4) == pack("<I", 1),
            "Empty read must set the retail overflow flag")
    print("PASS retail bit routines: 32 IDs across eight starting bit offsets, plus empty input")

    for active in range(4):
        field = CLIENTS + active * STRIDE
        vm.mem_write(field, pack("<I", 2))
        observed = call(SCAN) & 0xff
        require(observed == int(active < 2), "Unexpected retail client scan result")
        vm.mem_write(field, bytes(4))
    print("PASS retail scan: records 0 and 1 observed, records 2 and 3 ignored")


    # Exercise the paired walk in the retail function, from its loop header
    # through the conditional exit. Its preamble normally supplies these
    # pointers. Separate emulated allocations keep adjacent retail data intact.
    from unicorn.x86_const import (UC_X86_REG_RBX, UC_X86_REG_R10,
                                   UC_X86_REG_R9)
    from unicorn import UC_HOOK_CODE

    connections, clients = 0x3100000, 0x3200000
    vm.mem_map(connections, 0xa000)
    vm.mem_map(clients, 0x6000)
    # rdx addresses the connection state field at offset 0x10.
    def stop_at_exit(machine, address, size, user_data):
        if address in (0x22f05a, 0x22f05c):
            machine.emu_stop()

    hook = vm.hook_add(UC_HOOK_CODE, stop_at_exit, begin=0x22f05a, end=0x22f05c)
    try:
        for slot in range(4):
            client = clients + slot * STRIDE
            conn_state = connections + slot * 0x2650 + 0x10
            vm.mem_write(client, pack("<I", 3))
            for state in (2, 0):
                vm.mem_write(conn_state, pack("<I", state))
                for reg, value in ((UC_X86_REG_RBX, clients), (UC_X86_REG_RDX, connections + 0x10),
                                   (UC_X86_REG_RCX, 0), (UC_X86_REG_R8, 1), (UC_X86_REG_R9, 0),
                                   (UC_X86_REG_R10, clients + 4 * STRIDE)):
                    vm.reg_write(reg, value)
                vm.emu_start(0x22f020, STOP, count=200)
                expected = 0x22f05c if state == 2 else 0x22f05a
                require(vm.reg_read(UC_X86_REG_RIP) == expected, "Unexpected paired scan exit")
                if state == 2:
                    require(vm.reg_read(UC_X86_REG_RBX) == client and
                            vm.reg_read(UC_X86_REG_RDX) == conn_state and
                            vm.reg_read(UC_X86_REG_RCX) == slot, "Cross-slot state mismatch")
            vm.mem_write(client, bytes(4))
    finally:
        vm.hook_del(hook)
    print("PASS retail paired loop body: four isolated records selected by fixture state")
    check_lifecycle(pe, vm, require)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    check(parser.parse_args().exe)

