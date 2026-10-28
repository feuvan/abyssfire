"""Custom-node HLSL tests: lint, compile every node against tests/hlsl_emu.h, check the maths.

    python3 -m unittest discover -s unreal/Scripts/tests -v

Needs a C++17 compiler (clang++ or g++) for the compile / numeric tests; they are skipped without one.
"""
from __future__ import annotations

import dataclasses
import math
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from abyss_content import hlsl  # noqa: E402
from abyss_content.manifest import Shading, build_plan, hex_to_rgb255, linear_to_srgb1, srgb_to_linear1  # noqa: E402

CXX = shutil.which("clang++") or shutil.which("g++")
SHADING = Shading()


def manifest_shading() -> Shading:
    try:
        return build_plan(scan_textures=False).shading
    except Exception:  # noqa: BLE001 - the manifest may be absent in a stripped checkout
        return Shading()


def cpp_signature(node: hlsl.CustomNode) -> str:
    """The node as a C++ function: inputs by value (a Texture2D adds its sampler), then the additional outputs by
    reference (UE: `inout` parameters). Emulation defaults (keeping older calls compiling) apply to the trailing run of
    inputs that have one, and not at all with additional outputs (C++ defaults must be trailing)."""
    first_default = len(node.inputs)
    while first_default > 0 and node.inputs[first_default - 1].emu_default is not None:
        first_default -= 1
    params = []
    for i, inp in enumerate(node.inputs):
        if inp.ctype == "Texture2D":
            params.append(f"const Texture2D& {inp.name}, SamplerState {inp.name}Sampler")
        elif i >= first_default and not node.additional_outputs:
            params.append(f"{inp.ctype} {inp.name} = {inp.emu_default}")
        else:
            params.append(f"{inp.ctype} {inp.name}")
    params += [f"{out.ctype}& {out.name}" for out in node.additional_outputs]
    return f"static {node.output} {node.key}({', '.join(params)})"


def cpp_unit(nodes: list[hlsl.CustomNode], main: str = "int main() { return 0; }") -> str:
    parts = ['#include "hlsl_emu.h"', "#include <cstdio>", "#include <cstring>"]
    for node in nodes:
        parts.append(cpp_signature(node) + "\n{\n" + node.code + "}\n")
    parts.append(main)
    return "\n".join(parts)


def compile_and_run(source: str, run: bool) -> str:
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / "nodes.cpp"
        src.write_text(source, encoding="utf-8")
        exe = Path(tmp) / "nodes"
        cmd = [CXX, "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror=return-type", "-Werror=uninitialized",
               "-Wno-unused-variable", "-Wno-unused-parameter", "-Wno-unused-but-set-variable",
               "-Wno-gnu-anonymous-struct", "-Wno-nested-anon-types", "-Wno-unknown-warning-option",
               "-Wno-pedantic", f"-I{HERE}", str(src), "-o", str(exe)]
        if not run:
            cmd = cmd[:-2] + ["-fsyntax-only"]
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            numbered = "\n".join(f"{i + 1:5d} {line}" for i, line in enumerate(source.splitlines()))
            raise AssertionError(f"compile failed:\n{proc.stderr}\n--- source ---\n{numbered}")
        if not run:
            return ""
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True)
        return out.stdout


# ---------------------------------------------------------------------------------------------------------------------
# Python references (Art/blender/README.md 2)
# ---------------------------------------------------------------------------------------------------------------------
def smoothstep(e0: float, e1: float, x: float) -> float:
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3 - 2 * t)


def norm(v):
    n = math.sqrt(sum(x * x for x in v))
    return [x / n for x in v]


def toon_reference(sh: Shading, base_hex: str, s: float, l: float, e: float, n, v, grounding: float) -> list[float]:
    """README 2 formula -> display sRGB 0..255 (the kit's kit.shading.toon_reference, with the manifest constants)."""
    c = [x / 255.0 for x in hex_to_rgb255(base_hex)]
    cool = [x / 255.0 for x in sh.cool_shade]
    warm = [x / 255.0 for x in sh.warm_light]
    gt = [x / 255.0 for x in hex_to_rgb255(sh.ground_color)]
    rimc = [x / 255.0 for x in hex_to_rgb255(sh.rim_color)]
    N, V = norm(n), norm(v)
    L = norm(sh.key_light)
    ndl = sum(a * b for a, b in zip(N, L))
    h = ndl * 0.5 + 0.5
    hw = sh.band_width / 2
    wb = smoothstep(sh.t_shade - hw, sh.t_shade + hw, h)
    wl = smoothstep(sh.t_light - hw, sh.t_light + hw, h)
    fres = (1 - max(sum(a * b for a, b in zip(N, V)), 0.0)) ** sh.rim_power
    rim = sh.rim_alpha * smoothstep(sh.rim_band[0], sh.rim_band[1], fres) * smoothstep(0.0, sh.rim_mask_edge, ndl)
    out = []
    for i in range(3):
        shade = (c[i] + (cool[i] - c[i]) * sh.cool_shade_mix) * (1 - s)
        light = c[i] + (warm[i] - c[i]) * l
        col = shade + (c[i] - shade) * wb
        col = col + (light - col) * wl
        col = col + (gt[i] - col) * (sh.ground_alpha * grounding)
        col = col + (rimc[i] - col) * rim
        col = col + (c[i] - col) * e
        lin = srgb_to_linear1(min(max(col, 0.0), 1.0)) * (1 + e * sh.emissive_boost)
        out.append(linear_to_srgb1(min(lin, 1.0)) * 255.0)
    return out


def ink_reference(sh: Shading, base_hex: str, o: float) -> list[float]:
    c = [x / 255.0 for x in hex_to_rgb255(base_hex)]
    ink = [x / 255.0 for x in hex_to_rgb255(sh.ink)]
    tint = [x / 255.0 for x in sh.line_tint]
    out = []
    for i in range(3):
        line_c = (c[i] + (tint[i] - c[i]) * sh.line_tint_mix) * (1 - sh.line_darken)
        out.append((ink[i] + (line_c - ink[i]) * o) * 255.0)
    return out


def outline_wpo_reference(pos, n, f, tan_y, px, baked, eps):
    N = norm(n)
    d = math.sqrt(sum(x * x for x in pos))
    V = [x / d for x in pos]
    depth = max(sum(a * b for a, b in zip(pos, f)), 1.0)
    ndv = sum(a * b for a, b in zip(N, V))
    np_ = [N[i] - ndv * V[i] for i in range(3)]
    ln = max(math.sqrt(sum(x * x for x in np_)), eps)
    width = px * 2.0 * depth * tan_y / 1080.0
    return [np_[i] / ln * width - N[i] * baked for i in range(3)]


# ---------------------------------------------------------------------------------------------------------------------
class LintTests(unittest.TestCase):
    def test_nodes_are_well_formed(self):
        for node in hlsl.all_nodes(manifest_shading()):
            with self.subTest(node=node.key):
                self.assertNotIn("@", node.code, "unfilled placeholder")
                self.assertIn("return", node.code)
                self.assertEqual(len(set(node.input_names)), len(node.input_names), "duplicate input names")
                for name in node.input_names:
                    self.assertRegex(name, r"^[A-Za-z][A-Za-z0-9_]*$")
                    self.assertNotIn(name, hlsl.RESERVED_IDENTIFIERS)
                declared = set(re.findall(r"\b(?:float[234]?|int|uint|bool)\s+([A-Za-z_][A-Za-z0-9_]*)\s*=",
                                          node.code))
                bad = declared & hlsl.RESERVED_IDENTIFIERS
                self.assertFalse(bad, f"reserved identifiers declared: {bad}")
                self.assertNotRegex(node.code, r"\bstatic\b")
                self.assertNotRegex(node.code, r"#\s*include")
                self.assertNotRegex(node.code, r"\.(xxx|xx|rrr)\b", "scalar broadcast swizzles are not portable here")

    def test_temporal_dither_reads_the_view_jitter_index(self):
        for node in (hlsl.dither_node(), hlsl.dither_shadow_node(), hlsl.fx_mesh_mask_node()):
            self.assertIn("View.TemporalAAParams.x", node.code, node.key)
            self.assertNotIn("StateFrameIndex", node.code, f"{node.key}: a per-frame index flickers without TAA")

    def test_terrain_tables_agree(self):
        from abyss_content import paths
        self.assertEqual(hlsl.TERRAIN_TILES, paths.TERRAIN_PAINT_TILES)
        # detail atlas rows: Art/blender/environment/terrain.py DETAIL_ROWS ("0", "1", "2", "5")
        self.assertEqual(hlsl.TERRAIN_DETAIL_ROWS, (0, 1, 2, 5))
        painted = hlsl.terrain_painted_node(True)
        self.assertIn("ddx(g)", painted.code.split("float2 b0")[0], "derivatives before any branch")
        self.assertNotIn("Accent", painted.code)

    def test_literals(self):
        self.assertEqual(hlsl.lit(1), "1.0")
        self.assertEqual(hlsl.lit(0.5), "0.5")
        self.assertEqual(hlsl.lit(1e-6), "1.0e-06")
        self.assertEqual(hlsl.lit(-2.25), "-2.25")
        with self.assertRaises(ValueError):
            hlsl.lit(float("nan"))

    def test_lattice_noise_is_normalised_and_periodic(self):
        waves, lo, hi = hlsl.lattice_noise_waves()
        self.assertEqual(len(waves), 12)
        self.assertLess(lo, hi)
        for u, v in ((0.0, 0.0), (0.37, 1.2), (2.9, 0.4)):
            a = hlsl.lattice_noise_reference(u, v)
            b = hlsl.lattice_noise_reference(u + 3.0, v - 3.0)
            self.assertAlmostEqual(a, b, places=9)
            self.assertGreaterEqual(a, -1.05)
            self.assertLessEqual(a, 1.05)

    def test_mulberry32_matches_the_web(self):
        # TerrainLattice.ts rng(9176): first draws (computed with the JS implementation).
        r = hlsl.mulberry32(1)
        first = [r() for _ in range(3)]
        self.assertAlmostEqual(first[0], 0.6270739405881613, places=12)
        self.assertAlmostEqual(first[1], 0.002735721180215478, places=12)
        self.assertAlmostEqual(first[2], 0.5274470399599522, places=12)


@unittest.skipIf(CXX is None, "no C++ compiler")
class CompileTests(unittest.TestCase):
    def test_every_node_compiles(self):
        compile_and_run(cpp_unit(hlsl.all_nodes(manifest_shading())), run=False)


@unittest.skipIf(CXX is None, "no C++ compiler")
class MathTests(unittest.TestCase):
    def test_toon_matches_the_art_reference(self):
        sh = manifest_shading()
        node = hlsl.toon_node(sh)
        cases = []
        bases = ["#D0473A", "#8A93A6", "#2B2B38", "#F2E6C9", "#FFB45C"]
        normals = [(0, 0, 1), (1, 0, 0), (0, -1, 0), (-0.4, 0.4, 0.82), (0.3, -0.7, 0.2), (-0.6, 0.6, -0.5)]
        view = (0.4545, -0.4545, 0.766)   # towards the W1 camera (yaw 45, pitch -50)
        for b in bases:
            for n in normals:
                for (s, l, e, gr) in ((0.42, 0.32, 0.0, 0.0), (0.15, 0.5, 0.0, 0.6), (0.42, 0.32, 1.0, 0.0)):
                    cases.append((b, s, l, e, n, view, gr))
        lines = []
        for i, (b, s, l, e, n, v, gr) in enumerate(cases):
            base_lin = [srgb_to_linear1(x / 255.0) for x in hex_to_rgb255(b)]
            lines.append(
                f'{{ float3 o = AF_Toon(float3({base_lin[0]!r}f, {base_lin[1]!r}f, {base_lin[2]!r}f), '
                f'float4({float(s)!r}f, {float(l)!r}f, {float(e)!r}f, 1.f), float3({float(n[0])!r}f, {float(n[1])!r}f, {float(n[2])!r}f), '
                f'float3({float(v[0])!r}f, {float(v[1])!r}f, {float(v[2])!r}f), '
                f'float4({sh.key_light[0]!r}f, {sh.key_light[1]!r}f, {sh.key_light[2]!r}f, 0.f), '
                f'float4(1.f, 0.5f, 0.25f, 1.f), float4(1.f, 236.f / 255.f, 200.f / 255.f, 1.f), {float(gr)!r}f, 1.f, 0.f, '
                f'0.f, float3(0.f, 0.f, 0.f), 0.f, 0.f, float3(1.f, 1.f, 1.f), 0.f, float3(1.f, 1.f, 1.f), 0.f, '
                f'float3(1.f, 1.f, 1.f), 0.f, 0.f, 0.f, float2(0.f, 0.f), float3(1.f, 1.f, 1.f)); '
                f'std::printf("%.9g %.9g %.9g\\n", o.x, o.y, o.z); }}')
        main = "int main() {\n" + "\n".join(lines) + "\nreturn 0; }"
        out = compile_and_run(cpp_unit([node], main), run=True).split("\n")
        worst = 0.0
        for (b, s, l, e, n, v, gr), line in zip(cases, out):
            got = [float(x) for x in line.split()]
            disp = [linear_to_srgb1(min(x, 1.0)) * 255.0 for x in got]
            ref = toon_reference(sh, b, s, l, e, n, v, gr)
            for d, r in zip(disp, ref):
                worst = max(worst, abs(d - r))
        # The kit's own conformance tolerance is 0.64/255 (Art/blender/README.md 2).
        self.assertLess(worst, 0.6, f"max error {worst:.3f}/255")

    def test_toon_feedback_layers(self):
        node = hlsl.toon_node(SHADING)
        base = "float3(0.5f, 0.5f, 0.5f), float4(0.42f, 0.32f, 0.f, 1.f), float3(0.f, 0.f, 1.f), float3(0.f, 0.f, 1.f)," \
               " float4(0.40558f, -0.40558f, 0.819152f, 0.f), float4(1.f, 1.f, 1.f, 1.f), float4(1.f, 0.9f, 0.8f, 1.f)," \
               " 0.f, 1.f, 0.f, 0.f, float3(1.f, 1.f, 1.f), 0.6f"
        calls = {
            "flash": f"{base}, 1.f, float3(1.f, 1.f, 1.f), 0.f, float3(1.f, 1.f, 1.f), 0.f, float3(1.f, 1.f, 1.f), 0.f,"
                     " 0.f, 0.f, float2(0.f, 0.f), float3(1.f, 1.f, 1.f)",
            "pain": f"{base}, 0.f, float3(1.f, 0.f, 0.f), 1.f, float3(1.f, 1.f, 1.f), 0.f, float3(1.f, 1.f, 1.f), 0.f,"
                    " 0.f, 0.f, float2(0.f, 0.f), float3(1.f, 1.f, 1.f)",
            "plain": f"{base}, 0.f, float3(1.f, 1.f, 1.f), 0.f, float3(1.f, 1.f, 1.f), 0.f, float3(1.f, 1.f, 1.f), 0.f,"
                     " 0.f, 0.f, float2(0.f, 0.f), float3(1.f, 1.f, 1.f)",
        }
        main = "int main() {\n" + "\n".join(
            f'{{ float3 o = AF_Toon({args}); std::printf("{k} %.6f %.6f %.6f\\n", o.x, o.y, o.z); }}'
            for k, args in calls.items()) + "\nreturn 0; }"
        res = {}
        for line in compile_and_run(cpp_unit([node], main), run=True).strip().split("\n"):
            k, *vals = line.split()
            res[k] = [float(x) for x in vals]
        self.assertEqual(res["flash"], [1.0, 1.0, 1.0])
        self.assertGreater(res["pain"][0], 0.0)
        self.assertEqual(res["pain"][1], 0.0)
        self.assertGreater(res["plain"][1], 0.0)

    def test_ink_matches_the_reference(self):
        sh = manifest_shading()
        node = hlsl.ink_node(sh)
        cases = [("#D0473A", 0.4), ("#8A93A6", 1.0), ("#2B2B38", 0.0), ("#F2E6C9", 0.4)]
        lines = []
        for b, o in cases:
            base_lin = [srgb_to_linear1(x / 255.0) for x in hex_to_rgb255(b)]
            lines.append(f'{{ float3 c = AF_InkColor(float3({base_lin[0]!r}f, {base_lin[1]!r}f, {base_lin[2]!r}f), '
                         f'float4(0.f, 0.f, 0.f, {float(o)!r}f), 0.f, float2(0.f, 0.f), 0.f, float3(1.f, 1.f, 1.f)); '
                         f'std::printf("%.9g %.9g %.9g\\n", c.x, c.y, c.z); }}')
        out = compile_and_run(cpp_unit([node], "int main() {\n" + "\n".join(lines) + "\nreturn 0; }"), run=True)
        for (b, o), line in zip(cases, out.strip().split("\n")):
            got = [linear_to_srgb1(float(x)) * 255.0 for x in line.split()]
            for g, r in zip(got, ink_reference(sh, b, o)):
                self.assertAlmostEqual(g, r, delta=0.05)

    def test_outline_wpo_matches_the_manifest_formula(self):
        sh = manifest_shading()
        node = hlsl.outline_wpo_node(sh)
        cases = [((120.0, -80.0, 2400.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0), 0.1773, 3.5, 1.8),
                 ((-50.0, 40.0, 2600.0), (0.6, 0.0, 0.8), (0.0, 0.0, 1.0), 0.1773, 3.0, 1.5),
                 ((10.0, 5.0, 2500.0), (0.0, 0.0, -1.0), (0.0, 0.0, 1.0), 0.2, 1.2, 1.2)]
        lines = []
        for pos, n, f, t, px, baked in cases:
            fl = lambda x: f"{float(x)!r}f"  # noqa: E731
            lines.append(f'{{ float3 w = AF_OutlineWPO(float3({fl(pos[0])}, {fl(pos[1])}, {fl(pos[2])}), '
                         f'float3({fl(n[0])}, {fl(n[1])}, {fl(n[2])}), float3({fl(f[0])}, {fl(f[1])}, {fl(f[2])}), '
                         f'float2(0.3f, {fl(t)}), {fl(px)}, {fl(baked)}, 1.f); '
                         f'std::printf("%.9g %.9g %.9g\\n", w.x, w.y, w.z); }}')
        out = compile_and_run(cpp_unit([node], "int main() {\n" + "\n".join(lines) + "\nreturn 0; }"), run=True)
        for (pos, n, f, t, px, baked), line in zip(cases, out.strip().split("\n")):
            got = [float(x) for x in line.split()]
            ref = outline_wpo_reference(pos, n, f, t, px, baked, sh.outline_eps)
            for g, r in zip(got, ref):
                self.assertAlmostEqual(g, r, delta=1e-3)
        # 3.5 px at 1080p at the default distance ~ 2.92 cm of ink (manifest wpo.atDefaultDistance.heroInkCm)
        width = 3.5 * 2.0 * 2537.3 * math.tan(math.radians(35.0 / 2)) * 9.0 / 16.0 / 1080.0
        self.assertAlmostEqual(width, 2.917, delta=0.01)

    def test_terrain_runs_and_blends(self):
        nodes = [hlsl.terrain_node(True), hlsl.terrain_node(False)]
        # 6x4 map: left half grass (0), right half dirt (1), one water tile (3) at (1, 2).
        texels = []
        for row in range(4):
            for col in range(6):
                paint = 0 if col < 3 else 1
                if (col, row) == (1, 2):
                    paint = 3
                variant = (col * 7 + row * 3) % 12
                texels.append(f"float4({paint}.f / 255.f, {variant}.f / 255.f, 1.f, {paint}.f / 255.f)")
        colors = {0: "0.17f, 0.37f, 0.06f", 1: "0.52f, 0.33f, 0.12f", 2: "0.38f, 0.36f, 0.3f",
                  3: "0.05f, 0.29f, 0.48f", 5: "0.48f, 0.3f, 0.14f"}
        ranks = {0: 4, 1: 1, 2: 2, 3: 0, 5: 2}
        args_style = []
        for t in hlsl.TERRAIN_TILES:
            col = f"float3({colors[t]})"
            args_style += [col, col, col, "float3(0.6f, 0.75f, 0.3f)", "float3(0.9f, 0.8f, 0.2f)", f"{ranks[t]}.f"]
        style = ", ".join(args_style)
        water = "float3(0.15f, 0.55f, 0.6f), float3(0.85f, 0.95f, 0.9f), float3(0.27f, 0.18f, 0.07f)"
        main = f"""
int main() {{
    Texture2D T; T.Width = 6; T.Height = 4;
    T.Texels = {{ {", ".join(texels)} }};
    SamplerState S;
    int bad = 0;
    float sumGrass = 0.f, sumDirt = 0.f;
    for (int j = 0; j <= 60; ++j) for (int i = 0; i <= 100; ++i) {{
        float2 uv(-0.5f + i * 0.06f, -0.5f + j * 0.066f);
        float3 a = AF_Terrain(uv, 0.f, 0.f, T, S, 6.f, 4.f, {style}, {water});
        float3 b = AF_TerrainLow(uv, 0.f, 0.f, T, S, 6.f, 4.f, {style}, {water});
        if (!(a.x == a.x) || !(a.y == a.y) || !(a.z == a.z) || !(b.x == b.x)) ++bad;
        if (a.x < 0.f || a.y < 0.f || a.z < 0.f || a.x > 1.001f || a.y > 1.001f || a.z > 1.001f) ++bad;
        if (uv.x < 0.4f && uv.y < 1.2f) sumGrass += a.y - a.x;
        if (uv.x > 3.6f) sumDirt += a.x - a.z;
    }}
    std::printf("%d %.4f %.4f\\n", bad, sumGrass, sumDirt);
    return 0;
}}"""
        out = compile_and_run(cpp_unit(nodes, main), run=True).split()
        self.assertEqual(int(out[0]), 0, "NaN or out-of-range terrain colour")
        self.assertGreater(float(out[1]), 0.0, "grass region should read green")
        self.assertGreater(float(out[2]), 0.0, "dirt region should read ochre")

    def test_painted_terrain_layers_and_details(self):
        """M_AF_Terrain_Painted (terrain.composite in Art/blender/environment/terrain.py): the winner's layer colour,
        the detail atlas cell (row = own paint, column = variant % 3) only on tiles with variant >= 6, none on water."""
        nodes = [hlsl.terrain_painted_node(True), hlsl.terrain_painted_node(False)]
        # 6x4 map: grass (0) left, dirt (1) right, one water tile (3) at (1, 2); TileIds G = tileHash % 12.
        variants = {(1, 1): 7, (4, 1): 9, (2, 1): 6, (1, 3): 2, (1, 2): 8}
        texels = []
        for row in range(4):
            for col in range(6):
                paint = 3 if (col, row) == (1, 2) else (0 if col < 3 else 1)
                texels.append(f"float4({paint}.f / 255.f, {variants.get((col, row), 0)}.f / 255.f, 1.f, "
                              f"{paint}.f / 255.f)")
        # 4 x 4 atlas, one texel per cell: (row 0 grass, column 1) red, (row 1 dirt, column 0) blue, (row 0, column 0)
        # transparent (variant 6 has a detail cell, here an empty one), the water column / rest transparent.
        cells = ["float4(0.f, 0.f, 0.f, 0.f)"] * 16
        cells[0 * 4 + 1] = "float4(1.f, 0.f, 0.f, 1.f)"
        cells[1 * 4 + 0] = "float4(0.f, 0.f, 1.f, 1.f)"
        layers = {0: (0.1, 0.4, 0.05), 1: (0.5, 0.3, 0.1), 2: (0.35, 0.33, 0.28), 3: (0.05, 0.29, 0.48),
                  5: (0.45, 0.3, 0.15)}
        ranks = {0: 4, 1: 1, 2: 2, 3: 0, 5: 2}
        style = []
        for t in hlsl.TERRAIN_TILES:
            style += ["float3({:.6g}f, {:.6g}f, {:.6g}f)".format(*layers[t]), "float3(0.6f, 0.75f, 0.3f)",
                      f"{ranks[t]}.f"]
        water = "float3(0.15f, 0.55f, 0.6f), float3(0.85f, 0.95f, 0.9f), float3(0.27f, 0.18f, 0.07f)"
        probes = [(1.0, 1.0), (4.0, 1.0), (2.0, 1.0), (1.0, 3.0), (1.0, 2.0), (0.0, 0.0)]
        calls = "\n".join(
            f'{{ float3 a = {fn}(float2({u}f, {v}f), 0.f, 0.f, T, S, 6.f, 4.f, {", ".join(style)}, {water}, D, S);'
            f' std::printf("%.6f %.6f %.6f\\n", a.x, a.y, a.z); }}'
            for fn in ("AF_TerrainPainted", "AF_TerrainPaintedLow") for u, v in probes)
        main = f"""
int main() {{
    Texture2D T; T.Width = 6; T.Height = 4;
    T.Texels = {{ {", ".join(texels)} }};
    Texture2D D; D.Width = 4; D.Height = 4;
    D.Texels = {{ {", ".join(cells)} }};
    SamplerState S;
{calls}
    int bad = 0;
    for (int j = 0; j <= 60; ++j) for (int i = 0; i <= 100; ++i) {{
        float2 uv(-0.5f + i * 0.06f, -0.5f + j * 0.066f);
        float3 a = AF_TerrainPainted(uv, 0.f, 0.f, T, S, 6.f, 4.f, {", ".join(style)}, {water}, D, S);
        if (!(a.x == a.x) || !(a.y == a.y) || !(a.z == a.z)) ++bad;
        if (a.x < 0.f || a.y < 0.f || a.z < 0.f || a.x > 1.001f || a.y > 1.001f || a.z > 1.001f) ++bad;
    }}
    std::printf("%d\\n", bad);
    return 0;
}}"""
        out = compile_and_run(cpp_unit(nodes, main), run=True).strip().split("\n")
        got = [[float(x) for x in line.split()] for line in out[:-1]]
        self.assertEqual(int(out[-1]), 0, "NaN or out-of-range terrain colour")
        expected = [(1.0, 0.0, 0.0),   # grass, variant 7 -> cell (0, 1)
                    (0.0, 0.0, 1.0),   # dirt, variant 9 -> cell (1, 0)
                    layers[0],         # grass, variant 6 -> cell (0, 0), transparent
                    layers[0],         # variant 2 < 6: no detail
                    layers[3],         # water, variant 8: no painted detail (lily pads are meshes)
                    layers[0]]         # variant 0
        for tier in range(2):
            for k, exp in enumerate(expected):
                for g_, e in zip(got[tier * len(probes) + k], exp):
                    self.assertAlmostEqual(g_, e, delta=2e-3, msg=f"tier {tier} probe {probes[k]}")

    def test_dither_and_quads(self):
        nodes = [hlsl.dither_node(), hlsl.fade_opacity_node(), hlsl.blob_node()] + list(hlsl.quad_nodes().values())
        main = """
int main() {
    // translucent slime shell: BodyOpacity, faded by max(Fade, instance fade)
    if (std::fabs(AF_FadeOpacity(0.f, float2(0.f, 0.f), 0.62f) - 0.62f) > 1e-6f) return 4;
    if (std::fabs(AF_FadeOpacity(0.5f, float2(0.25f, 0.f), 0.62f) - 0.31f) > 1e-6f) return 5;
    if (std::fabs(AF_FadeOpacity(0.f, float2(0.5f, 0.f), 0.62f) - 0.31f) > 1e-6f) return 6;
    if (AF_FadeOpacity(1.f, float2(0.f, 0.f), 0.62f) != 0.f) return 7;
    int visible = 0;
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
        Parameters.SvPosition = float4(x + 0.5f, y + 0.5f, 0.f, 1.f);
        visible += AF_Dither(0.5f, float2(0.f, 0.f), 1.f) > 0.5f ? 1 : 0;
        if (AF_Dither(0.f, float2(0.f, 0.f), 1.f) < 0.5f) return 2;
        if (AF_Dither(1.f, float2(0.f, 0.f), 1.f) > 0.5f) return 3;
    }
    float3 centre = AF_LightPool(float2(0.5f, 0.5f), float3(1.f, 1.f, 1.f), 1.f);
    float3 edge = AF_LightPool(float2(1.f, 0.5f), float3(1.f, 1.f, 1.f), 1.f);
    std::printf("%d %.4f %.4f %.4f %.4f\\n", visible, centre.x, edge.x, AF_BlobAlpha(float2(0.5f, 0.5f), 0.f),
                AF_BlobAlpha(float2(0.5f, 0.5f), 1.f));
    return 0;
}"""
        out = compile_and_run(cpp_unit(nodes, main), run=True).split()
        self.assertEqual(int(out[0]), 8, "half the 4x4 Bayer cells pass at 50 % opacity")
        self.assertAlmostEqual(float(out[1]), 1.0, places=4)
        self.assertAlmostEqual(float(out[2]), 0.0, places=4)
        self.assertAlmostEqual(float(out[3]), 0.45, places=4)
        self.assertAlmostEqual(float(out[4]), 0.0, places=4)

    def test_temporal_dither(self):
        """AF_Dither / AF_DitherShadow / AF_FxMeshMask: the 4x4 Bayer rotated per temporal AA sample (DECISIONS R1).

        Every sample keeps exactly alpha x 16 of the 16 cells, every pixel is kept about alpha of the samples of a TSR
        sequence (8 jitter samples -> 11, the next prime, in UE 5.6+), and sample 0 (every frame without temporal AA)
        is the static ordered pattern."""
        nodes = [hlsl.dither_node(), hlsl.dither_shadow_node(), hlsl.fx_mesh_mask_node()]
        main = """
int main() {
    const float alphas[] = {0.25f, 0.45f, 0.5f, 0.55f, 0.75f};
    // per sample index: kept cells of AF_Dither (alpha via Fade, via the instance fade), AF_DitherShadow, AF_FxMeshMask
    for (int k = 0; k < 11; ++k) {
        View.TemporalAAParams = float4((float)k, 11.f, 0.f, 0.f);
        for (float a : alphas) {
            int dith = 0, inst = 0, shadow = 0, shadowInst = 0, fx = 0;
            for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
                Parameters.SvPosition = float4(x + 8.5f, y + 4.5f, 0.f, 1.f);
                dith += AF_Dither(1.f - a, float2(0.f, 0.f), 1.f) > 0.5f ? 1 : 0;
                inst += AF_Dither(0.f, float2(1.f - a, 0.f), 1.f) > 0.5f ? 1 : 0;
                shadow += AF_DitherShadow(1.f - a, 1.f) > 0.5f ? 1 : 0;
                shadowInst += AF_DitherShadow(0.f, a) > 0.5f ? 1 : 0;
                fx += AF_FxMeshMask(float4(1.f, 1.f, 1.f, a)) > 0.5f ? 1 : 0;
            }
            std::printf("%d %d %d %d %d\\n", dith, inst, shadow, shadowInst, fx);
        }
    }
    // per pixel over one sequence (TSR: 11 samples, TAA: 8) at alpha .25 / .45: samples kept
    for (int n : {11, 8}) for (float a : {0.25f, 0.45f}) {
        for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
            int kept = 0;
            for (int k = 0; k < n; ++k) {
                View.TemporalAAParams = float4((float)k, (float)n, 0.f, 0.f);
                Parameters.SvPosition = float4(x + 0.5f, y + 0.5f, 0.f, 1.f);
                kept += AF_Dither(0.f, float2(1.f - a, 0.f), 1.f) > 0.5f ? 1 : 0;
            }
            std::printf("%d ", kept);
        }
        std::printf("\\n");
    }
    // kept-sample runs: the longest run of consecutive kept samples of any pixel over two TSR sequences at alpha .25
    int longest = 0;
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
        int run = 0;
        for (int k = 0; k < 22; ++k) {
            View.TemporalAAParams = float4((float)(k % 11), 11.f, 0.f, 0.f);
            Parameters.SvPosition = float4(x + 0.5f, y + 0.5f, 0.f, 1.f);
            run = AF_Dither(0.f, float2(0.75f, 0.f), 1.f) > 0.5f ? run + 1 : 0;
            longest = run > longest ? run : longest;
        }
    }
    std::printf("%d\\n", longest);
    // no temporal AA: index 0, the ordered pattern
    View.TemporalAAParams = float4(0.f, 1.f, 0.f, 0.f);
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
        Parameters.SvPosition = float4(x + 0.5f, y + 0.5f, 0.f, 1.f);
        std::printf("%d ", AF_Dither(0.f, float2(0.75f, 0.f), 1.f) > 0.5f ? 1 : 0);
    }
    std::printf("\\n");
    return 0;
}"""
        lines = compile_and_run(cpp_unit(nodes, main), run=True).strip().split("\n")
        alphas = (0.25, 0.45, 0.5, 0.55, 0.75)
        for k in range(11):
            for j, a in enumerate(alphas):
                counts = [int(x) for x in lines[k * len(alphas) + j].split()]
                expected = math.ceil(a * 16 - 1e-6)   # cells with threshold (b + .5) / 16 + shift (mod 1) below alpha
                for name, got in zip(("Fade", "instance fade", "shadow", "shadow BodyOpacity", "FX mesh"), counts):
                    self.assertIn(got, (expected - 1, expected) if a * 16 % 1 else (expected,),
                                  f"sample {k}, alpha {a}, {name}: {got} of 16 cells kept")
                self.assertEqual(counts[0], counts[1], "Fade and the instance fade dither alike")
        base = 11 * len(alphas)
        tsr_25, tsr_45, taa_25, taa_45 = ([int(x) for x in lines[base + i].split()] for i in range(4))
        # every 1/n shift once per sequence: each pixel keeps floor or ceil of alpha x n samples
        self.assertTrue(all(n in (2, 3) for n in tsr_25), f"alpha .25 over 11 TSR samples: {tsr_25}")
        self.assertTrue(all(n in (4, 5) for n in tsr_45), f"alpha .45 over 11 TSR samples: {tsr_45}")
        self.assertEqual(sum(tsr_25), 4 * 11)
        self.assertEqual(taa_25, [2] * 16, "alpha .25 over 8 TAA samples")
        self.assertTrue(all(n in (3, 4) for n in taa_45), f"alpha .45 over 8 TAA samples: {taa_45}")
        self.assertLessEqual(int(lines[base + 4]), 2, "kept samples come in runs (TSR would see a slow blink)")
        static = [int(x) for x in lines[base + 5].split()]
        # the ordered 4x4 Bayer (the pre-temporal pattern): kept where (b + .5) / 16 < .25, i.e. b in 0..3
        ref = []
        for y in range(4):
            for x in range(4):
                m = x ^ y
                b = ((m & 1) << 3) | ((y & 1) << 2) | (m & 2) | ((y & 2) >> 1)
                ref.append(1 if (b + 0.5) / 16 < 0.25 else 0)
        self.assertEqual(static, ref)
        self.assertEqual(sum(static), 4)



# ---------------------------------------------------------------------------------------------------------------------
# Art pass (DECISIONS P11 revised, R3, R16, R19, P14)
# ---------------------------------------------------------------------------------------------------------------------
def f3(v) -> str:
    return f"float3({float(v[0])!r}f, {float(v[1])!r}f, {float(v[2])!r}f)"


def f4(v) -> str:
    return f"float4({float(v[0])!r}f, {float(v[1])!r}f, {float(v[2])!r}f, {float(v[3])!r}f)"


# AF_Toon / AF_ToonSplit inputs without feedback (C++ expressions); the tests override some per call.
TOON_DEFAULTS = {
    "Base": "float3(0.5f, 0.5f, 0.5f)", "P": "float4(0.42f, 0.32f, 0.f, 1.f)", "N": "float3(0.f, 0.f, 1.f)",
    "V": f3((0.4545, -0.4545, 0.766)), "L": f4((*SHADING.key_light, 0.0)), "SunCol": "float4(1.f, 1.f, 1.f, 1.f)",
    "RimCol": "float4(1.f, 236.f / 255.f, 200.f / 255.f, 1.f)", "Ground": "0.f", "GroundEnable": "1.f",
    "SunTint": "1.f", "RimOverride": "0.f", "RimOverrideColor": "float3(1.f, 1.f, 1.f)", "RimOverrideAlpha": "0.55f",
    "HitFlash": "0.f", "PainTint": "float3(1.f, 1.f, 1.f)", "PainAmt": "0.f", "Telegraph": "float3(1.f, 1.f, 1.f)",
    "TelegraphAmt": "0.f", "StatusTint": "float3(1.f, 1.f, 1.f)", "StatusAmt": "0.f", "Ghost": "0.f",
    "Highlight": "0.f", "Inst": "float2(0.f, 0.f)", "HighlightColor": "float3(1.f, 1.f, 1.f)",
    "BaseSaturation": "1.f", "BaseValue": "1.f", "Pw": "float3(0.f, 0.f, 0.f)", "CastShadowStrength": "0.7f",
    **{f"L{i}": "float4(0.f, 0.f, 0.f, 0.f)" for i in range(8)},
    **{f"C{i}": "float4(0.f, 0.f, 0.f, 0.f)" for i in range(8)},
}


def cpp_call(node: hlsl.CustomNode, args: dict[str, str], defaults: dict[str, str] | None = None) -> str:
    """`Key(arg, ...)` in the node's parameter order (inputs, then the additional outputs as lvalues)."""
    values = []
    for inp in node.inputs:
        if inp.name in args:
            values.append(args[inp.name])
        elif defaults is not None and inp.name in defaults:
            values.append(defaults[inp.name])
        else:
            raise KeyError(f"{node.key}: no value for input {inp.name}")
    values += [args[o.name] for o in node.additional_outputs]
    return f"{node.key}({', '.join(values)})"


def lookdev_toon(sh: Shading, base_hex: str, s: float, l: float, e: float, n, v, grounding: float, shade: bool):
    """The art-pass look-dev's toon group (artpass/lookdev/lookdev.py build_toon): mode lit = today's M_AF_Toon,
    mode shade = the shade band + grounding (ndl forced to -1, so no light band and no rim); linear, no tint."""
    c = [x / 255.0 for x in hex_to_rgb255(base_hex)]
    N, V, L = norm(n), norm(v), norm(sh.key_light)
    ndl = -1.0 if shade else sum(a * b for a, b in zip(N, L))
    h = ndl * 0.5 + 0.5
    hw = sh.band_width / 2
    wb, wl = smoothstep(sh.t_shade - hw, sh.t_shade + hw, h), smoothstep(sh.t_light - hw, sh.t_light + hw, h)
    cool, warm = [x / 255.0 for x in sh.cool_shade], [x / 255.0 for x in sh.warm_light]
    gt, rimc = [x / 255.0 for x in hex_to_rgb255(sh.ground_color)], [x / 255.0 for x in hex_to_rgb255(sh.rim_color)]
    fres = (1 - max(sum(a * b for a, b in zip(N, V)), 0.0)) ** sh.rim_power
    rim = smoothstep(sh.rim_band[0], sh.rim_band[1], fres) * sh.rim_alpha * smoothstep(0.0, sh.rim_mask_edge, ndl)
    out = []
    for i in range(3):
        sh_c = (c[i] + (cool[i] - c[i]) * sh.cool_shade_mix) * (1 - s)
        col = sh_c + (c[i] - sh_c) * wb
        col = col + (c[i] + (warm[i] - c[i]) * l - col) * wl
        col = col + (gt[i] - col) * (grounding * sh.ground_alpha)
        col = col + (rimc[i] - col) * rim
        col = col + (c[i] - col) * e
        out.append(srgb_to_linear1(min(max(col, 0.0), 1.0)) * (1 + e * sh.emissive_boost))
    return out


@unittest.skipIf(CXX is None, "no C++ compiler")
class ArtPassMathTests(unittest.TestCase):
    """The shadow-catcher toon, the ink scale, the selection rim, the ground grade and the AO composite."""

    NORMALS = [(0, 0, 1), (1, 0, 0), (0, -1, 0), (-0.4, 0.4, 0.82), (0.3, -0.7, 0.2), (-0.6, 0.6, -0.5),
               (0.40558, -0.40558, 0.819152)]
    PALETTE = [(0.42, 0.32, 0.0), (0.15, 0.5, 0.0), (0.42, 0.32, 0.45), (0.3, 0.2, 1.0)]
    BASES = ["#D0473A", "#8A93A6", "#2B2B38", "#F2E6C9", "#FFB45C", "#74A247"]

    def run_cpp(self, nodes: list[hlsl.CustomNode], body: str) -> list[str]:
        return compile_and_run(cpp_unit(nodes, "int main() {\n" + body + "\nreturn 0; }"), run=True).strip().split("\n")

    @staticmethod
    def base_lin(hex_: str) -> str:
        return f3([srgb_to_linear1(x / 255.0) for x in hex_to_rgb255(hex_)])

    def test_split_sums_to_the_unlit_toon_in_full_sun(self):
        """(a) S = 1: Emit + Lit x SunCol == AF_Toon (1e-4) over normals x palettes x SunTint x SunColor x every
        feedback combination; (c) k = 0: Lit == 0 and Emit == AF_Toon."""
        sh = manifest_shading()
        toon, split = hlsl.toon_node(sh), hlsl.toon_split_node(sh)
        feedback = {  # one partial value per layer; the loop switches each on / off
            "HitFlash": ("hf", "0.45f"), "PainAmt": ("pa", "0.8f"), "TelegraphAmt": ("ta", "0.6f"),
            "StatusAmt": ("sa", "0.7f"), "Ghost": ("gh", "0.55f"), "Highlight": ("hi", "0.65f"),
        }
        args = {"Base": "base", "P": "pal", "N": "nrm", "SunCol": "sun", "SunTint": "tint",
                "PainTint": "float3(1.f, 0.3f, 0.25f)", "Telegraph": "float3(1.f, 0.55f, 0.1f)",
                "StatusTint": "float3(0.5f, 0.8f, 1.f)", "HighlightColor": "float3(1.f, 0.9f, 0.6f)",
                "CastShadowStrength": "kc", "Lit": "lit"}
        for name, (var, _) in feedback.items():
            args[name] = var
        loops = "\n".join(f"float {var} = (mask >> {i}) & 1 ? {val} : 0.f;"
                          for i, (var, val) in enumerate(feedback.values()))
        body = f"""
const float3 bases[] = {{ {", ".join(self.base_lin(b) for b in self.BASES)} }};
const float4 pals[] = {{ {", ".join(f4((*p, 1.0)) for p in self.PALETTE)} }};
const float3 nrms[] = {{ {", ".join(f3(n) for n in self.NORMALS)} }};
const float4 suns[] = {{ float4(1.f, 1.f, 1.f, 1.f), float4(1.f, 0.94f, 0.82f, 1.f) }};
float worst = 0.f, worstK0 = 0.f, litK0 = 0.f; int n = 0;
for (const float3& base : bases) for (const float4& pal : pals) for (const float3& nrm : nrms)
for (const float4& sun : suns) for (float tint : {{0.f, 1.f}}) for (int mask = 0; mask < 64; ++mask) {{
    {loops}
    float3 ref = {cpp_call(toon, args, TOON_DEFAULTS)};
    for (float kc : {{0.f, 0.7f, 1.f}}) {{
        float3 lit;
        float3 emit = {cpp_call(split, args, TOON_DEFAULTS)};
        float3 sum = emit + lit * float3(sun.r, sun.g, sun.b);
        float err = std::fabs(sum.x - ref.x) + std::fabs(sum.y - ref.y) + std::fabs(sum.z - ref.z);
        worst = err > worst ? err : worst;
        if (kc == 0.f) {{
            float e0 = std::fabs(emit.x - ref.x) + std::fabs(emit.y - ref.y) + std::fabs(emit.z - ref.z);
            worstK0 = e0 > worstK0 ? e0 : worstK0;
            litK0 += std::fabs(lit.x) + std::fabs(lit.y) + std::fabs(lit.z);
        }}
        ++n;
    }}
}}
std::printf("%d %.9g %.9g %.9g\\n", n, worst, worstK0, litK0);"""
        out = self.run_cpp([toon, split], body)[0].split()
        self.assertEqual(int(out[0]), len(self.BASES) * len(self.PALETTE) * len(self.NORMALS) * 2 * 2 * 64 * 3)
        self.assertLess(float(out[1]), 1e-4, "Emit + Lit x SunCol differs from AF_Toon")
        self.assertLess(float(out[2]), 1e-4, "k = 0: Emit differs from AF_Toon")
        self.assertEqual(float(out[3]), 0.0, "k = 0: Lit must be exactly 0")

    def test_full_cast_shadow_is_the_shade_band(self):
        """(b) k = 1, Lit dropped (S = 0): Emit == decode(colS) x T, i.e. AF_Toon with the key light behind the
        surface (hl = 0: shade band + grounding + emissive mix, no light band, no rim); never brighter than lit."""
        sh = manifest_shading()
        toon, split = hlsl.toon_node(sh), hlsl.toon_split_node(sh)
        neg_l = f3([-x for x in sh.key_light])
        body = f"""
const float3 bases[] = {{ {", ".join(self.base_lin(b) for b in self.BASES)} }};
const float4 pals[] = {{ {", ".join(f4((*p, 1.0)) for p in self.PALETTE)} }};
const float3 nrms[] = {{ {", ".join(f3(n) for n in self.NORMALS)} }};
float worst = 0.f, brighter = 0.f;
for (const float3& base : bases) for (const float4& pal : pals) for (const float3& nrm : nrms)
for (float gr : {{0.f, 0.6f}}) for (float tint : {{0.f, 1.f}}) {{
    float3 lit;
    float3 emit = {cpp_call(split, {"Base": "base", "P": "pal", "N": "nrm", "Ground": "gr", "SunTint": "tint",
                                    "SunCol": "float4(1.f, 0.94f, 0.82f, 1.f)", "CastShadowStrength": "1.f",
                                    "Lit": "lit"}, TOON_DEFAULTS)};
    float3 lt = {cpp_call(toon, {"Base": "base", "P": "pal", "N": "nrm", "Ground": "gr", "SunTint": "tint",
                                 "SunCol": "float4(1.f, 0.94f, 0.82f, 1.f)"}, TOON_DEFAULTS)};
    float3 sd = {cpp_call(toon, {"Base": "base", "P": "pal", "N": neg_l, "Ground": "gr", "SunTint": "tint",
                                 "SunCol": "float4(1.f, 0.94f, 0.82f, 1.f)"}, TOON_DEFAULTS)};
    float3 want = min(lt, sd);   // where the shade band is brighter than the lit colour, a shadow never brightens
    float err = std::fabs(emit.x - want.x) + std::fabs(emit.y - want.y) + std::fabs(emit.z - want.z);
    worst = err > worst ? err : worst;
    brighter += (emit.x > lt.x + 1e-6f) + (emit.y > lt.y + 1e-6f) + (emit.z > lt.z + 1e-6f);
}}
std::printf("%.9g %.9g\\n", worst, brighter);"""
        worst, brighter = (float(x) for x in self.run_cpp([toon, split], body)[0].split())
        self.assertLess(worst, 1e-5)
        self.assertEqual(brighter, 0.0)

    def test_hit_flash_and_emissive_carry_no_lit_part(self):
        """(d) HitFlash 1: Emit == 1, Lit == 0; (f) e = 1: Lit == 0."""
        sh = manifest_shading()
        split = hlsl.toon_split_node(sh)
        body = f"""
const float3 bases[] = {{ {", ".join(self.base_lin(b) for b in self.BASES)} }};
const float3 nrms[] = {{ {", ".join(f3(n) for n in self.NORMALS)} }};
float flashEmit = 0.f, flashLit = 0.f, emLit = 0.f;
for (const float3& base : bases) for (const float3& nrm : nrms) {{
    float3 lit;
    float3 emit = {cpp_call(split, {"Base": "base", "N": "nrm", "HitFlash": "1.f", "StatusAmt": "0.5f",
                                    "StatusTint": "float3(0.2f, 0.5f, 1.f)", "Lit": "lit"}, TOON_DEFAULTS)};
    flashEmit += std::fabs(emit.x - 1.f) + std::fabs(emit.y - 1.f) + std::fabs(emit.z - 1.f);
    flashLit += std::fabs(lit.x) + std::fabs(lit.y) + std::fabs(lit.z);
    {cpp_call(split, {"Base": "base", "N": "nrm", "P": "float4(0.42f, 0.32f, 1.f, 1.f)", "CastShadowStrength": "1.f",
                      "Lit": "lit"}, TOON_DEFAULTS)};
    emLit += std::fabs(lit.x) + std::fabs(lit.y) + std::fabs(lit.z);
}}
std::printf("%.9g %.9g %.9g\\n", flashEmit, flashLit, emLit);"""
        flash_emit, flash_lit, em_lit = (float(x) for x in self.run_cpp([split], body)[0].split())
        self.assertLess(flash_emit, 1e-5)
        self.assertEqual(flash_lit, 0.0)
        self.assertLess(em_lit, 1e-6)

    def test_torch_slots(self):
        """(e) Slots with colour alpha 0 add exactly nothing; a slot facing the surface at d = r / 2 adds
        linBase x Col x 0.5625, one behind it x 0.35 of that; beyond the radius nothing."""
        sh = manifest_shading()
        split = hlsl.toon_split_node(sh)
        n = norm((-0.4, 0.4, 0.82))
        r = 240.0
        pw = (130.0, -2410.0, 35.0)
        near = [pw[i] + n[i] * r / 2 for i in range(3)]
        behind = [pw[i] - n[i] * r / 2 for i in range(3)]
        base = "float3(0.4f, 0.25f, 0.1f)"
        common = {"Base": base, "N": f3(n), "Pw": f3(pw), "Lit": "lit"}
        off = dict(common, **{f"L{i}": f4((near[0], near[1], near[2], r)) for i in range(8)},
                   **{f"C{i}": "float4(3.f, 2.f, 1.f, 0.f)" for i in range(8)})
        facing = dict(common, L3=f4((*near, r)), C3="float4(0.5f, 0.25f, 0.125f, 1.f)")
        away = dict(common, L7=f4((*behind, r)), C7="float4(0.5f, 0.25f, 0.125f, 1.f)")
        far = dict(common, L0=f4((pw[0] + r * 1.01, pw[1], pw[2], r)), C0="float4(0.5f, 0.25f, 0.125f, 1.f)")
        body = f"""
float3 lit;
float3 none = {cpp_call(split, common, TOON_DEFAULTS)};
float3 litNone = lit;
float3 off = {cpp_call(split, off, TOON_DEFAULTS)};
float3 litOff = lit;
float3 facing = {cpp_call(split, facing, TOON_DEFAULTS)};
float3 litFacing = lit;
float3 away = {cpp_call(split, away, TOON_DEFAULTS)};
float3 far = {cpp_call(split, far, TOON_DEFAULTS)};
std::printf("%d %d\\n", none.x == off.x && none.y == off.y && none.z == off.z,
            litNone.x == litOff.x && litNone.y == litOff.y && litNone.z == litOff.z && litNone.x == litFacing.x);
std::printf("%.9g %.9g %.9g\\n", facing.x - none.x, facing.y - none.y, facing.z - none.z);
std::printf("%.9g %.9g %.9g\\n", away.x - none.x, away.y - none.y, away.z - none.z);
std::printf("%.9g %.9g %.9g\\n", far.x - none.x, far.y - none.y, far.z - none.z);"""
        lines = self.run_cpp([split], body)
        self.assertEqual(lines[0], "1 1", "an off slot changed the colour, or a slot touched Lit")
        col = (0.5, 0.25, 0.125)
        lin = (0.4, 0.25, 0.1)
        for got, want in zip((float(x) for x in lines[1].split()), (b * c * 0.5625 for b, c in zip(lin, col))):
            self.assertAlmostEqual(got, want, delta=2e-5)
        for got, want in zip((float(x) for x in lines[2].split()), (b * c * 0.5625 * 0.35 for b, c in zip(lin, col))):
            self.assertAlmostEqual(got, want, delta=2e-5)
        self.assertEqual([float(x) for x in lines[3].split()], [0.0, 0.0, 0.0])

    def test_split_matches_the_python_reference_and_the_lookdev_composite(self):
        """toon_split_reference == the C++ node, and Emit + Lit x S (white sun) == the look-dev composite
        shade + (lit - shade)(1 - k + k S) (artpass/lookdev/lookdev.py) wherever lit >= shade."""
        sh = manifest_shading()
        split = hlsl.toon_split_node(sh)
        view = (0.4545, -0.4545, 0.766)
        cases = []
        for b in self.BASES:
            for n in self.NORMALS:
                for (s, l, e) in self.PALETTE:
                    for gr in (0.0, 0.6):
                        cases.append((b, s, l, e, n, gr))
        calls = []
        for b, s, l, e, n, gr in cases:
            calls.append(f"{{ float3 lit; float3 em = {cpp_call(split, {'Base': self.base_lin(b), 'P': f4((s, l, e, 1)), 'N': f3(n), 'V': f3(view), 'Ground': f'{gr}f', 'Lit': 'lit'}, TOON_DEFAULTS)};"
                         f' std::printf("%.9g %.9g %.9g %.9g %.9g %.9g\\n", em.x, em.y, em.z, lit.x, lit.y, lit.z); }}')
        lines = self.run_cpp([split], "\n".join(calls))
        worst_cpp = worst_lookdev = 0.0
        compared = 0
        for (b, s, l, e, n, gr), line in zip(cases, lines):
            got = [float(x) for x in line.split()]
            srgb = tuple(x / 255.0 for x in hex_to_rgb255(b))
            emit, lit_ = hlsl.toon_split_reference(sh, srgb, (s, l, e), n, view, gr, cast_shadow=0.7)
            worst_cpp = max(worst_cpp, max(abs(g_ - r_) for g_, r_ in zip(got, emit + lit_)))
            a = lookdev_toon(sh, b, s, l, e, n, view, gr, shade=False)
            bb = lookdev_toon(sh, b, s, l, e, n, view, gr, shade=True)
            if any(x < y for x, y in zip(a, bb)):
                continue    # a channel whose shade band is brighter: the node keeps it at the lit colour (min)
            compared += 1
            for sun in (0.0, 0.35, 1.0):
                for i in range(3):
                    comp = bb[i] + (a[i] - bb[i]) * (1 - 0.7 + 0.7 * sun)
                    worst_lookdev = max(worst_lookdev, abs(emit[i] + lit_[i] * sun - comp))
        self.assertLess(worst_cpp, 2e-5)
        self.assertGreater(compared, len(cases) // 2)
        self.assertLess(worst_lookdev, 1e-6)

    def test_split_twin_returns_the_lit_output(self):
        """The content build's fallback (graph.py): AF_ToonSplit_Lit returns exactly the additional output."""
        sh = manifest_shading()
        split = hlsl.toon_split_node(sh)
        twin = split.twin("Lit")
        self.assertEqual(twin.key, "AF_ToonSplit_Lit")
        self.assertEqual(twin.additional_outputs, ())
        args = {"Base": "float3(0.6f, 0.3f, 0.2f)", "N": f3((0.3, -0.7, 0.2)), "Lit": "lit",
                "PainAmt": "0.5f", "PainTint": "float3(1.f, 0.2f, 0.2f)"}
        body = f"""
float3 lit;
{cpp_call(split, args, TOON_DEFAULTS)};
float3 t = {cpp_call(twin, args, TOON_DEFAULTS)};
std::printf("%d\\n", t.x == lit.x && t.y == lit.y && t.z == lit.z && lit.x > 0.f);"""
        self.assertEqual(self.run_cpp([split, twin], body), ["1"])

    def test_base_grade_defaults_are_exact(self):
        """BaseSaturation / BaseValue 1 leave AF_Toon bit-identical; .85 / .93 match sat_value_reference."""
        sh = manifest_shading()
        toon = hlsl.toon_node(sh)
        old = dataclasses.replace(toon, key="AF_ToonOld",
                                  code=toon.code.replace(hlsl._sat_value("c", "BaseSaturation", "BaseValue"), ""))
        self.assertNotEqual(old.code, toon.code)
        body = f"""
const float3 bases[] = {{ {", ".join(self.base_lin(b) for b in self.BASES)} }};
const float3 nrms[] = {{ {", ".join(f3(n) for n in self.NORMALS)} }};
int diff = 0;
for (const float3& base : bases) for (const float3& nrm : nrms) {{
    float3 a = {cpp_call(toon, {"Base": "base", "N": "nrm"}, TOON_DEFAULTS)};
    float3 b = {cpp_call(old, {"Base": "base", "N": "nrm"}, TOON_DEFAULTS)};
    diff += std::memcmp(&a, &b, sizeof(a)) != 0;
}}
float3 g = {cpp_call(toon, {"Base": self.base_lin("#709C3D"), "P": "float4(0.f, 0.f, 1.f, 1.f)",
                            "BaseSaturation": "0.85f", "BaseValue": "0.93f"}, TOON_DEFAULTS)};
std::printf("%d %.9g %.9g %.9g\\n", diff, g.x, g.y, g.z);"""
        out = self.run_cpp([toon, old], body)[0].split()
        self.assertEqual(int(out[0]), 0)
        # e = 1 (emissive: unshaded, x 1.6 boost): the graded swatch comes straight through
        want = hlsl.sat_value_reference((112 / 255, 156 / 255, 61 / 255), 0.85, 0.93)
        for got, w in zip(out[1:], want):
            self.assertAlmostEqual(linear_to_srgb1(float(got) / (1 + sh.emissive_boost)) * 255.0, w * 255.0, delta=0.5)

    def test_ink_scale(self):
        """R3 revised: InkScale 1.5 gives exactly 1.5 x the width; at 2125 cm, T.y = tan(17.5 deg) x 9 / 16, Px 3.5
        the dilation is 3.5 px at 720 rows (5.25 px at 1080)."""
        sh = manifest_shading()
        node = hlsl.outline_wpo_node(sh)
        ty = math.tan(math.radians(17.5)) * 9.0 / 16.0
        depth = 2125.0
        pos = (40.0, -25.0, depth)          # view space = world space here: F = +Z
        args = {"Pos": f3(pos), "N": "float3(1.f, 0.f, 0.f)", "F": "float3(0.f, 0.f, 1.f)",
                "T": f"float2(0.3f, {ty!r}f)", "Px": "3.5f", "Baked": "0.f", "Enable": "1.f"}
        body = f"""
float3 a = {cpp_call(node, dict(args, InkScale="1.f", ExtraPx1080="0.f"))};
float3 b = {cpp_call(node, dict(args, InkScale="1.5f", ExtraPx1080="0.f"))};
float3 old = AF_OutlineWPO({args["Pos"]}, {args["N"]}, {args["F"]}, {args["T"]}, 3.5f, 0.f, 1.f);
std::printf("%.9g %.9g %.9g\\n", length(a), length(b), length(old));"""
        la, lb, lold = (float(x) for x in self.run_cpp([node], body)[0].split())
        self.assertEqual(la, lold, "InkScale 1 / ExtraPx1080 0 = the old formula")
        self.assertAlmostEqual(lb / la, 1.5, places=6)
        # px at 720 rows: the projected width over the 720-row view height at this depth (2 depth T.y cm)
        dist = math.sqrt(sum(x * x for x in pos))
        cos_lat = depth / dist           # the dilation is perpendicular to the view ray, not to +Z
        px720 = lb * cos_lat * 720.0 / (2.0 * depth * ty)
        self.assertAlmostEqual(px720, 3.5, delta=0.01)
        self.assertAlmostEqual(hlsl.outline_px_at(3.5, 720.0), 3.5)
        self.assertAlmostEqual(hlsl.outline_px_at(3.5, 1080.0), 5.25)

    def test_selection_rim(self):
        """R16: hero rim = (3.5 x 1.5 + 2 x 1.5) px at 1080 = 5.5 px at 720 beyond the silhouette; the 30 cm push keeps
        the projected position (< 1e-4) and moves the vertex 30 cm deeper; BodyMask 1 draws nothing."""
        sh = manifest_shading()
        wpo, push, mask = hlsl.outline_wpo_node(sh), hlsl.view_push_node(), hlsl.select_rim_mask_node()
        ty = math.tan(math.radians(17.5)) * 9.0 / 16.0
        pos = (-180.0, 95.0, 2125.0)
        v = norm(pos)
        side = norm((v[2], 0.0, -v[0]))     # perpendicular to the view ray
        body = f"""
float3 pos = {f3(pos)};
float3 w = {cpp_call(wpo, {"Pos": "pos", "N": f3(side), "F": "float3(0.f, 0.f, 1.f)", "T": f"float2(0.3f, {ty!r}f)",
                            "Px": "3.5f", "Baked": "0.f", "Enable": "1.f", "InkScale": "1.5f",
                            "ExtraPx1080": "2.f * 1.5f"})};
float3 t = AF_ViewPush(pos, w, 30.f);
float3 a = pos + w, b = pos + t;
std::printf("%.9g %.9g %.9g %.9g %.9g\\n", length(w), a.x / a.z - b.x / b.z, a.y / a.z - b.y / b.z,
            length(b) - length(a), AF_ViewPush(pos, float3(0.f, 0.f, 0.f), 30.f).z);
Parameters.SvPosition = float4(1.5f, 2.5f, 0.f, 1.f);
std::printf("%.9g %.9g %.9g %.9g\\n", AF_SelectRimMask(0.f, 1.f, 0.f), AF_SelectRimMask(0.f, 1.f, 1.f),
            AF_SelectRimMask(1.f, 1.f, 0.f), AF_SelectRimMask(0.f, 0.f, 0.f));"""
        lines = self.run_cpp([wpo, push, mask], body)
        width, dx, dy, deeper, _ = (float(x) for x in lines[0].split())
        self.assertAlmostEqual(width * 720.0 / (2.0 * 2125.0 * ty), 5.5, delta=0.01)
        self.assertLess(abs(dx), 1e-4)
        self.assertLess(abs(dy), 1e-4)
        self.assertAlmostEqual(deeper, 30.0, delta=1e-2)
        self.assertEqual([float(x) for x in lines[1].split()], [1.0, 0.0, 0.0, 0.0])

    def test_selection_rim_ground_clamp(self):
        """R16: near the feet the push stops RIM_GROUND_MARGIN_CM above the owner's ground plane (the rim closes round
        the boots / a slime instead of sinking under the terrain); higher up it is the full 30 cm. W1 camera: pitch -50,
        yaw 45, 2125 cm to a focus 50 cm above the feet; translated world (camera at the origin, z up)."""
        push_node = hlsl.view_push_node()
        pitch, yaw = math.radians(50.0), math.radians(45.0)
        fwd = (math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), -math.sin(pitch))
        feet = tuple(f * 2125.0 for f in fwd[:2]) + (fwd[2] * 2125.0 - 50.0,)
        side = (-math.sin(yaw), math.cos(yaw), 0.0)
        margin = hlsl.RIM_GROUND_MARGIN_CM
        cases = []
        for h in (0.5, 10.0, 20.0, 120.0):        # silhouette heights over the feet (cm)
            p = tuple(fc + 25.0 * sc for fc, sc in zip(feet, side))
            cases.append((h, (p[0], p[1], p[2] + h)))
        body = "\n".join(f"""{{
float3 pos = {f3(p)};
float3 off = float3(1.5f, -0.75f, -0.5f);
float3 t = AF_ViewPush(pos, off, 30.f, {f3(feet)});
float3 a = pos + off, b = pos + t;
float3 na = a / length(a), nb = b / length(b);
std::printf("%.9g %.9g %.9g %.9g\\n", length(t - off), b.z - {feet[2]!r}f, length(na - nb), length(b) - length(a));
}}""" for _, p in cases)
        lines = self.run_cpp([push_node], body)
        for (h, _), line in zip(cases, lines):
            push, above, ray_err, deeper = (float(x) for x in line.split())
            self.assertLess(ray_err, 1e-6, h)               # the projected position never moves
            self.assertAlmostEqual(deeper, push, delta=1e-2)
            self.assertGreaterEqual(above, min(h - 0.5, margin) - 1e-2, h)   # never below the ground plane (+ margin)
            if h - 0.5 <= margin:
                self.assertAlmostEqual(push, 0.0, delta=1e-4)
            elif h >= 60.0:
                self.assertAlmostEqual(push, 30.0, delta=1e-3)
            else:
                self.assertAlmostEqual(above, margin, delta=1e-2)   # pushed exactly down to the margin
                self.assertLess(push, 30.0)

    def test_ground_grade(self):
        """R19: GroundSaturation / GroundValue 1 reproduce today's terrain bit-for-bit (both nodes, both tiers); a plains
        grass texel (112, 156, 61) graded .85 / .93 matches the Python reference within 1/255."""
        nodes = [hlsl.terrain_node(True), hlsl.terrain_node(False), hlsl.terrain_painted_node(True),
                 hlsl.terrain_painted_node(False)]
        legacy = [dataclasses.replace(nd, key=nd.key + "Old", code=nd.code.replace(hlsl.GROUND_GRADE_CODE, ""))
                  for nd in nodes]
        for nd, old in zip(nodes, legacy):
            self.assertNotEqual(nd.code, old.code, nd.key)
        texels = []
        for row in range(4):
            for col in range(6):
                paint = 3 if (col, row) == (1, 2) else (0 if col < 3 else (1 if row < 2 else 5))
                texels.append(f"float4({paint}.f / 255.f, {(col * 7 + row * 3) % 12}.f / 255.f, 1.f, {paint}.f / 255.f)")
        ranks = {0: 4, 1: 1, 2: 2, 3: 0, 5: 2}
        proc = []
        for t in hlsl.TERRAIN_TILES:
            proc += ["float3(0.17f, 0.37f, 0.06f)", "float3(0.2f, 0.4f, 0.07f)", "float3(0.15f, 0.33f, 0.05f)",
                     "float3(0.6f, 0.75f, 0.3f)", "float3(0.9f, 0.8f, 0.2f)", f"{ranks[t]}.f"]
        grass = [srgb_to_linear1(x / 255.0) for x in (112, 156, 61)]
        painted = []
        for t in hlsl.TERRAIN_TILES:
            painted += [f3(grass) if t == 0 else "float3(0.5f, 0.3f, 0.12f)", "float3(0.6f, 0.75f, 0.3f)",
                        f"{ranks[t]}.f"]
        water = "float3(0.15f, 0.55f, 0.6f), float3(0.85f, 0.95f, 0.9f), float3(0.27f, 0.18f, 0.07f)"
        proc_args, painted_args = ", ".join(proc), ", ".join(painted)
        body = f"""
Texture2D T; T.Width = 6; T.Height = 4;
T.Texels = {{ {", ".join(texels)} }};
Texture2D D; D.Width = 4; D.Height = 4;
D.Texels = std::vector<float4>(16, float4(0.8f, 0.1f, 0.1f, 0.6f));
Texture2D G; G.Width = 2; G.Height = 2;
G.Texels = std::vector<float4>(4, float4(0.f, 0.f, 1.f, 0.f));
SamplerState S;
int diff = 0, n = 0;
for (int j = 0; j <= 40; ++j) for (int i = 0; i <= 60; ++i) {{
    float2 uv(-0.5f + i * 0.1f, -0.5f + j * 0.1f);
    float wall = (i % 7) * 0.15f, skirt = (j % 5) * 0.2f;
    float3 r[8] = {{
        AF_Terrain(uv, wall, skirt, T, S, 6.f, 4.f, {proc_args}, {water}, 1.f, 1.f),
        AF_TerrainOld(uv, wall, skirt, T, S, 6.f, 4.f, {proc_args}, {water}),
        AF_TerrainLow(uv, wall, skirt, T, S, 6.f, 4.f, {proc_args}, {water}, 1.f, 1.f),
        AF_TerrainLowOld(uv, wall, skirt, T, S, 6.f, 4.f, {proc_args}, {water}),
        AF_TerrainPainted(uv, wall, skirt, T, S, 6.f, 4.f, {painted_args}, {water}, D, S, 1.f, 1.f),
        AF_TerrainPaintedOld(uv, wall, skirt, T, S, 6.f, 4.f, {painted_args}, {water}, D, S),
        AF_TerrainPaintedLow(uv, wall, skirt, T, S, 6.f, 4.f, {painted_args}, {water}, D, S, 1.f, 1.f),
        AF_TerrainPaintedLowOld(uv, wall, skirt, T, S, 6.f, 4.f, {painted_args}, {water}, D, S) }};
    for (int k = 0; k < 8; k += 2) diff += std::memcmp(&r[k], &r[k + 1], sizeof(float3)) != 0;
    n += 4;
}}
float3 g = AF_TerrainPainted(float2(0.f, 0.f), 0.f, 0.f, G, S, 2.f, 2.f, {painted_args}, {water}, D, S, 0.85f, 0.93f);
std::printf("%d %d %.9g %.9g %.9g\\n", diff, n, g.x, g.y, g.z);"""
        out = self.run_cpp(nodes + legacy, body)[0].split()
        self.assertEqual(int(out[0]), 0, "GroundSaturation / GroundValue 1 changed the terrain")
        self.assertGreater(int(out[1]), 0)
        want = hlsl.sat_value_reference((112 / 255, 156 / 255, 61 / 255), 0.85, 0.93)
        got = [linear_to_srgb1(float(x)) * 255.0 for x in out[2:]]
        for g_, w in zip(got, want):
            self.assertAlmostEqual(g_, w * 255.0, delta=1.0)
        self.assertAlmostEqual(want[0] * 255.0, 106.95, delta=0.05)    # (lum + (c - lum) .85) .93, lum = 132.0

    def test_ao_composite(self):
        node = hlsl.ao_composite_node()
        body = """
float3 a = AF_AOComposite(float4(0.5f, 0.4f, 0.2f, 1.f), float4(0.2f, 0.f, 0.f, 0.f), 0.25f);
float3 b = AF_AOComposite(float4(0.5f, 0.4f, 0.2f, 1.f), float4(0.2f, 0.f, 0.f, 0.f), 0.f);
std::printf("%.6f %.6f %.6f %.6f %.6f %.6f\\n", a.x, a.y, a.z, b.x, b.y, b.z);"""
        got = [float(x) for x in self.run_cpp([node], body)[0].split()]
        for g_, w in zip(got, (0.4, 0.32, 0.16, 0.5, 0.4, 0.2)):
            self.assertAlmostEqual(g_, w, places=5)

    def test_ao_composite_ground_only(self):
        """P14: AO only where the visible surface is the terrain (scene depth reaches the custom depth the terrain alone
        writes); a character / prop / rim in front of it, or no custom depth (far plane), keeps its colour."""
        node = hlsl.ao_composite_node()
        tol, fade = hlsl.AO_GROUND_TOLERANCE_CM, hlsl.AO_GROUND_FADE_CM
        gaps = (0.0,                # the ground itself
                tol * 0.5,          # within the tolerance (ground under a boot sole)
                tol + fade * 0.5,   # half way through the fade
                35.0,               # a character / rim 35 cm in front of the ground
                6.5e4 - 2125.0)     # no terrain in custom depth (far)
        body = "\n".join(f"""{{ float3 c = AF_AOComposite(float4(0.5f, 0.4f, 0.2f, 1.f), float4(0.2f, 0.f, 0.f, 0.f),
    0.25f, float4(2125.f, 0.f, 0.f, 0.f), float4({2125.0 + gap!r}f, 0.f, 0.f, 0.f)); std::printf("%.6f\\n", c.x); }}"""
                          for gap in gaps)
        got = [float(x) for x in self.run_cpp([node], body)]
        for gap, g_ in zip(gaps, got):
            ground = 1.0 - min(max((gap - tol) / fade, 0.0), 1.0)
            self.assertAlmostEqual(g_, 0.5 * (1.0 - 0.8 * 0.25 * ground), places=5, msg=gap)
        self.assertAlmostEqual(got[0], 0.4, places=5)
        self.assertAlmostEqual(got[3], 0.5, places=5)
        self.assertAlmostEqual(got[4], 0.5, places=5)


class ArtPassLintTests(unittest.TestCase):
    def test_split_node_shape(self):
        split = hlsl.toon_split_node(manifest_shading())
        self.assertEqual([o.name for o in split.additional_outputs], ["Lit"])
        self.assertEqual(split.input_names[:len(hlsl.TOON_INPUTS)], [i.name for i in hlsl.TOON_INPUTS])
        self.assertEqual(split.input_names[len(hlsl.TOON_INPUTS):],
                         ["Pw", "CastShadowStrength"] + [f"L{i}" for i in range(8)] + [f"C{i}" for i in range(8)])
        self.assertTrue(split.code.rstrip().endswith("return Emit;"))
        self.assertEqual(split.code.count("if (C"), 8, "eight unrolled light slots")
        # the old inputs come first, so the existing material wiring and calls keep their order
        self.assertEqual(hlsl.OUTLINE_WPO_INPUTS[-2:], (hlsl.NodeInput("InkScale", "float", "1.f"),
                                                        hlsl.NodeInput("ExtraPx1080", "float", "0.f")))
        with self.assertRaises(ValueError):
            split.twin("Nope")


# ---------------------------------------------------------------------------------------------------------------------
# M_AF_Telegraph (dark remake U2 / 2G)
# ---------------------------------------------------------------------------------------------------------------------
RULES_HEADER = HERE.parent.parent / "Source" / "Abyssfire" / "Private" / "Vfx" / "AbyssTelegraphRules.h"


def runtime_cpd() -> dict[str, int]:
    """AbyssTelegraphRules.h `namespace Cpd { inline constexpr int <Name> = <n>; }` (the runtime's writer)."""
    text = RULES_HEADER.read_text(encoding="utf-8")
    block = text[text.index("namespace Cpd"):]
    block = block[:block.index("}")]
    return {m.group(1): int(m.group(2)) for m in re.finditer(r"inline constexpr int (\w+) = (\d+);", block)}


def telegraph_quad(shape: int, a: float, b: float, margin: float) -> tuple[float, float, float, float]:
    """(SizeX, SizeY, LocalOriginX, LocalOriginY) of AbyssTelegraphRules::MakeGeometry (the shape about +X)."""
    if shape in (0, 1):
        lo_x, hi_x, lo_y, hi_y = -a, a, -a, a
    elif shape == 2:
        hi_x = a
        lo_x = -a if b >= math.pi - 1e-12 else min(0.0, a * math.cos(b))
        half_y = a if b >= math.pi / 2 else a * math.sin(b)
        lo_y, hi_y = -half_y, half_y
    else:
        lo_x, hi_x, lo_y, hi_y = 0.0, a, -b, b
    lo_x, hi_x, lo_y, hi_y = lo_x - margin, hi_x + margin, lo_y - margin, hi_y + margin
    return hi_x - lo_x, hi_y - lo_y, -(lo_x + hi_x) / 2.0, -(lo_y + hi_y) / 2.0


def telegraph_uv(quad: tuple[float, float, float, float], x: float, y: float) -> tuple[float, float]:
    """The quad UV of shape-frame point (x along, y across; cm): SM_FX_Quad UV (0,0) at (-50, +50)."""
    sx, sy, ox, oy = quad
    return (x + ox) / sx + 0.5, 0.5 - (y + oy) / sy


RIM_W, RIM_H, FILL_N = 8, 64, 4


def rim_texel(col: int, row: int) -> tuple[float, float, float, float]:
    """The synthetic rim strip: colour by column, alpha fading down the rows (row 0 = the boundary)."""
    g = 0.6 + 0.05 * col
    return g, g, g, max(0.0, 1.0 - row / (RIM_H - 1))


def fill_texel(col: int, row: int) -> tuple[float, float, float, float]:
    v = 0.66 + 0.02 * ((col * 3 + row * 5) % 17)
    return v, v, v, 1.0


def nearest(texel, w: int, h: int, u: float, v: float):
    x = min(max(int(math.floor(u * w)), 0), w - 1)
    y = min(max(int(math.floor(v * h)), 0), h - 1)
    return texel(x, y)


def telegraph_reference(shape: int, a: float, b: float, rim_w: float, x: float, y: float, inner: tuple, mode: tuple,
                        alphas: tuple, world_xy: tuple, look: dict) -> tuple[float, float, float, float]:
    """Art/blender/vfx/fx_preview_dark.py telegraph_layer for one pixel, with the per-frame state given as the custom
    primitive data (the runtime's pulses / fades / flash already applied) and nearest-texel sampling (as the emulation).
    Points are chosen where the preview's shape measure equals the exact one (inside, or well outside)."""
    r = math.hypot(x, y)
    if shape == 0:
        w, u_edge, fill_r, fill_max = a - r, math.atan2(y, x) * a, r, a
    elif shape == 1:
        w, u_edge, fill_r, fill_max = min(a - r, r - b), math.atan2(y, x) * a, r - b, a - b
    elif shape == 2:
        ang = math.atan2(y, x)
        side = min(abs(x * math.sin(b) - y * math.cos(b)), abs(x * math.sin(-b) - y * math.cos(-b)))
        inside = abs(ang) <= b and r <= a
        w = min(a - r, side) if inside else -1.0
        u_edge = ang * a if a - r < side else r
        fill_r, fill_max = r, a
    else:
        inside = 0 <= x <= a and abs(y) <= b
        ends, sides = min(x, a - x), b - abs(y)
        w = min(ends, sides) if inside else -1.0
        u_edge = x if sides < ends else y
        fill_r, fill_max = x, a
    fill_on, outside, flash, prog = mode
    fill_alpha, front_alpha, rim_alpha, _ = alphas
    if outside > 0.5:
        w = -w
    v = min(max(w / rim_w, 0.0), 0.999)
    aa = min(max(w + 0.5, 0.0), 1.0)
    rs = nearest(rim_texel, RIM_W, RIM_H, (u_edge / look["RimTileCm"]) % 1.0, v)
    rim_a = rs[3] * rim_alpha * aa * (1.0 if w <= rim_w else 0.0)
    rim_c = [look["RimColor"][i] + (look["FlashRimColor"][i] - look["RimColor"][i]) * flash for i in range(3)]
    rim_rgb = [rs[i] * rim_c[i] for i in range(3)]
    if inner[0] > 0:
        w_in = -w if outside > 0.5 else w
        di = abs(w_in - inner[1])
        rim_a = max(rim_a, min(max(1.0 - (di - inner[2] / 2) / 1.5, 0.0), 1.0) * inner[0] * (1.0 if w_in > 0 else 0.0))
    fill_rgb, fill_a = [0.0, 0.0, 0.0], 0.0
    if outside > 0.5:
        band = inner[3]
        wash_out = min(max((band - w) / 100.0, 0.0), 1.0) if band > 0 else 0.0
        fill_rgb = list(look["WashColor"])
        fill_a = fill_alpha * min(max(w / look["WashFadeInCm"], 0.0), 1.0) * wash_out * (1.0 if w > 0 else 0.0)
    elif fill_on > 0.5:
        fs = nearest(fill_texel, FILL_N, FILL_N, (world_xy[0] / look["FillTileCm"]) % 1.0,
                     (world_xy[1] / look["FillTileCm"]) % 1.0)
        grain = [(1.0 - look["FillGrain"] * (1.0 - fs[i])) * look["FillColor"][i] for i in range(3)]
        front = prog * fill_max
        filled = w > 0 and fill_r <= front
        band = filled and fill_r >= front - look["FrontWidthCm"]
        mix = max(1.0 if band else 0.0, flash)
        fill_rgb = [grain[i] + (look["FrontColor"][i] - grain[i]) * mix for i in range(3)]
        fill_a = (front_alpha if band else fill_alpha) if filled else 0.0
    alpha = rim_a + fill_a * (1 - rim_a)
    rgb = [(rim_rgb[i] * rim_a + fill_rgb[i] * fill_a * (1 - rim_a)) / max(alpha, 1e-6) for i in range(3)]
    return rgb[0], rgb[1], rgb[2], min(max(alpha, 0.0), 1.0)


class TelegraphContractTests(unittest.TestCase):
    def test_cpd_layout_is_the_runtime_header(self):
        cpd = runtime_cpd()
        self.assertEqual(cpd["Count"], 20)
        for group, start in hlsl.TELEGRAPH_CPD.items():
            for channel, name in enumerate(hlsl.TELEGRAPH_CPD_FIELDS[group]):
                with self.subTest(group=group, name=name):
                    self.assertEqual(cpd[name], start + channel)
        self.assertEqual(sorted(hlsl.TELEGRAPH_CPD.values()), [0, 4, 8, 12, 16])
        self.assertEqual(len(set(cpd) - {"Count"}), cpd["Count"])
        # the wash band's fade-out is the runtime's ArenaWashFadeOutCm, and its built-in contract colours are ours
        rules = RULES_HEADER.read_text(encoding="utf-8")
        from abyss_content.manifest import TelegraphContract
        t = TelegraphContract()
        for field, value in (("FillRgb", t.fill_color), ("RimRgb", t.rim_color), ("FrontRgb", t.front_color),
                             ("FlashRimRgb", t.flash_rim_color)):
            self.assertRegex(rules, rf"uint32_t {field} = 0x{value[1:]};")
        self.assertRegex(rules, rf"double FillTileCm = {t.fill_tile_cm:g}\.0;")
        self.assertRegex(rules, rf"double RimTileCm = {t.rim_tile_cm:g}\.0;")
        self.assertRegex(rules, rf"ArenaWashFadeOutCm = {hlsl.TELEGRAPH_WASH_FADE_OUT_CM:g}\.0;")

    def test_rim_strip_rows(self):
        from abyss_content import pngio
        from abyss_content.manifest import build_plan
        plan = build_plan(scan_textures=False)
        self.assertTrue(plan.telegraph.present)
        rim = plan.texture(plan.telegraph.rim_texture)
        fill = plan.texture(plan.telegraph.fill_texture)
        self.assertIsNotNone(rim, "T_FX_TelegraphRim is planned even without the scan")
        self.assertIsNotNone(fill)
        info = pngio.read_png_info(rim.source)
        self.assertEqual(info.height, hlsl.TELEGRAPH_RIM_ROWS)
        self.assertEqual(rim.kind, "fx")
        self.assertFalse(rim.srgb)
        self.assertFalse(fill.srgb)

    def test_node_shape(self):
        node = hlsl.telegraph_node()
        self.assertEqual(node.output, "float4")
        self.assertIn("Rim.SampleGrad(RimSampler", node.code)
        self.assertIn("Fill.SampleGrad(FillSampler", node.code)
        # derivatives before the first branch (explicit gradients from continuous coordinates)
        before = node.code.split("if (")[0]
        for d in ("ddx(p)", "ddy(p)", "ddx(fw)", "ddy(fw)"):
            self.assertIn(d, before)
        self.assertIn(hlsl.lit(0.5 / hlsl.TELEGRAPH_RIM_ROWS), node.code)


@unittest.skipIf(CXX is None, "no C++ compiler")
class TelegraphMathTests(unittest.TestCase):
    @staticmethod
    def look() -> dict:
        from abyss_content.manifest import build_plan, hex_to_linear
        t = build_plan(scan_textures=False).telegraph
        return {"FillColor": hex_to_linear(t.fill_color), "RimColor": hex_to_linear(t.rim_color),
                "FrontColor": hex_to_linear(t.front_color), "FlashRimColor": hex_to_linear(t.flash_rim_color),
                "WashColor": hex_to_linear(t.wash_color), "FillGrain": t.fill_grain, "FillTileCm": t.fill_tile_cm,
                "FrontWidthCm": t.front_width_cm, "RimTileCm": t.rim_tile_cm, "WashFadeInCm": t.wash_fade_in_cm}

    def test_telegraph_matches_the_preview(self):
        look = self.look()
        cases = []
        # (shape, a, b, rim width, inner (alpha, inset, width, wash band), mode (fill, outside, flash, progress),
        #  alphas (fill, front, rim, spare), points (x, y) in the shape frame)
        normal = ((0.0, 0.0, 0.0, 0.0), (1.0, 0.0, 0.0, 0.5), (0.145, 0.85, 0.95, 0.0))
        circle_pts = [(0.0, 0.0), (50.0, 30.0), (98.0, 0.0), (101.0, 0.0), (0.0, 150.0), (150.0, 100.0), (215.0, 0.0),
                      (-180.0, 60.0), (199.0, 3.0), (130.0, -140.0), (250.0, 10.0)]
        cases.append((0, 220.0, 0.0, 26.0) + normal + (circle_pts,))
        # the boss style: wider rim, the inner rim at .82 of the radius
        cases.append((0, 220.0, 0.0, 36.0, (0.7, 0.18 * 220.0, 12.0, 0.0), (1.0, 0.0, 0.0, 0.75),
                      (0.22, 0.85, 0.8, 0.0), circle_pts))
        # the contact flash: the front colour over everything inside, the rim red
        cases.append((0, 220.0, 0.0, 26.0, (0.0, 0.0, 0.0, 0.0), (1.0, 0.0, 1.0, 1.0), (0.35, 0.35, 0.95, 0.0),
                      circle_pts))
        # a ring
        cases.append((1, 300.0, 150.0, 26.0) + normal + ([(160.0, 0.0), (225.0, 0.0), (0.0, 290.0), (100.0, 0.0),
                                                          (-200.0, -150.0), (320.0, 0.0)],))
        # a cone of 120 degrees (points inside, and well outside the opening)
        cases.append((2, 260.0, math.pi / 3.0, 30.0, (0.6, 0.18 * 260.0, 10.0, 0.0), (1.0, 0.0, 0.0, 0.6),
                      (0.16, 0.85, 0.95, 0.0), [(100.0, 0.0), (240.0, 10.0), (60.0, 90.0), (150.0, -200.0),
                                                (20.0, 5.0), (-50.0, 0.0), (10.0, 200.0), (255.0, 0.0)]))
        # a line (Breach-seep's windup): inside, the ends, far outside
        cases.append((3, 800.0, 50.0, 36.0, (0.7, 0.18 * 50.0, 12.0, 0.0), (1.0, 0.0, 0.0, 0.4),
                      (0.15, 0.85, 0.95, 0.0), [(10.0, 0.0), (400.0, 20.0), (319.0, -45.0), (790.0, 0.0), (500.0, 49.0),
                                                (400.0, 120.0), (-60.0, 0.0), (900.0, 0.0)]))
        # a hazard zone: rim only
        cases.append((3, 800.0, 50.0, 26.0, (0.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.6, 0.0),
                      [(10.0, 0.0), (400.0, 40.0), (400.0, 0.0)]))
        # the arena: rim outward, the wash ramping in and fading out over its band
        cases.append((0, 900.0, 0.0, 26.0, (0.0, 0.0, 0.0, 250.0), (0.0, 1.0, 0.0, 1.0), (0.1, 0.0, 0.85, 0.0),
                      [(0.0, 0.0), (850.0, 0.0), (905.0, 0.0), (930.0, 0.0), (990.0, 0.0), (1080.0, 0.0),
                       (1140.0, 0.0), (0.0, -1100.0)]))
        lines = []
        expected = []
        world_base = (12345.0, 6789.0)
        for case in cases:
            shape, a, b, rim_w, inner, mode, alphas, pts = case
            quad = telegraph_quad(shape, a, b, 300.0)
            for (x, y) in pts:
                u, v = telegraph_uv(quad, x, y)
                wx, wy = world_base[0] + x, world_base[1] + y
                args = (f"float2({u!r}f, {v!r}f), float3({wx!r}f, {wy!r}f, 0.f), "
                        f"float4({shape}.f, {a!r}f, {b!r}f, {rim_w!r}f), "
                        f"float4({quad[0]!r}f, {quad[1]!r}f, {quad[2]!r}f, {quad[3]!r}f), "
                        f"float4({inner[0]!r}f, {inner[1]!r}f, {inner[2]!r}f, {inner[3]!r}f), "
                        f"float4({mode[0]!r}f, {mode[1]!r}f, {mode[2]!r}f, {mode[3]!r}f), "
                        f"float4({alphas[0]!r}f, {alphas[1]!r}f, {alphas[2]!r}f, {alphas[3]!r}f), "
                        + ", ".join(f"float3({c[0]!r}f, {c[1]!r}f, {c[2]!r}f)" for c in (
                            look["FillColor"], look["RimColor"], look["FrontColor"], look["FlashRimColor"],
                            look["WashColor"]))
                        + f", {look['FillGrain']!r}f, {look['FillTileCm']!r}f, {look['FrontWidthCm']!r}f, "
                        f"{look['RimTileCm']!r}f, {look['WashFadeInCm']!r}f, rim, ss, fill, ss")
                lines.append(f"    {{ float4 o = AF_Telegraph({args}); std::printf(\"%.6f %.6f %.6f %.6f\\n\", o.x, o.y, o.z, o.w); }}")
                expected.append(telegraph_reference(shape, a, b, rim_w, x, y, inner, mode, alphas, (wx, wy), look))
        rim_texels = ", ".join(
            f"float4({c[0]!r}f, {c[1]!r}f, {c[2]!r}f, {c[3]!r}f)"
            for c in (rim_texel(x, y) for y in range(RIM_H) for x in range(RIM_W)))
        fill_texels = ", ".join(
            f"float4({c[0]!r}f, {c[1]!r}f, {c[2]!r}f, {c[3]!r}f)"
            for c in (fill_texel(x, y) for y in range(FILL_N) for x in range(FILL_N)))
        main = ("int main() {\n"
                f"    Texture2D rim; rim.Width = {RIM_W}; rim.Height = {RIM_H}; rim.Texels = {{ {rim_texels} }};\n"
                f"    Texture2D fill; fill.Width = {FILL_N}; fill.Height = {FILL_N}; fill.Texels = {{ {fill_texels} }};\n"
                "    SamplerState ss;\n" + "\n".join(lines) + "\n    return 0;\n}")
        out = compile_and_run(cpp_unit([hlsl.telegraph_node()], main), run=True).split("\n")
        rows = [list(map(float, line.split())) for line in out if line.strip()]
        self.assertEqual(len(rows), len(expected))
        drawn = 0
        for got, want in zip(rows, expected):
            with self.subTest(want=want):
                self.assertAlmostEqual(got[3], want[3], places=4)
                if want[3] > 1e-4:
                    drawn += 1
                    for i in range(3):
                        self.assertAlmostEqual(got[i], want[i], places=3)
        self.assertGreater(drawn, 20)
        # spot checks of the look: the circle's centre is a translucent bone fill (progress .5: inside the front)
        centre = rows[0]
        self.assertAlmostEqual(centre[3], 0.145, places=3)
        self.assertGreater(centre[0], 0.6)
        # beyond the radius: nothing (no square quad edge)
        self.assertEqual(rows[len(circle_pts) - 1][3], 0.0)

    def test_telegraph_early_exit_is_exact(self):
        """The blank-pixel early exit (2G review m2) changes no pixel: the node with and without it agree on a dense UV
        grid for every shape and mode (the arena with and without an inner rim, a ring's hole, a zone, a boss strike),
        and the arena's inside is blank."""
        look = self.look()
        node = hlsl.telegraph_node()
        exit_block = ("if (wr < -0.5 * px && (outside < 0.5 || InnerP.r <= 0.0))\n{\n"
                      "    return float4(0.0, 0.0, 0.0, 0.0);\n}\n")
        self.assertEqual(node.code.count(exit_block), 1)
        full = dataclasses.replace(node, key="AF_TelegraphNoExit", code=node.code.replace(exit_block, ""))
        normal = ((0.0, 0.0, 0.0, 0.0), (1.0, 0.0, 0.0, 0.6), (0.145, 0.85, 0.95, 0.0))
        cases = [
            (0, 220.0, 0.0, 36.0, (0.7, 0.18 * 220.0, 12.0, 0.0), (1.0, 0.0, 0.0, 0.75), (0.22, 0.85, 0.8, 0.0), 4.0),
            (1, 300.0, 150.0, 26.0) + normal + (4.0,),
            (2, 260.0, math.pi / 3.0, 30.0) + normal + (4.0,),
            (3, 800.0, 50.0, 26.0, (0.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.6, 0.0), 4.0),
            (0, 900.0, 0.0, 26.0, (0.0, 0.0, 0.0, 250.0), (0.0, 1.0, 0.0, 1.0), (0.1, 0.0, 0.85, 0.0), 284.0),
            (0, 900.0, 0.0, 26.0, (0.0, 0.0, 0.0, 0.0), (0.0, 1.0, 0.0, 1.0), (0.0, 0.0, 0.85, 0.0), 34.0),
            (0, 900.0, 0.0, 26.0, (0.6, 160.0, 10.0, 250.0), (0.0, 1.0, 0.0, 1.0), (0.1, 0.0, 0.85, 0.0), 284.0),
        ]
        steps = 37
        rows = []
        for shape, a, b, rim_w, inner, mode, alphas, margin in cases:
            quad = telegraph_quad(shape, a, b, margin)
            rows.append("{ " + ", ".join(f"{x!r}f" for x in (float(shape), a, b, rim_w, *quad, *inner, *mode, *alphas))
                        + " }")
        colours = ", ".join(f"float3({c[0]!r}f, {c[1]!r}f, {c[2]!r}f)" for c in (
            look["FillColor"], look["RimColor"], look["FrontColor"], look["FlashRimColor"], look["WashColor"]))
        consts = (f"{look['FillGrain']!r}f, {look['FillTileCm']!r}f, {look['FrontWidthCm']!r}f, "
                  f"{look['RimTileCm']!r}f, {look['WashFadeInCm']!r}f")
        rim_texels = ", ".join(
            f"float4({c[0]!r}f, {c[1]!r}f, {c[2]!r}f, {c[3]!r}f)"
            for c in (rim_texel(x, y) for y in range(RIM_H) for x in range(RIM_W)))
        fill_texels = ", ".join(
            f"float4({c[0]!r}f, {c[1]!r}f, {c[2]!r}f, {c[3]!r}f)"
            for c in (fill_texel(x, y) for y in range(FILL_N) for x in range(FILL_N)))
        main = ("int main() {\n"
                f"    Texture2D rim; rim.Width = {RIM_W}; rim.Height = {RIM_H}; rim.Texels = {{ {rim_texels} }};\n"
                f"    Texture2D fill; fill.Width = {FILL_N}; fill.Height = {FILL_N}; fill.Texels = {{ {fill_texels} }};\n"
                "    SamplerState ss;\n"
                f"    static const float cases[{len(rows)}][20] = {{ {', '.join(rows)} }};\n"
                f"    const int steps = {steps};\n"
                f"    for (int c = 0; c < {len(rows)}; ++c) for (int j = 0; j < steps; ++j) for (int i = 0; i < steps; ++i) {{\n"
                "        const float* k = cases[c];\n"
                "        const float u = (i + 0.5f) / steps, v = (j + 0.5f) / steps;\n"
                "        float4 o = AF_Telegraph(float2(u, v), float3(u * k[4], v * k[5], 0.f), float4(k[0], k[1], k[2], k[3]),\n"
                "            float4(k[4], k[5], k[6], k[7]), float4(k[8], k[9], k[10], k[11]), float4(k[12], k[13], k[14], k[15]),\n"
                f"            float4(k[16], k[17], k[18], k[19]), {colours}, {consts}, rim, ss, fill, ss);\n"
                "        float4 f = AF_TelegraphNoExit(float2(u, v), float3(u * k[4], v * k[5], 0.f), float4(k[0], k[1], k[2], k[3]),\n"
                "            float4(k[4], k[5], k[6], k[7]), float4(k[8], k[9], k[10], k[11]), float4(k[12], k[13], k[14], k[15]),\n"
                f"            float4(k[16], k[17], k[18], k[19]), {colours}, {consts}, rim, ss, fill, ss);\n"
                "        std::printf(\"%a %a %a %a | %a %a %a %a\\n\", o.x, o.y, o.z, o.w, f.x, f.y, f.z, f.w);\n"
                "    }\n    return 0;\n}")
        out = [line for line in compile_and_run(cpp_unit([node, full], main), run=True).splitlines() if line.strip()]
        self.assertEqual(len(out), len(cases) * steps * steps)
        differing = [n for n, line in enumerate(out) if line.split(" | ")[0] != line.split(" | ")[1]]
        self.assertEqual(differing, [], f"{len(differing)} pixels differ, first: {out[differing[0]] if differing else ''}")
        blank = sum(1 for line in out if float.fromhex(line.split(" | ")[0].split()[3]) == 0.0)
        self.assertGreater(blank, len(out) // 3)
        # the arena without its wash: the pixel at the centre is blank
        arena = out[5 * steps * steps:6 * steps * steps]
        self.assertEqual(float.fromhex(arena[(steps // 2) * steps + steps // 2].split(" | ")[0].split()[3]), 0.0)

    def test_telegraph_progress_sweeps_from_the_centre(self):
        """The fill reaches a radius exactly when progress x radius passes it (circle, ring, line); the front band is
        FrontWidthCm wide at its edge."""
        look = self.look()
        node = hlsl.telegraph_node()
        quad = telegraph_quad(0, 220.0, 0.0, 4.0)
        calls = []
        for prog in (0.25, 0.5, 0.75, 1.0):
            for x in (45.0, 52.0, 57.0, 103.0, 107.0, 112.0, 158.0, 162.0, 168.0, 200.0, 217.0):   # 2+ cm off the edges
                u, v = telegraph_uv(quad, x, 0.0)
                calls.append((prog, x, f"AF_Telegraph(float2({u!r}f, {v!r}f), float3(0.f, 0.f, 0.f), "
                              f"float4(0.f, 220.f, 0.f, 26.f), float4({quad[0]!r}f, {quad[1]!r}f, 0.f, 0.f), "
                              f"float4(0.f, 0.f, 0.f, 0.f), float4(1.f, 0.f, 0.f, {prog!r}f), float4(0.1f, 0.85f, 0.f, 0.f), "
                              "float3(1.f, 1.f, 0.f), float3(1.f, 0.f, 0.f), float3(0.f, 0.f, 1.f), float3(1.f, 0.f, 0.f), "
                              f"float3(1.f, 0.f, 0.f), 0.f, 160.f, {look['FrontWidthCm']!r}f, 120.f, 60.f, rim, ss, fill, ss)"))
        main = ("int main() {\n    Texture2D rim; rim.Width = 1; rim.Height = 1; rim.Texels = { float4(1.f, 1.f, 1.f, 1.f) };\n"
                "    Texture2D fill; fill.Width = 1; fill.Height = 1; fill.Texels = { float4(1.f, 1.f, 1.f, 1.f) };\n"
                "    SamplerState ss;\n"
                + "\n".join(f"    {{ float4 o = {c}; std::printf(\"%.6f %.6f\\n\", o.w, o.z); }}" for _, _, c in calls)
                + "\n    return 0;\n}")
        out = [list(map(float, line.split())) for line in compile_and_run(cpp_unit([node], main), run=True).splitlines()
               if line.strip()]
        for (prog, x, _), (alpha, blue) in zip(calls, out):
            with self.subTest(progress=prog, x=x):
                front = prog * 220.0
                if x > front:
                    self.assertEqual(alpha, 0.0)          # not reached yet (the rim alpha is 0 in this call)
                elif x >= front - look["FrontWidthCm"]:
                    self.assertAlmostEqual(alpha, 0.85, places=5)   # the bright front band
                    self.assertAlmostEqual(blue, 1.0, places=5)     # FrontColor (blue here)
                else:
                    self.assertAlmostEqual(alpha, 0.1, places=5)
                    self.assertAlmostEqual(blue, 0.0, places=5)     # FillColor (yellow here) x grain 1



# ---------------------------------------------------------------------------------------------------------------------
# The ash husk (dark remake U3 / 2G stage C): AF_AshMask / AF_AshGrey against the art's preview port
# (Art/blender/vfx/fx_preview_dark.py apply_ash) and the real T_FX_AshNoise
# ---------------------------------------------------------------------------------------------------------------------
ASH_N = 4


def ash_texel(col: int, row: int) -> tuple[float, float, float, float]:
    """A synthetic 4 x 4 noise: R front, G cracks, B plates (distinct per texel)."""
    r = ((col * 5 + row * 3) % 7) / 6.0
    g = ((col * 2 + row * 7) % 5) / 4.0
    b = ((col * 3 + row * 11) % 16) / 15.0
    return r, g, b, 1.0


def ash_mask_reference(lp: tuple, dz: float, dissolve: float, top: float, tile: float, edge_w: float,
                       texel=ash_texel, n: int = ASH_N) -> tuple[float, float]:
    """apply_ash's crumble for one pixel: (keep, edge) with nearest sampling (as the emulation)."""
    u = (lp[0] / tile) % 1.0
    v = (lp[2] / tile) % 1.0
    r, g, b, _ = nearest(texel, n, n, u, v)
    hf = min(max(dz / top, 0.0), 1.0)
    w_b, w_r, w_g, w_h = hlsl.ASH_SCORE_WEIGHTS
    score = b * w_b + r * w_r + (1.0 - g) * w_g + hf * w_h
    cut = min(max(dissolve, 0.0), 1.0) * hlsl.ASH_CUT_SCALE
    if dissolve <= 0.0:
        return 1.0, 0.0
    if score < cut:
        return 0.0, 0.0
    return 1.0, 1.0 if score < cut + edge_w else 0.0


def ash_grey_reference(img: tuple, grey: float, edge: float, ash: tuple, shade: tuple, edge_c: tuple) -> list[float]:
    """apply_ash's grey (and the crumble's edge glow) on one linear colour."""
    lum = sum(c * w for c, w in zip(img, hlsl.ASH_LUMA))
    k = min(max((lum - hlsl.ASH_LUMA_FLOOR) / hlsl.ASH_LUMA_SPAN, 0.0), 1.0)
    ashed = [s + (a - s) * k for a, s in zip(ash, shade)]
    if lum < hlsl.ASH_INK_LUMA:
        ashed = [c * hlsl.ASH_INK_KEEP for c in img]
    out = [c + (a - c) * grey for c, a in zip(img, ashed)]
    e = min(max(edge, 0.0), 1.0) * hlsl.ASH_EDGE_MIX
    return [c + (ec - c) * e for c, ec in zip(out, edge_c)]


class AshContractTests(unittest.TestCase):
    def test_nodes(self):
        mask = hlsl.ash_mask_node()
        self.assertEqual([o.name for o in mask.additional_outputs], ["Edge"])
        self.assertIn("Noise.SampleGrad(NoiseSampler", mask.code)
        before = mask.code.split("?")[0]
        for d in ("ddx(nuv)", "ddy(nuv)"):
            self.assertIn(d, before, "explicit gradients from the continuous UV before any branch")
        self.assertIn("frac(nuv.x)", mask.code)   # the FX import clamps: the node wraps
        grey = hlsl.ash_grey_node()
        self.assertEqual([o.name for o in grey.additional_outputs], ["AshLit"])
        # the runtime's contract copy (AbyssCorpseRules::FAshContract) has the same defaults as the plan's
        rules = (HERE.parent.parent / "Source/Abyssfire/Private/Actors/AbyssCorpseRules.h").read_text(encoding="utf-8")
        from abyss_content.manifest import AshContract
        a = AshContract()
        for field, value in (("AshRgb", a.ash_color), ("ShadeRgb", a.ash_shade), ("EdgeRgb", a.edge_color)):
            self.assertRegex(rules, rf"uint32_t {field} = 0x{value[1:]};")
        self.assertRegex(rules, rf"double EdgeWidth = {a.edge_width:g};")
        self.assertRegex(rules, rf"double NoiseTileCm = {a.noise_tile_cm:g}\.0;")
        self.assertRegex(rules, rf'std::string NoiseTexture = "{a.noise_texture}";')

    def test_real_noise_leaves_a_husk(self):
        """On the shipped T_FX_AshNoise the crumble removes plates monotonically, leaves most of the body standing at
        crumbleTo (.38) and nothing at 1; the plates (B) decide more than the height."""
        from abyss_content import pngio
        path = HERE.parent.parent / "Art/Export/VFX/T_FX_AshNoise.png"
        if not path.is_file():
            self.skipTest("T_FX_AshNoise not exported")
        w, h, px = pngio.decode_png_rgba(path.read_bytes())

        def kept(dissolve: float) -> float:
            n = keep = 0
            for y in range(0, h, 4):
                for x in range(0, w, 4):
                    i = (y * w + x) * 4
                    for hf in (0.1, 0.5, 0.9):
                        k, _ = ash_mask_reference((x / w * 60.0, 0.0, y / h * 60.0), hf * 100.0, dissolve, 100.0, 60.0,
                                                  0.06, texel=lambda c, r, i=i: (px[i] / 255, px[i + 1] / 255,
                                                                                 px[i + 2] / 255, 1.0), n=1)
                        keep += k
                        n += 1
            return keep / n

        fractions = [kept(d) for d in (0.0, 0.1, 0.2, 0.38, 0.6, 0.8, 1.0)]
        self.assertEqual(fractions[0], 1.0)
        self.assertEqual(fractions[-1], 0.0)
        for a, b in zip(fractions, fractions[1:]):
            self.assertGreaterEqual(a, b)
        self.assertGreater(fractions[3], 0.55)   # the husk
        self.assertLess(fractions[3], 0.95)


@unittest.skipIf(CXX is None, "no C++ compiler")
class AshMathTests(unittest.TestCase):
    def test_mask_matches_the_preview(self):
        cases = []
        for lp in ((5.0, 0.0, 7.0), (31.0, 4.0, 52.0), (-12.0, 0.0, 88.0), (130.0, -9.0, -40.0), (59.0, 0.0, 1.0)):
            for dz in (0.0, 40.0, 95.0, 160.0):
                for dissolve in (0.0, 0.1, 0.38, 0.7, 1.0):
                    cases.append((lp, dz, dissolve, 100.0, 60.0, 0.06))
        texels = ", ".join(f"float4({c[0]!r}f, {c[1]!r}f, {c[2]!r}f, {c[3]!r}f)"
                           for c in (ash_texel(x, y) for y in range(ASH_N) for x in range(ASH_N)))
        calls = []
        for lp, dz, d, top, tile, ew in cases:
            calls.append(f"    {{ float e = 0.f; float k = AF_AshMask(float3({lp[0]!r}f, {lp[1]!r}f, {lp[2]!r}f), "
                         f"float3(1.f, 2.f, {dz + 500.0!r}f), float3(0.f, 0.f, 500.f), {d!r}f, {top!r}f, {tile!r}f, "
                         f"{ew!r}f, noise, ss, e); std::printf(\"%.1f %.1f\\n\", k, e); }}")
        main = (f"int main() {{\n    Texture2D noise; noise.Width = {ASH_N}; noise.Height = {ASH_N}; "
                f"noise.Texels = {{ {texels} }};\n    SamplerState ss;\n" + "\n".join(calls) + "\n    return 0;\n}")
        out = [list(map(float, line.split())) for line in
               compile_and_run(cpp_unit([hlsl.ash_mask_node()], main), run=True).splitlines() if line.strip()]
        self.assertEqual(len(out), len(cases))
        dropped = edges = 0
        for case, got in zip(cases, out):
            want = ash_mask_reference(case[0], case[1], case[2], case[3], case[4], case[5])
            with self.subTest(case=case):
                self.assertEqual(tuple(got), want)
            dropped += got[0] == 0.0
            edges += got[1] == 1.0
        self.assertGreater(dropped, 10)
        self.assertGreater(edges, 0)
        # Dissolve 0 keeps everything, 1 drops everything
        for case, got in zip(cases, out):
            if case[2] == 0.0:
                self.assertEqual(got, [1.0, 0.0])
            if case[2] == 1.0:
                self.assertEqual(got[0], 0.0)

    def test_grey_matches_the_preview_in_sun_and_in_shadow(self):
        """The split's two parts grey so that the sunlit pixel (Emit + AshLit x sun) and the shadowed one (Emit) are what
        apply_ash makes of the unshadowed and shadowed toon colours; Grey 0 without an edge changes nothing."""
        from abyss_content.manifest import AshContract, hex_to_linear
        a = AshContract()
        ash, shade, edge_c = hex_to_linear(a.ash_color), hex_to_linear(a.ash_shade), hex_to_linear(a.edge_color)
        cases = []
        for emit in ((0.30, 0.12, 0.08), (0.02, 0.02, 0.03), (0.005, 0.004, 0.006), (0.7, 0.6, 0.9), (0.0, 0.0, 0.0)):
            for lit_ in ((0.25, 0.20, 0.10), (0.0, 0.0, 0.0), (0.05, 0.08, 0.02)):
                for sun in ((1.0, 0.95, 0.85), (0.6, 0.5, 0.45), (0.02, 1.0, 1.0)):
                    for grey in (0.0, 0.5, 1.0):
                        for edge in (0.0, 1.0):
                            cases.append((emit, lit_, sun, grey, edge))
        fmt = "float3({0!r}f, {1!r}f, {2!r}f)"
        calls = []
        for emit, lit_, sun, grey, edge in cases:
            calls.append(f"    {{ float3 l = float3(0.f, 0.f, 0.f); float3 e = AF_AshGrey({fmt.format(*emit)}, "
                         f"{fmt.format(*lit_)}, float4({sun[0]!r}f, {sun[1]!r}f, {sun[2]!r}f, 1.f), {grey!r}f, "
                         f"{fmt.format(*ash)}, {fmt.format(*shade)}, {fmt.format(*edge_c)}, {edge!r}f, l); "
                         f"std::printf(\"%.7f %.7f %.7f %.7f %.7f %.7f\\n\", e.x, e.y, e.z, l.x, l.y, l.z); }}")
        main = "int main() {\n" + "\n".join(calls) + "\n    return 0;\n}"
        out = [list(map(float, line.split())) for line in
               compile_and_run(cpp_unit([hlsl.ash_grey_node()], main), run=True).splitlines() if line.strip()]
        self.assertEqual(len(out), len(cases))
        for (emit, lit_, sun, grey, edge), got in zip(cases, out):
            with self.subTest(emit=emit, lit=lit_, sun=sun, grey=grey, edge=edge):
                sun_c = [max(x, 0.05) for x in sun]
                c_sun = [e + l * s for e, l, s in zip(emit, lit_, sun_c)]
                want_sh = ash_grey_reference(emit, grey, edge, ash, shade, edge_c)
                want_sun = ash_grey_reference(c_sun, grey, edge, ash, shade, edge_c)
                got_sun = [got[i] + got[3 + i] * sun_c[i] for i in range(3)]
                for i in range(3):
                    self.assertAlmostEqual(got[i], want_sh[i], places=5)
                    # the sunlit sum (AshLit clamps at 0 where the ash of the shadowed colour is the brighter)
                    self.assertAlmostEqual(got_sun[i], max(want_sun[i], want_sh[i]), places=5)
                if grey == 0.0 and edge == 0.0:
                    for i in range(3):
                        self.assertAlmostEqual(got[i], emit[i], places=6)
                        self.assertAlmostEqual(got[3 + i], lit_[i], places=5)

if __name__ == "__main__":
    unittest.main()
