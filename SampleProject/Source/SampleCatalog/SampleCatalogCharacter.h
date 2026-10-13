// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Character.h"
#include "SampleCatalogCharacter.generated.h"

class UCameraComponent;
class UCatalogAbilityComponent;
class UCatalogHealthComponent;
class UCatalogInteractionComponent;
class UCatalogInventoryComponent;
class UInputAction;
class UInputMappingContext;
class USamplePlayerSaveComponent;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/**
 * Personagem do sample: 3ª pessoa + os três componentes do plugin.
 * Entrada via Enhanced Input criado em código (sem assets): as ações e o mapping context
 * nascem na primeira vez que o pawn é controlado localmente — input nunca replica. O contexto é
 * adicionado ao ser possuído por um PlayerController local e removido ao perder o controller ou sair
 * do jogo, para não sobrar no LocalPlayer quando outro pawn é possuído.
 */
UCLASS()
class ASampleCatalogCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASampleCatalogCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Corpo visível (o mapa não tem malha de personagem): cilindro + "nariz" indicando a frente. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<UStaticMeshComponent> NoseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<UCatalogInteractionComponent> Interaction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<UCatalogInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<UCatalogAbilityComponent> Abilities;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<UCatalogHealthComponent> Health;

	/** Grava/lê posição e vida no save (participante do UCatalogSaveSubsystem). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog|Sample")
	TObjectPtr<USamplePlayerSaveComponent> PlayerSave;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	/** Morto (vida replicada) não pula: vale no cliente remoto e na predição do servidor. */
	virtual bool CanJumpInternal_Implementation() const override;

	/** Consome 1 poção e cura (o servidor confere inventário e vida). Sem parâmetros: _Validate só garante os componentes. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerUsePotion();

	/** Todas as máquinas: encerra a interação. Servidor: trava o movimento e agenda o respawn (atraso para os clientes verem a morte replicada). */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

	/** Servidor: volta ao centro da arena com vida cheia (o inventário é mantido). */
	void Respawn();

	FTimerHandle RespawnTimer;

private:
	/** Cria ações + mapping context uma vez (só na máquina que controla o pawn). */
	void EnsureInputObjects();

	/** Tira o MappingContext do LocalPlayer desse controller (se for um PlayerController local). */
	void RemoveInputContext(const AController* FromController) const;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void InteractPressed();
	void InteractReleased();
	void Dash();
	void Melee();
	void DropFirstItem();
	void UsePotion();
	void QuickSave();
	void QuickLoad();
	void ToggleSaveMenu();

	/** Aviso no HUD do jogador local (se houver). */
	void ShowHUDMessage(const FText& Text) const;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DashAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MeleeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DropAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> UseAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SaveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SaveMenuAction;
};
