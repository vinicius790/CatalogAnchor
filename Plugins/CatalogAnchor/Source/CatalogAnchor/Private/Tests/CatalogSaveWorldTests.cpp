// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogInventoryComponent.h"
#include "CatalogItemDefinition.h"
#include "CatalogSaveSubsystem.h"
#include "CatalogTestGuards.h"
#include "CatalogSaveTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CatalogSaveWorldTestsPrivate
{
	// GameInstance com subsystems reais + mundo Game em jogo, sem PIE. Desmonta tudo no destrutor.
	struct FTestWorld
	{
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		UCatalogSaveSubsystem* Save = nullptr;
		FName Slot;

		FTestWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone(); // contexto Game + mundo + Init() (sobe os subsystems)
			World = GameInstance->GetWorld();
			World->InitializeActorsForPlay(FURL());
			World->SetBegunPlay(true); // sem GameMode ninguém marcaria: atores spawnados rodam BeginPlay
			Save = GameInstance->GetSubsystem<UCatalogSaveSubsystem>();
			Slot = FName(*(TEXT("CatalogAnchorTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		}

		~FTestWorld()
		{
			if (Save)
			{
				Save->DeleteSlot(Slot);
			}
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				It->RouteEndPlay(EEndPlayReason::LevelTransition);
			}
			World->SetBegunPlay(false);
			GameInstance->Shutdown();
			GameInstance->RemoveFromRoot();
			World->DestroyWorld(false); // também tira o mundo do root
			GEngine->DestroyWorldContext(World);
		}
	};

	// ItemId = nome do objeto; o load resolve pela varredura de definições em memória.
	UCatalogItemDefinition* MakeItem(const TCHAR* BaseName)
	{
		UPackage* Package = GetTransientPackage();
		UCatalogItemDefinition* Item = NewObject<UCatalogItemDefinition>(Package,
			MakeUniqueObjectName(Package, UCatalogItemDefinition::StaticClass(), FName(BaseName)));
		Item->MaxStackSize = 99;
		return Item;
	}

	// bInPlay: registra o componente, o que roda BeginPlay (e a regra de registro no save).
	// Sem isso o componente só serve de participante manual.
	UCatalogInventoryComponent* MakeInventory(AActor* Owner, bool bInPlay)
	{
		UCatalogInventoryComponent* Inventory = NewObject<UCatalogInventoryComponent>(Owner);
		if (bInPlay)
		{
			Inventory->RegisterComponent();
		}
		return Inventory;
	}

	FCatalogSavePayload MakePayload(const UCatalogItemDefinition* Item, int32 Quantity)
	{
		FCatalogSavePayload Payload;
		FCatalogInventorySlot& InventorySlot = Payload.Inventory.AddDefaulted_GetRef();
		InventorySlot.ItemId = Item->GetItemId();
		InventorySlot.Quantity = Quantity;
		return Payload;
	}

	// Quantidade do item no arquivo do slot (-1 se não leu).
	int32 SavedCount(UCatalogSaveSubsystem* Save, FName Slot, const UCatalogItemDefinition* Item)
	{
		FCatalogSavePayload Payload;
		if (!Save->LoadPayloadFromSlot(Slot, Payload))
		{
			return -1;
		}
		int32 Count = 0;
		for (const FCatalogInventorySlot& InventorySlot : Payload.Inventory)
		{
			if (InventorySlot.ItemId == Item->GetItemId())
			{
				Count += InventorySlot.Quantity;
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveReRegisterTest, "CatalogAnchor.Save.ReRegisterSameGenerationDoesNotReread",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogSaveReRegisterTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveWorldTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	FTestWorld TestWorld;
	if (!TestNotNull(TEXT("subsystem de save"), TestWorld.Save))
	{
		return false;
	}
	UCatalogSaveSubsystem* Save = TestWorld.Save;
	UCatalogItemDefinition* Potion = MakeItem(TEXT("TestPotion"));
	UCatalogInventoryComponent* Inventory = MakeInventory(TestWorld.World->SpawnActor<AActor>(), false);

	TestTrue(TEXT("estado ativo criado"), Save->SavePayloadToSlot(MakePayload(Potion, 2), TestWorld.Slot));
	Save->RegisterParticipant(Inventory);
	TestEqual(TEXT("primeiro registro recebe o estado ativo"), Inventory->GetItemCount(Potion), 2);

	Inventory->AddItem(Potion, 3);
	Save->UnregisterParticipant(Inventory);
	Save->RegisterParticipant(Inventory);
	TestEqual(TEXT("re-registro na mesma geração não relê"), Inventory->GetItemCount(Potion), 5);

	// Save não abre geração: o payload veio do próprio participante.
	TestTrue(TEXT("save"), Save->SaveGame(TestWorld.Slot));
	Inventory->AddItem(Potion, 1);
	Save->UnregisterParticipant(Inventory);
	Save->RegisterParticipant(Inventory);
	TestEqual(TEXT("re-registro depois de save não relê"), Inventory->GetItemCount(Potion), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveLoadGenerationTest, "CatalogAnchor.Save.LoadStartsNewGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogSaveLoadGenerationTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveWorldTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	FTestWorld TestWorld;
	if (!TestNotNull(TEXT("subsystem de save"), TestWorld.Save))
	{
		return false;
	}
	UCatalogSaveSubsystem* Save = TestWorld.Save;
	UCatalogItemDefinition* Potion = MakeItem(TEXT("TestPotion"));
	UCatalogInventoryComponent* Inventory = MakeInventory(TestWorld.World->SpawnActor<AActor>(), false);

	// Registrado antes de haver estado ativo: nada a receber, e o save seguinte não o faz reler.
	Save->RegisterParticipant(Inventory);
	Inventory->AddItem(Potion, 4);
	TestTrue(TEXT("save"), Save->SaveGame(TestWorld.Slot));
	Inventory->AddItem(Potion, 1);
	Save->UnregisterParticipant(Inventory);
	Save->RegisterParticipant(Inventory);
	TestEqual(TEXT("registrado antes do primeiro save não relê"), Inventory->GetItemCount(Potion), 5);

	TestTrue(TEXT("load com registrado"), Save->LoadGame(TestWorld.Slot));
	TestEqual(TEXT("load entrega a quem está registrado"), Inventory->GetItemCount(Potion), 4);

	// Ausente durante o load: recebe ao voltar, uma vez.
	Save->UnregisterParticipant(Inventory);
	Inventory->AddItem(Potion, 2);
	TestTrue(TEXT("load com ausente"), Save->LoadGame(TestWorld.Slot));
	TestEqual(TEXT("ausente não é tocado pelo load"), Inventory->GetItemCount(Potion), 6);
	Save->RegisterParticipant(Inventory);
	TestEqual(TEXT("ausente recebe a geração nova ao registrar"), Inventory->GetItemCount(Potion), 4);

	// Objeto novo depois do load recebe o estado (mesma chave: substitui o anterior).
	UCatalogInventoryComponent* Newcomer = MakeInventory(TestWorld.World->SpawnActor<AActor>(), false);
	Save->RegisterParticipant(Newcomer);
	TestEqual(TEXT("participante novo recebe o estado ativo"), Newcomer->GetItemCount(Potion), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveRespawnTest, "CatalogAnchor.Save.RespawnReplacesByKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogSaveRespawnTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveWorldTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	FTestWorld TestWorld;
	if (!TestNotNull(TEXT("subsystem de save"), TestWorld.Save))
	{
		return false;
	}
	UCatalogSaveSubsystem* Save = TestWorld.Save;
	UWorld* World = TestWorld.World;
	const FName Slot = TestWorld.Slot;
	UCatalogItemDefinition* Potion = MakeItem(TEXT("TestPotion"));
	UCatalogItemDefinition* Key = MakeItem(TEXT("TestKey"));
	UCatalogItemDefinition* Gem = MakeItem(TEXT("TestGem"));
	TestTrue(TEXT("estado ativo criado"), Save->SavePayloadToSlot(MakePayload(Potion, 2), Slot));

	// Sem ULocalPlayer: SetAsLocalPlayerController é o que o GameMode usa para marcar o PC local.
	APlayerController* LocalPC = World->SpawnActor<APlayerController>();
	APlayerController* RemotePC = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("PC local"), LocalPC) || !TestNotNull(TEXT("PC remoto"), RemotePC))
	{
		return false;
	}
	LocalPC->SetAsLocalPlayerController();
	TestTrue(TEXT("PC local é local"), LocalPC->IsLocalPlayerController());
	TestFalse(TEXT("PC sem jogador local nem net driver não é local"), RemotePC->IsLocalPlayerController());

	APawn* OldPawn = World->SpawnActor<APawn>();
	UCatalogInventoryComponent* OldInventory = MakeInventory(OldPawn, true);
	TestEqual(TEXT("pawn sem controller não registra no BeginPlay"), OldInventory->GetItemCount(Potion), 0);

	LocalPC->Possess(OldPawn);
	TestEqual(TEXT("possessão local registra e recebe o estado"), OldInventory->GetItemCount(Potion), 2);
	OldInventory->AddItem(Key, 1);

	// Controller remoto tira do save; o local de volta registra de novo sem reler.
	RemotePC->Possess(OldPawn);
	LocalPC->Possess(OldPawn);
	TestEqual(TEXT("re-possessão na mesma geração não relê"), OldInventory->GetItemCount(Key), 1);

	// Morte: sem controller o pawn continua no save.
	LocalPC->UnPossess();
	TestTrue(TEXT("save com pawn morto"), Save->SaveGame(Slot));
	TestEqual(TEXT("pawn sem controller ainda grava"), SavedCount(Save, Slot, Key), 1);

	// Respawn: o pawn novo registra a mesma chave e substitui o velho, que ainda vive.
	APawn* NewPawn = World->SpawnActor<APawn>();
	UCatalogInventoryComponent* NewInventory = MakeInventory(NewPawn, true);
	LocalPC->Possess(NewPawn);
	TestEqual(TEXT("pawn novo recebe o estado ativo"), NewInventory->GetItemCount(Key), 1);
	NewInventory->AddItem(Gem, 5);
	OldInventory->AddItem(Key, 10);
	TestTrue(TEXT("save após respawn"), Save->SaveGame(Slot));
	TestEqual(TEXT("grava o inventário do pawn novo"), SavedCount(Save, Slot, Gem), 5);
	TestEqual(TEXT("pawn velho não grava mais"), SavedCount(Save, Slot, Key), 1);

	// EndPlay do velho depois do respawn não pode tirar o novo.
	OldPawn->Destroy();
	NewInventory->AddItem(Gem, 1);
	TestTrue(TEXT("save após destruir o velho"), Save->SaveGame(Slot));
	TestEqual(TEXT("pawn novo continua registrado"), SavedCount(Save, Slot, Gem), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInventoryNonPlayerOwnerTest, "CatalogAnchor.Inventory.NonPlayerOwnerNotSaved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInventoryNonPlayerOwnerTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveWorldTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	FTestWorld TestWorld;
	if (!TestNotNull(TEXT("subsystem de save"), TestWorld.Save))
	{
		return false;
	}
	UCatalogSaveSubsystem* Save = TestWorld.Save;
	UWorld* World = TestWorld.World;
	UCatalogItemDefinition* Potion = MakeItem(TEXT("TestPotion"));
	UCatalogItemDefinition* Loot = MakeItem(TEXT("TestLoot"));
	TestTrue(TEXT("estado ativo criado"), Save->SavePayloadToSlot(MakePayload(Potion, 2), TestWorld.Slot));

	// Baú: ator comum, nem Pawn nem Controller.
	UCatalogInventoryComponent* Chest = MakeInventory(World->SpawnActor<AActor>(), true);
	TestEqual(TEXT("baú não registra (não recebe o estado ativo)"), Chest->GetItemCount(Potion), 0);

	// NPC: pawn de controller não local.
	APawn* Npc = World->SpawnActor<APawn>();
	UCatalogInventoryComponent* NpcInventory = MakeInventory(Npc, true);
	APlayerController* RemotePC = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("PC remoto"), RemotePC))
	{
		return false;
	}
	RemotePC->Possess(Npc);
	TestEqual(TEXT("pawn de controller não local não registra"), NpcInventory->GetItemCount(Potion), 0);

	Chest->AddItem(Loot, 4);
	NpcInventory->AddItem(Loot, 3);
	TestTrue(TEXT("save"), Save->SaveGame(TestWorld.Slot));
	TestEqual(TEXT("save não leva o loot de baú/NPC"), SavedCount(Save, TestWorld.Slot, Loot), 0);
	TestEqual(TEXT("save mantém o inventário do estado ativo"), SavedCount(Save, TestWorld.Slot, Potion), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
