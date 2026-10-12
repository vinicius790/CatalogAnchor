// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CatalogHealthTestListener.generated.h"

class AActor;

/**
 * Só para os testes de mundo de UCatalogHealthComponent: delegates dinâmicos exigem UFUNCTION num UObject.
 * Conta OnHealthChanged/OnDeath e, se pedido, reage dentro do callback (dano reentrante, Destroy).
 */
UCLASS(Transient)
class UCatalogHealthTestListener : public UObject
{
	GENERATED_BODY()

public:
	int32 ChangedCount = 0;
	int32 DeathCount = 0;
	float LastHealth = 0.f;
	float LastDelta = 0.f;
	TWeakObjectPtr<AActor> LastKiller;

	/** Ator que as reações abaixo afetam. */
	TWeakObjectPtr<AActor> Target;
	/** > 0: no próximo OnHealthChanged aplica este dano em Target (uma vez). */
	float ExtraDamageOnChange = 0.f;
	bool bDestroyOnChange = false;
	bool bDestroyOnDeath = false;

	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float Delta);

	UFUNCTION()
	void HandleDeath(AActor* Killer);
};
