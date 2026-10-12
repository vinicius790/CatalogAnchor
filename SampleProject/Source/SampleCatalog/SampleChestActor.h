// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CatalogInventoryTypes.h"
#include "ICatalogInteractable.h"
#include "SampleChestActor.generated.h"

class UCatalogItemDefinition;
class UStaticMeshComponent;

/**
 * Baú de exemplo do contrato ICatalogInteractable com hold, tranca e loot.
 * - Segurar a interação por HoldSeconds abre; o servidor completa (UCatalogInteractionComponent).
 * - Com RequiredKey, fica bloqueado até o instigador ter o item no inventário (não consome a chave).
 * - Abrir põe o Loot no inventário do instigador; o que não couber fica para a próxima vez.
 */
UCLASS()
class ASampleChestActor : public AActor, public ICatalogInteractable
{
	GENERATED_BODY()

public:
	ASampleChestActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Some quando o baú esvazia: o estado aberto aparece para todos. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<UStaticMeshComponent> Lid;

	/** Item exigido para abrir (nulo = destrancado). Replicado: o prompt "Trancado" é montado no cliente. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Catalog|Sample")
	TObjectPtr<const UCatalogItemDefinition> RequiredKey = nullptr;

	/** Conteúdo; só o servidor usa (não replica). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TArray<FCatalogInventoryStack> Loot;

	/** Segundos segurando a interação para abrir. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Sample", meta = (ClampMin = "0.0"))
	float HoldSeconds = 1.5f;

	//~ ICatalogInteractable
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual bool OnInteract_Implementation(AActor* InstigatorActor) override;
	virtual float GetInteractionDuration_Implementation() const override;
	virtual bool IsInteractionBlocked_Implementation(AActor* InstigatorActor) const override;
	virtual void OnFocusChanged_Implementation(bool bHasFocus, AActor* InstigatorActor) override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** true depois que todo o Loot saiu. */
	UPROPERTY(ReplicatedUsing = OnRep_Opened)
	bool bOpened = false;

	UFUNCTION()
	void OnRep_Opened();

private:
	/** Sem RequiredKey sempre true; senão, o inventário do ator tem a chave. */
	bool HasKey(const AActor* InstigatorActor) const;
};
