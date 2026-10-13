// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogDefinitionValidators.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CatalogAbility.h"
#include "CatalogAbilityDefinition.h"
#include "CatalogItemDefinition.h"
#include "Engine/StreamableRenderAsset.h"
#include "GameplayTagContainer.h"
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "CatalogDefinitionValidators"

bool CatalogFollowsNamingConvention(const FString& AssetName, const FString& Prefix)
{
	return AssetName.Len() > Prefix.Len() && AssetName.StartsWith(Prefix, ESearchCase::CaseSensitive);
}

TMap<FName, TArray<FName>> CatalogFindDuplicateAssetNames(TConstArrayView<FAssetData> Assets)
{
	TMap<FName, TArray<FName>> ByName;
	for (const FAssetData& Asset : Assets)
	{
		ByName.FindOrAdd(Asset.AssetName).AddUnique(Asset.PackageName);
	}

	// Mesmo pacote repetido na lista não conta: duplicata é nome igual em pacotes diferentes.
	for (auto It = ByName.CreateIterator(); It; ++It)
	{
		if (It.Value().Num() < 2)
		{
			It.RemoveCurrent();
		}
	}
	return ByName;
}

namespace CatalogDefinitionValidatorsPrivate
{
	/**
	 * Avisa de TSoftObjectPtr<textura/mesh> nulo (ícone, mesh...) por reflexão: cobre campos novos
	 * sem depender do nome deles. Só aviso, a UI e o mundo toleram a referência vazia.
	 */
	static void AppendNullVisualWarnings(const UObject& Object, TArray<FText>& OutWarnings)
	{
		for (TFieldIterator<FSoftObjectProperty> It(Object.GetClass()); It; ++It)
		{
			if (It->PropertyClass && It->PropertyClass->IsChildOf(UStreamableRenderAsset::StaticClass())
				&& It->GetPropertyValue_InContainer(&Object).IsNull())
			{
				OutWarnings.Add(FText::Format(
					LOCTEXT("NullVisual", "CatalogAnchor: {0} não definido; a UI/mundo vai mostrar um slot sem imagem/mesh."),
					FText::FromName(It->GetFName())));
			}
		}
	}

	/**
	 * Falha o asset se outro asset da família de BaseClass tem o mesmo nome em outra pasta
	 * (ItemId e PrimaryAssetId são o nome). Pula enquanto o Asset Registry indexa: lista parcial enganaria.
	 */
	static bool FailIfNameDuplicated(UEditorValidatorBase& Validator, UObject* Asset, const UClass* BaseClass)
	{
		const IAssetRegistry* Registry = IAssetRegistry::Get();
		if (!Registry || Registry->IsLoadingAssets())
		{
			return false;
		}

		TArray<FAssetData> Assets;
		Registry->GetAssetsByClass(BaseClass->GetClassPathName(), Assets, /*bSearchSubClasses*/ true);

		// O pacote do próprio asset é excluído pelo nome do pacote: um asset novo ainda fora do registry não se auto-duplica.
		const FName OwnPackage = Asset->GetPackage()->GetFName();
		TArray<FString> Others;
		for (const FAssetData& Other : Assets)
		{
			if (Other.AssetName == Asset->GetFName() && Other.PackageName != OwnPackage)
			{
				Others.Add(Other.PackageName.ToString());
			}
		}

		if (Others.Num() == 0)
		{
			return false;
		}
		Validator.AssetFails(Asset, FText::Format(
			LOCTEXT("DuplicateName", "CatalogAnchor: o nome '{0}' (ItemId/PrimaryAssetId) já existe em: {1}. Renomeie um dos assets."),
			FText::FromName(Asset->GetFName()), FText::FromString(FString::Join(Others, TEXT(", ")))));
		return true;
	}
}

// ---------------------------------------------------------------------------
// Item
// ---------------------------------------------------------------------------

FCatalogValidationIssues UCatalogItemDefinitionValidator::GetIssues(const UCatalogItemDefinition& Item)
{
	FCatalogValidationIssues Issues;

	if (Item.DisplayName.IsEmptyOrWhitespace())
	{
		Issues.Errors.Add(LOCTEXT("ItemNoName", "CatalogAnchor: DisplayName vazio (aparece na UI e no prompt do pickup)."));
	}

	// ClampMin só vale no editor de propriedades; Python/CSV/edição em lote passam por fora.
	if (Item.MaxStackSize < 1)
	{
		Issues.Errors.Add(FText::Format(
			LOCTEXT("ItemBadStack", "CatalogAnchor: MaxStackSize {0} (precisa ser >= 1)."), Item.MaxStackSize));
	}

	CatalogDefinitionValidatorsPrivate::AppendNullVisualWarnings(Item, Issues.Warnings);

	if (!CatalogFollowsNamingConvention(Item.GetName(), TEXT("CA_Item_")))
	{
		Issues.Warnings.Add(LOCTEXT("ItemNaming", "CatalogAnchor: convenção de nome é CA_Item_<Nome> (e renomear depois quebra saves: ItemId == nome do asset)."));
	}

	return Issues;
}

bool UCatalogItemDefinitionValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject,
	FDataValidationContext& InContext) const
{
	return InObject && InObject->IsA<UCatalogItemDefinition>();
}

EDataValidationResult UCatalogItemDefinitionValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData,
	UObject* InAsset, FDataValidationContext& Context)
{
	const UCatalogItemDefinition* Item = Cast<UCatalogItemDefinition>(InAsset);
	if (!Item)
	{
		AssetFails(InAsset, LOCTEXT("NotAnItem", "CatalogAnchor: o asset não é um UCatalogItemDefinition."));
		return EDataValidationResult::Invalid;
	}

	const FCatalogValidationIssues Issues = GetIssues(*Item);
	for (const FText& Error : Issues.Errors)
	{
		AssetFails(InAsset, Error);
	}
	for (const FText& Warning : Issues.Warnings)
	{
		AssetWarning(InAsset, Warning);
	}

	const bool bDuplicated = CatalogDefinitionValidatorsPrivate::FailIfNameDuplicated(
		*this, InAsset, UCatalogItemDefinition::StaticClass());
	if (bDuplicated || Issues.Errors.Num() > 0)
	{
		return EDataValidationResult::Invalid;
	}
	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

// ---------------------------------------------------------------------------
// Habilidade
// ---------------------------------------------------------------------------

FCatalogValidationIssues UCatalogAbilityDefinitionValidator::GetIssues(const UCatalogAbilityDefinition& Definition)
{
	FCatalogValidationIssues Issues;

	const FGameplayTag& Tag = Definition.AbilityTag;
	if (!Tag.IsValid())
	{
		Issues.Errors.Add(LOCTEXT("AbilityNoTag", "CatalogAnchor: AbilityTag vazio; TryActivateAbility não acha a habilidade."));
	}
	else if (!FGameplayTag::RequestGameplayTag(Tag.GetTagName(), /*ErrorIfNotFound*/ false).IsValid())
	{
		// Tag apagada/renomeada no .ini continua serializada no asset, mas nada a encontra.
		Issues.Errors.Add(FText::Format(
			LOCTEXT("AbilityUnknownTag", "CatalogAnchor: AbilityTag '{0}' não está registrada no GameplayTagsManager."),
			FText::FromName(Tag.GetTagName())));
	}

	// 0 é válido e documentado ("sem cooldown"): vira aviso porque quase sempre é esquecimento.
	const float Cooldown = Definition.CooldownSeconds;
	if (!FMath::IsFinite(Cooldown) || Cooldown < 0.f)
	{
		Issues.Errors.Add(FText::Format(
			LOCTEXT("AbilityBadCooldown", "CatalogAnchor: CooldownSeconds {0} (precisa ser finito e >= 0)."), Cooldown));
	}
	else if (Cooldown == 0.f)
	{
		Issues.Warnings.Add(LOCTEXT("AbilityNoCooldown", "CatalogAnchor: CooldownSeconds 0 (sem cooldown); a habilidade pode ser ativada sem intervalo."));
	}

	if (!Definition.Behavior)
	{
		Issues.Errors.Add(LOCTEXT("AbilityNoBehavior", "CatalogAnchor: Behavior vazio; escolha uma subclasse de UCatalogAbility (Dash, Melee Sweep...)."));
	}
	else if (const UCatalogAbility_MeleeSweep* Melee = Cast<UCatalogAbility_MeleeSweep>(Definition.Behavior.Get()))
	{
		auto RequirePositive = [&Issues](FName Field, float Value)
		{
			if (!FMath::IsFinite(Value) || Value <= 0.f)
			{
				Issues.Errors.Add(FText::Format(
					LOCTEXT("MeleeNotPositive", "CatalogAnchor: Melee Sweep {0} = {1} (precisa ser finito e > 0)."),
					FText::FromName(Field), Value));
			}
		};
		RequirePositive(GET_MEMBER_NAME_CHECKED(UCatalogAbility_MeleeSweep, Range), Melee->Range);
		RequirePositive(GET_MEMBER_NAME_CHECKED(UCatalogAbility_MeleeSweep, Radius), Melee->Radius);
		RequirePositive(GET_MEMBER_NAME_CHECKED(UCatalogAbility_MeleeSweep, Damage), Melee->Damage);
	}

	if (Definition.DisplayName.IsEmptyOrWhitespace())
	{
		Issues.Warnings.Add(LOCTEXT("AbilityNoName", "CatalogAnchor: DisplayName vazio (a UI não tem o que mostrar)."));
	}

	CatalogDefinitionValidatorsPrivate::AppendNullVisualWarnings(Definition, Issues.Warnings);

	if (!CatalogFollowsNamingConvention(Definition.GetName(), TEXT("CA_Ability_")))
	{
		Issues.Warnings.Add(LOCTEXT("AbilityNaming", "CatalogAnchor: convenção de nome é CA_Ability_<Nome>."));
	}

	return Issues;
}

bool UCatalogAbilityDefinitionValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject,
	FDataValidationContext& InContext) const
{
	return InObject && InObject->IsA<UCatalogAbilityDefinition>();
}

EDataValidationResult UCatalogAbilityDefinitionValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData,
	UObject* InAsset, FDataValidationContext& Context)
{
	const UCatalogAbilityDefinition* Definition = Cast<UCatalogAbilityDefinition>(InAsset);
	if (!Definition)
	{
		AssetFails(InAsset, LOCTEXT("NotAnAbility", "CatalogAnchor: o asset não é um UCatalogAbilityDefinition."));
		return EDataValidationResult::Invalid;
	}

	const FCatalogValidationIssues Issues = GetIssues(*Definition);
	for (const FText& Error : Issues.Errors)
	{
		AssetFails(InAsset, Error);
	}
	for (const FText& Warning : Issues.Warnings)
	{
		AssetWarning(InAsset, Warning);
	}

	const bool bDuplicated = CatalogDefinitionValidatorsPrivate::FailIfNameDuplicated(
		*this, InAsset, UCatalogAbilityDefinition::StaticClass());
	if (bDuplicated || Issues.Errors.Num() > 0)
	{
		return EDataValidationResult::Invalid;
	}
	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE
