// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CatalogAbilityComponent.generated.h"

class UCatalogAbilityDefinition;

/** Quando uma habilidade volta a ficar pronta. */
USTRUCT()
struct CATALOGANCHOR_API FCatalogAbilityCooldown
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag AbilityTag;

	/** Em AGameStateBase::GetServerWorldTimeSeconds(): o mesmo relógio no servidor e no cliente. */
	UPROPERTY()
	double ReadyAtServerTime = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCatalogAbilityActivated, FGameplayTag, AbilityTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCatalogAbilityFailed, FGameplayTag, AbilityTag, FName, Reason);

/**
 * Habilidades "GAS-lite": o servidor decide, cooldowns replicam só para o dono.
 * - TryActivateAbility no cliente envia ServerTryActivateAbility (o ator precisa ter
 *   conexão dona, ex.: o pawn possuído) e devolve só uma estimativa local.
 * - Sucesso: cooldown + MulticastAbilityActivated (cosmético e OnAbilityActivated em todas as máquinas).
 * - Falha: OnAbilityFailed no servidor e no cliente dono. Reason: "NotGranted", "NoBehavior",
 *   "Cooldown" ou "CannotActivate".
 * GrantedAbilities não replica: vem dos defaults, iguais no servidor e nos clientes.
 */
UCLASS(ClassGroup = (Catalog), meta = (BlueprintSpawnableComponent))
class CATALOGANCHOR_API UCatalogAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCatalogAbilityComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Ability")
	TArray<TObjectPtr<UCatalogAbilityDefinition>> GrantedAbilities;

	/**
	 * Servidor: ativa de fato e devolve o resultado.
	 * Cliente: pede ao servidor e devolve se localmente parece possível (concedida, fora de cooldown, CanActivate).
	 */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Ability")
	bool TryActivateAbility(FGameplayTag AbilityTag);

	/** Segundos até poder ativar de novo (0 = pronta). */
	UFUNCTION(BlueprintPure, Category = "Catalog|Ability")
	float GetCooldownRemaining(FGameplayTag AbilityTag) const;

	UFUNCTION(BlueprintPure, Category = "Catalog|Ability")
	bool IsOnCooldown(FGameplayTag AbilityTag) const;

	/** Definição concedida com essa tag exata, ou nullptr. */
	UFUNCTION(BlueprintPure, Category = "Catalog|Ability")
	UCatalogAbilityDefinition* FindAbilityDefinition(FGameplayTag AbilityTag) const;

	/** Servidor e clientes que veem o ator (multicast não confiável: pode faltar num cliente). */
	UPROPERTY(BlueprintAssignable, Category = "Catalog|Ability")
	FCatalogAbilityActivated OnAbilityActivated;

	/** Servidor e cliente dono. */
	UPROPERTY(BlueprintAssignable, Category = "Catalog|Ability")
	FCatalogAbilityFailed OnAbilityFailed;

	/** Lógica pura: max(0, ReadyAt - Now). NaN ou infinito em qualquer entrada → 0 (pronta). */
	static double ComputeCooldownRemaining(double ReadyAtServerTime, double NowServerTime);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** _Validate recusa tag inválida (cliente adulterado: desconecta). TryActivateAbility já filtra a tag antes. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerTryActivateAbility(FGameplayTag AbilityTag);

	/** Aviso de falha ao dono; não confiável porque é só feedback. */
	UFUNCTION(Client, Unreliable)
	void ClientAbilityFailed(FGameplayTag AbilityTag, FName Reason);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastAbilityActivated(FGameplayTag AbilityTag);

	/** Uma entrada por tag já usada; só o dono precisa saber (HUD de cooldown). */
	UPROPERTY(Replicated)
	TArray<FCatalogAbilityCooldown> Cooldowns;

private:
	bool ActivateWithAuthority(FGameplayTag AbilityTag);
	bool NotifyFailure(FGameplayTag AbilityTag, FName Reason);
	void StartCooldown(FGameplayTag AbilityTag, float CooldownSeconds);

	/** Tempo do servidor (estimado no cliente); sem GameState ainda, cai no relógio local. */
	double GetServerTimeSeconds() const;
};
