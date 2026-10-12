// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogInventoryTypes.h"
#include "CatalogInventoryComponent.h"

void FCatalogInventoryEntry::PreReplicatedRemove(const FCatalogInventoryList& InArraySerializer)
{
	InArraySerializer.NotifyOwnerOfReplication();
}

void FCatalogInventoryEntry::PostReplicatedAdd(const FCatalogInventoryList& InArraySerializer)
{
	InArraySerializer.NotifyOwnerOfReplication();
}

void FCatalogInventoryEntry::PostReplicatedChange(const FCatalogInventoryList& InArraySerializer)
{
	InArraySerializer.NotifyOwnerOfReplication();
}

void FCatalogInventoryList::NotifyOwnerOfReplication() const
{
	if (UCatalogInventoryComponent* Component = OwnerComponent.Get())
	{
		Component->HandleReplicatedChange();
	}
}

int32 FCatalogInventoryRules::AddToStacks(TArray<int32>& InOutStacks, int32 Quantity, int32 MaxStackSize, int32 FreeSlots)
{
	if (Quantity <= 0)
	{
		return 0;
	}

	const int32 StackLimit = FMath::Max(1, MaxStackSize);
	int32 Remaining = Quantity;

	for (int32& Stack : InOutStacks)
	{
		if (Remaining <= 0)
		{
			break;
		}
		// Pilha acima do limite (MaxStackSize reduzido após o save) não recebe mais nada.
		const int32 Space = StackLimit - Stack;
		if (Space > 0)
		{
			const int32 Moved = FMath::Min(Space, Remaining);
			Stack += Moved;
			Remaining -= Moved;
		}
	}

	for (int32 Slot = 0; Slot < FreeSlots && Remaining > 0; ++Slot)
	{
		const int32 Moved = FMath::Min(StackLimit, Remaining);
		InOutStacks.Add(Moved);
		Remaining -= Moved;
	}

	return Quantity - Remaining;
}

int32 FCatalogInventoryRules::RemoveFromStacks(TArray<int32>& InOutStacks, int32 Quantity)
{
	if (Quantity <= 0)
	{
		return 0;
	}

	int32 Remaining = Quantity;
	for (int32 Index = InOutStacks.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		// Clamp em 0: uma pilha corrompida (negativa) não pode "devolver" quantidade.
		const int32 Moved = FMath::Clamp(InOutStacks[Index], 0, Remaining);
		InOutStacks[Index] -= Moved;
		Remaining -= Moved;
	}

	return Quantity - Remaining;
}
