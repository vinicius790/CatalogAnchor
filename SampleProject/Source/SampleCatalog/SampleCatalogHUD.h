// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SampleCatalogHUD.generated.h"

/**
 * HUD em Canvas (sem UMG nem assets): mira, alvo/prompt/hold da interação, inventário,
 * cooldowns, avisos de save/load e ajuda de teclas. Só lê a API pública dos componentes do plugin.
 */
UCLASS()
class ASampleCatalogHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Aviso no topo da tela por alguns segundos (substitui o anterior). */
	void ShowMessage(const FString& Text);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleSaveCompleted(FName SlotName, bool bSuccess);

	UFUNCTION()
	void HandleLoadCompleted(FName SlotName, bool bSuccess);

	FString Message;

	/** Em tempo real (s): o aviso some mesmo com o jogo pausado. */
	double MessageHideTime = 0.0;
};
