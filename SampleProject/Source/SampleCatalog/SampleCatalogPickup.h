// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "CatalogPickupActor.h"
#include "SampleCatalogPickup.generated.h"

/**
 * ACatalogPickupActor com a esfera da engine. A base não tem malha e o sample não tem .uasset;
 * a malha vem do construtor (e não de um SetStaticMesh no servidor) porque o componente não
 * replica: só assim o cliente também a vê. Também é o DropPickupClass do personagem.
 */
UCLASS()
class ASampleCatalogPickup : public ACatalogPickupActor
{
	GENERATED_BODY()

public:
	ASampleCatalogPickup();

protected:
	virtual void BeginPlay() override;
};
