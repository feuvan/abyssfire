"""Editor-side content build (requires the `unreal` module: run inside UnrealEditor / UnrealEditor-Cmd).

core.py       editor API wrapper (asset subsystem, imports, tags, enums, compilation wait)
graph.py      declarative material-graph builder (records a fingerprint, then builds with MaterialEditingLibrary)
textures.py   texture imports and per-kind settings
materials.py  MPC_AF_Lighting, every master material, the manifest's material instances, slot assignment
meshes.py     skeletal meshes (+ shared skeletons, LODs, sockets), animations, static meshes
fxmeshes.py   SM_FX_Quad (the VFX / blob-shadow quad) when the art does not ship one
level.py      L_Main and the project-settings check
audio.py      runs unreal/Audio/ue/import_audio.py (the audio agent's importer)
"""
