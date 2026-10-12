// Copyright 2026 Geraliza Castlevania. MIT License.
using UnrealBuildTool;

public class SampleCatalog : ModuleRules
{
	public SampleCatalog(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"NetCore",
			"CatalogAnchor",
		});
	}
}
