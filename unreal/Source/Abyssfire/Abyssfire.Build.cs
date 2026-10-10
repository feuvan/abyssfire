// Abyssfire: the UE presentation module (ue58-platform.md 4.5, ARCHITECTURE 1 and 6). It renders core state, feeds
// input to the core as commands, plays animation / VFX / audio from core events and builds the Slate UI. It never
// contains gameplay rules (those live in AbyssCore).
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
		RuntimeDependencies.Add("$(ProjectDir)/Fonts/...", StagedFileType.UFS);
	}
}
