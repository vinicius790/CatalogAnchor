// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "CatalogDashAbilityStub.generated.h"

/**
 * LEGADO (v0.1): placeholder pedagógico da "Gameplay Ability (dash)" do MVP §8.
 * Código novo: UCatalogAbilityDefinition + UCatalogAbility_Dash num UCatalogAbilityComponent
 * (cooldown replicado, Gameplay Tags, Server RPC). Mantido para não quebrar Blueprints da v0.1.
 * Consumido por UCatalogGameplaySubsystem::TryTriggerDashFromAbility.
 *
 * Sem depender de GameplayAbilities neste scaffold (plugin opcional no .uplugin).
 * Quando GAS estiver habilitado, troque esta UObject por UGameplayAbility e
 * mova a lógica de cooldown/montagem para ActivateAbility + CommitAbility.
 *
 * NÃO é um sistema de combate completo nem um cooked montage.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class CATALOGANCHOR_API UCatalogDashAbilityStub : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Ability", meta = (ClampMin = "0.1"))
	float CooldownSeconds = 1.2f;

	/** Velocidade do dash (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Ability", meta = (ClampMin = "0.0"))
	float ImpulseStrength = 1200.f;

	/** Reservado para a variante GAS (ainda não lido pelo stub). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Ability")
	FName MontageSlot = TEXT("FullBody");

	/** Montagem da variante GAS — asset real vive no Content do seu projeto, não neste repo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Ability", meta = (AllowedClasses = "/Script/Engine.AnimMontage"))
	FSoftObjectPath MontagePath;
};
