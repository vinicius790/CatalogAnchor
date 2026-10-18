// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "CatalogAbilityComponent.h"
#include "CatalogAbilityTestComponent.generated.h"

/** Só para teste: expõe o _Validate protegido do RPC de ativação. */
UCLASS(Transient, HideDropdown)
class UCatalogAbilityTestComponent : public UCatalogAbilityComponent
{
	GENERATED_BODY()

public:
	bool ValidateActivate(FGameplayTag AbilityTag) { return ServerTryActivateAbility_Validate(AbilityTag); }
};
