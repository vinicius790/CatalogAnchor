// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "EditorValidatorBase.h"
#include "CatalogDefinitionValidators.generated.h"

class UCatalogAbilityDefinition;
class UCatalogItemDefinition;

/** Problemas de um asset, separados do UObject do validador para serem testáveis e reusados no relatório. */
struct FCatalogValidationIssues
{
	/** Tornam o asset Invalid. */
	TArray<FText> Errors;
	/** Não bloqueiam; o asset continua Valid. */
	TArray<FText> Warnings;
};

/**
 * true se AssetName é Prefix + algo (maiúsculas contam: "ca_item_x" não passa).
 * Base dos avisos de convenção CA_Item_/CA_Ability_/CA_Save_.
 */
CATALOGANCHOREDITOR_API bool CatalogFollowsNamingConvention(const FString& AssetName, const FString& Prefix);

/**
 * Nomes de asset que aparecem mais de uma vez em Assets (pastas diferentes), com os pacotes de cada um.
 * ItemId == nome do asset (e PrimaryAssetId também): dois assets com o mesmo nome colidem no inventário e no save.
 * Só entram nomes repetidos; a comparação é a do FName (sem diferenciar maiúsculas).
 */
CATALOGANCHOREDITOR_API TMap<FName, TArray<FName>> CatalogFindDuplicateAssetNames(TConstArrayView<FAssetData> Assets);

/**
 * Data Validation de UCatalogItemDefinition.
 * Erros: DisplayName vazio, MaxStackSize < 1, nome de asset (== ItemId) repetido em outra pasta.
 * Avisos: Icon/mesh soft nulos, nome fora de CA_Item_<Nome>.
 */
UCLASS()
class CATALOGANCHOREDITOR_API UCatalogItemDefinitionValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	/** Problemas do item em si (sem o Asset Registry, que é estado global). */
	static FCatalogValidationIssues GetIssues(const UCatalogItemDefinition& Item);

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject,
		FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData,
		UObject* InAsset, FDataValidationContext& Context) override;
};

/**
 * Data Validation de UCatalogAbilityDefinition.
 * Erros: AbilityTag vazio/não registrado, Behavior nulo, CooldownSeconds negativo ou não finito,
 * Melee Sweep com Range/Radius/Damage <= 0, nome de asset repetido em outra pasta.
 * Avisos: CooldownSeconds == 0 (sem cooldown), DisplayName vazio, ícone/mesh soft nulos,
 * nome fora de CA_Ability_<Nome>.
 *
 * AbilityTag duplicado entre assets NÃO é checado: o campo não é AssetRegistrySearchable, então
 * exigiria carregar todos os assets de habilidade a cada validação.
 */
UCLASS()
class CATALOGANCHOREDITOR_API UCatalogAbilityDefinitionValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	/** Problemas da habilidade em si (sem o Asset Registry, que é estado global). */
	static FCatalogValidationIssues GetIssues(const UCatalogAbilityDefinition& Definition);

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject,
		FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData,
		UObject* InAsset, FDataValidationContext& Context) override;
};
