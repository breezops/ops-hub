"""Authenticated two-player UDP transport for the Stray co-op prototype.

Uses only Python's standard library. The game exchanges bounded TSV snapshots
with this process; network data is never executed as code. This is not shared
campaign replication: it transports player/animation snapshots and the opening
scene's three interaction bits and sleep request. Other story events are absent.
"""
from __future__ import annotations

import argparse
import hashlib
import hmac
import ipaddress
import json
import logging
import math
import os
from pathlib import Path
import re
import secrets
import select
import socket
import time
from shared_objects import Authority, MAX_OBJECTS, read_objects, read_effects, validate_effect, validate_object, validate_record, render_tsv
import cinematics
import cooperative

VERSION = 6
MAX_PACKET = 4096
TIMEOUT = 5.0
MAP = re.compile(r"^/Game/[A-Za-z0-9_/]+$")
ANIMATION = re.compile(r"^/Game/Animation/CAT/[A-Za-z0-9_/.-]+$")
LOG = logging.getLogger("StrayCoop")


def integer(value, low=0, high=2**53 - 1):
    if type(value) is not int or not low <= value <= high:
        raise ValueError("invalid integer")
    return value


def validate_state(state):
    if not isinstance(state, dict) or set(state) != {"seq", "map", "chapter", "stage", "pose", "velocity", "meows", "visual", "animation", "story", "pause", "clock", "cinematic"}:
        raise ValueError("invalid state fields")
    integer(state["seq"])
    integer(state["chapter"], -1, 1000)
    integer(state["stage"], -1, 100000)
    integer(state["meows"])
    validate_story(state["story"])
    integer(state["pause"], 0, 1)
    cinematics.validate(state['cinematic'])
    if type(state['clock']) not in (float,int) or not math.isfinite(state['clock']) or not 0<=state['clock']<=1e9:
        raise ValueError('invalid source clock')
    if not isinstance(state["map"], str) or len(state["map"]) > 240 or not (state["map"] == "-" or MAP.fullmatch(state["map"])):
        raise ValueError("invalid map")
    for key, count, limit in (("pose", 6, 1e7), ("visual", 6, 1e7), ("velocity", 3, 1e5)):
        values = state[key]
        if not isinstance(values, list) or len(values) != count:
            raise ValueError("invalid vector")
        if any(type(v) not in (int, float) or not math.isfinite(v) or abs(v) > limit for v in values):
            raise ValueError("invalid number")
    if any(abs(v) > 36000 for v in state["pose"][3:] + state["visual"][3:]):
        raise ValueError("invalid rotation")
    a = state["animation"]
    if not isinstance(a, list) or len(a) != 5:
        raise ValueError("invalid animation")
    integer(a[0], 0, 6)
    integer(a[4], 0, 1)
    if not isinstance(a[1], str) or len(a[1]) > 240 or not (a[1] == "-" or ANIMATION.fullmatch(a[1])):
        raise ValueError("invalid animation asset")
    for value, low, high in ((a[2], .1, 4), (a[3], -1, 1)):
        if type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high:
            raise ValueError("invalid animation parameter")
    return state


def validate_story(story):
    if not isinstance(story, list) or len(story) != 3:
        raise ValueError("invalid opening state")
    for value, maximum in zip(story, (1, 7, 1)):
        integer(value, 0, maximum)
    if story[2] and story[1] != 7:
        raise ValueError("sleep requires three completed interactions")
    return story


def read_story(path):
    try:
        if path.stat().st_size > 80 or time.time()-path.stat().st_mtime > 2:
            return [0, 0, 0]
        text = path.read_text(encoding="ascii")
        fields = text.strip().split("\t")
        if not text.endswith("\n") or len(fields) != 4 or fields[0] != "OPENING":
            return [0, 0, 0]
        return validate_story([int(v) for v in fields[1:]])
    except (OSError, ValueError, UnicodeError):
        return [0, 0, 0]


def idle_state():
    return dict(seq=0, map="-", chapter=-1, stage=-1, pose=[0.] * 6, velocity=[0.] * 3, meows=0,
                visual=[0.]*6, animation=[0,"-",1,-1,1], story=[0,0,0], pause=0,clock=0,cinematic=['-',0,0,0])


def read_pause(path, previous=0):
    try:
        if path.stat().st_size > 32 or time.time()-path.stat().st_mtime > 2:
            return 0
        text=path.read_text(encoding='ascii')
        if text not in ('PAUSE\t0\n','PAUSE\t1\n'):
            return previous  # A partially written local file is retried.
        return int(text[6])
    except OSError:
        return 0
    except UnicodeError:
        return previous


def read_local(path, previous=None):
    try:
        if path.stat().st_size > 1024 or time.time() - path.stat().st_mtime > 2:
            return idle_state()
        text = path.read_text(encoding="ascii")
        if not text.endswith("\n"):
            return previous or idle_state()  # Writer has not finished the snapshot.
        fields = text.strip().split("\t")
        if len(fields) != 28 or fields[:2] != ["STRAYCOOP", str(VERSION)]:
            return previous or idle_state()
        return validate_state(dict(seq=int(fields[2]), map=fields[3], chapter=int(fields[4]),
                                   stage=int(fields[5]), pose=[float(v) for v in fields[6:12]],
                                   velocity=[float(v) for v in fields[12:15]], meows=int(fields[15]),
                                   visual=[float(v) for v in fields[16:22]],
                                   animation=[int(fields[22]),fields[23],float(fields[24]),float(fields[25]),int(fields[26])],
                                   story=read_story(path.with_name("story-local.tsv")), pause=0,clock=float(fields[27]),cinematic=['-',0,0,0]))
    except (OSError, ValueError, UnicodeError):
        return previous or idle_state()


def canonical(payload):
    return json.dumps(payload, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("ascii")


def encode(payload, key):
    data = canonical(payload)
    packet = canonical({"payload": payload, "mac": hmac.new(key, data, hashlib.sha256).hexdigest()})
    if len(packet) > MAX_PACKET:
        raise ValueError("packet too large")
    return packet


def decode(packet, key):
    if len(packet) > MAX_PACKET:
        raise ValueError("packet too large")
    wrapper = json.loads(packet)
    if not isinstance(wrapper, dict) or set(wrapper) != {"payload", "mac"}:
        raise ValueError("invalid envelope")
    payload, mac = wrapper["payload"], wrapper["mac"]
    expected = hmac.new(key, canonical(payload), hashlib.sha256).hexdigest()
    if not isinstance(mac, str) or not hmac.compare_digest(mac, expected):
        raise ValueError("authentication failed")
    if not isinstance(payload, dict) or payload.get("v") != VERSION:
        raise ValueError("protocol mismatch")
    return payload


def atomic_text(path, text):
    temp = path.with_suffix(".tmp")
    temp.write_text(text, encoding="ascii", newline="\n")
    try:
        os.replace(temp, path)
    except PermissionError:
        pass  # Lua may have the old snapshot open. Retry on the next tick.


class Bridge:
    def __init__(self, role, mod_dir, key, host="127.0.0.1", port=7777, bind="0.0.0.0"):
        self.role = role
        self.directory = Path(mod_dir)
        self.directory.mkdir(parents=True, exist_ok=True)
        self.key = hashlib.sha256(key.encode("utf-8")).digest()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # Do not set SO_REUSEADDR: two hosts must not silently share a port.
        self.sock.bind((bind, port if role == "host" else 0))
        self.sock.setblocking(False)
        self.target = (str(ipaddress.IPv4Address(host)), port)
        self.nonce = secrets.token_hex(16)
        self.session = secrets.token_hex(16) if role == "host" else None
        self.peer = None
        self.last_rx = 0.
        self.peer_seq = -1
        self.peer_state_seq = -1
        self.peer_state = idle_state()
        self.sequence = 0
        self.rejected = 0
        self.accepted = 0
        self.own_id = 1 if role == "host" else 2
        self.running = True
        self.last_remote_map = None
        self.local_state = idle_state()
        self.shared_story = [0, 0, 0]
        self.pause_votes = [0, 0]
        self.objects=Authority()
        self.object_sequence=0
        self.peer_object_sequence=-1
        self.local_objects=[]
        self.object_cursor=0
        self.object_scene=None
        self.skips=cinematics.Skips()
        self.shared_skips=[]
        self.peer_coop=cooperative.empty()

    def object_send(self, target, kind, **fields):
        self.object_sequence+=1
        self.send(target,kind=kind,nonce=self.peer[1] if self.role=='host' else self.nonce,
                  session=self.session,seq=self.object_sequence,**fields)

    def send(self, target, **fields):
        self.sock.sendto(encode(dict(v=VERSION, **fields), self.key), target)

    def receive(self, data, address, now):
        p = decode(data, self.key)
        kind = p.get("kind")
        if kind in ('object_request','object_state','effect_request','effect_state'):
            expected='object_request' if self.role=='host' else 'object_state'
            if kind.startswith('effect'): expected='effect_request' if self.role=='host' else 'effect_state'
            address_ok=self.peer and address==self.peer[0] if self.role=='host' else address==self.target
            nonce=self.peer[1] if self.role=='host' and self.peer else self.nonce
            if kind!=expected or not address_ok or not self.connected(now) or p.get('session')!=self.session or p.get('nonce')!=nonce:
                raise ValueError('object session')
            sequence=integer(p.get('seq'))
            if sequence<=self.peer_object_sequence: raise ValueError('old object packet')
            if p.get('map')!=self.local_state['map'] or p.get('chapter')!=self.local_state['chapter'] or p.get('map')=='-':
                return
            if kind.startswith('effect'):
                e=validate_effect(p.get('effect'))
                if self.role=='host': self.objects.effect(2,e)
                elif e['fan'] in self.objects.effects or len(self.objects.effects)<16: self.objects.effects[e['fan']]=e
            elif self.role=='host': self.objects.merge(2,validate_object(p.get('object')))
            else:
                r=validate_record(p.get('record'));ident=r['object']['id']
                old=self.objects.records.get(ident)
                if not old and len(self.objects.records)>=MAX_OBJECTS: raise ValueError('object registry full')
                if not old or r['revision']>old['revision']: self.objects.records[ident]=r
            self.peer_object_sequence=sequence
            self.accepted+=1
            return
        if self.role == "host":
            if kind == "hello":
                nonce = p.get("nonce")
                if not isinstance(nonce, str) or not re.fullmatch(r"[0-9a-f]{32}", nonce):
                    raise ValueError("invalid nonce")
                if self.peer and (address, nonce) != self.peer:
                    raise ValueError("session full")
                if not self.peer:
                    self.peer = (address, nonce)
                    self.peer_seq = self.peer_state_seq = -1
                    self.peer_object_sequence=-1
                    self.peer_coop=cooperative.empty()
                    self.objects.seen[2]={}
                    LOG.info("Peer connected as player 2")
                self.last_rx = now
                self.send(address, kind="welcome", nonce=nonce, session=self.session, player=2)
            elif kind == "state":
                if not self.peer or address != self.peer[0] or p.get("nonce") != self.peer[1] or p.get("session") != self.session:
                    raise ValueError("unrecognized peer/session")
                seq = integer(p.get("seq"))
                if seq <= self.peer_seq:
                    raise ValueError("old packet")
                state = validate_state(p.get("state"))
                coop=cooperative.validate(p.get("coop",cooperative.empty()))
                self.peer_seq, self.peer_state, self.last_rx = seq, state, now
                self.peer_coop=coop
            else:
                raise ValueError("unexpected packet")
        else:
            if address != self.target:
                raise ValueError("unexpected sender")
            if kind == "welcome":
                if p.get("nonce") != self.nonce or p.get("player") != 2:
                    raise ValueError("invalid welcome")
                session = p.get("session")
                if not isinstance(session, str) or not re.fullmatch(r"[0-9a-f]{32}", session):
                    raise ValueError("invalid session")
                if self.session and self.session != session:
                    raise ValueError("session changed before timeout")
                if not self.session:
                    self.session = session
                    self.last_rx = now
                    self.peer_seq = -1
                    self.peer_object_sequence=-1
                    self.peer_coop=cooperative.empty()
                    LOG.info("Connected to host as player 2")
            elif kind == "roster":
                if not self.session or p.get("session") != self.session or p.get("nonce") != self.nonce:
                    raise ValueError("incorrect session")
                seq = integer(p.get("seq"))
                if seq <= self.peer_seq:
                    raise ValueError("old packet")
                roster = p.get("players")
                if not isinstance(roster, dict) or set(roster) != {"1", "2"}:
                    raise ValueError("invalid roster")
                state = validate_state(roster["1"])
                validate_state(roster["2"])
                skips=cinematics.validate_events(p.get('skips'))
                coop=cooperative.validate(p.get("coop",cooperative.empty()))
                self.peer_seq, self.peer_state, self.last_rx = seq, state, now
                self.peer_coop=coop
                self.shared_story = state["story"][:]
                self.pause_votes = [state["pause"], roster["2"]["pause"]]
                self.shared_skips=skips
            else:
                raise ValueError("unexpected packet")
        self.accepted += 1

    def connected(self, now):
        return (self.peer is not None if self.role == "host" else self.session is not None) and now - self.last_rx < TIMEOUT

    def publish(self, own, now):
        connected = self.connected(now)
        rows = [f"STRAYCOOP\t{VERSION}\t{self.own_id}\t{self.sequence}\t{int(connected)}\n"]
        if connected:
            peer_id = 2 if self.own_id == 1 else 1
            s = self.peer_state
            fields = ["PLAYER", str(peer_id), str(s["seq"]), s["map"], str(s["chapter"]), str(s["stage"])]
            fields += [format(v, ".6f") for v in s["pose"] + s["velocity"]] + [str(s["meows"])]
            fields += [format(v, ".6f") for v in s["visual"]] + [str(v) for v in s["animation"]]
            fields += [format(s['clock'],'.6f')]
            rows.append("\t".join(fields) + "\n")
        atomic_text(self.directory / "remote.tsv", "".join(rows))
        story = self.shared_story if connected else [0, 0, 0]
        atomic_text(self.directory / "story-remote.tsv",
                    "OPENING\t" + "\t".join(str(v) for v in [self.sequence, int(connected), *story]) + "\n")
        votes=self.pause_votes if connected else [0,0]
        atomic_text(self.directory / 'pause-remote.tsv',
                    f'PAUSE\t{self.sequence}\t{int(connected)}\t{votes[0]}\t{votes[1]}\n')
        skips=self.shared_skips if connected else []
        atomic_text(self.directory/'cinematic-remote.tsv',f'SKIPS\t{self.sequence}\t{int(connected)}\n'+''.join(f'SKIP\t{a}\t{c}\t{phase}\n' for a,c,phase in skips))
        same_scene=connected and own['map']!='-' and own['map']==self.peer_state['map'] and own['chapter']==self.peer_state['chapter']
        atomic_text(self.directory/'coop-remote.tsv',cooperative.render(self.peer_coop,self.own_id,self.sequence,same_scene,self.session or '-'))
        object_text=render_tsv(self.objects.records,self.own_id,self.sequence,connected)
        for effect in self.objects.effects.values(): object_text+=f"FAN\t{effect['fan']}\t{effect['bucket']}\n"
        atomic_text(self.directory/'objects-remote.tsv',object_text)
        status = dict(protocol=VERSION, role=self.role, connected=connected, player=self.own_id,
                      local_map=own["map"], remote_map=self.peer_state["map"] if connected else "-",
                      local_ready=own["map"] != "-", packets_accepted=self.accepted, packets_rejected=self.rejected,
                      local_sequence=own["seq"], remote_sequence=self.peer_state["seq"] if connected else 0)
        status["opening"] = self.shared_story
        status["pause_votes"] = votes
        status['shared_objects']=len(self.objects.records)
        atomic_text(self.directory / "bridge-status.json", json.dumps(status, indent=2) + "\n")

    def tick(self, now):
        own = read_local(self.directory / "local.tsv", self.local_state)
        # Pause intent must keep flowing even while gameplay snapshots stop.
        own = dict(own, pause=read_pause(self.directory/'pause-local.tsv', self.local_state['pause']),
                   cinematic=cinematics.read(self.directory/'cinematic-local.tsv'))
        own_coop=cooperative.read(self.directory/"coop-local.tsv")
        self.local_state = own
        scene=(own['map'],own['chapter'])
        if own['map']!='-' and self.object_scene!=scene:
            self.objects=Authority();self.local_objects=[];self.object_scene=scene
        objects=read_objects(self.directory/'objects-local.tsv')
        if objects is not None: self.local_objects=objects
        self.sequence += 1
        if self.role == "host":
            # Only the host can activate opening synchronization. Client bits
            # are requests; repeated full state makes lost UDP packets harmless.
            if own["story"][0]:
                self.shared_story[0] = 1
                self.shared_story[1] |= own["story"][1]
                self.shared_story[2] |= own["story"][2]
                if self.connected(now) and self.peer_state["story"][0] and own["map"] == self.peer_state["map"]:
                    self.shared_story[1] |= self.peer_state["story"][1]
                    self.shared_story[2] |= self.peer_state["story"][2]
            own = dict(own, story=self.shared_story[:])
            if self.peer and now - self.last_rx >= TIMEOUT:
                LOG.info("Player 2 timed out; slot available")
                self.peer = None
                self.peer_state = idle_state()
                self.objects.disconnect(2)
            self.pause_votes = [own['pause'], self.peer_state['pause'] if self.connected(now) else 0]
            self.skips.merge(own['cinematic'],now,1)
            if self.connected(now): self.skips.merge(self.peer_state['cinematic'],now,2)
            self.shared_skips=self.skips.snapshot(now)
            if self.peer:
                self.send(self.peer[0], kind="roster", nonce=self.peer[1], session=self.session,
                          seq=self.sequence, players={"1": own, "2": self.peer_state},skips=self.shared_skips,coop=own_coop)
        else:
            if self.session and now - self.last_rx >= TIMEOUT:
                LOG.info("Host timed out; reconnecting")
                self.session = None
                self.peer_seq = -1
                self.nonce = secrets.token_hex(16)
                self.peer_state = idle_state()
                self.objects=Authority()
            if self.session:
                self.send(self.target, kind="state", nonce=self.nonce, session=self.session, seq=self.sequence, state=own,coop=own_coop)
            elif self.sequence % 20 == 1:
                self.send(self.target, kind="hello", nonce=self.nonce)
        if own['map']!='-':
            if self.role=='host':
                for o in self.local_objects: self.objects.merge(1,o)
                for e in read_effects(self.directory/'objects-local.tsv'): self.objects.effect(1,e)
            if self.connected(now):
                if self.role=='host':
                    records=list(self.objects.records.values())
                    active=[r for r in records if r['owner']]
                    # Held objects update at movement rate; free objects cycle.
                    if records:
                        self.object_cursor=(self.object_cursor+1)%len(records)
                        if records[self.object_cursor] not in active: active.append(records[self.object_cursor])
                    for r in active[:3]:
                        self.object_send(self.peer[0],'object_state',record=r,map=own['map'],chapter=own['chapter'])
                    effects=list(self.objects.effects.values())
                    if effects: self.object_send(self.peer[0],'effect_state',effect=effects[self.sequence%len(effects)],map=own['map'],chapter=own['chapter'])
                else:
                    # Local tracking is capped at eight. Send all held states
                    # and one rotating recently dropped body per transport tick.
                    active=[o for o in self.local_objects if o['held']]
                    if self.local_objects:
                        self.object_cursor=(self.object_cursor+1)%len(self.local_objects)
                        if self.local_objects[self.object_cursor] not in active: active.append(self.local_objects[self.object_cursor])
                    for o in active[:3]:
                        self.object_send(self.target,'object_request',object=o,map=own['map'],chapter=own['chapter'])
                    effects=read_effects(self.directory/'objects-local.tsv')
                    if effects: self.object_send(self.target,'effect_request',effect=effects[self.sequence%len(effects)],map=own['map'],chapter=own['chapter'])
        self.publish(own, now)

    def run(self, duration=None):
        started = time.monotonic()
        next_tick = started
        LOG.info("%s bridge ready on UDP port %s (protocol %s)", self.role, self.sock.getsockname()[1], VERSION)
        try:
            while self.running and (duration is None or time.monotonic() - started < duration):
                now = time.monotonic()
                if now >= next_tick:
                    try:
                        self.tick(now)
                    except OSError as exc:
                        LOG.warning("Transport tick: %s", exc)
                    next_tick = now + .05
                ready, _, _ = select.select([self.sock], [], [], max(0., min(.05, next_tick - time.monotonic())))
                if ready:
                    for _ in range(64):
                        try:
                            data, address = self.sock.recvfrom(MAX_PACKET + 1)
                        except BlockingIOError:
                            break
                        except ConnectionResetError:
                            break  # Windows UDP ICMP response when peer is not up yet.
                        try:
                            self.receive(data, address, time.monotonic())
                        except (ValueError, TypeError, KeyError, OverflowError, RecursionError, UnicodeError):
                            self.rejected += 1
        finally:
            self.peer = self.session = None
            self.publish(idle_state(), time.monotonic())
            self.sock.close()
            LOG.info("Bridge stopped")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("role", choices=["host", "join"])
    p.add_argument("--game-root", type=Path, default=Path(__file__).resolve().parents[2])
    p.add_argument("--host", default="127.0.0.1", help="Host IPv4 address, LAN or private VPN")
    p.add_argument("--port", type=int, default=7777)
    p.add_argument("--bind", default="0.0.0.0")
    p.add_argument("--key-file", type=Path, required=True, help="Text file containing the same session key on both PCs")
    p.add_argument("--duration", type=float, help=argparse.SUPPRESS)
    args = p.parse_args()
    if not 1024 <= args.port <= 65535:
        p.error("port must be between 1024 and 65535")
    key = args.key_file.read_text(encoding="utf-8").strip()
    if len(key) < 16 or len(key) > 256:
        p.error("session key must contain 16 to 256 characters")
    mod = args.game_root.resolve() / "Hk_project/Binaries/Win64/Mods/StrayCoop"
    mod.mkdir(parents=True, exist_ok=True)
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s",
                        handlers=[logging.StreamHandler(), logging.FileHandler(mod / "bridge.log", encoding="utf-8")])
    try:
        Bridge(args.role, mod, key, args.host, args.port, args.bind).run(args.duration)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
