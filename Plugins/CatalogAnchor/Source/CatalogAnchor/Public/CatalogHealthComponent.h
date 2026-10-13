// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CatalogHealthComponent.generated.h"

class AController;
class UDamageType;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCatalogHealthChanged, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCatalogDeath, AActor*, Killer);

/**
 * Vida replicada do dono. Escuta AActor::OnTakeAnyDamage, então qualquer UGameplayStatics::ApplyDamage
 * (ex.: UCatalogAbility_MeleeSweep) a reduz; ator com bCanBeDamaged = false não perde vida.
 * O servidor decide; Health replica para todos (barra sobre inimigos), e OnHealthChanged/OnDeath
 * disparam no servidor e nos clientes. OnDeath: uma vez por transição vivo→morto (ResetHealth revive).
 * Cliente vê só o valor replicado: dano e respawn no mesmo envio (ex.: ResetHealth dentro de OnDeath)
 * se fundem e o cliente pode não receber OnDeath; o estado final (GetHealth/IsDead) sempre converge,
 * então reaja a IsDead() e não conte só com OnDeath. Na 1ª replicação recebida o cliente emite
 * OnHealthChanged(Health, 0) (sem delta falso contra o default) e OnDeath(nullptr) se já nasce morto.
 */
UCLASS(ClassGroup = (Catalog), meta = (BlueprintSpawnableComponent))
class CATALOGANCHOR_API UCatalogHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCatalogHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(BlueprintAssignable, Category = "Catalog|Health")
	FCatalogHealthChanged OnHealthChanged;

	/** Vida chegou a 0. Killer = DamageCauser (pode ser nulo no cliente). */
	UPROPERTY(BlueprintAssignable, Category = "Catalog|Health")
	FCatalogDeath OnDeath;

	UFUNCTION(BlueprintPure, Category = "Catalog|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Catalog|Health")
	float GetHealthFraction() const;

	UFUNCTION(BlueprintPure, Category = "Catalog|Health")
	bool IsDead() const { return Health <= 0.f; }

	/** Cura (só autoridade; mortos não curam). Devolve quanto curou de fato. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Catalog|Health")
	float Heal(float Amount);

	/**
	 * Define a vida direto (ex.: restaurar um save). Só autoridade. Limita a [0, MaxHealth]; valor não
	 * finito é ignorado; não depende de bCanBeDamaged. Eventos com a mesma regra do dano (OnDeath ao
	 * chegar a 0); valor > 0 num morto o revive, como ResetHealth.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Catalog|Health")
	void SetHealth(float NewHealth);

	/** Volta a MaxHealth (respawn). Só autoridade. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Catalog|Health")
	void ResetHealth();

	/** Lógica pura: Current + Delta limitado a [0, Max]; Delta/estado inválido (NaN) mantém Current. */
	static float ComputeNewHealth(float Current, float Delta, float Max);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Catalog|Health")
	float Health = 100.f;

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);

private:
	/** Aplica no servidor e dispara os eventos (o OnRep faz o mesmo nos clientes). */
	void SetHealthWithAuthority(float NewHealth, AActor* Causer);

	void BroadcastChange(float OldHealth, AActor* Causer);

	/** Cliente: já recebeu o 1º valor replicado (antes dele, Health vale o default, não o do servidor). */
	bool bReceivedFirstHealth = false;
};
