// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SampleAbilityCatalog.generated.h"

class UCatalogAbilityDefinition;

/**
 * Habilidades do sample sem .uasset, no mesmo padrão de USampleItemCatalog: default subobjects
 * do CDO existem idênticos em toda máquina e têm nome estável para a rede.
 * O Behavior de cada definição é compartilhado por todos os atores (sem estado por ator).
 */
UCLASS()
class USampleAbilityCatalog : public UObject
{
	GENERATED_BODY()

public:
	USampleAbilityCatalog();

	static UCatalogAbilityDefinition* Dash();
	static UCatalogAbilityDefinition* Melee();

private:
	UPROPERTY()
	TObjectPtr<UCatalogAbilityDefinition> DashAbility;

	UPROPERTY()
	TObjectPtr<UCatalogAbilityDefinition> MeleeAbility;
};
