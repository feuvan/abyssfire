"""CC0 recordings: decode, gapless trim, loop seam check (audio.md 7, 9.4).

* Decoding goes through libsndfile's mpg123 (soundfile) at the file's native 48 kHz. Both mpg123 and dr_mp3 honour the
  LAME / Info tag (encoder delay + 529 at the start, padding - 529 at the end): `inspect` parses the tag and
  `decode` checks that the decoded length equals frames * 1152 - delay - padding, i.e. that the trim happened.
* Files whose tag is missing / unreadable fall back to trimming digital silence (< -80 dBFS) at both ends; looping
  recordings always lose leading / trailing digital silence (e.g. 122 ms before "Battle Theme A"), otherwise the loop
  would gap at every wrap.
* Seam: if the jump between the last and the first frame exceeds the local 99th-percentile sample delta, a 10 ms
  equal-power crossfade of the tail into the head is applied.
"""

from __future__ import annotations

import hashlib
import math
from dataclasses import dataclass

import numpy as np
import soundfile as sf

SILENCE = 10.0 ** (-80.0 / 20.0)


@dataclass
class Mp3Info:
    first_frame_offset: int
    tag: str  # "Xing" / "Info" / ""
    mpeg_frames: int
    encoder: str
    enc_delay: int
    enc_padding: int


def inspect(data: bytes) -> Mp3Info:
    p = 0
    if data[:3] == b"ID3":
        size = (data[6] << 21) | (data[7] << 14) | (data[8] << 7) | data[9]
        p = 10 + size + (10 if data[5] & 0x10 else 0)
    while p + 1 < len(data) and not (data[p] == 0xFF and (data[p + 1] & 0xE0) == 0xE0):
        p += 1
    h = data[p:p + 4]
    version = (h[1] >> 3) & 3
    mode = (h[3] >> 6) & 3
    mpeg1 = version == 3
    side = (32 if mode != 3 else 17) if mpeg1 else (17 if mode != 3 else 9)
    x = p + 4 + side
    tag = data[x:x + 4]
    if tag not in (b"Xing", b"Info"):
        return Mp3Info(p, "", 0, "", 0, 0)
    flags = int.from_bytes(data[x + 4:x + 8], "big")
    o = x + 8
    frames = 0
    if flags & 1:
        frames = int.from_bytes(data[o:o + 4], "big")
        o += 4
    if flags & 2:
        o += 4
    if flags & 4:
        o += 100
    if flags & 8:
        o += 4
    lame = data[o:o + 36]
    encoder = lame[:9].decode("latin-1", errors="replace")
    delay = (lame[21] << 4) | (lame[22] >> 4)
    padding = ((lame[22] & 0x0F) << 8) | lame[23]
    if not encoder[:4].isalnum():
        return Mp3Info(p, tag.decode(), frames, "", 0, 0)
    return Mp3Info(p, tag.decode(), frames, encoder, delay, padding)


@dataclass
class Decoded:
    audio: np.ndarray  # (2, n) float64
    sr: int
    info: Mp3Info
    sha256: str
    gapless_ok: bool
    trimmed_head: int
    trimmed_tail: int
    seam_jump: float
    seam_p99: float
    seam_crossfaded: bool


def decode(path: str, loop: bool, sr_expected: int = 48000) -> Decoded:
    raw = open(path, "rb").read()
    info = inspect(raw)
    x, sr = sf.read(path, dtype="float64", always_2d=True)
    if sr != sr_expected:
        raise ValueError(f"{path}: {sr} Hz, expected {sr_expected} (resampling is not part of the pipeline)")
    x = x.T
    if x.shape[0] == 1:
        x = np.repeat(x, 2, axis=0)
    gapless_ok = bool(info.mpeg_frames) and x.shape[1] == info.mpeg_frames * 1152 - info.enc_delay - info.enc_padding
    head = tail = 0
    if loop or not gapless_ok:
        a = np.max(np.abs(x), axis=0)
        idx = np.nonzero(a > SILENCE)[0]
        if idx.size:
            head = int(idx[0])
            tail = int(x.shape[1] - 1 - idx[-1])
            x = x[:, head:x.shape[1] - tail]
    elif not loop:
        # One-shots keep their head; trailing digital silence is dropped.
        a = np.max(np.abs(x), axis=0)
        idx = np.nonzero(a > SILENCE)[0]
        if idx.size:
            tail = int(x.shape[1] - 1 - idx[-1])
            x = x[:, :x.shape[1] - tail]
    jump, p99, faded = 0.0, 0.0, False
    if loop:
        x, jump, p99, faded = fix_seam(x, sr)
    return Decoded(x, sr, info, hashlib.sha256(raw).hexdigest(), gapless_ok, head, tail, jump, p99, faded)


def seam_stats(x: np.ndarray) -> tuple[float, float]:
    d = np.abs(np.diff(x, axis=1)).max(axis=0)
    jump = float(np.max(np.abs(x[:, 0] - x[:, -1])))
    return jump, float(np.percentile(d, 99))


def fix_seam(x: np.ndarray, sr: int, fade_sec: float = 0.010):
    jump, p99 = seam_stats(x)
    if jump <= p99:
        return x, jump, p99, False
    n = int(round(fade_sec * sr))
    t = (np.arange(n) + 0.5) / n
    fade_in = np.sin(t * math.pi / 2)
    fade_out = np.cos(t * math.pi / 2)
    y = x[:, :-n].copy()
    # The last n frames cross-fade into the first n frames (equal power); the loop shortens by n frames.
    y[:, :n] = x[:, :n] * fade_in + x[:, -n:] * fade_out
    j2, p2 = seam_stats(y)
    return y, j2, p2, True
