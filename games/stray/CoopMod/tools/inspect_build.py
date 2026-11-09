"""Read-only build fingerprint and networking capability inventory."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def inspect(root: Path) -> dict:
    exe = root / "Hk_project/Binaries/Win64/Stray-Win64-Shipping.exe"
    data = exe.read_bytes()
    strings = set(m.group().decode("ascii") for m in re.finditer(rb"[\x20-\x7e]{6,}", data))
    strings.update(m.group().decode("utf-16-le") for m in re.finditer(rb"(?:[\x20-\x7e]\x00){6,}", data))
    patterns = ("IpNetDriver", "SteamNetDriver", "OnlineSubsystem", "ServerTravel", "ClientTravel", "PendingNetGame", "GameNetDriver", "SetReplicates", "GetLifetimeReplicatedProps")
    return {
        "executable": str(exe), "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
        "network_markers": {p: sorted(s for s in strings if p in s)[:12] for p in patterns},
        "engine_versions": sorted(s for s in strings if re.search(r"(?:\+\+UE4\+Release|4\.27\.\d)", s))[:20],
        "game_class_names": sorted(s for s in strings if re.fullmatch(r"(?:AHk|UHk|HK|Hk)[A-Za-z0-9_]*(?:Character|Pawn|Controller|GameMode|GameInstance|SaveGame|Network)[A-Za-z0-9_]*", s)),
        "note": "String presence is evidence of compiled symbols, not proof of working multiplayer.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = json.dumps(inspect(args.game_root), indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report + "\n", encoding="utf-8")
    print(report)
