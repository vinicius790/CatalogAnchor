// Copyright 2026 Geraliza Castlevania. MIT License.
using UnrealBuildTool;
using System.Collections.Generic;

public class SampleCatalogServerTarget : TargetRules
{
	public SampleCatalogServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("SampleCatalog");
	}
}
