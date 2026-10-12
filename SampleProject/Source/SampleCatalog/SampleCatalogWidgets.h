// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SampleCatalogWidgets.generated.h"

class UButton;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 * HUD em UMG montado em código (sem Widget Blueprint): vida, prompt + barra de hold da interação,
 * inventário, cooldowns, avisos e ajuda de teclas. Lê a API pública dos componentes do pawn a cada
 * frame (cooldown muda continuamente; o resto é barato). Não recebe clique: HitTestInvisible.
 */
UCLASS()
class USampleHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Aviso no topo da tela por alguns segundos (substitui o anterior). */
	void ShowMessage(const FText& Text);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InventoryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AbilitiesText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptText;

	/** Caixa da barra de hold: some quando não há interação em andamento. */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> InteractBarBox;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> InteractBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;

	/** Em tempo real (s): o aviso some mesmo com o jogo pausado. */
	double MessageHideTime = 0.0;
};

/**
 * Menu de slots de save (UMG em código). Lista os slots fixos (Quick, Slot1..3) e os .sav achados no
 * disco que o UCatalogSaveSubsystem consegue ler, com os metadados de cada um. Quem abre/fecha e troca
 * o modo de input é o ASampleCatalogHUD; Tab/Esc aqui só pedem para fechar.
 */
UCLASS()
class USampleSaveMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Relê os slots do disco e remonta a lista. */
	void RefreshSlots();

	void SetStatus(const FText& Text);

	/** Save/load só na autoridade: o subsystem do cliente remoto não tem participantes registrados. */
	bool CanUseSaves() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> SlotList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
};

/** Uma linha do menu: nome + metadados do slot e os botões Salvar / Carregar / Apagar. */
UCLASS()
class USampleSaveSlotRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Chame logo após criar (a árvore já existe: NativeOnInitialized roda na criação). */
	void Setup(FName InSlotName, USampleSaveMenuWidget* InMenu);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleLoadClicked();

	UFUNCTION()
	void HandleDeleteClicked();

	FName SlotName;

	TWeakObjectPtr<USampleSaveMenuWidget> Menu;

	/** Apagar pede um segundo clique ("Confirmar?"): não há como desfazer. */
	bool bConfirmDelete = false;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InfoText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LoadButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DeleteButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DeleteLabel;
};
