"""Small, exact PNG I/O.

* ``write_png`` is a pure numpy + zlib encoder (no colour management between our numbers and the bytes,
  adaptive per-row filters, optional palette quantisation) — used for palette atlases (exact swatch values)
  and for the review sheets that must stay under the size budget.
* ``read_png`` decodes through Blender's image loader with a Non-Color colourspace (raw bytes).
"""
from __future__ import annotations

import struct
import zlib
from pathlib import Path

import numpy as np

PREVIEW_MAX_BYTES = 200 * 1024


def _chunk(tag: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def _filter_rows(img: np.ndarray, bpp: int) -> bytes:
    """Adaptive filtering (min sum of abs) per row; all candidates computed vectorised."""
    h = img.shape[0]
    rows = img.reshape(h, -1).astype(np.int16)
    prev = np.vstack([np.zeros((1, rows.shape[1]), np.int16), rows[:-1]])
    left = np.hstack([np.zeros((h, bpp), np.int16), rows[:, :-bpp]])
    upleft = np.hstack([np.zeros((h, bpp), np.int16), prev[:, :-bpp]])
    p = left + prev - upleft
    pa, pb, pc = np.abs(p - left), np.abs(p - prev), np.abs(p - upleft)
    paeth = np.where((pa <= pb) & (pa <= pc), left, np.where(pb <= pc, prev, upleft))
    cands = [
        rows,
        rows - left,
        rows - prev,
        rows - ((left + prev) >> 1),
        rows - paeth,
    ]
    cands = [(c & 0xFF).astype(np.uint8) for c in cands]
    # Heuristic: signed magnitude sum.
    scores = np.stack([np.abs(c.astype(np.int8).astype(np.int16)).sum(axis=1) for c in cands], axis=1)
    best = scores.argmin(axis=1)
    out = bytearray()
    for y in range(h):
        out.append(int(best[y]))
        out += cands[best[y]][y].tobytes()
    return bytes(out)


def quantize(img: np.ndarray, max_colors: int = 256) -> tuple[np.ndarray, np.ndarray]:
    """Map an RGB(A) uint8 image onto <= max_colors colours (most frequent colours, nearest match).

    Toon renders are dominated by a few flat band colours, so frequency-picked palettes keep them exact and
    only anti-aliased edge pixels move to their nearest neighbour.
    Returns (indices HxW uint8, palette Nx{3,4} uint8).
    """
    h, w, c = img.shape
    flat = img.reshape(-1, c)
    keys = np.zeros(flat.shape[0], np.uint64)
    for i in range(c):
        keys = (keys << np.uint64(8)) | flat[:, i].astype(np.uint64)
    uniq, inverse, counts = np.unique(keys, return_inverse=True, return_counts=True)
    if len(uniq) <= max_colors:
        pal = np.zeros((len(uniq), c), np.uint8)
        for i in range(c):
            pal[:, i] = ((uniq >> np.uint64(8 * (c - 1 - i))) & np.uint64(0xFF)).astype(np.uint8)
        return inverse.reshape(h, w).astype(np.uint8), pal
    order = np.argsort(-counts, kind="stable")[:max_colors]
    pal = np.zeros((len(order), c), np.uint8)
    for i in range(c):
        pal[:, i] = ((uniq[order] >> np.uint64(8 * (c - 1 - i))) & np.uint64(0xFF)).astype(np.uint8)
    # nearest palette colour for every unique colour (chunked to bound memory)
    ucol = np.zeros((len(uniq), c), np.int32)
    for i in range(c):
        ucol[:, i] = ((uniq >> np.uint64(8 * (c - 1 - i))) & np.uint64(0xFF)).astype(np.int32)
    palf = pal.astype(np.int32)
    nearest = np.empty(len(uniq), np.int64)
    step = 4096
    for s in range(0, len(uniq), step):
        d = ((ucol[s:s + step, None, :] - palf[None, :, :]) ** 2).sum(axis=2)
        nearest[s:s + step] = d.argmin(axis=1)
    idx = nearest[inverse].reshape(h, w).astype(np.uint8)
    return idx, pal


def write_png(path: str | Path, img: np.ndarray, *, palette: bool = False, level: int = 9) -> int:
    """Write an HxWx3/4 uint8 (top row first) PNG. Returns the file size in bytes."""
    img = np.ascontiguousarray(img)
    if img.dtype != np.uint8:
        raise TypeError("write_png expects uint8")
    if img.ndim == 2:
        img = img[:, :, None].repeat(3, axis=2)
    h, w, c = img.shape
    if c not in (3, 4):
        raise ValueError("expected RGB or RGBA")
    chunks = [b"\x89PNG\r\n\x1a\n"]
    if palette:
        idx, pal = quantize(img)
        chunks.append(_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0)))
        chunks.append(_chunk(b"PLTE", pal[:, :3].tobytes()))
        if c == 4:
            chunks.append(_chunk(b"tRNS", pal[:, 3].tobytes()))
        raw = _filter_rows(idx[:, :, None], 1)
    else:
        color_type = 2 if c == 3 else 6
        chunks.append(_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, color_type, 0, 0, 0)))
        raw = _filter_rows(img, c)
    chunks.append(_chunk(b"IDAT", zlib.compress(raw, level)))
    chunks.append(_chunk(b"IEND", b""))
    data = b"".join(chunks)
    p = Path(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(data)
    return len(data)


def write_png_budget(path: str | Path, img: np.ndarray, max_bytes: int = PREVIEW_MAX_BYTES) -> int:
    """Write a review PNG under ``max_bytes``: truecolour → 256-colour palette → downscale steps."""
    size = write_png(path, img)
    if size <= max_bytes:
        return size
    size = write_png(path, img, palette=True)
    cur = img
    while size > max_bytes and min(cur.shape[:2]) > 64:
        cur = downscale(cur, 0.85)
        size = write_png(path, cur, palette=True)
    return size


def downscale(img: np.ndarray, factor: float) -> np.ndarray:
    """Area-ish downscale (box filter on a resampled grid)."""
    h, w = img.shape[:2]
    nh, nw = max(1, int(h * factor)), max(1, int(w * factor))
    ys = (np.arange(nh + 1) * h / nh).astype(int)
    xs = (np.arange(nw + 1) * w / nw).astype(int)
    f = img.astype(np.float32)
    # integral image for box averaging
    ii = np.zeros((h + 1, w + 1, img.shape[2]), np.float64)
    ii[1:, 1:] = f.cumsum(0).cumsum(1)
    y0, y1 = ys[:-1], np.maximum(ys[1:], ys[:-1] + 1)
    x0, x1 = xs[:-1], np.maximum(xs[1:], xs[:-1] + 1)
    s = ii[y1][:, x1] - ii[y0][:, x1] - ii[y1][:, x0] + ii[y0][:, x0]
    area = ((y1 - y0)[:, None] * (x1 - x0)[None, :])[:, :, None]
    return np.clip(np.rint(s / area), 0, 255).astype(np.uint8)


def read_png(path: str | Path) -> np.ndarray:
    """Read a PNG as HxWx4 uint8 (top row first) through bpy with raw (Non-Color) values."""
    import bpy  # local import: pngio writing works without bpy

    img = bpy.data.images.load(str(path), check_existing=False)
    try:
        img.colorspace_settings.name = "Non-Color"
        w, h = img.size
        buf = np.empty(w * h * 4, np.float32)
        img.pixels.foreach_get(buf)
        arr = np.clip(np.rint(buf.reshape(h, w, 4) * 255.0), 0, 255).astype(np.uint8)
        return arr[::-1].copy()
    finally:
        bpy.data.images.remove(img)
