// Game target (ue58-platform.md 4.3). Launcher (installed) engine: game targets use the shared build environment, so
// no target-wide compiler switches (RTTI, exceptions, definitions) are set here - per-module settings live in the
// .Build.cs files.
// [Verify] after the first successful 5.8.3 build, pin DefaultBuildSettings / IncludeOrderVersion to the values a fresh
// 5.8.3 C++ template generates (ue58-platform.md 17 item 1); `Latest` is always valid meanwhile.
using UnrealBuildTool;

public class AbyssfireTarget : TargetRules
{
	public AbyssfireTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Abyssfire");
	}
}
