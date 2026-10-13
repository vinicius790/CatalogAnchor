// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "Factories/Factory.h"
#include "CatalogAssetFactories.generated.h"

/** Content Browser → Add → Catalog Anchor → Item Definition. */
UCLASS()
class UCatalogItemDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UCatalogItemDefinitionFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn) override;

	/** Já nasce dentro da convenção CA_Item_<Nome> (o nome vira o ItemId). */
	virtual FString GetDefaultNewAssetName() const override;
};

/** Content Browser → Add → Catalog Anchor → Ability Definition. */
UCLASS()
class UCatalogAbilityDefinitionFactory : public UFactory
{
	GENERATED_BODY()

public:
	UCatalogAbilityDefinitionFactory();

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn) override;

	virtual FString GetDefaultNewAssetName() const override;
};

/**
 * Registro do item no Content Browser. A categoria "Catalog Anchor" daqui é a que o menu Add usa
 * para as factories acima (o sistema UAssetDefinition substituiu o FAssetTypeActions_Base).
 */
UCLASS()
class UAssetDefinition_CatalogItem : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};

UCLASS()
class UAssetDefinition_CatalogAbility : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};
