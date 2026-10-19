// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogAbilityComponent.h"
#include "CatalogAbility.h"
#include "CatalogAbilityDefinition.h"
#include "CatalogAnchor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace CatalogAbilityFailure
{
	const FName NotGranted(TEXT("NotGranted"));
	const FName NoBehavior(TEXT("NoBehavior"));
	const FName Cooldown(TEXT("Cooldown"));
	const FName CannotActivate(TEXT("CannotActivate"));
}

UCatalogAbilityComponent::UCatalogAbilityComponent()
{
	// Cooldown é calculado sob demanda (relógio do servidor): não precisa de tick.
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCatalogAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UCatalogAbilityComponent, Cooldowns, COND_OwnerOnly);
}

double UCatalogAbilityComponent::ComputeCooldownRemaining(double ReadyAtServerTime, double NowServerTime)
{
	const double Remaining = ReadyAtServerTime - NowServerTime;
	// NaN falha em qualquer comparação; infinito travaria a habilidade para sempre.
	return (Remaining > 0.0 && FMath::IsFinite(Remaining)) ? Remaining : 0.0;
}

double UCatalogAbilityComponent::GetServerTimeSeconds() const
{
	// Ponteiros não-const: a doc 5.8 lista GetGameState/GetServerWorldTimeSeconds sem const.
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	if (AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
}

UCatalogAbilityDefinition* UCatalogAbilityComponent::FindAbilityDefinition(FGameplayTag AbilityTag) const
{
	if (!AbilityTag.IsValid())
	{
		return nullptr;
	}
	for (UCatalogAbilityDefinition* Definition : GrantedAbilities)
	{
		if (Definition && Definition->AbilityTag == AbilityTag)
		{
			return Definition;
		}
	}
	return nullptr;
}

float UCatalogAbilityComponent::GetCooldownRemaining(FGameplayTag AbilityTag) const
{
	const FCatalogAbilityCooldown* Entry = Cooldowns.FindByPredicate(
		[&AbilityTag](const FCatalogAbilityCooldown& Item) { return Item.AbilityTag == AbilityTag; });
	if (!Entry)
	{
		return 0.f;
	}
	return static_cast<float>(ComputeCooldownRemaining(Entry->ReadyAtServerTime, GetServerTimeSeconds()));
}

bool UCatalogAbilityComponent::IsOnCooldown(FGameplayTag AbilityTag) const
{
	return GetCooldownRemaining(AbilityTag) > 0.f;
}

bool UCatalogAbilityComponent::TryActivateAbility(FGameplayTag AbilityTag)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	if (!Owner->HasAuthority())
	{
		// Tag inválida nem sai do cliente: o _Validate do servidor desconectaria quem só errou o Blueprint.
		if (!AbilityTag.IsValid())
		{
			OnAbilityFailed.Broadcast(AbilityTag, CatalogAbilityFailure::NotGranted);
			return false;
		}
		// O servidor decide e avisa falhas; aqui só uma estimativa para o input/HUD responder já.
		ServerTryActivateAbility(AbilityTag);
		const UCatalogAbilityDefinition* Definition = FindAbilityDefinition(AbilityTag);
		return Definition && Definition->Behavior && !IsOnCooldown(AbilityTag)
			&& Definition->Behavior->CanActivate(Owner);
	}
	return ActivateWithAuthority(AbilityTag);
}

bool UCatalogAbilityComponent::ServerTryActivateAbility_Validate(FGameplayTag AbilityTag)
{
	return AbilityTag.IsValid();
}

void UCatalogAbilityComponent::ServerTryActivateAbility_Implementation(FGameplayTag AbilityTag)
{
	ActivateWithAuthority(AbilityTag);
}

bool UCatalogAbilityComponent::ActivateWithAuthority(FGameplayTag AbilityTag)
{
	AActor* Avatar = GetOwner();
	if (!Avatar)
	{
		return false;
	}

	const UCatalogAbilityDefinition* Definition = FindAbilityDefinition(AbilityTag);
	if (!Definition)
	{
		return NotifyFailure(AbilityTag, CatalogAbilityFailure::NotGranted);
	}
	UCatalogAbility* Behavior = Definition->Behavior;
	if (!Behavior)
	{
		return NotifyFailure(AbilityTag, CatalogAbilityFailure::NoBehavior);
	}
	if (IsOnCooldown(AbilityTag))
	{
		return NotifyFailure(AbilityTag, CatalogAbilityFailure::Cooldown);
	}
	if (!Behavior->CanActivate(Avatar) || !Behavior->Activate(Avatar))
	{
		return NotifyFailure(AbilityTag, CatalogAbilityFailure::CannotActivate);
	}

	StartCooldown(AbilityTag, Definition->CooldownSeconds);
	MulticastAbilityActivated(AbilityTag);
	return true;
}

bool UCatalogAbilityComponent::NotifyFailure(FGameplayTag AbilityTag, FName Reason)
{
	UE_LOG(LogCatalogAnchor, Verbose, TEXT("Habilidade '%s' falhou em %s: %s."),
		*AbilityTag.ToString(), *GetNameSafe(GetOwner()), *Reason.ToString());
	OnAbilityFailed.Broadcast(AbilityTag, Reason);
	ClientAbilityFailed(AbilityTag, Reason);
	return false;
}

void UCatalogAbilityComponent::ClientAbilityFailed_Implementation(FGameplayTag AbilityTag, FName Reason)
{
	// Dono local (listen server, standalone) ou ator sem dono: o RPC roda no próprio servidor,
	// que já disparou o evento em NotifyFailure.
	const AActor* Owner = GetOwner();
	if (Owner && !Owner->HasAuthority())
	{
		OnAbilityFailed.Broadcast(AbilityTag, Reason);
	}
}

void UCatalogAbilityComponent::MulticastAbilityActivated_Implementation(FGameplayTag AbilityTag)
{
	// Roda no servidor e nos clientes: é aqui que a montagem/VFX aparece para todos.
	if (const UCatalogAbilityDefinition* Definition = FindAbilityDefinition(AbilityTag))
	{
		if (UCatalogAbility* Behavior = Definition->Behavior)
		{
			Behavior->PlayCosmetics(GetOwner());
		}
	}
	OnAbilityActivated.Broadcast(AbilityTag);
}

void UCatalogAbilityComponent::StartCooldown(FGameplayTag AbilityTag, float CooldownSeconds)
{
	if (!(CooldownSeconds > 0.f) || !FMath::IsFinite(CooldownSeconds))
	{
		return;
	}

	FCatalogAbilityCooldown* Entry = Cooldowns.FindByPredicate(
		[&AbilityTag](const FCatalogAbilityCooldown& Item) { return Item.AbilityTag == AbilityTag; });
	if (!Entry)
	{
		Entry = &Cooldowns.AddDefaulted_GetRef();
		Entry->AbilityTag = AbilityTag;
	}
	Entry->ReadyAtServerTime = GetServerTimeSeconds() + CooldownSeconds;
}
