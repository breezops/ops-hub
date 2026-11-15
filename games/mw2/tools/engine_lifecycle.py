"""Bounded tests for the retail level setup, entity binding and record reset paths."""
from struct import pack

from unicorn import UC_PROT_EXEC, UC_PROT_READ
from unicorn.x86_const import (
    UC_X86_REG_RBP, UC_X86_REG_RCX,
    UC_X86_REG_RDX, UC_X86_REG_RIP, UC_X86_REG_R8,
)

ENTITY_BASE = 0xd9e070
ENTITY_STRIDE = 0x2a0
CLIENT_LINK = 0x108
PLAYER_STRIDE = 0xafe0
PLAYER_BASE_PTR = 0xd96e70
PLAYER_COUNT = 0xd96ec0
FIXTURE_PLAYERS = 0x3400000
FIXTURE_ENTITIES = 0x3500000


def check_lifecycle(pe, vm, require):
    for page in (0x171000, 0x231000, 0x232000, 0x26a000, 0x22c000):
        vm.mem_map(page, 4096, UC_PROT_READ | UC_PROT_EXEC)
        vm.mem_write(page, pe.get_data(page, 4096))
    vm.mem_map(PLAYER_BASE_PTR & ~4095, 4096)
    vm.mem_map(ENTITY_BASE & ~4095, 4096)
    vm.mem_map(0xeed000, 4096)
    vm.mem_map(FIXTURE_PLAYERS, 0x2c000)
    vm.mem_map(FIXTURE_ENTITIES, 4096)

    def run(begin, end):
        vm.emu_start(begin, end, count=100)
        require(vm.reg_read(UC_X86_REG_RIP) == end, "Unexpected lifecycle branch")

    # Inspect the actual arguments at the call boundary. No CRT calls are stubbed.
    run(0x1716af, 0x1716d5)
    require(vm.mem_read(PLAYER_COUNT, 4) == pack("<I", 2), "Retail count mismatch")
    require(vm.reg_read(UC_X86_REG_R8) == 2 * PLAYER_STRIDE, "Retail clear size mismatch")
    stock_players = vm.reg_read(UC_X86_REG_RCX)
    run(0x1716da, 0x1716e8)
    require(vm.mem_read(PLAYER_BASE_PTR, 8) == pack("<Q", stock_players), "Player base mismatch")

    for count in (2, 4):
        # Seed adjacent fields and a fifth entity so unintended writes are visible.
        original = bytes([0xa5]) * (5 * ENTITY_STRIDE)
        expected = bytearray(original)
        vm.mem_write(ENTITY_BASE, original)
        vm.mem_write(PLAYER_COUNT, pack("<I", count))
        vm.mem_write(PLAYER_BASE_PTR, pack("<Q", FIXTURE_PLAYERS))
        run(0x171707, 0x17173c)
        for slot in range(count):
            offset = slot * ENTITY_STRIDE + CLIENT_LINK
            expected[offset:offset + 8] = pack("<Q", FIXTURE_PLAYERS + slot * PLAYER_STRIDE)
        require(vm.mem_read(ENTITY_BASE, len(expected)) == expected, "Unexpected entity binding")

    # Check the inverse calculation made by the record-to-entity callback.
    # Stop before its player setup call. Exercise the later link store separately.
    clients, stride = 0x19ada98, 0x1678
    entity_base_global = 0x22cff1 + 0x1796857
    vm.mem_map(entity_base_global & ~4095, 4096)
    vm.mem_write(entity_base_global, pack("<Q", FIXTURE_ENTITIES))
    for slot in range(4):
        record = clients + slot * stride
        vm.reg_write(UC_X86_REG_RCX, record)
        run(0x22cf9a, 0x22cfcb)
        require(vm.reg_read(UC_X86_REG_RCX) == slot, "Record converted to wrong slot")
        run(0x22cfdb, 0x22cfde)
        run(0x22cfe3, 0x22cff8)
        require(vm.mem_read(record + 0x1670, 8) == pack("<Q", FIXTURE_ENTITIES + slot * ENTITY_STRIDE),
                "Server record linked to wrong entity")
    print("PASS retail count and clear size; two/four entity bindings; four inverse record indices")

    # Execute the stock reset loop up to its helper, check the exact range it
    # requests and model only that clear. Check the entire fixture afterwards.
    source = bytes([0x5a]) * (4 * stride)
    vm.mem_write(clients, source)
    expected = bytearray(source)
    vm.reg_write(UC_X86_REG_RBP, 0)
    run(0x231ae1, 0x231af8)
    for slot in range(2):
        offset = slot * stride
        require(vm.reg_read(UC_X86_REG_RCX) == clients + offset, "Reset selected wrong record")
        run(0x232180, 0x26aa40)
        require(vm.reg_read(UC_X86_REG_RCX) == clients + offset + 4 and
                vm.reg_read(UC_X86_REG_R8) == 12 and vm.reg_read(UC_X86_REG_RDX) == 0,
                "Unexpected reset range")
        vm.mem_write(clients + offset + 4, bytes(12))
        expected[offset + 4:offset + 16] = bytes(12)
        expected[offset + 0x1670:offset + 0x1678] = bytes(8)
        run(0x231afd, 0x231af8 if slot == 0 else 0x231b11)
    require(vm.mem_read(clients, len(source)) == expected, "Unexpected stock reset footprint")
    print("PASS stock bulk reset unlinks two server records; two adjacent fixture records untouched")
