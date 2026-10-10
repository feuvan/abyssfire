"""Abyssfire content build (ue58-platform.md 11): UE editor Python, CPython 3.11, standard library only.

Package layout:

    paths.py      repository / export / content paths and the content-build constants (no `unreal` import)
    manifest.py   Art/Export/manifest.json -> a validated import plan (no `unreal` import; unit-tested)
    pngio.py      tiny PNG reader / writer and the generated default textures (no `unreal` import; unit-tested)
    hlsl.py       the Custom-node HLSL of every material, generated from the manifest shading contract (unit-tested
                  with a C++ HLSL emulation harness, tests/hlsl_emu.h)
    report.py     the build report (JSON + Markdown, CI contract `AF_CONTENT_REPORT`)
    ue/           everything that talks to the editor (`import unreal`): imports, materials, meshes, level, audio

Entry point: Scripts/build_content.py.
"""

__version__ = "1.0.0"
