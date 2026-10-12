// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "SampleTrainingDummy.generated.h"

class UCatalogHealthComponent;
class UStaticMeshComponent;

/**
 * Boneco de treino: alvo do golpe (UCatalogAbility_MeleeSweep varre objetos do tipo Pawn).
 * Some ao morrer e renasce após RespawnSeconds. A visibilidade segue a vida replicada,
 * então todo cliente vê o mesmo estado sem RPC extra.
 */
UCLASS()
class ASampleTrainingDummy : public AActor
{
	GENERATED_BODY()

public:
	ASampleTrainingDummy();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<UCatalogHealthComponent> Health;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Sample", meta = (ClampMin = "0.1"))
	float RespawnSeconds = 3.f;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float Delta);

	UFUNCTION()
	void HandleDeath(AActor* Killer);

	void Respawn();

	FTimerHandle RespawnTimer;
};
