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
	UPROPERTY(SaveGame)
	FCatalogSavePayload Payload;
};
