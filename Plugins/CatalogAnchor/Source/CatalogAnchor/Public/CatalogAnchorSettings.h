// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CatalogAnchorSettings.generated.h"

/**
 * Project Settings → Plugins → Catalog Anchor (gravado em DefaultGame.ini).
 * Fonte única dos defaults do plugin: componentes e subsystems leem daqui em vez
 * de repetir números no código.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Catalog Anchor"))
class CATALOGANCHOR_API UCatalogAnchorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }

	// --- Interaction ---

	/** TraceDistance inicial de cada UCatalogInteractionComponent (cm). */
	UPROPERTY(config, EditAnywhere, Category = "Interaction", meta = (ClampMin = "10.0"))
	float DefaultTraceDistance = 250.f;

	// --- Save ---

	/**
	 * Versão de schema de save que este projeto declara usar. Se uma atualização do
	 * plugin mudar GCatalogSaveSchemaVersion, o módulo avisa no log ao iniciar:
	 * revise as migrações e então atualize este valor.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Save", meta = (ClampMin = "1"))
	int32 ExpectedSaveSchemaVersion = 2;

	/** Intervalo do autosave do UCatalogSaveSubsystem (s). 0 = desligado. */
	UPROPERTY(config, EditAnywhere, Category = "Save", meta = (ClampMin = "0.0"))
	float AutosaveIntervalSeconds = 0.f;

	/** Slot usado pelo autosave. */
	UPROPERTY(config, EditAnywhere, Category = "Save")
	FName AutosaveSlotName = TEXT("Autosave");

	/**
	 * Quantas versões anteriores de cada slot o save mantém (`<slot>_bak`, `<slot>_bak2`...).
	 * Um slot corrompido (arquivo ilegível ou CRC que não confere) é lido do backup mais novo íntegro.
	 * 0 = sem backup nem fallback.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Save", meta = (ClampMin = "0", ClampMax = "9"))
	int32 BackupCount = 1;

	// --- Inventory ---

	/** Capacidade inicial (número de pilhas) de cada UCatalogInventoryComponent. */
	UPROPERTY(config, EditAnywhere, Category = "Inventory", meta = (ClampMin = "1"))
	int32 DefaultInventoryCapacity = 24;

	// --- Ability ---

	/** Cooldown usado por TryTriggerDashStub quando CooldownSeconds < 0 (s). */
	UPROPERTY(config, EditAnywhere, Category = "Ability", meta = (ClampMin = "0.0"))
	float DefaultDashCooldown = 1.2f;

	/** Velocidade aplicada por TryTriggerDashStub (cm/s). */
	UPROPERTY(config, EditAnywhere, Category = "Ability", meta = (ClampMin = "0.0"))
	float DefaultDashStrength = 1200.f;
};
