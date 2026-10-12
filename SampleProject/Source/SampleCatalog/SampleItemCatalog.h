// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SampleItemCatalog.generated.h"

class UCatalogItemDefinition;

/**
 * Itens do sample sem .uasset: default subobjects do CDO desta classe.
 * Por que não NewObject(GetTransientPackage()): objeto criado em runtime não tem nome estável
 * para a rede e chegaria nulo no cliente (FastArray do inventário, ACatalogPickupActor::Item, RPC).
 * Subobjeto do CDO existe idêntico em toda máquina desde o load do módulo (RF_DefaultSubObject) e é
 * resolvido pelo path; o nome ("Item_Gem") é o ItemId do save.
 */
UCLASS()
class USampleItemCatalog : public UObject
{
	GENERATED_BODY()

public:
	USampleItemCatalog();

	static const UCatalogItemDefinition* Gem();
	static const UCatalogItemDefinition* Potion();
	static const UCatalogItemDefinition* Key();

private:
	/** Só durante a construção (usa CreateDefaultSubobject). */
	UCatalogItemDefinition* CreateItem(FName ItemId, const FText& DisplayName, int32 MaxStackSize);

	UPROPERTY()
	TObjectPtr<UCatalogItemDefinition> GemItem;

	UPROPERTY()
	TObjectPtr<UCatalogItemDefinition> PotionItem;

	UPROPERTY()
	TObjectPtr<UCatalogItemDefinition> KeyItem;
};
