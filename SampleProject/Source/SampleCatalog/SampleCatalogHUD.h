// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SampleCatalogHUD.generated.h"

class USampleHUDWidget;
class USampleSaveMenuWidget;

/**
 * Dono da UI do jogador local. Texto e painéis são UMG em código (USampleHUDWidget e o menu de saves);
 * no Canvas ficou só o que é projetado do mundo: mira, marcador do alvo e barras sobre os bonecos.
 */
UCLASS()
class ASampleCatalogHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Aviso no topo da tela por alguns segundos (substitui o anterior). */
	void ShowMessage(const FText& Text);

	/** Abre/fecha o menu de saves, trocando o input entre UI e jogo. */
	void ToggleSaveMenu();

	void CloseSaveMenu();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Cria o HUD UMG na primeira vez em que o PlayerController já tem LocalPlayer (CreateWidget exige). */
	USampleHUDWidget* EnsureHUDWidget();

	void OpenSaveMenu();

	UFUNCTION()
	void HandleSaveCompleted(FName SlotName, bool bSuccess);

	UFUNCTION()
	void HandleLoadCompleted(FName SlotName, bool bSuccess);

	UPROPERTY(Transient)
	TObjectPtr<USampleHUDWidget> HUDWidget;

	/** Não nulo = menu aberto. Recriado a cada abertura: a lista sempre reflete o disco. */
	UPROPERTY(Transient)
	TObjectPtr<USampleSaveMenuWidget> SaveMenu;
};
