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
#include "SamplePlayerSaveComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "SampleCatalog"

namespace SampleCharacterPrivate
{
	const FName QuickSlot(TEXT("Quick"));
	constexpr float PotionHeal = 35.f;
	/** Tempo morto antes do respawn: vida 0 precisa replicar antes do ResetHealth (OnRep coalesce). */
	constexpr float RespawnSeconds = 2.f;
	/** Centro da arena (piso em Z = -200 + meia cápsula). */
	const FVector RespawnLocation(0.f, 0.f, -100.f);

	UInputAction* NewAction(UObject* Outer, const TCHAR* Name, EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = ValueType;
		return Action;
	}

	/** Subsystem de input do LocalPlayer, só se o controller for um PlayerController local. */
	UEnhancedInputLocalPlayerSubsystem* GetLocalInputSubsystem(const AController* Controller)
	{
		const APlayerController* PlayerController = Cast<APlayerController>(Controller);
		return PlayerController && PlayerController->IsLocalController()
			? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer())
			: nullptr;
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
	// Alcance medido a partir do pawn.
	Interaction->TraceDistance = 350.f;

	Inventory = CreateDefaultSubobject<UCatalogInventoryComponent>(TEXT("Inventory"));
	Inventory->DropPickupClass = ASampleCatalogPickup::StaticClass();

	Abilities = CreateDefaultSubobject<UCatalogAbilityComponent>(TEXT("Abilities"));
	Abilities->GrantedAbilities = { USampleAbilityCatalog::Dash(), USampleAbilityCatalog::Melee() };

	Health = CreateDefaultSubobject<UCatalogHealthComponent>(TEXT("Health"));

	PlayerSave = CreateDefaultSubobject<USamplePlayerSaveComponent>(TEXT("PlayerSave"));
}

void ASampleCatalogCharacter::BeginPlay()
{
	Super::BeginPlay();
	// Também nos clientes (o OnRep dispara OnDeath): a interação em andamento termina onde o input nasce.
	Health->OnDeath.AddDynamic(this, &ASampleCatalogCharacter::HandleDeath);
}

bool ASampleCatalogCharacter::CanJumpInternal_Implementation() const
{
	return !Health->IsDead() && Super::CanJumpInternal_Implementation();
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
	SaveMenuAction = NewAction(this, TEXT("IA_SaveMenu"), EInputActionValueType::Boolean);

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
	// Só abre: com o menu aberto o input é UI-only e o próprio menu fecha no Tab/Esc.
	MappingContext->MapKey(SaveMenuAction, EKeys::Tab);
}

void ASampleCatalogCharacter::NotifyControllerChanged()
{
	using namespace SampleCharacterPrivate;
	// Roda no servidor (PossessedBy/UnPossessed) e no cliente dono (OnRep_Controller). PreviousController
	// ainda é o dono anterior até o Super atualizá-lo: unpossess ou troca de controller tira o contexto dele.
	if (PreviousController != Controller)
	{
		RemoveInputContext(PreviousController);
	}
	Super::NotifyControllerChanged();

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetLocalInputSubsystem(Controller))
	{
		EnsureInputObjects();
		// Reaplicar o mesmo contexto só atualiza a prioridade (mapa por contexto): não duplica.
		InputSubsystem->AddMappingContext(MappingContext, 0);
	}
}

void ASampleCatalogCharacter::RemoveInputContext(const AController* FromController) const
{
	if (!MappingContext)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = SampleCharacterPrivate::GetLocalInputSubsystem(FromController))
	{
		InputSubsystem->RemoveMappingContext(MappingContext);
	}
}

void ASampleCatalogCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Pawn destruído ainda possuído (Destroy direto, réplica destruída no cliente, troca de mapa): sem isto o
	// contexto deste pawn ficaria ativo no LocalPlayer junto com o do próximo pawn possuído.
	RemoveInputContext(Controller);
	RemoveInputContext(PreviousController);
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	Super::EndPlay(EndPlayReason);
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
	Input->BindAction(SaveMenuAction, ETriggerEvent::Started, this, &ASampleCatalogCharacter::ToggleSaveMenu);
}

void ASampleCatalogCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller || Health->IsDead())
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
	if (Health->IsDead())
	{
		return;
	}
	Interaction->BeginInteract();
}

void ASampleCatalogCharacter::InteractReleased()
{
	Interaction->EndInteract();
}

void ASampleCatalogCharacter::Dash()
{
	if (Health->IsDead())
	{
		return;
	}
	Abilities->TryActivateAbility(CatalogGameplayTags::Ability_Dash);
}

void ASampleCatalogCharacter::Melee()
{
	if (Health->IsDead())
	{
		return;
	}
	Abilities->TryActivateAbility(CatalogGameplayTags::Ability_Melee);
}

void ASampleCatalogCharacter::DropFirstItem()
{
	if (Health->IsDead())
	{
		return;
	}
	const TArray<FCatalogInventoryStack> Stacks = Inventory->GetStacks();
	if (Stacks.IsEmpty() || !Stacks[0].Item)
	{
		ShowHUDMessage(LOCTEXT("Msg.InventoryEmpty", "Inventário vazio"));
		return;
	}
	// Server RPC: o servidor confere se o item existe antes de remover.
	Inventory->ServerDropItem(Stacks[0].Item.Get(), 1);
}

void ASampleCatalogCharacter::UsePotion()
{
	if (Health->IsDead())
	{
		return;
	}
	if (!Inventory->HasItem(USampleItemCatalog::Potion()))
	{
		ShowHUDMessage(LOCTEXT("Msg.NoPotion", "Sem poção"));
		return;
	}
	ServerUsePotion();
}

bool ASampleCatalogCharacter::ServerUsePotion_Validate()
{
	// Falso derruba o cliente: vida/inventário do momento (morto, cheio, sem poção) ficam para a _Implementation.
	return Health && Inventory;
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
	// Habilidades e poção já recusam morto no servidor; MOVE_None cobre pulo/movimento do cliente.
	Interaction->EndInteract();
	if (!HasAuthority())
	{
		return;
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &ASampleCatalogCharacter::Respawn, SampleCharacterPrivate::RespawnSeconds, false);
}

void ASampleCatalogCharacter::Respawn()
{
	using namespace SampleCharacterPrivate;
	// Um load durante o atraso pode já ter restaurado vida/posição: só reposiciona quem continua morto.
	if (Health->IsDead())
	{
		Health->ResetHealth();
		SetActorLocation(RespawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ASampleCatalogCharacter::QuickSave()
{
	// Save mora no GameInstance da autoridade: num cliente remoto não há o que gravar.
	if (!HasAuthority())
	{
		ShowHUDMessage(LOCTEXT("Msg.HostOnly", "Save/load só no host (standalone ou listen server)"));
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
		ShowHUDMessage(LOCTEXT("Msg.HostOnly", "Save/load só no host (standalone ou listen server)"));
		return;
	}
	if (UCatalogSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UCatalogSaveSubsystem>())
	{
		SaveSubsystem->LoadGame(SampleCharacterPrivate::QuickSlot);
	}
}

void ASampleCatalogCharacter::ToggleSaveMenu()
{
	const APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (ASampleCatalogHUD* HUD = PlayerController ? Cast<ASampleCatalogHUD>(PlayerController->GetHUD()) : nullptr)
	{
		HUD->ToggleSaveMenu();
	}
}

void ASampleCatalogCharacter::ShowHUDMessage(const FText& Text) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (ASampleCatalogHUD* HUD = PlayerController ? Cast<ASampleCatalogHUD>(PlayerController->GetHUD()) : nullptr)
	{
		HUD->ShowMessage(Text);
	}
}

#undef LOCTEXT_NAMESPACE
