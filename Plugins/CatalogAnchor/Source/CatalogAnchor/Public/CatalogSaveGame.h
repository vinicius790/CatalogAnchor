// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CatalogSaveTypes.h"
#include "CatalogSaveGame.generated.h"

/** Envelope gravado pelo backend SaveGame da plataforma (UGameplayStatics::SaveGameToSlot). */
UCLASS()
class CATALOGANCHOR_API UCatalogSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/**
	 * CRC32 do Payload (CatalogComputePayloadCrc), conferido no load. Vem ANTES do Payload de propósito:
	 * um arquivo truncado perde o fim do Payload, não o CRC, e a conferência pega.
	 * 0 = arquivo de antes do CRC existir (v0.4): carrega sem conferência.
	 */
	UPROPERTY(SaveGame)
	uint32 PayloadCrc = 0;

	UPROPERTY(SaveGame)
	FCatalogSavePayload Payload;
};
