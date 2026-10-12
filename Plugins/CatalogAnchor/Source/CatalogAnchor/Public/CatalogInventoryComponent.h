// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CatalogInventoryTypes.h"
#include "ICatalogSaveParticipant.h"
#include "CatalogInventoryComponent.generated.h"

class ACatalogPickupActor;
class AController;
class APawn;
class UCatalogItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCatalogInventoryChanged);

/**
 * Inventário de pilhas com servidor autoritativo.
 * - Add/Remove só com autoridade; o cliente pede ao servidor (ex.: ServerDropItem, interação).
 * - Replica por FFastArraySerializer com COND_OwnerOnly: só o cliente dono recebe (menos
 *   banda e nada de ler mochila alheia). Por isso o dono do ator precisa ser uma conexão
 *   de jogador (PlayerController, Pawn possuído, PlayerState); num baú nenhum cliente recebe.
 * - Salva via ICatalogSaveParticipant (chave "Inventory", grava em FCatalogSavePayload::Inventory).
 */
UCLASS(ClassGroup = (Catalog), meta = (BlueprintSpawnableComponent))
class CATALOGANCHOR_API UCatalogInventoryComponent : public UActorComponent, public ICatalogSaveParticipant
{
	GENERATED_BODY()

public:
	UCatalogInventoryComponent();

	/** Número máximo de pilhas (padrão: Project Settings → Catalog Anchor). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory", meta = (ClampMin = "1"))
	int32 Capacity;

	/**
	 * Registra no UCatalogSaveSubsystem. O payload tem UM Inventory, o do jogador LOCAL: só registra
	 * em Pawn/PlayerController/PlayerState cujo controller seja de jogador local. Baú/NPC nunca
	 * registram (sobrescreveriam o do jogador) e, num listen server, os remotos não entram no save do host.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory")
	bool bSaveWithGame = true;

	/** Pickup gerado por ServerDropItem. A classe base não tem malha: use um BP filho com mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Inventory")
	TSubclassOf<ACatalogPickupActor> DropPickupClass;

	/** Dispara no servidor após Add/Remove/Load e nos clientes quando a replicação chega. */
	UPROPERTY(BlueprintAssignable, Category = "Catalog|Inventory")
	FCatalogInventoryChanged OnInventoryChanged;

	/** Completa pilhas do item e abre novas até Capacity. Devolve quanto entrou. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Catalog|Inventory")
	int32 AddItem(const UCatalogItemDefinition* Item, int32 Quantity);

	/** Retira até Quantity (parcial se faltar). Devolve quanto saiu. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Catalog|Inventory")
	int32 RemoveItem(const UCatalogItemDefinition* Item, int32 Quantity);

	UFUNCTION(BlueprintPure, Category = "Catalog|Inventory")
	int32 GetItemCount(const UCatalogItemDefinition* Item) const;

	/** true se há pelo menos Quantity (mínimo 1) do item. */
	UFUNCTION(BlueprintPure, Category = "Catalog|Inventory")
	bool HasItem(const UCatalogItemDefinition* Item, int32 Quantity = 1) const;

	UFUNCTION(BlueprintPure, Category = "Catalog|Inventory")
	TArray<FCatalogInventoryStack> GetStacks() const;

	/**
	 * Cliente dono pede para soltar itens: o servidor remove só o que existe e gera
	 * DropPickupClass à frente do Pawn. Chamado no servidor, executa direto.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Catalog|Inventory")
	void ServerDropItem(const UCatalogItemDefinition* Item, int32 Quantity);

	//~ ICatalogSaveParticipant
	virtual FName GetSaveRecordKey() const override;
	virtual void WriteSaveData(FCatalogSavePayload& Payload) const override;
	virtual void ReadSaveData(const FCatalogSavePayload& Payload) override;

protected:
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Possessão mudou (servidor): (des)registra no save conforme o novo controller seja local. */
	UFUNCTION()
	void HandleOwnerControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	/** Registra/desregistra no save conforme o dono atual deva persistir nesta máquina. */
	void UpdateSaveRegistration();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	friend struct FCatalogInventoryList;

	UPROPERTY(Replicated)
	FCatalogInventoryList InventoryList;

	bool bReplicatedBroadcastPending = false;

	/** Loga e devolve false sem autoridade (BlueprintAuthorityOnly não protege chamadas C++). */
	bool HasInventoryAuthority(const TCHAR* Operation) const;

	/** Índices em InventoryList.Entries e quantidades das pilhas de Item, na ordem do array. */
	void CollectStacks(const UCatalogItemDefinition* Item, TArray<int32>& OutEntryIndices, TArray<int32>& OutQuantities) const;

	/** Add sem checagens nem broadcast (Item válido, Quantity > 0, autoridade já garantidos). */
	int32 AddInternal(const UCatalogItemDefinition* Item, int32 Quantity);

	/** Cliente: agrupa os callbacks do FastArray num broadcast no próximo tick, já com o array final. */
	void HandleReplicatedChange();
	void FlushReplicatedBroadcast();
};
