// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SampleCatalogGameMode.generated.h"

class UCatalogItemDefinition;

/**
 * Monta a demo inteira em C++ sobre o mapa vazio /Engine/Maps/Entry (zero .uasset/.umap):
 * arena replicada, pickups e dois baús (um trancado por chave). Só roda no servidor;
 * os clientes recebem tudo por replicação.
 */
UCLASS()
class ASampleCatalogGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASampleCatalogGameMode();

protected:
	virtual void BeginPlay() override;

private:
	void SpawnPickup(const UCatalogItemDefinition* Item, int32 Quantity, const FVector& Location);
};
