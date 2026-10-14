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


if __name__ == "__main__":
    unittest.main()
