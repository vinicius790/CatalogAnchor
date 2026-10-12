// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogGameplaySubsystem.h"
#include "CatalogAnchor.h"
#include "CatalogAnchorSettings.h"
#include "CatalogDashAbilityStub.h"
#include "CatalogSaveSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

void UCatalogGameplaySubsystem::Deinitialize()
{
	DashReadyAt.Reset();
	Super::Deinitialize();
}

bool UCatalogGameplaySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UCatalogGameplaySubsystem::IsValidSlotName(FName SlotName)
{
	return UCatalogSaveSubsystem::IsValidSlotName(SlotName);
}

UCatalogSaveSubsystem* UCatalogGameplaySubsystem::GetSaveSubsystem(const TCHAR* Operation) const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UCatalogSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UCatalogSaveSubsystem>() : nullptr;
	if (!SaveSubsystem)
	{
		// Falha limpa em vez de gravar por fora: só o UCatalogSaveSubsystem sabe que slots estão ocupados.
		UE_LOG(LogCatalogAnchor, Warning, TEXT("%s: mundo sem GameInstance/UCatalogSaveSubsystem; nada foi feito."), Operation);
	}
	return SaveSubsystem;
}

bool UCatalogGameplaySubsystem::SaveToSlot(const FCatalogSavePayload& Payload, FName SlotName)
{
	UCatalogSaveSubsystem* SaveSubsystem = GetSaveSubsystem(TEXT("SaveToSlot"));
	return SaveSubsystem && SaveSubsystem->SavePayloadToSlot(Payload, SlotName);
}

bool UCatalogGameplaySubsystem::LoadFromSlot(FName SlotName, FCatalogSavePayload& OutPayload)
{
	UCatalogSaveSubsystem* SaveSubsystem = GetSaveSubsystem(TEXT("LoadFromSlot"));
	if (!SaveSubsystem || !SaveSubsystem->LoadPayloadFromSlot(SlotName, OutPayload))
	{
		return false;
	}
	OnSaveLoaded.Broadcast(OutPayload);
	return true;
}

bool UCatalogGameplaySubsystem::TryTriggerDashStub(AActor* Instigator, float CooldownSeconds)
{
	if (!FMath::IsFinite(CooldownSeconds))
	{
		return false;
	}
	const UCatalogAnchorSettings* Settings = GetDefault<UCatalogAnchorSettings>();
	const float Cooldown = CooldownSeconds < 0.f ? Settings->DefaultDashCooldown : CooldownSeconds;
	return TriggerDash(Instigator, Cooldown, Settings->DefaultDashStrength);
}

bool UCatalogGameplaySubsystem::TryTriggerDashFromAbility(AActor* Instigator, const UCatalogDashAbilityStub* Ability)
{
	if (!Ability)
	{
		return false;
	}
	return TriggerDash(Instigator, Ability->CooldownSeconds, Ability->ImpulseStrength);
}

bool UCatalogGameplaySubsystem::TriggerDash(AActor* Instigator, double CooldownSeconds, float Strength)
{
	if (!Instigator || !FMath::IsFinite(CooldownSeconds) || CooldownSeconds < 0.0 || !FMath::IsFinite(Strength))
	{
		return false;
	}

	// Movimento tem de nascer na autoridade: feito só no cliente, a correção do servidor desfaz.
	if (!Instigator->HasAuthority())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const double Now = World->GetTimeSeconds();
	const TWeakObjectPtr<AActor> Key(Instigator);
	if (const double* ReadyAt = DashReadyAt.Find(Key))
	{
		if (Now < *ReadyAt)
		{
			return false;
		}
	}

	// Stub sem montagem/predição (isso é trabalho do GAS — ver ARQUITETURA).
	const FVector DashVelocity = Instigator->GetActorForwardVector() * Strength;
	if (ACharacter* Character = Cast<ACharacter>(Instigator))
	{
		Character->LaunchCharacter(DashVelocity, /*bXYOverride*/ true, /*bZOverride*/ false);
	}
	else if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Instigator->GetRootComponent());
		Root && Root->IsSimulatingPhysics())
	{
		Root->AddImpulse(DashVelocity, NAME_None, /*bVelChange*/ true);
	}
	else
	{
		// Nada a impulsionar: não consome o cooldown.
		return false;
	}

	for (auto It = DashReadyAt.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	DashReadyAt.Add(Key, Now + CooldownSeconds);
	return true;
}
