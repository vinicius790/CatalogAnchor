// Copyright 2026 Geraliza Castlevania. MIT License.
#include "SampleCatalogCharacter.h"
#include "CatalogAbilityComponent.h"
#include "CatalogAnchor.h"
#include "CatalogGameplayTags.h"
#include "CatalogHealthComponent.h"
#include "CatalogInteractionComponent.h"
#include "CatalogInventoryComponent.h"
#include "CatalogInventoryTypes.h"
#include "CatalogSaveSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "SampleAbilityCatalog.h"
#include "SampleCatalogHUD.h"
#include "SampleCatalogPickup.h"
#include "SampleItemCatalog.h"
#include "UObject/ConstructorHelpers.h"

namespace SampleCharacterPrivate
{
	const FName QuickSlot(TEXT("Quick"));
	constexpr float PotionHeal = 35.f;
	/** Centro da arena (piso em Z = -200 + meia cápsula). */
	const FVector RespawnLocation(0.f, 0.f, -100.f);

	UInputAction* NewAction(UObject* Outer, const TCHAR* Name, EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = ValueType;
		return Action;
	}
}

ASampleCatalogCharacter::ASampleCatalogCharacter()
{
	// Gira o corpo para onde anda; a câmera segue o controller (padrão 3ª pessoa).
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->SocketOffset = FVector(0.f, 60.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Cilindro de 100 cm escalado para a cápsula padrão (raio 34, meia-altura 88); só visual.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->SetStaticMesh(CylinderMesh.Object);
	BodyMesh->SetRelativeScale3D(FVector(0.68f, 0.68f, 1.76f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	NoseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NoseMesh"));
	NoseMesh->SetupAttachment(GetCapsuleComponent());
	NoseMesh->SetStaticMesh(CubeMesh.Object);
	NoseMesh->SetRelativeLocation(FVector(38.f, 0.f, 50.f));
	NoseMesh->SetRelativeScale3D(FVector(0.2f, 0.2f, 0.2f));
	NoseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Interaction = CreateDefaultSubobject<UCatalogInteractionComponent>(TEXT("Interaction"));
	// O trace sai da câmera (400 cm atrás do corpo): o default de 250 cm não passaria do próprio personagem.
	Interaction->TraceDistance = CameraBoom->TargetArmLength + 350.f;

	Inventory = CreateDefaultSubobject<UCatalogInventoryComponent>(TEXT("Inventory"));
	Inventory->DropPickupClass = ASampleCatalogPickup::StaticClass();

	Abilities = CreateDefaultSubobject<UCatalogAbilityComponent>(TEXT("Abilities"));
	Abilities->GrantedAbilities = { USampleAbilityCatalog::Dash(), USampleAbilityCatalog::Melee() };

	Health = CreateDefaultSubobject<UCatalogHealthComponent>(TEXT("Health"));
}

void ASampleCatalogCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Health->OnDeath.AddDynamic(this, &ASampleCatalogCharacter::HandleDeath);
	}
}

void ASampleCatalogCharacter::EnsureInputObjects()
{
	using namespace SampleCharacterPrivate;
	if (MappingContext)
	{
		return;
	}

	MoveAction = NewAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = NewAction(this, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	JumpAction = NewAction(this, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	InteractAction = NewAction(this, TEXT("IA_Interact"), EInputActionValueType::Boolean);
	DashAction = NewAction(this, TEXT("IA_Dash"), EInputActionValueType::Boolean);
	MeleeAction = NewAction(this, TEXT("IA_Melee"), EInputActionValueType::Boolean);
	DropAction = NewAction(this, TEXT("IA_Drop"), EInputActionValueType::Boolean);
	UseAction = NewAction(this, TEXT("IA_UsePotion"), EInputActionValueType::Boolean);
	SaveAction = NewAction(this, TEXT("IA_QuickSave"), EInputActionValueType::Boolean);
	LoadAction = NewAction(this, TEXT("IA_QuickLoad"), EInputActionValueType::Boolean);

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Sample"));

	// WASD num Axis2D: W/S viram eixo Y (swizzle); S e A são negados.
	auto MapMove = [this](const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(MoveAction, Key);
		if (bSwizzle)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MappingContext));
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
		}
	};
	MapMove(EKeys::W, true, false);
	MapMove(EKeys::S, true, true);
	MapMove(EKeys::D, false, false);
	MapMove(EKeys::A, false, true);

	// Mouse para cima = olhar para cima: nega só o Y.
	FEnhancedActionKeyMapping& LookMapping = MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	UInputModifierNegate* NegateY = NewObject<UInputModifierNegate>(MappingContext);
	NegateY->bX = false;
	NegateY->bY = true;
	NegateY->bZ = false;
	LookMapping.Modifiers.Add(NegateY);

	MappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	MappingContext->MapKey(InteractAction, EKeys::E);
	MappingContext->MapKey(DashAction, EKeys::LeftShift);
	MappingContext->MapKey(MeleeAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(DropAction, EKeys::G);
	MappingContext->MapKey(UseAction, EKeys::Q);
	MappingContext->MapKey(SaveAction, EKeys::F5);
	MappingContext->MapKey(LoadAction, EKeys::F9);
}

void ASampleCatalogCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	const APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}
	EnsureInputObjects();
	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(MappingContext, 0);
	}
}

void ASampleCatalogCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogCatalogAnchor, Error, TEXT("SampleCatalog: InputComponent nao e Enhanced (ver Config/DefaultInput.ini)."));
		return;
	}
	EnsureInputObjects();

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASampleCatalogCharacter::Move);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASampleCatalogCharacter::Look);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	// Hold-to-interact: pressionar inicia, soltar cancela (o servidor completa sozinho).
	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &ASampleCatalogCharacter::InteractPressed);
	Input->BindAction(InteractAction, ETriggerEvent::Completed, this, &ASampleCatalogCharacter::InteractReleased);
	Input->BindAction(DashAction, ETriggerEvent::Started, this, &ASampleCatalogCharacter::Dash);
	Input->BindAction(MeleeAction, ETriggerEvent::Started, this, &ASampleCatalogCharacter::Melee);
	Input->BindAction(DropAction, ETriggerEvent::Started, this, &ASampleCatalogCharacter::DropFirstItem);
	Input->BindAction(UseAction, ETriggerEvent::Started, this, &ASampleCatalogCharacter::UsePotion);
	Input->BindAction(SaveAction, ETriggerEvent::Started, this, &ASampleCatalogCharacter::QuickSave);
	Input->BindAction(LoadAction, ETriggerEvent::Started, this, &ASampleCatalogCharacter::QuickLoad);
}

void ASampleCatalogCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void ASampleCatalogCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ASampleCatalogCharacter::InteractPressed()
{
	Interaction->BeginInteract();
}

void ASampleCatalogCharacter::InteractReleased()
{
	Interaction->EndInteract();
}

void ASampleCatalogCharacter::Dash()
{
	Abilities->TryActivateAbility(CatalogGameplayTags::Ability_Dash);
}

void ASampleCatalogCharacter::Melee()
{
	Abilities->TryActivateAbility(CatalogGameplayTags::Ability_Melee);
}

void ASampleCatalogCharacter::DropFirstItem()
{
	const TArray<FCatalogInventoryStack> Stacks = Inventory->GetStacks();
	if (Stacks.IsEmpty() || !Stacks[0].Item)
	{
		ShowHUDMessage(TEXT("Inventario vazio"));
		return;
	}
	// Server RPC: o servidor confere se o item existe antes de remover.
	Inventory->ServerDropItem(Stacks[0].Item.Get(), 1);
}

void ASampleCatalogCharacter::UsePotion()
{
	if (!Inventory->HasItem(USampleItemCatalog::Potion()))
	{
		ShowHUDMessage(TEXT("Sem pocao"));
		return;
	}
	ServerUsePotion();
}

void ASampleCatalogCharacter::ServerUsePotion_Implementation()
{
	using namespace SampleCharacterPrivate;
	// Revalida no servidor: o cliente pode estar desatualizado ou mentindo.
	const UCatalogItemDefinition* Potion = USampleItemCatalog::Potion();
	if (Health->IsDead() || Health->GetHealthFraction() >= 1.f || !Inventory->HasItem(Potion))
	{
		return;
	}
	if (Inventory->RemoveItem(Potion, 1) == 1)
	{
		Health->Heal(PotionHeal);
	}
}

void ASampleCatalogCharacter::HandleDeath(AActor* Killer)
{
	using namespace SampleCharacterPrivate;
	Health->ResetHealth();
	GetCharacterMovement()->StopMovementImmediately();
	SetActorLocation(RespawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
}

void ASampleCatalogCharacter::QuickSave()
{
	// Save mora no GameInstance da autoridade: num cliente remoto não há o que gravar.
	if (!HasAuthority())
	{
		ShowHUDMessage(TEXT("Save/load so no host (standalone ou listen server)"));
		return;
	}
	if (UCatalogSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UCatalogSaveSubsystem>())
	{
		SaveSubsystem->SaveGame(SampleCharacterPrivate::QuickSlot);
	}
}

void ASampleCatalogCharacter::QuickLoad()
{
	if (!HasAuthority())
	{
		ShowHUDMessage(TEXT("Save/load so no host (standalone ou listen server)"));
		return;
	}
	if (UCatalogSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UCatalogSaveSubsystem>())
	{
		SaveSubsystem->LoadGame(SampleCharacterPrivate::QuickSlot);
	}
}

void ASampleCatalogCharacter::ShowHUDMessage(const FString& Text) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (ASampleCatalogHUD* HUD = PlayerController ? Cast<ASampleCatalogHUD>(PlayerController->GetHUD()) : nullptr)
	{
		HUD->ShowMessage(Text);
	}
}
