// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CatalogSaveTypes.h"
#include "CatalogBlueprintLibrary.generated.h"

class AActor;
class UCatalogHealthComponent;
class UCatalogInventoryComponent;
class UCatalogItemDefinition;
class UCatalogSaveSubsystem;

/**
 * Atalhos de Blueprint sobre a API pública do plugin: save/load/lista/apagar sem precisar
 * achar o subsystem no grafo, e consultas de vida/inventário a partir de qualquer ator.
 * Não tem regra própria: cada função delega ao subsystem/componente e falha limpo
 * (false/0/nullptr) quando o mundo, o GameInstance ou o componente não existem.
 */
UCLASS()
class CATALOGANCHOR_API UCatalogBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// --- Save ---

	/** UCatalogSaveSubsystem do GameInstance do mundo do contexto; nullptr sem mundo ou GameInstance. */
	UFUNCTION(BlueprintPure, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject"))
	static UCatalogSaveSubsystem* GetSaveSubsystem(const UObject* WorldContextObject);

	/** = UCatalogSaveSubsystem::SaveGame. */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject"))
	static bool SaveCatalogGame(const UObject* WorldContextObject, FName SlotName);

	/** = UCatalogSaveSubsystem::LoadGame. */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject"))
	static bool LoadCatalogGame(const UObject* WorldContextObject, FName SlotName);

	/** = UCatalogSaveSubsystem::AsyncSaveGame (resultado em OnSaveCompleted). */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject"))
	static bool AsyncSaveCatalogGame(const UObject* WorldContextObject, FName SlotName);

	/** = UCatalogSaveSubsystem::AsyncLoadGame (resultado em OnLoadCompleted). */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject"))
	static bool AsyncLoadCatalogGame(const UObject* WorldContextObject, FName SlotName);

	/** = UCatalogSaveSubsystem::ListSaveSlots. Vazio sem subsystem. */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject"))
	static TArray<FCatalogSaveSlotInfo> ListCatalogSaveSlots(const UObject* WorldContextObject);

	/** = UCatalogSaveSubsystem::DeleteSlot (apaga também os backups). */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject"))
	static bool DeleteCatalogSaveSlot(const UObject* WorldContextObject, FName SlotName);

	/** = UCatalogSaveSubsystem::DoesSlotExist. */
	UFUNCTION(BlueprintPure, Category = "Catalog|Save", meta = (WorldContext = "WorldContextObject"))
	static bool DoesCatalogSaveSlotExist(const UObject* WorldContextObject, FName SlotName);

	// --- Vida e inventário ---

	/** Componente de vida do ator (para um Controller, o do pawn controlado). nullptr se não houver. */
	UFUNCTION(BlueprintPure, Category = "Catalog|Health")
	static UCatalogHealthComponent* GetHealthComponent(const AActor* Actor);

	/** true e a fração 0..1 se o ator tem vida. */
	UFUNCTION(BlueprintPure, Category = "Catalog|Health")
	static bool GetActorHealthFraction(const AActor* Actor, float& OutFraction);

	/** false também para quem não tem componente de vida. */
	UFUNCTION(BlueprintPure, Category = "Catalog|Health")
	static bool IsActorDead(const AActor* Actor);

	/** = UCatalogHealthComponent::Heal (só na autoridade). Devolve o que foi curado; 0 sem componente. */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Health")
	static float HealActor(AActor* Actor, float Amount);

	/** Componente de inventário do ator (para um Controller, o do pawn controlado). nullptr se não houver. */
	UFUNCTION(BlueprintPure, Category = "Catalog|Inventory")
	static UCatalogInventoryComponent* GetInventoryComponent(const AActor* Actor);

	/** Quantidade do item no inventário do ator; 0 sem inventário ou sem item. */
	UFUNCTION(BlueprintPure, Category = "Catalog|Inventory")
	static int32 GetActorItemCount(const AActor* Actor, const UCatalogItemDefinition* Item);

	/** false sem inventário. */
	UFUNCTION(BlueprintPure, Category = "Catalog|Inventory")
	static bool ActorHasItem(const AActor* Actor, const UCatalogItemDefinition* Item, int32 Quantity = 1);
};
