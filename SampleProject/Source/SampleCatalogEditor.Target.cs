using UnrealBuildTool;
using System.Collections.Generic;

public class SampleCatalogEditorTarget : TargetRules
{
	public SampleCatalogEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("SampleCatalog");
	}
}
