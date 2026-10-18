// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "CatalogInteractionComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/Actor.h"
#include "ICatalogInteractable.h"
#include "UObject/Object.h"
#include "CatalogInteractionTestTypes.generated.h"

/**
 * Só para os testes de mundo da interação: interagível com caixa de colisão (bloqueia tudo, 40 cm)
 * e prioridade, duração e bloqueio configuráveis. Também serve de dono "solto" do componente.
 */
UCLASS(Transient, HideDropdown)
class ACatalogTestInteractable : public AActor, public ICatalogInteractable
{
	GENERATED_BODY()

public:
	ACatalogTestInteractable()
	{
		Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
		Box->InitBoxExtent(FVector(20.0));
		Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		RootComponent = Box;
	}

	UPROPERTY()
	TObjectPtr<UBoxComponent> Box;

	int32 Priority = 0;
	float Duration = 0.f;
	bool bBlocked = false;
	int32 InteractCount = 0;

	//~ ICatalogInteractable
	virtual int32 GetInteractionPriority_Implementation() const override { return Priority; }
	virtual float GetInteractionDuration_Implementation() const override { return Duration; }
	virtual bool IsInteractionBlocked_Implementation(AActor* InstigatorActor) const override { return bBlocked; }
	virtual bool OnInteract_Implementation(AActor* InstigatorActor) override
	{
		++InteractCount;
		return true;
	}
};

/** Só para teste: expõe os _Validate protegidos dos Server RPCs. */
UCLASS(Transient, HideDropdown)
class UCatalogInteractionTestComponent : public UCatalogInteractionComponent
{
	GENERATED_BODY()

public:
	bool ValidateTry() { return ServerTryInteract_Validate(); }
	bool ValidateBegin() { return ServerBeginInteract_Validate(); }
	bool ValidateEnd() { return ServerEndInteract_Validate(); }
};

/** Só para teste: delegates dinâmicos exigem UFUNCTION num UObject. Registra falhas e conclusões. */
UCLASS(Transient)
class UCatalogInteractionTestListener : public UObject
{
	GENERATED_BODY()

public:
	TArray<ECatalogInteractionFailReason> Reasons;
	TArray<TWeakObjectPtr<AActor>> Targets;
	int32 CompletedCount = 0;

	UFUNCTION()
	void HandleFailed(AActor* Target, ECatalogInteractionFailReason Reason)
	{
		Reasons.Add(Reason);
		Targets.Add(Target);
	}

	UFUNCTION()
	void HandleCompleted(AActor* Target, bool bConsumed)
	{
		++CompletedCount;
	}
};
