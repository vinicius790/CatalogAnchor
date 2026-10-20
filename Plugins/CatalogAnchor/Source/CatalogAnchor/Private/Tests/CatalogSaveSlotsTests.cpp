// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogBlueprintLibrary.h"
#include "CatalogHealthComponent.h"
#include "CatalogInventoryComponent.h"
#include "CatalogItemDefinition.h"
#include "CatalogSaveGame.h"
#include "CatalogSaveSubsystem.h"
#include "CatalogSaveTypes.h"
#include "CatalogTestGuards.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/StringOutputDevice.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

#define CATALOG_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

namespace CatalogSaveSlotsTestsPrivate
{
	constexpr int32 UserIndex = 0;

	FName UniqueSlot(const TCHAR* Tag)
	{
		return FName(*FString::Printf(TEXT("CatalogAnchorTest_%s_%s"), Tag, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	// Arquivo escrito à mão no layout do plugin, com o CRC certo: controla timestamp e versão.
	void WriteSlotFile(FName Slot, const FCatalogSavePayload& Payload)
	{
		UCatalogSaveGame* Envelope = NewObject<UCatalogSaveGame>();
		Envelope->Payload = Payload;
		Envelope->PayloadCrc = CatalogComputePayloadCrc(Payload);
		UGameplayStatics::SaveGameToSlot(Envelope, Slot.ToString().ToLower(), UserIndex);
	}

	const FCatalogSaveSlotInfo* FindInfo(const TArray<FCatalogSaveSlotInfo>& Slots, FName Slot)
	{
		return Slots.FindByPredicate([Slot](const FCatalogSaveSlotInfo& Info) { return Info.SlotName == Slot; });
	}

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
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
			World->InitializeActorsForPlay(FURL());
			World->SetBegunPlay(true);
			Save = GameInstance->GetSubsystem<UCatalogSaveSubsystem>();
			Slot = UniqueSlot(TEXT("World"));
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
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	};

	UCatalogItemDefinition* MakeItem(const TCHAR* BaseName)
	{
		UPackage* Package = GetTransientPackage();
		UCatalogItemDefinition* Item = NewObject<UCatalogItemDefinition>(Package,
			MakeUniqueObjectName(Package, UCatalogItemDefinition::StaticClass(), FName(BaseName)));
		Item->MaxStackSize = 99;
		return Item;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveListSlotsTest, "CatalogAnchor.Save.Slots.ListSaveSlotsMetadataAndOrder", CATALOG_TEST_FLAGS)

bool FCatalogSaveListSlotsTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveSlotsTestsPrivate;
	// Outer cru: a classe exige GameInstance (ClassWithin), mas o disco não precisa de Init nem de mundo.
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	UCatalogSaveSubsystem* Save = NewObject<UCatalogSaveSubsystem>(GameInstance);
	const FName OldSlot = UniqueSlot(TEXT("Old"));
	const FName NewSlot = UniqueSlot(TEXT("New"));
	const FName BrokenSlot = UniqueSlot(TEXT("Broken"));

	FCatalogSavePayload Old;
	Old.Header.SaveVersion = GCatalogSaveSchemaVersion;
	Old.Header.Timestamp = FDateTime(2026, 1, 1);
	Old.Metadata.MapName = TEXT("Lvl_Old");
	Old.Metadata.PlaytimeSeconds = 100.0;
	Old.Metadata.DisplayName = TEXT("Alpha");
	WriteSlotFile(OldSlot, Old);

	// v1 no disco: SaveVersion mostra a versão do arquivo, não a migrada.
	FCatalogSavePayload NewV1;
	NewV1.Header.SaveVersion = 1;
	NewV1.Header.Timestamp = FDateTime(2026, 3, 1);
	NewV1.Metadata.MapName = TEXT("Lvl_New");
	WriteSlotFile(NewSlot, NewV1);

	// Sem Timestamp nem backup: ilegível, e vai para o fim.
	FCatalogSavePayload Broken;
	Broken.Header.SaveVersion = GCatalogSaveSchemaVersion;
	UCatalogSaveGame* BrokenEnvelope = NewObject<UCatalogSaveGame>();
	BrokenEnvelope->Payload = Broken;
	BrokenEnvelope->PayloadCrc = CatalogComputePayloadCrc(Broken) ^ 0xFFu;
	UGameplayStatics::SaveGameToSlot(BrokenEnvelope, BrokenSlot.ToString().ToLower(), UserIndex);

	const TArray<FCatalogSaveSlotInfo> Slots = Save->ListSaveSlots();
	const FCatalogSaveSlotInfo* OldInfo = FindInfo(Slots, OldSlot);
	const FCatalogSaveSlotInfo* NewInfo = FindInfo(Slots, NewSlot);
	const FCatalogSaveSlotInfo* BrokenInfo = FindInfo(Slots, BrokenSlot);
	if (TestNotNull(TEXT("slot antigo listado"), OldInfo) && TestNotNull(TEXT("slot v1 listado"), NewInfo)
		&& TestNotNull(TEXT("slot corrompido listado"), BrokenInfo))
	{
		TestTrue(TEXT("estado Ok"), OldInfo->State == ECatalogSaveSlotState::Ok && NewInfo->State == ECatalogSaveSlotState::Ok);
		TestEqual(TEXT("mapa"), OldInfo->MapName, FString(TEXT("Lvl_Old")));
		TestEqual(TEXT("tempo de jogo"), OldInfo->PlaytimeSeconds, 100.0);
		TestEqual(TEXT("nome mostrado"), OldInfo->DisplayName, FString(TEXT("Alpha")));
		TestTrue(TEXT("data"), OldInfo->Timestamp == FDateTime(2026, 1, 1));
		TestEqual(TEXT("versão do arquivo v2"), OldInfo->SaveVersion, GCatalogSaveSchemaVersion);
		TestEqual(TEXT("versão do arquivo v1 (antes da migração)"), NewInfo->SaveVersion, 1);
		TestTrue(TEXT("corrompido sem backup é Unreadable"), BrokenInfo->State == ECatalogSaveSlotState::Unreadable);

		// Ordem relativa: mais novo, mais antigo, ilegível.
		const int32 NewAt = Slots.IndexOfByPredicate([&](const FCatalogSaveSlotInfo& Info) { return Info.SlotName == NewSlot; });
		const int32 OldAt = Slots.IndexOfByPredicate([&](const FCatalogSaveSlotInfo& Info) { return Info.SlotName == OldSlot; });
		const int32 BrokenAt = Slots.IndexOfByPredicate([&](const FCatalogSaveSlotInfo& Info) { return Info.SlotName == BrokenSlot; });
		TestTrue(TEXT("mais novo primeiro"), NewAt < OldAt);
		TestTrue(TEXT("ilegível por último"), OldAt < BrokenAt);
	}

	for (const FName Slot : { OldSlot, NewSlot, BrokenSlot })
	{
		TestTrue(TEXT("limpeza"), Save->DeleteSlot(Slot));
	}
	TestNull(TEXT("apagado some da lista"), FindInfo(Save->ListSaveSlots(), OldSlot));
	GameInstance->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveLibrarySaveTest, "CatalogAnchor.Library.SaveWrappers", CATALOG_TEST_FLAGS)

bool FCatalogSaveLibrarySaveTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveSlotsTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	FTestWorld TestWorld;
	if (!TestNotNull(TEXT("subsystem de save"), TestWorld.Save))
	{
		return false;
	}
	const UObject* Context = TestWorld.World;
	const FName Slot = TestWorld.Slot;

	TestTrue(TEXT("GetSaveSubsystem acha o do GameInstance"), UCatalogBlueprintLibrary::GetSaveSubsystem(Context) == TestWorld.Save);
	TestNull(TEXT("sem contexto"), UCatalogBlueprintLibrary::GetSaveSubsystem(nullptr));
	TestFalse(TEXT("save sem contexto falha limpo"), UCatalogBlueprintLibrary::SaveCatalogGame(nullptr, Slot));
	TestEqual(TEXT("lista sem contexto é vazia"), UCatalogBlueprintLibrary::ListCatalogSaveSlots(nullptr).Num(), 0);

	TestFalse(TEXT("slot ainda não existe"), UCatalogBlueprintLibrary::DoesCatalogSaveSlotExist(Context, Slot));
	TestTrue(TEXT("SaveCatalogGame"), UCatalogBlueprintLibrary::SaveCatalogGame(Context, Slot));
	TestTrue(TEXT("slot existe"), UCatalogBlueprintLibrary::DoesCatalogSaveSlotExist(Context, Slot));

	// Lista num local: FindInfo aponta para dentro dela, e um temporário morreria antes do uso abaixo.
	const TArray<FCatalogSaveSlotInfo> Listed = UCatalogBlueprintLibrary::ListCatalogSaveSlots(Context);
	const FCatalogSaveSlotInfo* Info = FindInfo(Listed, Slot);
	TestTrue(TEXT("ListCatalogSaveSlots lista o slot"), Info && Info->State == ECatalogSaveSlotState::Ok);

	TestTrue(TEXT("LoadCatalogGame"), UCatalogBlueprintLibrary::LoadCatalogGame(Context, Slot));
	TestFalse(TEXT("nome inválido"), UCatalogBlueprintLibrary::SaveCatalogGame(Context, FName(TEXT("../evil"))));
	TestFalse(TEXT("nome de backup é reservado"), UCatalogBlueprintLibrary::SaveCatalogGame(Context, FName(*(Slot.ToString() + TEXT("_bak")))));

	TestTrue(TEXT("DeleteCatalogSaveSlot"), UCatalogBlueprintLibrary::DeleteCatalogSaveSlot(Context, Slot));
	TestFalse(TEXT("slot apagado"), UCatalogBlueprintLibrary::DoesCatalogSaveSlotExist(Context, Slot));
	TestFalse(TEXT("apagar de novo"), UCatalogBlueprintLibrary::DeleteCatalogSaveSlot(Context, Slot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogLibraryHealthInventoryTest, "CatalogAnchor.Library.HealthAndInventoryHelpers", CATALOG_TEST_FLAGS)

bool FCatalogLibraryHealthInventoryTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveSlotsTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	FTestWorld TestWorld;
	if (!TestNotNull(TEXT("subsystem de save"), TestWorld.Save))
	{
		return false;
	}
	UWorld* World = TestWorld.World;

	// Sem componentes: valores neutros, sem log nem crash.
	AActor* Bare = World->SpawnActor<AActor>();
	float Fraction = 1.f;
	TestFalse(TEXT("sem vida"), UCatalogBlueprintLibrary::GetActorHealthFraction(Bare, Fraction));
	TestEqual(TEXT("fração neutra"), Fraction, 0.f);
	TestFalse(TEXT("sem vida não está morto"), UCatalogBlueprintLibrary::IsActorDead(Bare));
	TestEqual(TEXT("sem vida não cura"), UCatalogBlueprintLibrary::HealActor(Bare, 10.f), 0.f);
	TestNull(TEXT("sem componente de vida"), UCatalogBlueprintLibrary::GetHealthComponent(nullptr));
	TestEqual(TEXT("sem inventário"), UCatalogBlueprintLibrary::GetActorItemCount(Bare, nullptr), 0);
	TestFalse(TEXT("sem inventário não tem item"), UCatalogBlueprintLibrary::ActorHasItem(Bare, nullptr));

	APawn* Pawn = World->SpawnActor<APawn>();
	UCatalogHealthComponent* Health = NewObject<UCatalogHealthComponent>(Pawn);
	Health->RegisterComponent();
	UCatalogInventoryComponent* Inventory = NewObject<UCatalogInventoryComponent>(Pawn);
	Inventory->RegisterComponent();

	TestTrue(TEXT("acha a vida do ator"), UCatalogBlueprintLibrary::GetHealthComponent(Pawn) == Health);
	TestTrue(TEXT("acha o inventário do ator"), UCatalogBlueprintLibrary::GetInventoryComponent(Pawn) == Inventory);
	TestTrue(TEXT("vida cheia"), UCatalogBlueprintLibrary::GetActorHealthFraction(Pawn, Fraction) && FMath::IsNearlyEqual(Fraction, 1.f));

	const float Half = Health->GetHealth() * 0.5f;
	Health->SetHealth(Half);
	const float Healed = UCatalogBlueprintLibrary::HealActor(Pawn, Half * 0.5f);
	TestTrue(TEXT("HealActor cura"), FMath::IsNearlyEqual(Healed, Half * 0.5f) && FMath::IsNearlyEqual(Health->GetHealth(), Half * 1.5f));
	TestFalse(TEXT("vivo"), UCatalogBlueprintLibrary::IsActorDead(Pawn));
	Health->SetHealth(0.f);
	TestTrue(TEXT("morto"), UCatalogBlueprintLibrary::IsActorDead(Pawn));

	UCatalogItemDefinition* Potion = MakeItem(TEXT("TestPotion"));
	Inventory->AddItem(Potion, 3);
	TestEqual(TEXT("quantidade"), UCatalogBlueprintLibrary::GetActorItemCount(Pawn, Potion), 3);
	TestTrue(TEXT("tem 3"), UCatalogBlueprintLibrary::ActorHasItem(Pawn, Potion, 3));
	TestFalse(TEXT("não tem 4"), UCatalogBlueprintLibrary::ActorHasItem(Pawn, Potion, 4));

	// HUD e GameMode costumam ter o controller na mão: ele resolve para o pawn.
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (TestNotNull(TEXT("controller"), Controller))
	{
		Controller->Possess(Pawn);
		TestTrue(TEXT("controller resolve a vida do pawn"), UCatalogBlueprintLibrary::GetHealthComponent(Controller) == Health);
		TestEqual(TEXT("controller resolve o inventário do pawn"), UCatalogBlueprintLibrary::GetActorItemCount(Controller, Potion), 3);
	}
	return true;
}

#if !UE_BUILD_SHIPPING

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveConsoleTest, "CatalogAnchor.Console.SaveCommands", CATALOG_TEST_FLAGS)

bool FCatalogSaveConsoleTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveSlotsTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	FTestWorld TestWorld;
	if (!TestNotNull(TEXT("subsystem de save"), TestWorld.Save))
	{
		return false;
	}
	UWorld* World = TestWorld.World;
	const FString Slot = TestWorld.Slot.ToString();

	const auto Run = [World](const FString& Command)
	{
		FStringOutputDevice Output;
		IConsoleManager::Get().ProcessUserConsoleInput(*Command, Output, World);
		return FString(Output);
	};

	TestTrue(TEXT("comandos registrados"), IConsoleManager::Get().FindConsoleObject(TEXT("catalog.Save.List")) != nullptr
		&& IConsoleManager::Get().FindConsoleObject(TEXT("catalog.Save.Now")) != nullptr
		&& IConsoleManager::Get().FindConsoleObject(TEXT("catalog.Save.Load")) != nullptr
		&& IConsoleManager::Get().FindConsoleObject(TEXT("catalog.Save.Delete")) != nullptr);

	TestTrue(TEXT("Now grava"), Run(TEXT("catalog.Save.Now ") + Slot).Contains(TEXT("gravado")));
	TestTrue(TEXT("slot em disco"), TestWorld.Save->DoesSlotExist(TestWorld.Slot));
	TestTrue(TEXT("List mostra o slot"), Run(TEXT("catalog.Save.List")).Contains(Slot, ESearchCase::IgnoreCase));
	TestTrue(TEXT("Load carrega"), Run(TEXT("catalog.Save.Load ") + Slot).Contains(TEXT("carregado")));
	TestTrue(TEXT("nome inválido é recusado"), Run(TEXT("catalog.Save.Now ../evil")).Contains(TEXT("inválido")));
	TestTrue(TEXT("Delete apaga"), Run(TEXT("catalog.Save.Delete ") + Slot).Contains(TEXT("slot apagado")));
	TestFalse(TEXT("slot sumiu"), TestWorld.Save->DoesSlotExist(TestWorld.Slot));
	TestTrue(TEXT("Load de slot ausente falha"), Run(TEXT("catalog.Save.Load ") + Slot).Contains(TEXT("falhou")));
	return true;
}

#endif // !UE_BUILD_SHIPPING

#undef CATALOG_TEST_FLAGS

#endif // WITH_DEV_AUTOMATION_TESTS
