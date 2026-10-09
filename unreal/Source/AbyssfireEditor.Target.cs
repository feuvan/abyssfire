// Editor target (ue58-platform.md 4.3). Must be built before the editor Python content build runs
// (Scripts/build_content.py references project classes).
using UnrealBuildTool;

public class AbyssfireEditorTarget : TargetRules
{
	public AbyssfireEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Abyssfire");
	}
}
