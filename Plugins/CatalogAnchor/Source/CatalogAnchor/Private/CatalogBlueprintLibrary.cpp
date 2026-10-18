// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogBlueprintLibrary.h"
#include "CatalogHealthComponent.h"
#include "CatalogInventoryComponent.h"
#include "CatalogSaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace CatalogBlueprintLibraryPrivate
{
	// Vida e inventário moram no pawn; quem tem o controller (HUD, GameMode) pergunta por ele.
	const AActor* ResolveOwner(const AActor* Actor)
	{
		if (const AController* Controller = Cast<AController>(Actor))
		{
			return Controller->GetPawn<AActor>(); // GetPawn() devolve TObjectPtr<APawn>, que não converte para const AActor* no alvo Game
		}
		return Actor;
	}
}

UCatalogSaveSubsystem* UCatalogBlueprintLibrary::GetSaveSubsystem(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCatalogSaveSubsystem>() : nullptr;
}

bool UCatalogBlueprintLibrary::SaveCatalogGame(const UObject* WorldContextObject, FName SlotName)
{
	UCatalogSaveSubsystem* Save = GetSaveSubsystem(WorldContextObject);
	return Save && Save->SaveGame(SlotName);
}

bool UCatalogBlueprintLibrary::LoadCatalogGame(const UObject* WorldContextObject, FName SlotName)
{
	UCatalogSaveSubsystem* Save = GetSaveSubsystem(WorldContextObject);
	return Save && Save->LoadGame(SlotName);
}

bool UCatalogBlueprintLibrary::AsyncSaveCatalogGame(const UObject* WorldContextObject, FName SlotName)
{
	UCatalogSaveSubsystem* Save = GetSaveSubsystem(WorldContextObject);
	return Save && Save->AsyncSaveGame(SlotName);
}

bool UCatalogBlueprintLibrary::AsyncLoadCatalogGame(const UObject* WorldContextObject, FName SlotName)
{
	UCatalogSaveSubsystem* Save = GetSaveSubsystem(WorldContextObject);
	return Save && Save->AsyncLoadGame(SlotName);
}

TArray<FCatalogSaveSlotInfo> UCatalogBlueprintLibrary::ListCatalogSaveSlots(const UObject* WorldContextObject)
{
	UCatalogSaveSubsystem* Save = GetSaveSubsystem(WorldContextObject);
	return Save ? Save->ListSaveSlots() : TArray<FCatalogSaveSlotInfo>();
}

bool UCatalogBlueprintLibrary::DeleteCatalogSaveSlot(const UObject* WorldContextObject, FName SlotName)
{
	UCatalogSaveSubsystem* Save = GetSaveSubsystem(WorldContextObject);
	return Save && Save->DeleteSlot(SlotName);
}

bool UCatalogBlueprintLibrary::DoesCatalogSaveSlotExist(const UObject* WorldContextObject, FName SlotName)
{
	const UCatalogSaveSubsystem* Save = GetSaveSubsystem(WorldContextObject);
	return Save && Save->DoesSlotExist(SlotName);
}

UCatalogHealthComponent* UCatalogBlueprintLibrary::GetHealthComponent(const AActor* Actor)
{
	const AActor* Owner = CatalogBlueprintLibraryPrivate::ResolveOwner(Actor);
	return Owner ? Owner->FindComponentByClass<UCatalogHealthComponent>() : nullptr;
}

bool UCatalogBlueprintLibrary::GetActorHealthFraction(const AActor* Actor, float& OutFraction)
{
	const UCatalogHealthComponent* Health = GetHealthComponent(Actor);
	OutFraction = Health ? Health->GetHealthFraction() : 0.f;
	return Health != nullptr;
}

bool UCatalogBlueprintLibrary::IsActorDead(const AActor* Actor)
{
	const UCatalogHealthComponent* Health = GetHealthComponent(Actor);
	return Health && Health->IsDead();
}

float UCatalogBlueprintLibrary::HealActor(AActor* Actor, float Amount)
{
	UCatalogHealthComponent* Health = GetHealthComponent(Actor);
	return Health ? Health->Heal(Amount) : 0.f;
}

UCatalogInventoryComponent* UCatalogBlueprintLibrary::GetInventoryComponent(const AActor* Actor)
{
	const AActor* Owner = CatalogBlueprintLibraryPrivate::ResolveOwner(Actor);
	return Owner ? Owner->FindComponentByClass<UCatalogInventoryComponent>() : nullptr;
}

int32 UCatalogBlueprintLibrary::GetActorItemCount(const AActor* Actor, const UCatalogItemDefinition* Item)
{
	const UCatalogInventoryComponent* Inventory = GetInventoryComponent(Actor);
	return Inventory ? Inventory->GetItemCount(Item) : 0;
}

bool UCatalogBlueprintLibrary::ActorHasItem(const AActor* Actor, const UCatalogItemDefinition* Item, int32 Quantity)
{
	const UCatalogInventoryComponent* Inventory = GetInventoryComponent(Actor);
	return Inventory && Inventory->HasItem(Item, Quantity);
}
