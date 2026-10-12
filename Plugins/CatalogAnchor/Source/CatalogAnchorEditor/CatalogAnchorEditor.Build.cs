// Copyright 2026 Geraliza Castlevania. MIT License.
// UBT module rules — Editor only (Data Validation + menu Tools → Catalog Anchor).

using UnrealBuildTool;

public class CatalogAnchorEditor : ModuleRules
{
	public CatalogAnchorEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"ToolMenus",
			"AssetRegistry",
			"GameplayTags",
			"DataValidation",
			"EditorSubsystem",
			"CatalogAnchor",
		});
	}
}
