// Copyright 2026 Geraliza Castlevania. MIT License.
#include "SamplePlayerSaveComponent.h"
#include "CatalogAnchor.h"
#include "CatalogHealthComponent.h"
#include "CatalogSaveSubsystem.h"
#include "CatalogSaveTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineDefines.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace SamplePlayerSavePrivate
{
	/** Chave estável do registro: não mude (saves gravados a procuram por ela). */
	const FName RecordKey(TEXT("SamplePlayer"));

	/** Começa em 1: 0 é o default e significa "sem versão" (ver FCatalogSaveRecord::Version). */
	constexpr int32 RecordVersion = 1;

	/** v1: X, Y, Z (double) + Yaw + Health (float). */
	constexpr int32 RecordBytes = 3 * sizeof(double) + 2 * sizeof(float);

	UCatalogSaveSubsystem* FindSaveSubsystem(const UActorComponent* Component)
	{
		const UWorld* World = Component ? Component->GetWorld() : nullptr;
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UCatalogSaveSubsystem>() : nullptr;
	}

	/** Mesmo código lê e escreve: a ordem dos campos não tem como divergir. */
	void SerializeState(FArchive& Archive, FSamplePlayerSaveState& State)
	{
		double X = State.Location.X;
		double Y = State.Location.Y;
		double Z = State.Location.Z;
		Archive << X << Y << Z << State.Yaw << State.Health;
		State.Location = FVector(X, Y, Z);
	}
}

USamplePlayerSaveComponent::USamplePlayerSaveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FName USamplePlayerSaveComponent::GetSaveRecordKey() const
{
	return SamplePlayerSavePrivate::RecordKey;
}

void USamplePlayerSaveComponent::WriteRecord(const FSamplePlayerSaveState& State, FCatalogSaveRecord& OutRecord)
{
	OutRecord.Version = SamplePlayerSavePrivate::RecordVersion;
	OutRecord.Data.Reset();
	FMemoryWriter Writer(OutRecord.Data, true);
	FSamplePlayerSaveState Copy = State;
	SamplePlayerSavePrivate::SerializeState(Writer, Copy);
}

bool USamplePlayerSaveComponent::ReadRecord(const FCatalogSaveRecord& Record, FSamplePlayerSaveState& OutState)
{
	using namespace SamplePlayerSavePrivate;
	if (Record.Version != RecordVersion || Record.Data.Num() != RecordBytes)
	{
		return false;
	}
	FMemoryReader Reader(Record.Data, true);
	FSamplePlayerSaveState State;
	SerializeState(Reader, State);
	if (Reader.IsError())
	{
		return false;
	}
	OutState = State;
	return true;
}

bool USamplePlayerSaveComponent::IsLocationValid(const FVector& Location, double KillZ, double HalfWorldExtent)
{
	return !Location.ContainsNaN() && Location.Z > KillZ && Location.GetAbsMax() <= HalfWorldExtent;
}

float USamplePlayerSaveComponent::SanitizeHealth(float SavedHealth, float MaxHealth)
{
	if (!FMath::IsFinite(SavedHealth) || SavedHealth <= 0.f)
	{
		return MaxHealth;
	}
	return FMath::Min(SavedHealth, MaxHealth);
}

void USamplePlayerSaveComponent::BeginPlay()
{
	Super::BeginPlay();

	// Só o servidor grava/lê save; clientes recebem posição e vida pela replicação.
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->HasAuthority())
	{
		return;
	}
	// No BeginPlay o pawn ainda não foi possuído: a decisão sai na possessão.
	OwnerPawn->ReceiveControllerChangedDelegate.AddDynamic(this, &USamplePlayerSaveComponent::HandleOwnerControllerChanged);
	UpdateSaveRegistration();
}

void USamplePlayerSaveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &USamplePlayerSaveComponent::HandleOwnerControllerChanged);
	}
	if (UCatalogSaveSubsystem* SaveSubsystem = SamplePlayerSavePrivate::FindSaveSubsystem(this))
	{
		SaveSubsystem->UnregisterParticipant(this);
	}
	Super::EndPlay(EndPlayReason);
}

void USamplePlayerSaveComponent::HandleOwnerControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	UpdateSaveRegistration();
}

void USamplePlayerSaveComponent::UpdateSaveRegistration()
{
	UCatalogSaveSubsystem* SaveSubsystem = SamplePlayerSavePrivate::FindSaveSubsystem(this);
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!SaveSubsystem || !OwnerPawn)
	{
		return;
	}
	const AController* OwnerController = OwnerPawn->GetController();
	if (!OwnerController)
	{
		// Sem controller (veículo, morte antes de um respawn): mantém o registro para o save ainda gravar este
		// pawn, como o inventário. Um pawn novo com a mesma chave o substitui no subsystem.
		return;
	}
	if (OwnerController->IsLocalPlayerController())
	{
		SaveSubsystem->RegisterParticipant(this);
	}
	else
	{
		SaveSubsystem->UnregisterParticipant(this);
	}
}

void USamplePlayerSaveComponent::WriteSaveData(FCatalogSavePayload& Payload) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}
	FSamplePlayerSaveState State;
	State.Location = OwnerPawn->GetActorLocation();
	State.Yaw = static_cast<float>(OwnerPawn->GetActorRotation().Yaw);
	// Sem componente de vida grava 0, que o load lê como "vida cheia".
	const UCatalogHealthComponent* Health = OwnerPawn->FindComponentByClass<UCatalogHealthComponent>();
	State.Health = Health ? Health->GetHealth() : 0.f;

	FCatalogSaveRecord* Record = CatalogFindSaveRecord(Payload, SamplePlayerSavePrivate::RecordKey);
	if (!Record)
	{
		Record = &Payload.Records.AddDefaulted_GetRef();
		Record->Key = SamplePlayerSavePrivate::RecordKey;
	}
	WriteRecord(State, *Record);
}

void USamplePlayerSaveComponent::ReadSaveData(const FCatalogSavePayload& Payload)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const FCatalogSaveRecord* Record = CatalogFindSaveRecord(Payload, SamplePlayerSavePrivate::RecordKey);
	if (!OwnerPawn || !Record)
	{
		// Save sem o registro (anterior a este componente): mantém posição e vida atuais.
		return;
	}
	FSamplePlayerSaveState State;
	if (!ReadRecord(*Record, State))
	{
		UE_LOG(LogCatalogAnchor, Warning, TEXT("SamplePlayer: registro v%d com %d bytes ilegível; estado atual mantido."),
			Record->Version, Record->Data.Num());
		return;
	}

	const UWorld* World = GetWorld();
	const AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;
	const double KillZ = WorldSettings ? WorldSettings->KillZ : -HALF_WORLD_MAX;
	if (IsLocationValid(State.Location, KillZ, HALF_WORLD_MAX))
	{
		// Mesmo efeito do respawn: zera a velocidade antes de teleportar.
		if (UPawnMovementComponent* Movement = OwnerPawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}
		const float Yaw = FMath::IsFinite(State.Yaw) ? State.Yaw : static_cast<float>(OwnerPawn->GetActorRotation().Yaw);
		// TeleportTo procura um ponto livre perto se algo ocupa o lugar salvo; false = nenhum serviu.
		if (OwnerPawn->TeleportTo(State.Location, FRotator(0.f, Yaw, 0.f)))
		{
			if (AController* OwnerController = OwnerPawn->GetController())
			{
				OwnerController->SetControlRotation(FRotator(OwnerController->GetControlRotation().Pitch, Yaw, 0.f));
			}
		}
		else
		{
			UE_LOG(LogCatalogAnchor, Warning, TEXT("SamplePlayer: sem espaço livre em %s; posição atual mantida."), *State.Location.ToString());
		}
	}
	else
	{
		UE_LOG(LogCatalogAnchor, Warning, TEXT("SamplePlayer: posição salva inválida (%s, KillZ %.0f); posição atual mantida."),
			*State.Location.ToString(), KillZ);
	}

	if (UCatalogHealthComponent* Health = OwnerPawn->FindComponentByClass<UCatalogHealthComponent>())
	{
		Health->SetHealth(SanitizeHealth(State.Health, Health->MaxHealth));
	}
}
