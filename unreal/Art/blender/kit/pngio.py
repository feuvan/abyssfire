"""Small, exact PNG I/O.

* ``write_png`` is a pure numpy + zlib encoder (no colour management between our numbers and the bytes,
  adaptive per-row filters, optional palette quantisation) — used for palette atlases (exact swatch values)
  and for the review sheets that must stay under the size budget.
* ``write_png_native`` stores review renders truecolour at scale 1 (split into tiles instead of quantising or
  downscaling when over the budget) and checks every file (``assert_native``, ``png_info``).
* ``read_png`` decodes through Blender's image loader with a Non-Color colourspace (raw bytes).
"""
from __future__ import annotations

import os
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
    # atomic: parallel ships rewrite a shared family atlas (``Palette.save``) while other processes render with it —
    # a reader must see the old file or the new one, never a half-written PNG (Cycles: "Not a PNG file" → magenta)
    tmp = p.with_name(f".{p.name}.{os.getpid()}.tmp")
    tmp.write_bytes(data)
    os.replace(tmp, p)
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


class NativePreviewError(RuntimeError):
    """A native review render could not be stored truecolour at scale 1 within the budget."""


def png_info(path: str | Path) -> dict:
    """IHDR of a PNG file: ``{width, height, bitDepth, colorType, truecolor}`` (colour type 2 RGB / 6 RGBA =
    truecolour; 3 = indexed)."""
    data = Path(path).read_bytes()[:33]
    if data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        raise ValueError(f"{path}: not a PNG")
    w, h, bd, ct = struct.unpack(">IIBB", data[16:26])
    return {"width": w, "height": h, "bitDepth": bd, "colorType": ct, "truecolor": ct in (2, 6) and bd == 8}


def _separators(img: np.ndarray, axis: int) -> np.ndarray:
    """Indices along ``axis`` (0 rows, 1 columns) of uniform lines (a ``review.grid`` pad / label band)."""
    lines = img if axis == 0 else img.transpose(1, 0, 2)
    flat = lines.reshape(lines.shape[0], -1, img.shape[2])
    return np.nonzero((flat == flat[:, :1, :]).all(axis=(1, 2)))[0]


def split_tiles(img: np.ndarray, k: int, axis: int) -> list[np.ndarray]:
    """``k`` contiguous tiles of ``img`` along ``axis``, each cut snapped to the nearest uniform separator line
    (grid pads) within half a tile of the even cut, so panels of a sheet stay whole when possible."""
    n = img.shape[axis]
    seps = _separators(img, axis)
    cuts = [0]
    for i in range(1, k):
        ideal = round(n * i / k)
        if len(seps):
            j = int(seps[np.argmin(np.abs(seps - ideal))])
            if abs(j - ideal) <= n / (2 * k) and j - cuts[-1] >= n / (2 * k) and n - j >= n / (2 * k):
                ideal = j
        cuts.append(max(cuts[-1] + 1, min(ideal, n - (k - i))))
    cuts.append(n)
    return [img[a:b] if axis == 0 else img[:, a:b] for a, b in zip(cuts, cuts[1:])]


def assert_native(written: list[tuple[Path, int]], img: np.ndarray, axis: int | None = None) -> None:
    """Every file of a native review render is truecolour 8-bit and the tiles add up to the source pixels (scale
    1: no file was resampled). Raises ``NativePreviewError``."""
    h, w = img.shape[:2]
    infos = [png_info(p) for p, _ in written]
    for (p, _), inf in zip(written, infos):
        if not inf["truecolor"]:
            raise NativePreviewError(f"{p}: colour type {inf['colorType']} (not truecolour)")
        if inf["width"] > w or inf["height"] > h:
            raise NativePreviewError(f"{p}: {inf['width']}x{inf['height']} is larger than the {w}x{h} render")
    if len(infos) == 1:
        ok = (infos[0]["width"], infos[0]["height"]) == (w, h)
    elif axis == 0:
        ok = all(i["width"] == w for i in infos) and sum(i["height"] for i in infos) == h
    elif axis == 1:
        ok = all(i["height"] == h for i in infos) and sum(i["width"] for i in infos) == w
    else:                                  # panels, then rows: the pixels must add up exactly
        ok = sum(i["width"] * i["height"] for i in infos) <= w * h and all(i["height"] <= h for i in infos)
    if not ok:
        dims = ", ".join(f"{i['width']}x{i['height']}" for i in infos)
        raise NativePreviewError(f"{written[0][0]}: stored {dims} for a {w}x{h} render (not scale 1)")


def _panels(img: np.ndarray, axis: int) -> list[np.ndarray]:
    """Split ``img`` at every run of uniform separator lines along ``axis`` (a sheet's pads): the panels, each with
    the pad that follows it (so the pieces add up to the whole)."""
    n = img.shape[axis]
    seps = set(int(i) for i in _separators(img, axis))
    cuts, i = [0], 0
    while i < n:
        if i in seps and i > 0:
            j = i
            while j in seps:
                j += 1
            if j < n and j - cuts[-1] > 8:
                cuts.append(j)
            i = j
        else:
            i += 1
    cuts.append(n)
    return [img[a:b] if axis == 0 else img[:, a:b] for a, b in zip(cuts, cuts[1:]) if b > a]


def write_png_native(path: str | Path, img: np.ndarray, max_bytes: int = PREVIEW_MAX_BYTES,
                     axis: int | str = "auto", max_tiles: int = 8, annotate=None) -> list[tuple[Path, int]]:
    """Write a review render **truecolour at scale 1** (never palette-quantised, never resampled): one file when
    it fits ``max_bytes``, else the fewest contiguous tiles along ``axis`` (0 = stacked rows, 1 = side by side;
    ``"auto"`` = the longer side) whose cuts all fall on the sheet's pad lines and that each fit — ``<stem>.png``,
    ``<stem>_2.png``, …; failing that, one tile per panel, an oversized panel split into stacked strips (a panel is
    never cut side to side). Stale ``<stem>_<n>.png`` tiles of an earlier write are removed. Every written file is checked
    (``assert_native``). Raises ``NativePreviewError`` when ``max_tiles`` tiles still do not fit (cut a smaller
    native crop instead). Returns [(path, bytes)]."""
    p = Path(path)
    img = np.ascontiguousarray(img)
    ax = (0 if img.shape[0] >= img.shape[1] else 1) if axis == "auto" else int(axis)
    written: list[tuple[Path, int]] = []

    def encode(tiles):
        if annotate is not None:
            tiles = [t.copy() for t in tiles]
            for i, t in enumerate(tiles):
                annotate(t, i, len(tiles))
        blobs = []
        for t in tiles:
            tmp = p.with_name(f".{p.stem}.tmp.png")
            size = write_png(tmp, t)
            blobs.append((tmp.read_bytes(), size))
            tmp.unlink()
        return blobs

    chosen, layout = None, ax
    seps = set(int(i) for i in _separators(img, ax))
    sheet = any(8 < i < img.shape[ax] - 8 for i in seps)      # a grid of panels (a single render has no pads)
    for k in range(1, max_tiles + 1):
        tiles = [img] if k == 1 else split_tiles(img, k, ax)
        if k > 1 and sheet:                # only cuts that fall on the sheet's pads keep panels whole
            edges = np.cumsum([t.shape[ax] for t in tiles])[:-1]
            if not all(int(e) in seps or int(e) - 1 in seps for e in edges):
                continue
        blobs = encode(tiles)
        if all(s <= max_bytes for _, s in blobs):
            chosen = blobs
            break
    if chosen is None:
        # panels first (never cut a panel side-to-side), then each oversized panel in stacked strips
        tiles = []
        for panel in _panels(img, ax):
            other = 1 - ax
            for k in range(1, max_tiles + 1):
                parts = [panel] if k == 1 else split_tiles(panel, k, other)
                if all(write_png(p.with_name(f".{p.stem}.probe.png"), t) <= max_bytes * 0.97 for t in parts):
                    tiles += parts
                    break
            else:
                raise NativePreviewError(f"{p}: a {panel.shape[1]}x{panel.shape[0]} panel does not fit {max_bytes} B "
                                         f"in {max_tiles} truecolour strips — render a smaller native crop")
        probe = p.with_name(f".{p.stem}.probe.png")
        if probe.exists():
            probe.unlink()
        if len(tiles) > 9:
            raise NativePreviewError(f"{p}: needs {len(tiles)} tiles — render a smaller native crop")
        chosen, layout = encode(tiles), None
        if any(s > max_bytes for _, s in chosen):
            raise NativePreviewError(f"{p}: a labelled tile passed {max_bytes} B")
    names = [p] + [p.with_name(f"{p.stem}_{i}{p.suffix}") for i in range(2, len(chosen) + 1)]
    for name, (data, size) in zip(names, chosen):
        name.parent.mkdir(parents=True, exist_ok=True)
        name.write_bytes(data)
        written.append((name, size))
    for i in range(len(written) + 1, 10):
        stale = p.with_name(f"{p.stem}_{i}{p.suffix}")
        if stale.exists():
            stale.unlink()
    assert_native(written, img, layout if len(written) > 1 else None)
    return written


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
