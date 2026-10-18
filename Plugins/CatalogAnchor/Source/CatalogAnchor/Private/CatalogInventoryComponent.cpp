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

	/** Quem "age" pelo inventário: o Pawn se o dono é um Controller (padrão Lyra), senão o próprio dono. */
	AActor* GetActingActor(AActor* Owner)
	{
		if (const AController* OwnerController = Cast<AController>(Owner))
		{
			if (APawn* OwnerPawn = OwnerController->GetPawn())
			{
				return OwnerPawn;
			}
		}
		return Owner;
	}

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

int32 UCatalogInventoryComponent::SimulateAdd(const UCatalogItemDefinition* Item, int32 Quantity, bool bEnforceWeight,
	TArray<int32>& OutEntryIndices, TArray<int32>& OutStacks) const
{
	CollectStacks(Item, OutEntryIndices, OutStacks);

	const int32 Allowed = bEnforceWeight
		? FCatalogInventoryRules::MaxQuantityByWeight(GetTotalWeight(), MaxWeight, Item->Weight, Quantity)
		: Quantity;
	const int32 FreeSlots = FMath::Max(0, Capacity - InventoryList.Entries.Num());
	return FCatalogInventoryRules::AddToStacks(OutStacks, Allowed, Item->MaxStackSize, FreeSlots);
}

int32 UCatalogInventoryComponent::GetAddableQuantity(const UCatalogItemDefinition* Item, int32 Quantity) const
{
	if (!Item || Quantity <= 0)
	{
		return 0;
	}
	TArray<int32> EntryIndices;
	TArray<int32> Stacks;
	return SimulateAdd(Item, Quantity, /*bEnforceWeight*/ true, EntryIndices, Stacks);
}

int32 UCatalogInventoryComponent::AddInternal(const UCatalogItemDefinition* Item, int32 Quantity, bool bEnforceWeight)
{
	TArray<int32> EntryIndices;
	TArray<int32> Stacks;
	const int32 Added = SimulateAdd(Item, Quantity, bEnforceWeight, EntryIndices, Stacks);

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

	const int32 Added = AddInternal(Item, Quantity, /*bEnforceWeight*/ true);
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

	const int32 Removed = RemoveInternal(Item, Quantity);
	if (Removed > 0)
	{
		OnInventoryChanged.Broadcast();
	}
	return Removed;
}

int32 UCatalogInventoryComponent::RemoveInternal(const UCatalogItemDefinition* Item, int32 Quantity)
{
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

TArray<FCatalogInventoryStack> UCatalogInventoryComponent::GetItemsByTag(FGameplayTag Tag) const
{
	TArray<FCatalogInventoryStack> Result;
	if (!Tag.IsValid())
	{
		return Result;
	}
	for (const FCatalogInventoryEntry& Entry : InventoryList.Entries)
	{
		const UCatalogItemDefinition* Item = Entry.Item.Get();
		if (Item && (Item->ItemTags.HasTag(Tag) || Item->Category.MatchesTag(Tag)))
		{
			FCatalogInventoryStack& Stack = Result.AddDefaulted_GetRef();
			Stack.Item = Item;
			Stack.Quantity = Entry.Quantity;
		}
	}
	return Result;
}

float UCatalogInventoryComponent::GetTotalWeight() const
{
	double Total = 0.0;
	for (const FCatalogInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.Item)
		{
			Total += static_cast<double>(Entry.Item->Weight) * Entry.Quantity;
		}
	}
	return static_cast<float>(Total);
}

bool UCatalogInventoryComponent::UseItem(const UCatalogItemDefinition* Item, int32 Quantity)
{
	if (!Item || !HasInventoryAuthority(TEXT("UseItem")))
	{
		return false;
	}
	if (!FCatalogInventoryRules::CanUse(Item->bUsable, GetItemCount(Item), Quantity))
	{
		// Verbose: com latência a UI do cliente pode pedir um uso que o servidor já gastou.
		UE_LOG(LogCatalogAnchor, Verbose, TEXT("UseItem recusado: Item=%s, Quantity=%d, usável=%d, tem=%d."),
			*GetNameSafe(Item), Quantity, Item->bUsable, GetItemCount(Item));
		return false;
	}

	// Item que não se consome não tem estoque limitando: sem isso um cliente repete o efeito a cada RPC.
	if (!Item->bConsumeOnUse && Item->UseCooldownSeconds > 0.f)
	{
		const UWorld* World = GetWorld();
		const double Now = World ? World->GetTimeSeconds() : 0.0;
		const double* NextUse = NextUseTimes.Find(Item);
		if (NextUse && Now < *NextUse)
		{
			UE_LOG(LogCatalogAnchor, Verbose, TEXT("UseItem recusado: %s em espera por mais %.2f s."), *GetNameSafe(Item), *NextUse - Now);
			return false;
		}
		NextUseTimes.Add(Item, Now + Item->UseCooldownSeconds);
	}

	// Consome antes dos efeitos: um OnUsed reentrante não gasta a mesma unidade duas vezes.
	if (Item->bConsumeOnUse)
	{
		RemoveItem(Item, Quantity); // CanUse garantiu a quantidade
	}
	AActor* User = CatalogInventoryPrivate::GetActingActor(GetOwner());
	for (int32 Use = 0; Use < Quantity; ++Use)
	{
		Item->OnUsed(User);
	}
	OnItemUsed.Broadcast(Item, Quantity);
	return true;
}

bool UCatalogInventoryComponent::ServerUseItem_Validate(const UCatalogItemDefinition* Item, int32 Quantity)
{
	// Só o que um cliente legítimo nunca envia (derruba a conexão); "não tem"/"não usável" ficam no UseItem.
	return Item != nullptr && Quantity >= 1 && Quantity <= FCatalogInventoryRules::MaxUseQuantity;
}

void UCatalogInventoryComponent::ServerUseItem_Implementation(const UCatalogItemDefinition* Item, int32 Quantity)
{
	UseItem(Item, Quantity); // UseItem valida tudo: o pedido do cliente não é confiável
}

bool UCatalogInventoryComponent::TransferTo(UCatalogInventoryComponent* Other, const UCatalogItemDefinition* Item, int32 Quantity)
{
	if (!Other || Other == this || !Item || Quantity <= 0)
	{
		UE_LOG(LogCatalogAnchor, Warning, TEXT("TransferTo ignorado: Other=%s, Item=%s, Quantity=%d."),
			*GetNameSafe(Other), *GetNameSafe(Item), Quantity);
		return false;
	}
	if (!HasInventoryAuthority(TEXT("TransferTo")) || !Other->HasInventoryAuthority(TEXT("TransferTo (destino)")))
	{
		return false;
	}
	// Tudo ou nada: valida origem e destino antes de mexer. Abaixo só há versões sem broadcast, então
	// nenhum código de usuário roda entre a validação e a escrita e ela vale até o fim.
	if (GetItemCount(Item) < Quantity || Other->GetAddableQuantity(Item, Quantity) < Quantity)
	{
		return false;
	}

	const int32 Removed = RemoveInternal(Item, Quantity);
	Other->AddInternal(Item, Removed, /*bEnforceWeight*/ true);
	OnInventoryChanged.Broadcast();
	Other->OnInventoryChanged.Broadcast();
	return true;
}

bool UCatalogInventoryComponent::ServerDropItem_Validate(const UCatalogItemDefinition* Item, int32 Quantity)
{
	return Item != nullptr && Quantity >= 1;
}

void UCatalogInventoryComponent::ServerDropItem_Implementation(const UCatalogItemDefinition* Item, int32 Quantity)
{
	UWorld* World = GetWorld();
	const AActor* DropOrigin = CatalogInventoryPrivate::GetActingActor(GetOwner()); // Controller: solta à frente do Pawn
	if (!World || !DropOrigin || !DropPickupClass)
	{
		return;
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
		// Spawn falhou: devolve os itens em vez de sumir com eles (sem enforçar peso: pode estar acima do máximo).
		AddInternal(Item, Removed, /*bEnforceWeight*/ false);
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
		// AddInternal reempilha: absorve MaxStackSize/Capacity que mudaram desde o save. Peso não enforça no load:
		// MaxWeight reduzido não pode apagar itens do jogador (só barra novas entradas).
		const int32 Added = AddInternal(Item, Slot.Quantity, /*bEnforceWeight*/ false);
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
