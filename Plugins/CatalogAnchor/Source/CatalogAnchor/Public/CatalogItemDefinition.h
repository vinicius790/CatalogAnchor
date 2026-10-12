// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "CatalogItemDefinition.generated.h"

class UTexture2D;

/**
 * Definição estática de um item (dados de design, não estado do jogador).
 * O inventário guarda ponteiros para estes assets; o save guarda só o ItemId.
 *
 * Asset Manager: o load resolve ItemId → asset via UAssetManager, então o projeto
 * PRECISA registrar o tipo "CatalogItem" em DefaultGame.ini
 * ([/Script/Engine.AssetManagerSettings] PrimaryAssetTypesToScan, classe base
 * CatalogItemDefinition, bHasBlueprintClasses=False). Sem isso o load só acha definições já
 * carregadas em memória (ex.: criadas em runtime); as demais ele avisa e pula.
 */
UCLASS(BlueprintType)
class CATALOGANCHOR_API UCatalogItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Tipo de primary asset de todos os itens ("CatalogItem"); fixo para o ItemId do save não mudar de significado. */
	static const FPrimaryAssetType ItemAssetType;

	/** Nome mostrado na UI e no prompt do pickup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory", meta = (MultiLine = true))
	FText Description;

	/** Soft: o ícone só carrega quando a UI pede, não junto com o inventário. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Máximo por pilha (1 = não empilha, ex.: armas). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory", meta = (ClampMin = "1"))
	int32 MaxStackSize = 1;

	/** Tags livres para filtros de gameplay/UI (ex.: Item.Consumable). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory")
	FGameplayTagContainer ItemTags;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Id gravado em FCatalogInventorySlot::ItemId (== nome do asset): renomear o asset quebra saves antigos. */
	UFUNCTION(BlueprintPure, Category = "Catalog|Inventory")
	FName GetItemId() const;
};
