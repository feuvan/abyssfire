"""Exact clip timing in a binary FBX written by Blender's exporter (run by ``export.export_animation``).

Blender 5.0's ``io_scene_fbx`` ignores the scene range for the file timeline and rounds the take down:

* ``GlobalSettings`` ``TimeSpanStart`` / ``TimeSpanStop`` are hard-coded to 0 .. 1 s, so every clip that is not exactly
  60 frames long carries a timeline that is not its own (an importer that takes the source timeline — Interchange
  ``animation_range = Timeline``, the legacy importer's "Exported Time" — reads a 1 s clip);
* the take (``AnimationStack`` ``LocalStart/Stop``, ``ReferenceStart/Stop``; ``Takes/Take`` ``LocalTime`` /
  ``ReferenceTime``) and every ``AnimationCurve`` ``KeyTime`` are ``int(frame / fps * KTIME)``: truncated, so some frames land one tick short (frame 42 at 60 fps = 41.9999999987
  frames) and a key count derived from the range (``range × rate + 1``) can disagree with the baked keys (the
  Interchange ensure ``Transforms.Num() == BakeKeyCountForAnimationPayload``).

``set_clip_range(path, start, end, fps)`` rewrites the file in place: timeline and take = ``[start, end] / fps`` in
exact FBX ticks, every key time snapped to its whole frame. Everything else is copied byte for byte (node tree
re-serialised with the exporter's own layout rules; ``KeyTime`` arrays re-encoded the way the exporter encodes them).
"""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

KTIME = 46186158000                     # FBX 7.x ticks per second

_HEAD = b"Kaydara FBX Binary  \x00\x1a\x00"
_SCALAR = {b"B": 1, b"C": 1, b"Z": 1, b"Y": 2, b"I": 4, b"L": 8, b"F": 4, b"D": 8}
_ARRAYS = b"ilfdbc"


class _Node:
    __slots__ = ("name", "props", "children", "sentinel")

    def __init__(self, name: bytes, props: list, children: list, sentinel: bool):
        self.name, self.props, self.children, self.sentinel = name, props, children, sentinel

    def child(self, name: bytes):
        return next((c for c in self.children if c.name == name), None)


def _read_prop(data: bytes, at: int) -> tuple[tuple[bytes, bytes], int]:
    t = data[at:at + 1]
    at += 1
    if t in _SCALAR:
        n = _SCALAR[t]
    elif t in (b"S", b"R"):
        n = 4 + struct.unpack_from("<I", data, at)[0]
    elif t in _ARRAYS:
        n = 12 + struct.unpack_from("<I", data, at + 8)[0]
    else:
        raise ValueError(f"unknown FBX property type {t!r} at {at - 1}")
    return (t, data[at:at + n]), at + n


def _read_node(data: bytes, at: int, meta: str, null: int) -> tuple[_Node | None, int]:
    msize = struct.calcsize(meta)
    end, nprops, _plen = struct.unpack_from(meta, data, at)
    if end == 0:
        return None, at + null
    nlen = data[at + msize]
    at += msize + 1
    name = data[at:at + nlen]
    at += nlen
    props = []
    for _ in range(nprops):
        p, at = _read_prop(data, at)
        props.append(p)
    children, sentinel = [], False
    while at < end:
        if at + null == end and data[at:end] == b"\0" * null:
            sentinel = True
            at = end
            break
        c, at = _read_node(data, at, meta, null)
        children.append(c)
    if at != end:
        raise ValueError(f"FBX node {name!r}: scope ends at {at}, expected {end}")
    return _Node(name, props, children, sentinel), end


def _size(n: _Node, msize: int, null: int) -> int:
    return (msize + 1 + len(n.name) + sum(1 + len(raw) for _, raw in n.props)
            + sum(_size(c, msize, null) for c in n.children) + (null if n.sentinel else 0))


def _write_node(out: bytearray, n: _Node, meta: str, null: int) -> None:
    msize = struct.calcsize(meta)
    end = len(out) + _size(n, msize, null)
    out += struct.pack(meta, end, len(n.props), sum(1 + len(raw) for _, raw in n.props))
    out += bytes((len(n.name),)) + n.name
    for t, raw in n.props:
        out += t + raw
    for c in n.children:
        _write_node(out, c, meta, null)
    if n.sentinel:
        out += b"\0" * null
    assert len(out) == end


def read(path: Path) -> tuple[int, list[_Node], bytes]:
    """(version, top-level nodes, footer bytes after the top-level null record)."""
    data = Path(path).read_bytes()
    if not data.startswith(_HEAD):
        raise ValueError(f"{path}: not a binary FBX")
    version = struct.unpack_from("<I", data, len(_HEAD))[0]
    meta, null = ("<3Q", 25) if version >= 7500 else ("<3I", 13)
    at, top = len(_HEAD) + 4, []
    while True:
        n, at = _read_node(data, at, meta, null)
        if n is None:
            break
        top.append(n)
    return version, top, data[at:]


def write(path: Path, version: int, top: list[_Node], footer: bytes) -> None:
    """Serialise like ``io_scene_fbx.encode_bin.write`` (footer padding recomputed for the new length)."""
    meta, null = ("<3Q", 25) if version >= 7500 else ("<3I", 13)
    out = bytearray(_HEAD + struct.pack("<I", version))
    for n in top:
        _write_node(out, n, meta, null)
    out += b"\0" * null
    out += footer[:20]                                   # footer id + 4 zero bytes
    pad = ((len(out) + 15) & ~15) - len(out)
    out += b"\0" * (pad or 16)
    out += footer[-140:]                                 # version + 120 zero bytes + magic
    tmp = Path(path).with_suffix(".fbx.tmp")
    tmp.write_bytes(bytes(out))
    tmp.replace(path)


def _p70_int64(node: _Node | None, names: dict[bytes, int]) -> dict[str, tuple[int, int]]:
    """Set the int64 value of the ``P`` records ``names`` under ``node/Properties70`` → {name: (old, new)}."""
    done = {}
    p70 = node.child(b"Properties70") if node is not None else None
    for p in (p70.children if p70 is not None else []):
        if p.name != b"P" or len(p.props) < 5 or p.props[0][0] != b"S":
            continue
        key = p.props[0][1][4:]
        if key in names and p.props[4][0] == b"L":
            old = struct.unpack("<q", p.props[4][1])[0]
            p.props[4] = (b"L", struct.pack("<q", names[key]))
            done[key.decode()] = (old, names[key])
    return done


def _take_times(takes: _Node | None, t0: int, t1: int) -> dict[str, tuple[tuple[int, ...], tuple[int, int]]]:
    """Set ``Takes/Take/LocalTime`` and ``ReferenceTime`` (two int64: start, stop) → {"Take.<record>": (old, new)}."""
    done = {}
    for take in (takes.children if takes is not None else []):
        if take.name != b"Take":
            continue
        for rec in take.children:
            if rec.name in (b"LocalTime", b"ReferenceTime") and len(rec.props) == 2 \
                    and all(t == b"L" for t, _ in rec.props):
                old = tuple(struct.unpack("<q", raw)[0] for _, raw in rec.props)
                rec.props = [(b"L", struct.pack("<q", t0)), (b"L", struct.pack("<q", t1))]
                done[f"Take.{rec.name.decode()}"] = (old, (t0, t1))
    return done


def _int64_array(raw: bytes) -> list[int]:
    length, enc, clen = struct.unpack_from("<3I", raw)
    body = raw[12:12 + clen]
    return list(struct.unpack(f"<{length}q", zlib.decompress(body) if enc == 1 else body))


def _encode_int64_array(vals: list[int]) -> bytes:
    body = struct.pack(f"<{len(vals)}q", *vals)
    if len(body) <= 128:                                 # the exporter's rule (encode_bin._add_array_helper)
        return struct.pack("<3I", len(vals), 0, len(body)) + body
    z = zlib.compress(body, 1)
    return struct.pack("<3I", len(vals), 1, len(z)) + z


def set_clip_range(path: Path, frame_start: int, frame_end: int, fps: float) -> dict:
    """Timeline + take of ``path`` = ``[frame_start, frame_end] / fps`` exactly, key times on whole frames.
    Returns what changed: ``{"timeline": {...}, "take": {...}, "keysSnapped": n, "keyRange": (lo, hi)}``."""
    tick = KTIME / fps
    t0, t1 = round(frame_start * tick), round(frame_end * tick)
    version, top, footer = read(path)
    by = {n.name: n for n in top}
    res = {"timeline": _p70_int64(by.get(b"GlobalSettings"), {b"TimeSpanStart": t0, b"TimeSpanStop": t1}),
           "take": _take_times(by.get(b"Takes"), t0, t1), "keysSnapped": 0, "keyRange": None}
    objects = by.get(b"Objects")
    lo, hi = None, None
    for n in (objects.children if objects is not None else []):
        if n.name == b"AnimationStack":
            res["take"].update(_p70_int64(n, {b"LocalStart": t0, b"LocalStop": t1,
                                              b"ReferenceStart": t0, b"ReferenceStop": t1}))
        elif n.name == b"AnimationCurve":
            kt = n.child(b"KeyTime")
            if kt is None or not kt.props or kt.props[0][0] != b"l":
                continue
            old = _int64_array(kt.props[0][1])
            new = [round(round(v / tick) * tick) for v in old]
            if new != old:
                res["keysSnapped"] += sum(a != b for a, b in zip(old, new))
                kt.props[0] = (b"l", _encode_int64_array(new))
            if new:
                lo = min(new[0], lo) if lo is not None else new[0]
                hi = max(new[-1], hi) if hi is not None else new[-1]
    if lo is not None:
        res["keyRange"] = (lo / tick, hi / tick)
    write(path, version, top, footer)
    return res
