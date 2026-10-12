// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogHealthComponent.h"
#include "CatalogAnchor.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "Net/UnrealNetwork.h"

UCatalogHealthComponent::UCatalogHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

float UCatalogHealthComponent::ComputeNewHealth(float Current, float Delta, float Max)
{
	if (!FMath::IsFinite(Current) || !FMath::IsFinite(Delta) || !FMath::IsFinite(Max) || Max <= 0.f)
	{
		return FMath::IsFinite(Current) ? Current : 0.f;
	}
	return FMath::Clamp(Current + Delta, 0.f, Max);
}

void UCatalogHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		Health = MaxHealth;
		Owner->OnTakeAnyDamage.AddDynamic(this, &UCatalogHealthComponent::HandleTakeAnyDamage);
	}
}

void UCatalogHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.RemoveDynamic(this, &UCatalogHealthComponent::HandleTakeAnyDamage);
	}
	Super::EndPlay(EndPlayReason);
}

void UCatalogHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Todos veem: barra de vida sobre inimigos/bonecos.
	DOREPLIFETIME(UCatalogHealthComponent, Health);
}

float UCatalogHealthComponent::GetHealthFraction() const
{
	return MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f;
}

void UCatalogHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	AController* InstigatedBy, AActor* DamageCauser)
{
	// Dano negativo não cura por aqui (use Heal): evita "dano" vindo de fora virar cura.
	if (IsDead() || !FMath::IsFinite(Damage) || Damage <= 0.f)
	{
		return;
	}
	SetHealthWithAuthority(ComputeNewHealth(Health, -Damage, MaxHealth), DamageCauser);
}

float UCatalogHealthComponent::Heal(float Amount)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || IsDead() || !FMath::IsFinite(Amount) || Amount <= 0.f)
	{
		return 0.f;
	}
	const float OldHealth = Health;
	SetHealthWithAuthority(ComputeNewHealth(Health, Amount, MaxHealth), nullptr);
	return Health - OldHealth;
}

void UCatalogHealthComponent::ResetHealth()
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		SetHealthWithAuthority(MaxHealth, nullptr);
	}
}

void UCatalogHealthComponent::SetHealthWithAuthority(float NewHealth, AActor* Causer)
{
	const float OldHealth = Health;
	if (NewHealth == OldHealth)
	{
		return;
	}
	Health = NewHealth;
	// OnRep não roda no servidor: o host (listen/standalone) recebe os eventos por aqui.
	BroadcastChange(OldHealth, Causer);
}

void UCatalogHealthComponent::OnRep_Health(float OldHealth)
{
	BroadcastChange(OldHealth, nullptr);
}

void UCatalogHealthComponent::BroadcastChange(float OldHealth, AActor* Causer)
{
	OnHealthChanged.Broadcast(Health, Health - OldHealth);
	if (OldHealth > 0.f && Health <= 0.f)
	{
		UE_LOG(LogCatalogAnchor, Verbose, TEXT("%s morreu (causador: %s)."), *GetNameSafe(GetOwner()), *GetNameSafe(Causer));
		OnDeath.Broadcast(Causer);
	}
}
