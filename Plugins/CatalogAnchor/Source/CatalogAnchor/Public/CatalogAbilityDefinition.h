// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CatalogAbility.h"
#include "CatalogAbilityDefinition.generated.h"

/**
 * Asset de uma habilidade: tag (identidade), nome, cooldown e o comportamento (Instanced).
 * Concedido a atores via UCatalogAbilityComponent::GrantedAbilities.
 * Para achá-lo pelo AssetManager, registre o tipo "CatalogAbility" em PrimaryAssetTypesToScan.
 */
UCLASS(BlueprintType)
class CATALOGANCHOR_API UCatalogAbilityDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Tipo de primary asset: "CatalogAbility". */
	static const FPrimaryAssetType AbilityAssetType;

	/** Identidade da habilidade; é o que TryActivateAbility recebe. Ex.: Catalog.Ability.Dash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability")
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability")
	FText DisplayName;

	/** Segundos até poder ativar de novo, contados no relógio do servidor. 0 = sem cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 1.f;

	/** O que a habilidade faz. Compartilhado por todos os atores que usam este asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Catalog|Ability")
	TObjectPtr<UCatalogAbility> Behavior;

	/** FPrimaryAssetId(AbilityAssetType, GetFName()). */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
