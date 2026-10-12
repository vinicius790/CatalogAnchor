// Copyright 2026 Geraliza Castlevania. MIT License.
#include "SampleChestActor.h"
#include "CatalogInventoryComponent.h"
#include "CatalogItemDefinition.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "SampleCatalog"

ASampleChestActor::ASampleChestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// Root próprio: a escala das malhas fica nelas, longe da escala do spawn.
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetStaticMesh(CubeMesh.Object);
	Body->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
	Body->SetRelativeScale3D(FVector(0.6f, 1.f, 0.6f));
	// Bloqueia Visibility (canal do trace de interação) e o Pawn.
	Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));
	Lid->SetupAttachment(RootComponent);
	Lid->SetMobility(EComponentMobility::Movable);
	Lid->SetStaticMesh(CubeMesh.Object);
	Lid->SetRelativeLocation(FVector(0.f, 0.f, 66.f));
	Lid->SetRelativeScale3D(FVector(0.65f, 1.05f, 0.12f));
	Lid->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ASampleChestActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASampleChestActor, RequiredKey);
	DOREPLIFETIME(ASampleChestActor, bOpened);
}

bool ASampleChestActor::HasKey(const AActor* InstigatorActor) const
{
	if (!RequiredKey)
	{
		return true;
	}
	const UCatalogInventoryComponent* Inventory = InstigatorActor ? InstigatorActor->FindComponentByClass<UCatalogInventoryComponent>() : nullptr;
	return Inventory && Inventory->HasItem(RequiredKey);
}

FText ASampleChestActor::GetInteractionPrompt_Implementation() const
{
	if (bOpened)
	{
		return LOCTEXT("Chest.Empty", "Baú vazio");
	}
	// O prompt não recebe instigador: vale o jogador local (é quem lê a UI; o inventário dele replica para ele).
	if (!HasKey(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		return FText::Format(LOCTEXT("Chest.Locked", "Trancado (precisa de {0})"), RequiredKey->DisplayName);
	}
	return LOCTEXT("Chest.Open", "Abrir baú");
}

bool ASampleChestActor::OnInteract_Implementation(AActor* InstigatorActor)
{
	// Via Execute_: respeita override de subclasse Blueprint.
	if (!HasAuthority() || ICatalogInteractable::Execute_IsInteractionBlocked(this, InstigatorActor))
	{
		return false;
	}
	UCatalogInventoryComponent* Inventory = InstigatorActor ? InstigatorActor->FindComponentByClass<UCatalogInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return false;
	}

	bool bGaveAny = false;
	for (FCatalogInventoryStack& Stack : Loot)
	{
		const int32 Added = Inventory->AddItem(Stack.Item, Stack.Quantity);
		Stack.Quantity -= Added;
		bGaveAny |= Added > 0;
	}
	// Inventário cheio: o resto fica no baú, que continua fechado.
	Loot.RemoveAll([](const FCatalogInventoryStack& Stack) { return Stack.Quantity <= 0; });

	bOpened = Loot.IsEmpty();
	// OnRep não roda no servidor; o host (listen) também precisa ver a tampa sumir.
	OnRep_Opened();
	return bGaveAny;
}

float ASampleChestActor::GetInteractionDuration_Implementation() const
{
	return HoldSeconds;
}

bool ASampleChestActor::IsInteractionBlocked_Implementation(AActor* InstigatorActor) const
{
	return bOpened || !HasKey(InstigatorActor);
}

void ASampleChestActor::OnFocusChanged_Implementation(bool bHasFocus, AActor* InstigatorActor)
{
	// Custom Depth (r.CustomDepth=3 no DefaultEngine.ini): o contorno aparece com um material de pós-processo.
	Body->SetRenderCustomDepth(bHasFocus);
	Lid->SetRenderCustomDepth(bHasFocus);
}

void ASampleChestActor::OnRep_Opened()
{
	Lid->SetVisibility(!bOpened);
}

#undef LOCTEXT_NAMESPACE
