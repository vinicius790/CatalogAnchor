// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "Templates/SubclassOf.h"
#include "CatalogAbility.generated.h"

class AActor;
class UAnimMontage;
class UDamageType;
class UWorld;

/**
 * Comportamento de uma habilidade "GAS-lite" (sem o plugin GameplayAbilities).
 * Vive como subobjeto Instanced de um UCatalogAbilityDefinition: a MESMA instância atende
 * todo ator que usa o asset (e todo mundo de PIE), então não guarde estado por ator aqui.
 *
 * Fluxo, orquestrado por UCatalogAbilityComponent:
 * - CanActivate: consulta sem efeito colateral; no servidor e, como estimativa, no cliente.
 * - Activate: efeito de gameplay; só com autoridade sobre o avatar.
 * - PlayCosmetics: animação/VFX; em todas as máquinas, via multicast do componente.
 *
 * Subclasses Blueprint: implemente os eventos K2_ (todos opcionais).
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class CATALOGANCHOR_API UCatalogAbility : public UObject
{
	GENERATED_BODY()

public:
	/** Avatar válido e K2_CanActivate concorda. Overrides: checagens nativas e depois Super. */
	virtual bool CanActivate(AActor* Avatar) const;

	/** Exige autoridade. Base: dispara K2_OnActivated e devolve true. Overrides: efeito e depois Super. */
	virtual bool Activate(AActor* Avatar);

	/** Cosmético (montagem, VFX). Base: dispara K2_OnCosmetics. */
	virtual void PlayCosmetics(AActor* Avatar);

	/** Mundo do avatar da chamada em curso: o outer é um asset, e sem isto os nós com WorldContext somem do Blueprint. */
	virtual UWorld* GetWorld() const override;

protected:
	/** Veto opcional em Blueprint. Padrão: true. */
	UFUNCTION(BlueprintNativeEvent, Category = "Catalog|Ability", meta = (DisplayName = "Can Activate"))
	bool K2_CanActivate(AActor* Avatar) const;
	virtual bool K2_CanActivate_Implementation(AActor* Avatar) const;

	/** Depois do efeito nativo, só no servidor. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Catalog|Ability", meta = (DisplayName = "On Activated"))
	void K2_OnActivated(AActor* Avatar);

	/** Em todas as máquinas que veem o avatar (servidor incluso). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Catalog|Ability", meta = (DisplayName = "On Cosmetics"))
	void K2_OnCosmetics(AActor* Avatar);

private:
	/** Avatar da chamada em curso; vazio fora de CanActivate/Activate/PlayCosmetics. */
	mutable TWeakObjectPtr<AActor> CurrentAvatar;
};

/**
 * Dash: mesma física de UCatalogGameplaySubsystem::TriggerDash — ACharacter recebe
 * LaunchCharacter; ator com física no root recebe impulso (mudança de velocidade).
 */
UCLASS(meta = (DisplayName = "Dash"))
class CATALOGANCHOR_API UCatalogAbility_Dash : public UCatalogAbility
{
	GENERATED_BODY()

public:
	UCatalogAbility_Dash();

	/** Velocidade do dash (cm/s). Padrão: Project Settings → Catalog Anchor → DefaultDashStrength. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability", meta = (ClampMin = "0.0"))
	float Strength;

	virtual bool CanActivate(AActor* Avatar) const override;
	virtual bool Activate(AActor* Avatar) override;
};

/**
 * Golpe corpo a corpo: varre uma esfera à frente do avatar e aplica dano uma vez por ator.
 * Activate (servidor) só faz o dano. A montagem toca em PlayCosmetics: tocada só no servidor
 * ela não apareceria nos clientes, por isso o multicast do componente a chama em todos.
 */
UCLASS(meta = (DisplayName = "Melee Sweep"))
class CATALOGANCHOR_API UCatalogAbility_MeleeSweep : public UCatalogAbility
{
	GENERATED_BODY()

public:
	/** Distância (cm) do centro do avatar até o centro da esfera no fim da varredura. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability", meta = (ClampMin = "0.0"))
	float Range = 150.f;

	/** Raio da esfera (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability", meta = (ClampMin = "1.0"))
	float Radius = 60.f;

	/** Dano por ator atingido (UGameplayStatics::ApplyDamage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability", meta = (ClampMin = "0.0"))
	float Damage = 20.f;

	/** Vazio = UDamageType. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability")
	TSubclassOf<UDamageType> DamageTypeClass;

	/** Opcional; só em ACharacter. Carregado sob demanda (LoadSynchronous): pré-carregue se o hitch importar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability")
	TSoftObjectPtr<UAnimMontage> Montage;

	virtual bool Activate(AActor* Avatar) override;
	virtual void PlayCosmetics(AActor* Avatar) override;
};
