// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogInventoryComponent.h"
#include "CatalogAnchor.h"
#include "CatalogAnchorSettings.h"
#include "CatalogItemDefinition.h"
#include "CatalogPickupActor.h"
#include "CatalogSaveSubsystem.h"
#include "CatalogSaveTypes.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

namespace CatalogInventoryPrivate
{
	/** Distância à frente do Pawn onde ServerDropItem solta o pickup (cm). */
	constexpr float DropDistance = 100.f;

	UCatalogSaveSubsystem* FindSaveSubsystem(const UActorComponent* Component)
	{
		const UWorld* World = Component ? Component->GetWorld() : nullptr;
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UCatalogSaveSubsystem>() : nullptr;
	}

	const UCatalogItemDefinition* ResolveItem(FName ItemId)
	{
		if (ItemId.IsNone())
		{
			return nullptr;
		}
		if (UAssetManager::IsInitialized())
		{
			const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(
				FPrimaryAssetId(UCatalogItemDefinition::ItemAssetType, ItemId));
			// Load síncrono: definições são pequenas e o load de save já é um ponto de espera.
			if (const UCatalogItemDefinition* Loaded = Path.IsValid() ? Cast<UCatalogItemDefinition>(Path.TryLoad()) : nullptr)
			{
				return Loaded;
			}
		}

		// Fallback: definições já em memória que o Asset Manager não conhece (ex.: NewObject em runtime no transient package).
		// shortcut: varre todo UCatalogItemDefinition vivo, O(n) por slot do save; troque por um TMap ItemId→definição
		// (preenchido ao criar/carregar itens) quando houver centenas de definições ou saves com muitos slots.
		for (TObjectIterator<UCatalogItemDefinition> It(RF_ClassDefaultObject); It; ++It)
		{
			const UCatalogItemDefinition* Candidate = *It;
			if (IsValid(Candidate) && Candidate->GetItemId() == ItemId)
			{
				return Candidate;
			}
		}
		return nullptr;
	}
}

UCatalogInventoryComponent::UCatalogInventoryComponent()
	: Capacity(GetDefault<UCatalogAnchorSettings>()->DefaultInventoryCapacity)
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
	DropPickupClass = ACatalogPickupActor::StaticClass();
	InventoryList.OwnerComponent = this;
}

void UCatalogInventoryComponent::InitializeComponent()
{
	Super::InitializeComponent();
	// Reaponta após cópia de template/duplicação, que pode trazer o ponteiro do arquétipo.
	InventoryList.OwnerComponent = this;
}

void UCatalogInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// Só o servidor grava/lê save; clientes recebem o resultado pela replicação.
	AActor* OwnerActor = GetOwner();
	if (!bSaveWithGame || !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}
	// No BeginPlay o pawn ainda não foi possuído: a decisão sai na possessão.
	if (APawn* OwnerPawn = Cast<APawn>(OwnerActor))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.AddDynamic(this, &UCatalogInventoryComponent::HandleOwnerControllerChanged);
	}
	UpdateSaveRegistration();
}

void UCatalogInventoryComponent::HandleOwnerControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	UpdateSaveRegistration();
}

void UCatalogInventoryComponent::UpdateSaveRegistration()
{
	UCatalogSaveSubsystem* SaveSubsystem = CatalogInventoryPrivate::FindSaveSubsystem(this);
	AActor* OwnerActor = GetOwner();
	if (!SaveSubsystem || !OwnerActor)
	{
		return;
	}

	// Pawn/PlayerState: vale o controller deles; PlayerController: ele mesmo. Outros donos (baú, NPC): nunca,
	// pois o payload tem UM Inventory (o do jogador local) e eles o sobrescreveriam.
	APawn* OwnerPawn = Cast<APawn>(OwnerActor);
	APlayerState* OwnerPlayerState = Cast<APlayerState>(OwnerActor);
	AController* OwnerController = OwnerPawn ? OwnerPawn->GetController()
		: OwnerPlayerState ? OwnerPlayerState->GetOwningController()
		: Cast<AController>(OwnerActor);
	if (OwnerPawn && !OwnerController)
	{
		// Pawn sem controller (veículo, morte antes do respawn): mantém o registro para o save ainda
		// gravar este inventário. No respawn o pawn novo registra a mesma chave e o substitui.
		return;
	}
	const bool bShouldPersist = OwnerController && OwnerController->IsLocalPlayerController();

	if (bShouldPersist)
	{
		SaveSubsystem->RegisterParticipant(this); // repetir não duplica nem relê o save (mesma geração)
	}
	else
	{
		SaveSubsystem->UnregisterParticipant(this);
	}
}

void UCatalogInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UCatalogInventoryComponent::HandleOwnerControllerChanged);
	}
	if (UCatalogSaveSubsystem* SaveSubsystem = CatalogInventoryPrivate::FindSaveSubsystem(this))
	{
		SaveSubsystem->UnregisterParticipant(this); // só este: o pawn do respawn, se já o substituiu, continua
	}
	Super::EndPlay(EndPlayReason);
}

void UCatalogInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UCatalogInventoryComponent, InventoryList, COND_OwnerOnly);
}

bool UCatalogInventoryComponent::HasInventoryAuthority(const TCHAR* Operation) const
{
	const AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasAuthority())
	{
		return true;
	}
	UE_LOG(LogCatalogAnchor, Warning, TEXT("Inventário de %s: %s exige autoridade; peça ao servidor via RPC."),
		*GetNameSafe(OwnerActor), Operation);
	return false;
}

void UCatalogInventoryComponent::CollectStacks(const UCatalogItemDefinition* Item,
	TArray<int32>& OutEntryIndices, TArray<int32>& OutQuantities) const
{
	OutEntryIndices.Reset();
	OutQuantities.Reset();
	for (int32 Index = 0; Index < InventoryList.Entries.Num(); ++Index)
	{
		const FCatalogInventoryEntry& Entry = InventoryList.Entries[Index];
		if (Entry.Item.Get() == Item)
		{
			OutEntryIndices.Add(Index);
			OutQuantities.Add(Entry.Quantity);
		}
	}
}

int32 UCatalogInventoryComponent::AddInternal(const UCatalogItemDefinition* Item, int32 Quantity)
{
	TArray<int32> EntryIndices;
	TArray<int32> Stacks;
	CollectStacks(Item, EntryIndices, Stacks);

	const int32 FreeSlots = FMath::Max(0, Capacity - InventoryList.Entries.Num());
	const int32 Added = FCatalogInventoryRules::AddToStacks(Stacks, Quantity, Item->MaxStackSize, FreeSlots);

	for (int32 StackIndex = 0; StackIndex < Stacks.Num(); ++StackIndex)
	{
		if (StackIndex < EntryIndices.Num())
		{
			FCatalogInventoryEntry& Entry = InventoryList.Entries[EntryIndices[StackIndex]];
			if (Entry.Quantity != Stacks[StackIndex])
			{
				Entry.Quantity = Stacks[StackIndex];
				InventoryList.MarkItemDirty(Entry);
			}
		}
		else
		{
			FCatalogInventoryEntry& NewEntry = InventoryList.Entries.AddDefaulted_GetRef();
			NewEntry.Item = Item;
			NewEntry.Quantity = Stacks[StackIndex];
			InventoryList.MarkItemDirty(NewEntry);
		}
	}
	return Added;
}

int32 UCatalogInventoryComponent::AddItem(const UCatalogItemDefinition* Item, int32 Quantity)
{
	if (!Item || Quantity <= 0)
	{
		UE_LOG(LogCatalogAnchor, Warning, TEXT("AddItem ignorado: Item=%s, Quantity=%d."), *GetNameSafe(Item), Quantity);
		return 0;
	}
	if (!HasInventoryAuthority(TEXT("AddItem")))
	{
		return 0;
	}

	const int32 Added = AddInternal(Item, Quantity);
	if (Added > 0)
	{
		OnInventoryChanged.Broadcast();
	}
	return Added;
}

int32 UCatalogInventoryComponent::RemoveItem(const UCatalogItemDefinition* Item, int32 Quantity)
{
	if (!Item || Quantity <= 0)
	{
		UE_LOG(LogCatalogAnchor, Warning, TEXT("RemoveItem ignorado: Item=%s, Quantity=%d."), *GetNameSafe(Item), Quantity);
		return 0;
	}
	if (!HasInventoryAuthority(TEXT("RemoveItem")))
	{
		return 0;
	}

	TArray<int32> EntryIndices;
	TArray<int32> Stacks;
	CollectStacks(Item, EntryIndices, Stacks);

	const int32 Removed = FCatalogInventoryRules::RemoveFromStacks(Stacks, Quantity);
	if (Removed <= 0)
	{
		return 0;
	}

	bool bRemovedEntries = false;
	// De trás para frente: RemoveAt não desloca os índices que ainda faltam visitar.
	for (int32 StackIndex = EntryIndices.Num() - 1; StackIndex >= 0; --StackIndex)
	{
		const int32 EntryIndex = EntryIndices[StackIndex];
		if (Stacks[StackIndex] <= 0)
		{
			InventoryList.Entries.RemoveAt(EntryIndex);
			bRemovedEntries = true;
		}
		else if (InventoryList.Entries[EntryIndex].Quantity != Stacks[StackIndex])
		{
			FCatalogInventoryEntry& Entry = InventoryList.Entries[EntryIndex];
			Entry.Quantity = Stacks[StackIndex];
			InventoryList.MarkItemDirty(Entry);
		}
	}
	if (bRemovedEntries)
	{
		InventoryList.MarkArrayDirty();
	}

	OnInventoryChanged.Broadcast();
	return Removed;
}

int32 UCatalogInventoryComponent::GetItemCount(const UCatalogItemDefinition* Item) const
{
	int64 Count = 0; // int64: várias pilhas com MaxStackSize enorme estourariam int32
	if (Item)
	{
		for (const FCatalogInventoryEntry& Entry : InventoryList.Entries)
		{
			if (Entry.Item.Get() == Item)
			{
				Count += Entry.Quantity;
			}
		}
	}
	return static_cast<int32>(FMath::Min<int64>(Count, MAX_int32));
}

bool UCatalogInventoryComponent::HasItem(const UCatalogItemDefinition* Item, int32 Quantity) const
{
	return Item && GetItemCount(Item) >= FMath::Max(1, Quantity);
}

TArray<FCatalogInventoryStack> UCatalogInventoryComponent::GetStacks() const
{
	TArray<FCatalogInventoryStack> Result;
	Result.Reserve(InventoryList.Entries.Num());
	for (const FCatalogInventoryEntry& Entry : InventoryList.Entries)
	{
		FCatalogInventoryStack& Stack = Result.AddDefaulted_GetRef();
		Stack.Item = Entry.Item;
		Stack.Quantity = Entry.Quantity;
	}
	return Result;
}

void UCatalogInventoryComponent::ServerDropItem_Implementation(const UCatalogItemDefinition* Item, int32 Quantity)
{
	UWorld* World = GetWorld();
	const AActor* DropOrigin = GetOwner();
	if (!World || !DropOrigin || !DropPickupClass)
	{
		return;
	}
	// Inventário no Controller: solta à frente do Pawn, não na origem do Controller.
	if (const AController* OwnerController = Cast<AController>(DropOrigin))
	{
		if (const APawn* OwnerPawn = OwnerController->GetPawn())
		{
			DropOrigin = OwnerPawn;
		}
	}

	// RemoveItem valida Item/Quantity e só tira o que existe: o pedido do cliente não é confiável.
	const int32 Removed = RemoveItem(Item, Quantity);
	if (Removed <= 0)
	{
		return;
	}

	const FTransform SpawnTransform(DropOrigin->GetActorRotation(),
		DropOrigin->GetActorLocation() + DropOrigin->GetActorForwardVector() * CatalogInventoryPrivate::DropDistance);
	ACatalogPickupActor* Pickup = World->SpawnActorDeferred<ACatalogPickupActor>(DropPickupClass, SpawnTransform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		// Spawn falhou: devolve os itens em vez de sumir com eles.
		AddInternal(Item, Removed);
		OnInventoryChanged.Broadcast();
		UE_LOG(LogCatalogAnchor, Warning, TEXT("ServerDropItem: falha ao gerar %s; itens devolvidos."), *GetNameSafe(DropPickupClass.Get()));
		return;
	}
	Pickup->Item = Item;
	Pickup->Quantity = Removed;
	Pickup->FinishSpawning(SpawnTransform);
}

FName UCatalogInventoryComponent::GetSaveRecordKey() const
{
	return FName(TEXT("Inventory"));
}

void UCatalogInventoryComponent::WriteSaveData(FCatalogSavePayload& Payload) const
{
	Payload.Inventory.Reset();
	for (const FCatalogInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.Item && Entry.Quantity > 0)
		{
			FCatalogInventorySlot& Slot = Payload.Inventory.AddDefaulted_GetRef();
			Slot.ItemId = Entry.Item->GetItemId();
			Slot.Quantity = Entry.Quantity;
		}
	}
}

void UCatalogInventoryComponent::ReadSaveData(const FCatalogSavePayload& Payload)
{
	if (!HasInventoryAuthority(TEXT("ReadSaveData")))
	{
		return;
	}

	InventoryList.Entries.Reset();
	InventoryList.MarkArrayDirty();

	for (const FCatalogInventorySlot& Slot : Payload.Inventory)
	{
		if (Slot.Quantity <= 0)
		{
			continue;
		}
		const UCatalogItemDefinition* Item = CatalogInventoryPrivate::ResolveItem(Slot.ItemId);
		if (!Item)
		{
			UE_LOG(LogCatalogAnchor, Warning,
				TEXT("Inventory load: item '%s' não encontrado no Asset Manager nem em memória (asset removido/renomeado ou tipo CatalogItem fora de PrimaryAssetTypesToScan); pulando."),
				*Slot.ItemId.ToString());
			continue;
		}
		// AddInternal reempilha: absorve MaxStackSize/Capacity que mudaram desde o save.
		const int32 Added = AddInternal(Item, Slot.Quantity);
		if (Added < Slot.Quantity)
		{
			UE_LOG(LogCatalogAnchor, Warning, TEXT("Inventory load: sem espaço; %d de '%s' descartados."),
				Slot.Quantity - Added, *Slot.ItemId.ToString());
		}
	}

	OnInventoryChanged.Broadcast();
}

void UCatalogInventoryComponent::HandleReplicatedChange()
{
	if (bReplicatedBroadcastPending)
	{
		return;
	}
	// Em PreReplicatedRemove o item ainda está no array: adiar evita UI lendo estado velho.
	UWorld* World = GetWorld();
	if (!World)
	{
		OnInventoryChanged.Broadcast();
		return;
	}
	bReplicatedBroadcastPending = true;
	World->GetTimerManager().SetTimerForNextTick(this, &UCatalogInventoryComponent::FlushReplicatedBroadcast);
}

void UCatalogInventoryComponent::FlushReplicatedBroadcast()
{
	bReplicatedBroadcastPending = false;
	OnInventoryChanged.Broadcast();
}
