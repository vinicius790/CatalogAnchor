// Copyright 2026 Geraliza Castlevania. MIT License.
using UnrealBuildTool;
using System.Collections.Generic;

public class SampleCatalogClientTarget : TargetRules
{
	public SampleCatalogClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("SampleCatalog");
	}
}
