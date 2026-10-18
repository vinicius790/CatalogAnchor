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
		if (!bHealthPreset)
		{
			Health = MaxHealth;
		}
		Owner->OnTakeAnyDamage.AddDynamic(this, &UCatalogHealthComponent::HandleTakeAnyDamage);
	}
	else
	{
		// O estado inicial replicado chega (e dispara OnRep) antes do BeginPlay do cliente. Se não chegou,
		// o servidor estava no default: o valor local já é a base certa e o 1º OnRep traz delta real.
		bReceivedFirstHealth = true;
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
	// AActor::TakeDamage não checa bCanBeDamaged (só APawn checa): respeitamos aqui para todo ator.
	if (IsDead() || !FMath::IsFinite(Damage) || Damage <= 0.f || (DamagedActor && !DamagedActor->CanBeDamaged()))
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
	// Delta calculado antes de notificar: um ouvinte reentrante pode mudar Health e falsear o retorno.
	const float NewHealth = ComputeNewHealth(Health, Amount, MaxHealth);
	const float Healed = NewHealth - Health;
	SetHealthWithAuthority(NewHealth, nullptr);
	return Healed;
}

void UCatalogHealthComponent::SetHealth(float NewHealth)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !FMath::IsFinite(NewHealth))
	{
		return;
	}
	// Clamp direto (Current + (New - Current) perderia bits e o save não restauraria o valor exato);
	// MaxHealth inválido mantém a vida atual, como no dano.
	if (!FMath::IsFinite(MaxHealth) || MaxHealth <= 0.f)
	{
		return;
	}
	SetHealthWithAuthority(FMath::Clamp(NewHealth, 0.f, MaxHealth), nullptr);
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
	bHealthPreset |= !HasBegunPlay();
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
	if (!bReceivedFirstHealth)
	{
		// OldHealth é só o default da classe (≠ MaxHealth se customizado): delta falso. Anuncia o valor
		// atual sem delta; morto na entrada ainda avisa OnDeath (esconder boneco morto de quem chega depois).
		bReceivedFirstHealth = true;
		BroadcastChange(Health, nullptr);
		if (IsDead())
		{
			OnDeath.Broadcast(nullptr);
		}
		return;
	}
	BroadcastChange(OldHealth, nullptr);
}

void UCatalogHealthComponent::BroadcastChange(float OldHealth, AActor* Causer)
{
	// Captura antes de notificar: um ouvinte pode causar dano/cura reentrante e mudar Health,
	// e a morte seria detectada de novo aqui (OnDeath duplo) ou com o valor errado.
	const float NewHealth = Health;
	const bool bDied = OldHealth > 0.f && NewHealth <= 0.f;
	OnHealthChanged.Broadcast(NewHealth, NewHealth - OldHealth);
	if (bDied)
	{
		UE_LOG(LogCatalogAnchor, Verbose, TEXT("%s morreu (causador: %s)."), *GetNameSafe(GetOwner()), *GetNameSafe(Causer));
		OnDeath.Broadcast(Causer);
	}
}
