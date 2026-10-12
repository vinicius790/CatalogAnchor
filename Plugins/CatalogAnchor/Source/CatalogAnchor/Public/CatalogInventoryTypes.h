// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "CatalogInventoryTypes.generated.h"

class UCatalogItemDefinition;
class UCatalogInventoryComponent;
struct FCatalogInventoryList;

/** Cópia de leitura de uma pilha para UI/Blueprint (alterá-la não mexe no inventário). */
USTRUCT(BlueprintType)
struct CATALOGANCHOR_API FCatalogInventoryStack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Catalog|Inventory")
	TObjectPtr<const UCatalogItemDefinition> Item = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Catalog|Inventory")
	int32 Quantity = 0;
};

/**
 * Uma pilha replicada. Os callbacks rodam só nos clientes, item a item, e avisam o
 * componente dono para disparar OnInventoryChanged (o servidor dispara direto em Add/Remove).
 */
USTRUCT()
struct CATALOGANCHOR_API FCatalogInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const UCatalogItemDefinition> Item = nullptr;

	UPROPERTY()
	int32 Quantity = 0;

	//~ Callbacks do FFastArraySerializer: o parâmetro é o tipo concreto da lista (ver FastArraySerializer.h).
	void PreReplicatedRemove(const FCatalogInventoryList& InArraySerializer);
	void PostReplicatedAdd(const FCatalogInventoryList& InArraySerializer);
	void PostReplicatedChange(const FCatalogInventoryList& InArraySerializer);
};

/** Lista replicada por delta: só as pilhas marcadas como sujas trafegam (padrão Lyra). */
USTRUCT()
struct CATALOGANCHOR_API FCatalogInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FCatalogInventoryEntry> Entries;

	/** Componente dono, usado pelos callbacks de cliente; local, nunca vai pela rede. */
	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UCatalogInventoryComponent> OwnerComponent = nullptr;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FCatalogInventoryEntry, FCatalogInventoryList>(Entries, DeltaParms, *this);
	}

	/** Repassa ao dono uma mudança recebida da rede. */
	void NotifyOwnerOfReplication() const;
};

template<>
struct TStructOpsTypeTraits<FCatalogInventoryList> : public TStructOpsTypeTraitsBase2<FCatalogInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

/**
 * Regras puras de empilhamento, sem UObject/mundo/rede, para teste de automação.
 * Operam sobre as quantidades das pilhas de UM item; o componente traduz o resultado
 * para entradas do FastArray (e marca o que mudou como sujo).
 */
struct CATALOGANCHOR_API FCatalogInventoryRules
{
	/**
	 * Completa as pilhas existentes até MaxStackSize (< 1 conta como 1) e depois anexa
	 * pilhas novas enquanto houver FreeSlots. Retorna quanto entrou (0 se Quantity <= 0).
	 */
	static int32 AddToStacks(TArray<int32>& InOutStacks, int32 Quantity, int32 MaxStackSize, int32 FreeSlots);

	/**
	 * Retira até Quantity, da última pilha para a primeira (preserva pilhas cheias).
	 * Pilhas que chegam a 0 ficam no array; o chamador as apaga. Retorna quanto saiu.
	 */
	static int32 RemoveFromStacks(TArray<int32>& InOutStacks, int32 Quantity);
};
