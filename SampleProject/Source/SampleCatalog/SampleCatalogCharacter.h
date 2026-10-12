// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SampleCatalogCharacter.generated.h"

class UCameraComponent;
class UCatalogAbilityComponent;
class UCatalogHealthComponent;
class UCatalogInteractionComponent;
class UCatalogInventoryComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/**
 * Personagem do sample: 3ª pessoa + os três componentes do plugin.
 * Entrada via Enhanced Input criado em código (sem assets): as ações e o mapping context
 * nascem na primeira vez que o pawn é controlado localmente — input nunca replica.
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

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	/** Consome 1 poção e cura (o servidor confere inventário e vida). */
	UFUNCTION(Server, Reliable)
	void ServerUsePotion();

	/** Servidor: volta ao centro da arena com vida cheia (o inventário é mantido). */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

private:
	/** Cria ações + mapping context uma vez (só na máquina que controla o pawn). */
	void EnsureInputObjects();

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

	/** Aviso no HUD do jogador local (se houver). */
	void ShowHUDMessage(const FString& Text) const;

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
};
