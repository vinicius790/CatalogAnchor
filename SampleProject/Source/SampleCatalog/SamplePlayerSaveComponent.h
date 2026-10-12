// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ICatalogSaveParticipant.h"
#include "SamplePlayerSaveComponent.generated.h"

class AController;
class APawn;

/** Estado do jogador gravado no save (dados puros, sem UObject). */
struct FSamplePlayerSaveState
{
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
	float Health = 0.f;
};

/**
 * Participante de save do sample: posição, yaw e vida do pawn dono, num FCatalogSaveRecord próprio
 * (chave "SamplePlayer"). Mesma regra do inventário: só na autoridade e só quando o pawn é do
 * PlayerController local (num listen server, o pawn de um cliente remoto não escreve no save do host).
 */
UCLASS(ClassGroup = (Catalog))
class USamplePlayerSaveComponent : public UActorComponent, public ICatalogSaveParticipant
{
	GENERATED_BODY()

public:
	USamplePlayerSaveComponent();

	//~ ICatalogSaveParticipant
	virtual FName GetSaveRecordKey() const override;
	virtual void WriteSaveData(FCatalogSavePayload& Payload) const override;
	virtual void ReadSaveData(const FCatalogSavePayload& Payload) override;

	/** Lógica pura: grava o estado no registro (Version + bytes). */
	static void WriteRecord(const FSamplePlayerSaveState& State, FCatalogSaveRecord& OutRecord);

	/** Lógica pura: false para versão desconhecida ou bytes de tamanho errado (registro corrompido). */
	static bool ReadRecord(const FCatalogSaveRecord& Record, FSamplePlayerSaveState& OutState);

	/** Lógica pura: posição utilizável = finita, acima de KillZ e dentro de ±HalfWorldExtent em todo eixo. */
	static bool IsLocationValid(const FVector& Location, double KillZ, double HalfWorldExtent);

	/** Lógica pura: vida NaN/infinita ou <= 0 volta cheia (MaxHealth); acima do máximo é limitada. */
	static float SanitizeHealth(float SavedHealth, float MaxHealth);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleOwnerControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	void UpdateSaveRegistration();
};
