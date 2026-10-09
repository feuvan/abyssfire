"""Palette atlas system (spec art-inventory-ch1.md §1.2, §1.7).

Every material *region* of every generated mesh stores only **base colour + (shadowAmt, lightAmt)** plus an
emissive strength and an outline-colour mix. Regions are packed into one atlas per family:

* ``T_AF_Palette_<Family>_BC.png`` — 256², 16×16 swatches of 16 px, **sRGB** base colour.
* ``T_AF_Palette_<Family>_P.png``  — same layout, **linear** data: R = shadowAmt, G = lightAmt,
  B = emissive strength (0..1), A = outline-colour mix (0 = ink ``#120C18``, 1 = the region's ``line`` tone).

Meshes carry a face attribute ``af_swatch`` (int) while they are modelled; ``Palette.bake_uvs`` collapses every
face's UVs onto the centre of its swatch, so one material (``MI_AF_Toon_<Family>``) draws the whole asset.
Unused swatches are magenta so a wrong UV is obvious. Sample with **nearest filtering and no mips**
(UE: TextureGroup UI / Filter Nearest / NoMipmaps; BC sRGB on, P sRGB off).

The swatch registry ``Art/blender/palettes/<Family>.json`` is committed and append-only (stable indices
across runs); re-registering a name with new values moves that name to a new swatch and frees the old one
once no name uses it.
"""
from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from . import color, paths, pngio

GRID = 16          # swatches per row/column
SWATCH_PX = 16     # pixels per swatch edge
ATLAS_PX = GRID * SWATCH_PX
UNUSED_RGB = (255, 0, 255)

FAMILIES = ("Heroes", "Monsters_Plains", "NPC", "Props_Plains", "Foliage_Plains", "FX", "Test")


def _q8(v: float) -> int:
    return int(round(max(0.0, min(1.0, float(v))) * 255.0))


@dataclass(frozen=True)
class Region:
    """One material region: web ``tone(base, {shadow, light})`` + emissive + outline mix."""

    hex: str
    s: float = color.DEFAULT_SHADOW   # shadowAmt
    l: float = color.DEFAULT_LIGHT    # lightAmt
    e: float = 0.0                    # emissive strength 0..1 (unshaded, blooms in UE)
    o: float = 1.0                    # outline colour: 0 ink #120C18 … 1 this region's line tone (R3)

    def key(self) -> tuple:
        return (color.rgb_to_hex(color.hex_to_rgb(self.hex)), _q8(self.s), _q8(self.l), _q8(self.e), _q8(self.o))

    def tone(self) -> dict:
        return color.tone(self.hex, self.s, self.l)

    def line_region(self, s: float | None = None) -> "Region":
        """A region whose base is this region's ``line`` tone — for modelled seams / crease strips."""
        t = self.tone()
        return Region(color.rgb_to_hex(t["line"]), s if s is not None else 0.25, 0.15, 0.0, 1.0)

    def shade_region(self, s: float = 0.25, l: float = 0.15) -> "Region":
        """A region whose base is this region's ``shade`` tone — painted seams / mail rows (web strokes in the
        material's shade colour) on flat strips that follow the surface, so they band with the plate under them."""
        return Region(color.rgb_to_hex(self.tone()["shade"]), s, l, 0.0, self.o)

    def light_region(self, s: float | None = None, l: float = 0.20) -> "Region":
        """A region whose base is this region's ``light`` tone — painted ridges / highlight lines."""
        return Region(color.rgb_to_hex(self.tone()["light"]), self.s if s is None else s, l, 0.0, self.o)


class Palette:
    def __init__(self, family: str):
        if family not in FAMILIES:
            raise ValueError(f"unknown palette family {family!r}; add it to palette.FAMILIES")
        self.family = family
        self.swatches: list[dict | None] = []
        self._by_name: dict[str, int] = {}
        self._dirty = False
        self._touched: set[int] = set()      # swatch indices this instance created / changed (merge-on-save)
        self._released: set[str] = set()     # names this instance moved off their loaded swatch
        self._load()

    # ── registry ────────────────────────────────────────────────────────
    @property
    def registry_path(self) -> Path:
        return paths.palette_root() / f"{self.family}.json"

    def _load(self) -> None:
        p = self.registry_path
        if not p.exists():
            return
        data = json.loads(p.read_text())
        self.swatches = [None] * len(data["swatches"])
        for sw in data["swatches"]:
            if sw.get("free"):
                continue
            self.swatches[sw["i"]] = sw
            for n in sw["names"]:
                self._by_name[n] = sw["i"]

    def _read_disk(self) -> list[dict | None]:
        """The registry as it is on disk now (another generator may have registered swatches since we loaded)."""
        p = self.registry_path
        if not p.exists():
            return []
        try:
            data = json.loads(p.read_text())
        except (OSError, ValueError):
            return []
        out: list[dict | None] = [None] * len(data["swatches"])
        for sw in data["swatches"]:
            if not sw.get("free"):
                out[sw["i"]] = sw
        return out

    def _adopt(self, disk: list[dict | None]) -> None:
        """Take over the disk's swatches at every index this instance has not touched (several generators share
        a family: heroes, monsters …); a name we hold elsewhere stays ours."""
        n = max(len(disk), len(self.swatches))
        self.swatches += [None] * (n - len(self.swatches))
        for i in range(n):
            if i in self._touched:
                continue
            d = disk[i] if i < len(disk) else None
            if d is not None:
                d = dict(d, names=[nm for nm in d["names"]
                                   if nm not in self._released and self._by_name.get(nm, i) == i])
                if not d["names"]:
                    d = None
            old = self.swatches[i]
            if old is not None:
                for nm in old["names"]:
                    if self._by_name.get(nm) == i:
                        del self._by_name[nm]
            self.swatches[i] = d
            if d is not None:
                for nm in d["names"]:
                    self._by_name[nm] = i
        while self.swatches and self.swatches[-1] is None:
            self.swatches.pop()

    def add(self, name: str, region: Region) -> int:
        """Register a named region; returns its swatch index (identical values share a swatch)."""
        key = region.key()
        cur = self._by_name.get(name)
        if cur is not None and self._swatch_key(self.swatches[cur]) == key:
            return cur
        if cur is not None:  # value changed: detach the name from its old swatch
            sw = self.swatches[cur]
            keep = set(sw["names"])
            disk = self._read_disk()                 # names other generators joined to it since we loaded
            d = disk[cur] if cur < len(disk) else None
            if d is not None and self._swatch_key(d) == self._swatch_key(sw):
                for nm in d["names"]:
                    if nm not in self._by_name and nm not in self._released:
                        keep.add(nm)
                        self._by_name[nm] = cur
            keep.discard(name)
            sw["names"] = sorted(keep)
            if not sw["names"]:
                self.swatches[cur] = None
            del self._by_name[name]
            self._released.add(name)
        for sw in self.swatches:
            if sw is not None and self._swatch_key(sw) == key:
                sw["names"] = sorted(set(sw["names"]) | {name})
                self._by_name[name] = sw["i"]
                self._dirty = True
                self._touched.add(sw["i"])
                return sw["i"]
        # a new swatch: allocated under the registry lock against what is on disk now (other generators of the
        # family may have registered swatches since we loaded) and reserved on disk at once, so two generators
        # running together never take the same index; ``save`` merges the rest
        with self._lock():
            self._adopt(self._read_disk())
            for sw in self.swatches:
                if sw is not None and self._swatch_key(sw) == key:
                    sw["names"] = sorted(set(sw["names"]) | {name})
                    self._by_name[name] = sw["i"]
                    self._dirty = True
                    self._touched.add(sw["i"])
                    self._write_registry()
                    return sw["i"]
            idx = next((i for i, sw in enumerate(self.swatches) if sw is None), len(self.swatches))
            if idx >= GRID * GRID:
                raise RuntimeError(f"palette {self.family} is full ({GRID * GRID} swatches)")
            sw = {"i": idx, "hex": key[0], "s": key[1] / 255.0, "l": key[2] / 255.0, "e": key[3] / 255.0,
                  "o": key[4] / 255.0, "names": [name]}
            if idx == len(self.swatches):
                self.swatches.append(sw)
            else:
                self.swatches[idx] = sw
            self._by_name[name] = idx
            self._dirty = True
            self._touched.add(idx)
            self._write_registry()
        return idx

    def _lock(self):
        """Exclusive lock on ``<family>.json.lock`` (context manager)."""
        import contextlib
        import fcntl

        @contextlib.contextmanager
        def cm():
            rp = self.registry_path
            rp.parent.mkdir(parents=True, exist_ok=True)
            with open(rp.with_name(rp.name + ".lock"), "w") as lk:
                fcntl.flock(lk, fcntl.LOCK_EX)
                try:
                    yield
                finally:
                    fcntl.flock(lk, fcntl.LOCK_UN)
        return cm()

    def _write_registry(self) -> None:
        import os
        rp = self.registry_path
        out = []
        for i, sw in enumerate(self.swatches):
            out.append(sw if sw is not None else {"i": i, "free": True})
        tmp = rp.with_name(f".{rp.name}.{os.getpid()}.tmp")
        tmp.write_text(json.dumps({"family": self.family, "atlasPx": ATLAS_PX, "swatchPx": SWATCH_PX,
                                   "grid": GRID, "swatches": out}, indent=1, sort_keys=True) + "\n")
        os.replace(tmp, rp)

    @staticmethod
    def _swatch_key(sw: dict) -> tuple:
        return (sw["hex"], _q8(sw["s"]), _q8(sw["l"]), _q8(sw["e"]), _q8(sw["o"]))

    def index(self, name: str) -> int:
        return self._by_name[name]

    def region(self, name: str) -> Region:
        sw = self.swatches[self._by_name[name]]
        return Region(sw["hex"], sw["s"], sw["l"], sw["e"], sw["o"])

    def names(self) -> list[str]:
        return sorted(self._by_name)

    # ── UVs ─────────────────────────────────────────────────────────────
    @staticmethod
    def uv(index: int) -> tuple[float, float]:
        """UV of a swatch centre (Blender UV: v = 0 at the bottom; UE's importer flips V consistently)."""
        col, row = index % GRID, index // GRID
        return ((col + 0.5) / GRID, 1.0 - (row + 0.5) / GRID)

    def bake_uvs(self, mesh, uv_name: str = "UVMap") -> None:
        """Collapse every face's UVs onto the centre of its ``af_swatch`` swatch."""
        attr = mesh.attributes.get("af_swatch")
        if attr is None:
            raise RuntimeError(f"mesh {mesh.name} has no af_swatch face attribute")
        nf = len(mesh.polygons)
        sw = np.zeros(nf, np.int32)
        attr.data.foreach_get("value", sw)
        lt = np.zeros(nf, np.int32)
        mesh.polygons.foreach_get("loop_total", lt)
        per_loop = np.repeat(sw, lt)
        u = (per_loop % GRID + 0.5) / GRID
        v = 1.0 - (per_loop // GRID + 0.5) / GRID
        uvl = mesh.uv_layers.get(uv_name) or mesh.uv_layers.new(name=uv_name)
        mesh.uv_layers.active = uvl
        uvl.data.foreach_set("uv", np.stack([u, v], axis=1).astype(np.float32).ravel())

    # ── atlas textures ──────────────────────────────────────────────────
    @property
    def material_name(self) -> str:
        return f"MI_AF_Toon_{self.family}"

    def texture_names(self) -> tuple[str, str]:
        return (f"T_AF_Palette_{self.family}_BC", f"T_AF_Palette_{self.family}_P")

    def texture_paths(self) -> tuple[Path, Path]:
        bc, p = self.texture_names()
        return paths.texture_dir() / f"{bc}.png", paths.texture_dir() / f"{p}.png"

    def atlas_arrays(self) -> tuple[np.ndarray, np.ndarray]:
        bc = np.zeros((ATLAS_PX, ATLAS_PX, 3), np.uint8)
        bc[:, :] = UNUSED_RGB
        pp = np.zeros((ATLAS_PX, ATLAS_PX, 4), np.uint8)
        pp[:, :] = (_q8(color.DEFAULT_SHADOW), _q8(color.DEFAULT_LIGHT), 0, 255)
        for sw in self.swatches:
            if sw is None:
                continue
            i = sw["i"]
            col, row = i % GRID, i // GRID
            ys, xs = slice(row * SWATCH_PX, (row + 1) * SWATCH_PX), slice(col * SWATCH_PX, (col + 1) * SWATCH_PX)
            bc[ys, xs] = [int(c) for c in color.hex_to_rgb(sw["hex"])]
            pp[ys, xs] = (_q8(sw["s"]), _q8(sw["l"]), _q8(sw["e"]), _q8(sw["o"]))
        return bc, pp

    def save(self) -> tuple[Path, Path]:
        """Write the registry JSON and both atlas PNGs.

        **Merges** (a family is shared by several generators, which may run at the same time): under an exclusive
        lock (``<family>.json.lock``) the registry is re-read and every swatch this instance did not create or
        change is taken from disk, so another generator's swatches are never dropped from the JSON or the atlas.
        Single-writer results are unchanged."""
        with self._lock():
            disk = self._read_disk()
            for i in sorted(self._touched):        # same key on disk at a touched index: keep both name lists
                mine = self.swatches[i] if i < len(self.swatches) else None
                d = disk[i] if i < len(disk) else None
                if mine is not None and d is not None and self._swatch_key(d) == self._swatch_key(mine):
                    extra = [nm for nm in d["names"] if nm not in self._by_name]
                    if extra:
                        mine["names"] = sorted(set(mine["names"]) | set(extra))
                        for nm in extra:
                            self._by_name[nm] = i
            self._adopt(disk)
            self._write_registry()
            bc, pp = self.atlas_arrays()
            pbc, ppp = self.texture_paths()
            pngio.write_png(pbc, bc)
            pngio.write_png(ppp, pp)
        self._dirty = False
        return pbc, ppp

    def manifest_entry(self) -> dict:
        pbc, ppp = self.texture_paths()
        return {
            "material": self.material_name,
            "parent": "M_AF_Toon",
            "baseColor": {"name": self.texture_names()[0], "file": paths.rel_to_export(pbc), "srgb": True},
            "params": {"name": self.texture_names()[1], "file": paths.rel_to_export(ppp), "srgb": False,
                        "channels": {"r": "shadowAmt", "g": "lightAmt", "b": "emissive", "a": "outlineMix"}},
            "atlasPx": ATLAS_PX, "swatchPx": SWATCH_PX, "filter": "nearest", "mips": False,
        }
