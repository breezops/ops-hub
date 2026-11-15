"""Inventory direct x64 references to the retail co-op player storage.

The exact executable hash gates all reported offsets. Output is an inventory,
not a patch list. Pointer-derived accesses still need dataflow review.
"""

import argparse
import hashlib
import json
from pathlib import Path

import pefile
from capstone import CS_ARCH_X86, CS_MODE_64, Cs
from capstone.x86 import X86_OP_MEM, X86_REG_RIP

EXPECTED_SHA256 = "2f8c9cfda1b8c0fb308fd9719577154e8222fc7654f5e8f1bf54579bca69d805"
STORAGE = {
    "server_clients_base": 0x19ADA98,
    "server_clients_end": 0x19ADA98 + 2 * 0x1678,
    "coop_connections_base": 0x540BA0,
    "coop_connections_end": 0x540BA0 + 2 * 0x2650,
    "player_state_base_pointer": 0xD96E70,
    "player_count": 0xD96EC0,
    "entity_storage_base": 0xD9E070,
}


def audit(executable: Path):
    data = executable.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError("Unsupported executable hash")

    pe = pefile.PE(data=data, fast_load=True)
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    decoder.detail = True
    decoder.skipdata = True
    references = {name: [] for name in STORAGE}

    for section in pe.sections:
        if not section.Characteristics & 0x20000000:
            continue
        for instruction in decoder.disasm(section.get_data(), section.VirtualAddress):
            if instruction.id == 0:
                continue
            for operand in instruction.operands:
                if operand.type != X86_OP_MEM or operand.mem.base != X86_REG_RIP:
                    continue
                target = instruction.address + instruction.size + operand.mem.disp
                for name, address in STORAGE.items():
                    if target == address:
                        references[name].append({
                            "rva": hex(instruction.address),
                            "bytes": instruction.bytes.hex(),
                            "instruction": f"{instruction.mnemonic} {instruction.op_str}",
                        })

    expected_minimum = {
        "server_clients_base": 17,
        "server_clients_end": 3,
        "coop_connections_base": 12,
        "coop_connections_end": 7,
        "player_state_base_pointer": 22,
        "player_count": 8,
    }
    for name, minimum in expected_minimum.items():
        if len(references[name]) < minimum:
            raise ValueError(f"Reference inventory incomplete for {name}")
    return {
        "executable_sha256": digest,
        "scope": "direct RIP-relative references; indirect pointer uses are not included",
        "addresses": {name: hex(address) for name, address in STORAGE.items()},
        "references": references,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--out", type=Path)
    options = parser.parse_args()
    result = audit(options.executable)
    if options.out:
        options.out.parent.mkdir(parents=True, exist_ok=True)
        options.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    for name, entries in result["references"].items():
        print(f"{name}: {len(entries)} direct references")
