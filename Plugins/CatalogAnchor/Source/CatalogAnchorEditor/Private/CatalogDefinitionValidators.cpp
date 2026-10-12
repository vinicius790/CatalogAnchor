// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogDefinitionValidators.h"
#include "CatalogAbility.h"
#include "CatalogAbilityDefinition.h"
#include "CatalogItemDefinition.h"
#include "GameplayTagContainer.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "CatalogDefinitionValidators"

bool CatalogFollowsNamingConvention(const FString& AssetName, const FString& Prefix)
{
	return AssetName.Len() > Prefix.Len() && AssetName.StartsWith(Prefix, ESearchCase::CaseSensitive);
}

// ---------------------------------------------------------------------------
// Item
// ---------------------------------------------------------------------------

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

	bool bValid = true;

	if (Item->DisplayName.IsEmptyOrWhitespace())
	{
		AssetFails(InAsset, LOCTEXT("ItemNoName", "CatalogAnchor: DisplayName vazio (aparece na UI e no prompt do pickup)."));
		bValid = false;
	}

	// ClampMin só vale no editor de propriedades; Python/CSV/edição em lote passam por fora.
	if (Item->MaxStackSize < 1)
	{
		AssetFails(InAsset, FText::Format(
			LOCTEXT("ItemBadStack", "CatalogAnchor: MaxStackSize {0} (precisa ser >= 1)."), Item->MaxStackSize));
		bValid = false;
	}

	if (Item->Icon.IsNull())
	{
		AssetWarning(InAsset, LOCTEXT("ItemNoIcon", "CatalogAnchor: Icon não definido; a UI vai mostrar um slot sem imagem."));
	}

	if (!CatalogFollowsNamingConvention(InAsset->GetName(), TEXT("CA_Item_")))
	{
		AssetWarning(InAsset, LOCTEXT("ItemNaming", "CatalogAnchor: convenção de nome é CA_Item_<Nome> (e renomear depois quebra saves: ItemId == nome do asset)."));
	}

	if (!bValid)
	{
		return EDataValidationResult::Invalid;
	}
	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

// ---------------------------------------------------------------------------
// Habilidade
// ---------------------------------------------------------------------------

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

	bool bValid = true;

	const FGameplayTag& Tag = Definition->AbilityTag;
	if (!Tag.IsValid())
	{
		AssetFails(InAsset, LOCTEXT("AbilityNoTag", "CatalogAnchor: AbilityTag vazio; TryActivateAbility não acha a habilidade."));
		bValid = false;
	}
	else if (!FGameplayTag::RequestGameplayTag(Tag.GetTagName(), /*ErrorIfNotFound*/ false).IsValid())
	{
		// Tag apagada/renomeada no .ini continua serializada no asset, mas nada a encontra.
		AssetFails(InAsset, FText::Format(
			LOCTEXT("AbilityUnknownTag", "CatalogAnchor: AbilityTag '{0}' não está registrada no GameplayTagsManager."),
			FText::FromName(Tag.GetTagName())));
		bValid = false;
	}

	const float Cooldown = Definition->CooldownSeconds;
	if (!FMath::IsFinite(Cooldown) || Cooldown < 0.f)
	{
		AssetFails(InAsset, FText::Format(
			LOCTEXT("AbilityBadCooldown", "CatalogAnchor: CooldownSeconds {0} (precisa ser finito e >= 0)."), Cooldown));
		bValid = false;
	}

	if (!Definition->Behavior)
	{
		AssetFails(InAsset, LOCTEXT("AbilityNoBehavior", "CatalogAnchor: Behavior vazio; escolha uma subclasse de UCatalogAbility (Dash, Melee Sweep...)."));
		bValid = false;
	}
	else if (const UCatalogAbility_MeleeSweep* Melee = Cast<UCatalogAbility_MeleeSweep>(Definition->Behavior.Get()))
	{
		auto RequirePositive = [&](FName Field, float Value)
		{
			if (!FMath::IsFinite(Value) || Value <= 0.f)
			{
				AssetFails(InAsset, FText::Format(
					LOCTEXT("MeleeNotPositive", "CatalogAnchor: Melee Sweep {0} = {1} (precisa ser finito e > 0)."),
					FText::FromName(Field), Value));
				bValid = false;
			}
		};
		RequirePositive(GET_MEMBER_NAME_CHECKED(UCatalogAbility_MeleeSweep, Range), Melee->Range);
		RequirePositive(GET_MEMBER_NAME_CHECKED(UCatalogAbility_MeleeSweep, Radius), Melee->Radius);
		RequirePositive(GET_MEMBER_NAME_CHECKED(UCatalogAbility_MeleeSweep, Damage), Melee->Damage);
	}

	if (Definition->DisplayName.IsEmptyOrWhitespace())
	{
		AssetWarning(InAsset, LOCTEXT("AbilityNoName", "CatalogAnchor: DisplayName vazio (a UI não tem o que mostrar)."));
	}

	if (!CatalogFollowsNamingConvention(InAsset->GetName(), TEXT("CA_Ability_")))
	{
		AssetWarning(InAsset, LOCTEXT("AbilityNaming", "CatalogAnchor: convenção de nome é CA_Ability_<Nome>."));
	}

	if (!bValid)
	{
		return EDataValidationResult::Invalid;
	}
	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE
