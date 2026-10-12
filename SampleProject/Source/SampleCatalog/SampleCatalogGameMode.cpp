// Copyright 2026 Geraliza Castlevania. MIT License.
#include "SampleCatalogGameMode.h"
#include "CatalogInventoryTypes.h"
#include "CatalogItemDefinition.h"
#include "Engine/World.h"
#include "SampleArenaActor.h"
#include "SampleCatalogCharacter.h"
#include "SampleCatalogHUD.h"
#include "SampleCatalogPickup.h"
#include "SampleChestActor.h"
#include "SampleItemCatalog.h"
#include "SampleTrainingDummy.h"

namespace SampleGameModePrivate
{
	/**
	 * Sem PlayerStart o jogador nasce na origem, ANTES do BeginPlay do GameMode (LoadMap faz o
	 * spawn dos jogadores locais primeiro). Piso 200 cm abaixo: o pawn cai sobre ele em vez de
	 * nascer atravessado no chão.
	 */
	constexpr float FloorZ = -200.f;

	FCatalogInventoryStack Stack(const UCatalogItemDefinition* Item, int32 Quantity)
	{
		FCatalogInventoryStack Result;
		Result.Item = Item;
		Result.Quantity = Quantity;
		return Result;
	}
}

ASampleCatalogGameMode::ASampleCatalogGameMode()
{
	DefaultPawnClass = ASampleCatalogCharacter::StaticClass();
	HUDClass = ASampleCatalogHUD::StaticClass();
}

void ASampleCatalogGameMode::BeginPlay()
{
	using namespace SampleGameModePrivate;
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters AlwaysSpawn;
	AlwaysSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<ASampleArenaActor>(FVector(0.f, 0.f, FloorZ), FRotator::ZeroRotator, AlwaysSpawn);

	// Pickups em semicírculo à frente do spawn.
	SpawnPickup(USampleItemCatalog::Gem(), 3, FVector(400.f, -300.f, FloorZ + 40.f));
	SpawnPickup(USampleItemCatalog::Gem(), 5, FVector(450.f, 0.f, FloorZ + 40.f));
	SpawnPickup(USampleItemCatalog::Potion(), 2, FVector(400.f, 300.f, FloorZ + 40.f));
	SpawnPickup(USampleItemCatalog::Key(), 1, FVector(-500.f, 600.f, FloorZ + 40.f));

	// Baú aberto: só segurar E.
	if (ASampleChestActor* Chest = World->SpawnActor<ASampleChestActor>(FVector(800.f, -400.f, FloorZ), FRotator(0.f, 180.f, 0.f), AlwaysSpawn))
	{
		Chest->Loot = { Stack(USampleItemCatalog::Potion(), 3), Stack(USampleItemCatalog::Gem(), 10) };
	}

	// Baú trancado: fica "Trancado" até o jogador pegar a chave (que fica atrás do spawn).
	if (ASampleChestActor* LockedChest = World->SpawnActor<ASampleChestActor>(FVector(800.f, 400.f, FloorZ), FRotator(0.f, 180.f, 0.f), AlwaysSpawn))
	{
		LockedChest->RequiredKey = USampleItemCatalog::Key();
		LockedChest->Loot = { Stack(USampleItemCatalog::Gem(), 25) };
	}

	// Bonecos de treino para o golpe (Botão esq.); o cilindro tem o pivô no centro (180 cm de altura).
	World->SpawnActor<ASampleTrainingDummy>(FVector(-600.f, -300.f, FloorZ + 90.f), FRotator::ZeroRotator, AlwaysSpawn);
	World->SpawnActor<ASampleTrainingDummy>(FVector(-600.f, 300.f, FloorZ + 90.f), FRotator::ZeroRotator, AlwaysSpawn);
}

void ASampleCatalogGameMode::SpawnPickup(const UCatalogItemDefinition* Item, int32 Quantity, const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World || !Item)
	{
		return;
	}
	// Deferred: Item e Quantity precisam estar certos antes do BeginPlay e da primeira replicação.
	ASampleCatalogPickup* Pickup = World->SpawnActorDeferred<ASampleCatalogPickup>(ASampleCatalogPickup::StaticClass(),
		FTransform(Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return;
	}
	Pickup->Item = Item;
	Pickup->Quantity = Quantity;
	Pickup->FinishSpawning(FTransform(Location));
}
