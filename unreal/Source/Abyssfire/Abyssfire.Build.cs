// Abyssfire: the UE presentation module (ue58-platform.md 4.5, ARCHITECTURE 1 and 6). It renders core state, feeds
// input to the core as commands, plays animation / VFX / audio from core events and builds the Slate UI. It never
// contains gameplay rules (those live in AbyssCore).
using System.IO;
using UnrealBuildTool;

public class Abyssfire : ModuleRules
{
	public Abyssfire(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Overriding FPSemantics requires a private PCH: UBT would otherwise silently drop the module to NoPCHs (shared
		// PCHs are compiled with the target's FP semantics). UEBuildModuleCPP: "Overriding FPSemantics requires a
		// private PCH".
		PrivatePCHHeaderFile = "Private/AbyssfirePCH.h";
		CppStandard = CppStandardVersion.Cpp20;
		// The core's public headers carry inline math (Lerp, DistSq, Vec2::Length, IsoPx, SpatialGrid, ...) that is
		// compiled in this module too: same precise FP semantics as AbyssCore (ue58-platform.md 3.3; /fp:precise,
		// -ffp-contract=off). FPSemanticsMode { Default, Precise, Imprecise } exists in UBT 5.x.
		FPSemantics = FPSemanticsMode.Precise;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			// Public headers of this module include the core's public headers (abyss/...).
			"AbyssCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate", "SlateCore",          // UI (ue58-platform.md 9)
			"ApplicationCore",             // FPlatformApplicationMisc (screen density, screensaver, DPI)
			"RenderCore", "RHI",           // FUpdateTextureRegion2D, dynamic textures (fog of war, minimap)
			"DeveloperSettings",           // UDeveloperSettings for project tunables
			"AudioMixer",                  // runtime audio layer (audio.md)
			"ProceduralMeshComponent",     // zone terrain / water meshes built at runtime (world agent; plugin enabled in
			                               // Abyssfire.uproject)
		});
		// "UMG" only if UMG widgets are used (9.1); "Niagara" only if Niagara assets are adopted (6.9).

		// Exported data tables and bundled fonts are staged into the pak / IoStore container (UFS) on every platform and
		// read at runtime through the pak-aware platform file (ue58-platform.md 10.1). `...` = recursive.
		RuntimeDependencies.Add("$(ProjectDir)/Data/...", StagedFileType.UFS);
		// Fonts/ is committed (Scripts/fonts/build_fonts.py output + OFL licences). Without it every CJK string renders
		// as nothing / tofu at runtime, so a missing directory fails the build instead of staging nothing.
		string ProjectRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		if (!Directory.Exists(Path.Combine(ProjectRoot, "Fonts")))
		{
			throw new DirectoryNotFoundException("Abyssfire: unreal/Fonts is missing - run `python3 unreal/Scripts/fonts/build_fonts.py` (or restore the committed fonts)");
		}
		RuntimeDependencies.Add("$(ProjectDir)/Fonts/...", StagedFileType.UFS);
		// Audio manifest (Audio/README.md "Platform settings"): per-asset gains, loop flags, levels / ducking, spatial
		// settings, menu track, credits. FAbyssAudioManifest::Load reads it from here in packaged builds too.
		RuntimeDependencies.Add("$(ProjectDir)/Audio/Export/audio_manifest.json", StagedFileType.UFS);
		// VFX recipe overrides written by the art pipeline (FAbyssVfxLibrary::LoadAll, code defaults otherwise). Staged once
		// the file exists; re-run UBT (regenerate / clean the makefile) after the art kit first writes it.
		if (File.Exists(Path.Combine(ProjectRoot, "Art", "Export", "VFX", "vfx_recipes.json")))
		{
			RuntimeDependencies.Add("$(ProjectDir)/Art/Export/VFX/vfx_recipes.json", StagedFileType.UFS);
		}
	}
}
