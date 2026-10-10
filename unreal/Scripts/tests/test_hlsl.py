"""Custom-node HLSL tests: lint, compile every node against tests/hlsl_emu.h, check the maths.

    python3 -m unittest discover -s unreal/Scripts/tests -v

Needs a C++17 compiler (clang++ or g++) for the compile / numeric tests; they are skipped without one.
"""
from __future__ import annotations

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
    params = []
    for inp in node.inputs:
        params.append(f"const Texture2D& {inp.name}, SamplerState {inp.name}Sampler" if inp.ctype == "Texture2D"
                      else f"{inp.ctype} {inp.name}")
    return f"static {node.output} {node.key}({', '.join(params)})"


def cpp_unit(nodes: list[hlsl.CustomNode], main: str = "int main() { return 0; }") -> str:
    parts = ['#include "hlsl_emu.h"', "#include <cstdio>"]
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

    def test_dither_and_quads(self):
        nodes = [hlsl.dither_node(), hlsl.blob_node()] + list(hlsl.quad_nodes().values())
        main = """
int main() {
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


if __name__ == "__main__":
    unittest.main()
