// Copyright 2026 Geraliza Castlevania. MIT License.
// UBT module rules — Editor only (Data Validation, factories do Content Browser + menu Tools → Catalog Anchor).

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
			"AssetDefinition",
			"GameplayTags",
			"DataValidation",
			"EditorSubsystem",
			"CatalogAnchor",
		});
	}
}
