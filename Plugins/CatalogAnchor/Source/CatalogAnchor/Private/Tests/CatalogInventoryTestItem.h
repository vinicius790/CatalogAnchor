// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "CatalogItemDefinition.h"
#include "GameFramework/Actor.h"
#include "UObject/Object.h"
#include "CatalogInventoryTestItem.generated.h"

/** Só para os testes de mundo do inventário: a definição real não guarda estado, esta registra cada OnUsed. */
UCLASS(Transient, HideDropdown)
class UCatalogTestItemDefinition : public UCatalogItemDefinition
{
	GENERATED_BODY()

public:
	// mutable: OnUsed é const na definição (asset compartilhado).
	mutable int32 UseCount = 0;
	mutable TWeakObjectPtr<AActor> LastUser;

	virtual void OnUsed_Implementation(AActor* User) const override
	{
		++UseCount;
		LastUser = User;
	}
};

/** Só para teste: delegates dinâmicos exigem UFUNCTION num UObject. Conta OnItemUsed e OnInventoryChanged. */
UCLASS(Transient, HideDropdown)
class UCatalogInventoryTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 UsedCount = 0;
	int32 LastQuantity = 0;
	TWeakObjectPtr<const UCatalogItemDefinition> LastItem;
	int32 ChangedCount = 0;

	UFUNCTION()
	void HandleItemUsed(const UCatalogItemDefinition* Item, int32 Quantity)
	{
		++UsedCount;
		LastItem = Item;
		LastQuantity = Quantity;
	}

	UFUNCTION()
	void HandleChanged()
	{
		++ChangedCount;
	}
};
