// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogPickupActor.h"
#include "CatalogAnchor.h"
#include "CatalogInventoryComponent.h"
#include "CatalogItemDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "CatalogPickupActor"

namespace CatalogPickupPrivate
{
	/** O inventário pode morar no Pawn ou no Controller (padrão Lyra): procura nos dois. */
	UCatalogInventoryComponent* FindInventory(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}
		if (UCatalogInventoryComponent* Inventory = Actor->FindComponentByClass<UCatalogInventoryComponent>())
		{
			return Inventory;
		}
		if (const APawn* AsPawn = Cast<APawn>(Actor))
		{
			const AController* PawnController = AsPawn->GetController();
			return PawnController ? PawnController->FindComponentByClass<UCatalogInventoryComponent>() : nullptr;
		}
		if (const AController* AsController = Cast<AController>(Actor))
		{
			const APawn* ControlledPawn = AsController->GetPawn();
			return ControlledPawn ? ControlledPawn->FindComponentByClass<UCatalogInventoryComponent>() : nullptr;
		}
		return nullptr;
	}
}

ACatalogPickupActor::ACatalogPickupActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// Pickup parado: replica o estado inicial (Item/Quantity) e dorme; OnInteract acorda ao mudar Quantity.
	NetDormancy = DORM_Initial;
	SetNetUpdateFrequency(2.f);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	// Movable: pickups são gerados e destruídos em runtime (drop).
	Mesh->SetMobility(EComponentMobility::Movable);
	// Só consulta: o trace de interação (Visibility) acha o pickup, mas nada colide fisicamente.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ACatalogPickupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACatalogPickupActor, Item);
	DOREPLIFETIME(ACatalogPickupActor, Quantity);
}

FText ACatalogPickupActor::GetInteractionPrompt_Implementation() const
{
	if (!Item)
	{
		return FText::GetEmpty();
	}
	const FText ItemName = Item->DisplayName.IsEmpty() ? FText::FromName(Item->GetFName()) : Item->DisplayName;
	return FText::Format(LOCTEXT("PickupPrompt", "Pegar {0} x{1}"), ItemName, FText::AsNumber(Quantity));
}

bool ACatalogPickupActor::OnInteract_Implementation(AActor* InstigatorActor)
{
	if (!HasAuthority() || !Item || Quantity <= 0 || IsActorBeingDestroyed())
	{
		return false;
	}

	UCatalogInventoryComponent* Inventory = CatalogPickupPrivate::FindInventory(InstigatorActor);
	if (!Inventory)
	{
		UE_LOG(LogCatalogAnchor, Verbose, TEXT("%s: %s não tem UCatalogInventoryComponent."),
			*GetName(), *GetNameSafe(InstigatorActor));
		return false;
	}

	const int32 Added = Inventory->AddItem(Item.Get(), Quantity);
	if (Added <= 0)
	{
		// Inventário cheio: o pickup continua no mundo.
		return false;
	}

	Quantity -= Added;
	if (Quantity <= 0)
	{
		Destroy();
	}
	else
	{
		FlushNetDormancy(); // coleta parcial: os clientes precisam do novo Quantity (prompt)
	}
	return true;
}

float ACatalogPickupActor::GetInteractionDuration_Implementation() const
{
	return 0.f;
}

bool ACatalogPickupActor::IsInteractionBlocked_Implementation(AActor* InstigatorActor) const
{
	return !Item || Quantity <= 0;
}

void ACatalogPickupActor::OnFocusChanged_Implementation(bool bHasFocus, AActor* InstigatorActor)
{
	if (Mesh)
	{
		Mesh->SetRenderCustomDepth(bHasFocus);
	}
}

#undef LOCTEXT_NAMESPACE
