// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "CatalogDefinitionValidators.generated.h"

/**
 * true se AssetName é Prefix + algo (maiúsculas contam: "ca_item_x" não passa).
 * Base dos avisos de convenção CA_Item_/CA_Ability_/CA_Save_.
 */
CATALOGANCHOREDITOR_API bool CatalogFollowsNamingConvention(const FString& AssetName, const FString& Prefix);

/**
 * Data Validation de UCatalogItemDefinition.
 * Erros: DisplayName vazio, MaxStackSize < 1. Avisos: sem Icon, nome fora de CA_Item_<Nome>.
 */
UCLASS()
class CATALOGANCHOREDITOR_API UCatalogItemDefinitionValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject,
		FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData,
		UObject* InAsset, FDataValidationContext& Context) override;
};

/**
 * Data Validation de UCatalogAbilityDefinition.
 * Erros: AbilityTag vazio/não registrado, Behavior nulo, CooldownSeconds negativo ou não finito,
 * Melee Sweep com Range/Radius/Damage <= 0. Avisos: DisplayName vazio, nome fora de CA_Ability_<Nome>.
 *
 * AbilityTag duplicado entre assets NÃO é checado: o campo não é AssetRegistrySearchable, então
 * exigiria carregar todos os assets de habilidade a cada validação.
 */
UCLASS()
class CATALOGANCHOREDITOR_API UCatalogAbilityDefinitionValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject,
		FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData,
		UObject* InAsset, FDataValidationContext& Context) override;
};
