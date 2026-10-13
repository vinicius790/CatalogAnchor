// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogAssetFactories.h"
#include "CatalogAbilityDefinition.h"
#include "CatalogItemDefinition.h"
#include "Misc/AssetCategoryPath.h"

#define LOCTEXT_NAMESPACE "CatalogAssetFactories"

namespace CatalogAssetFactoriesPrivate
{
	/** Mesma categoria para itens e habilidades: as duas factories caem no mesmo submenu do Add. */
	static TConstArrayView<FAssetCategoryPath> GetCatalogCategories()
	{
		static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(LOCTEXT("CategoryName", "Catalog Anchor")) };
		return Categories;
	}
}

// ---------------------------------------------------------------------------
// Factories
// ---------------------------------------------------------------------------

UCatalogItemDefinitionFactory::UCatalogItemDefinitionFactory()
{
	SupportedClass = UCatalogItemDefinition::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UCatalogItemDefinitionFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UCatalogItemDefinition>(InParent, InClass, InName, Flags | RF_Transactional);
}

FString UCatalogItemDefinitionFactory::GetDefaultNewAssetName() const
{
	return TEXT("CA_Item_New");
}

UCatalogAbilityDefinitionFactory::UCatalogAbilityDefinitionFactory()
{
	SupportedClass = UCatalogAbilityDefinition::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UCatalogAbilityDefinitionFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UCatalogAbilityDefinition>(InParent, InClass, InName, Flags | RF_Transactional);
}

FString UCatalogAbilityDefinitionFactory::GetDefaultNewAssetName() const
{
	return TEXT("CA_Ability_New");
}

// ---------------------------------------------------------------------------
// Asset definitions
// ---------------------------------------------------------------------------

FText UAssetDefinition_CatalogItem::GetAssetDisplayName() const
{
	return LOCTEXT("ItemDefinition", "Catalog Item Definition");
}

FLinearColor UAssetDefinition_CatalogItem::GetAssetColor() const
{
	return FLinearColor(FColor(230, 150, 40));
}

TSoftClassPtr<UObject> UAssetDefinition_CatalogItem::GetAssetClass() const
{
	return UCatalogItemDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_CatalogItem::GetAssetCategories() const
{
	return CatalogAssetFactoriesPrivate::GetCatalogCategories();
}

FText UAssetDefinition_CatalogAbility::GetAssetDisplayName() const
{
	return LOCTEXT("AbilityDefinition", "Catalog Ability Definition");
}

FLinearColor UAssetDefinition_CatalogAbility::GetAssetColor() const
{
	return FLinearColor(FColor(60, 170, 190));
}

TSoftClassPtr<UObject> UAssetDefinition_CatalogAbility::GetAssetClass() const
{
	return UCatalogAbilityDefinition::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_CatalogAbility::GetAssetCategories() const
{
	return CatalogAssetFactoriesPrivate::GetCatalogCategories();
}

#undef LOCTEXT_NAMESPACE
