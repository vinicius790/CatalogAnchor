// Copyright 2026 Geraliza Castlevania. MIT License.
// UBT module rules — Runtime. Compila apenas dentro de um projeto UE5 com UnrealBuildTool.

using UnrealBuildTool;

public class CatalogAnchor : ModuleRules
{
	public CatalogAnchor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"NetCore",
			"DeveloperSettings",
			"GameplayTags",
		});

		// catalog.Debug.Key (CatalogDebugCommands.cpp, só fora de Shipping) injeta teclas: FKey e o mapeador de dispositivos.
		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "InputCore", "ApplicationCore" });
		}

		// GAS fica opcional: ligue "GameplayAbilities" no .uplugin e descomente abaixo.
		// PublicDependencyModuleNames.AddRange(new string[] { "GameplayAbilities", "GameplayTags", "GameplayTasks" });
	}
}
