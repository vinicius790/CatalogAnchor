// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ICatalogInteractable.h"
#include "CatalogPickupActor.generated.h"

class UCatalogItemDefinition;
class UStaticMeshComponent;

/**
 * Item solto no mundo. Interagir põe Item no UCatalogInventoryComponent do instigador
 * (no Pawn ou no Controller dele); some quando tudo entrou, senão fica com o resto.
 * A malha bloqueia só Visibility (canal padrão do trace de interação): pawns atravessam.
 * Highlight de foco usa Custom Depth: habilite-o no projeto e use um post-process.
 */
UCLASS()
class CATALOGANCHOR_API ACatalogPickupActor : public AActor, public ICatalogInteractable
{
	GENERATED_BODY()

public:
	ACatalogPickupActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Replicado: o prompt é montado no cliente e pickups soltos em runtime não existem no mapa. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Catalog|Inventory", meta = (ExposeOnSpawn = true))
	TObjectPtr<const UCatalogItemDefinition> Item = nullptr;

	/** Replicado: coleta parcial reduz o valor e o prompt dos clientes acompanha. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Catalog|Inventory", meta = (ExposeOnSpawn = true, ClampMin = "1"))
	int32 Quantity = 1;

	//~ ICatalogInteractable
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual bool OnInteract_Implementation(AActor* InstigatorActor) override;
	virtual float GetInteractionDuration_Implementation() const override;
	virtual bool IsInteractionBlocked_Implementation(AActor* InstigatorActor) const override;
	virtual void OnFocusChanged_Implementation(bool bHasFocus, AActor* InstigatorActor) override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
