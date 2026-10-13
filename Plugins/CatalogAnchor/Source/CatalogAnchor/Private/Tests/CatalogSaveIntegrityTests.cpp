// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogAnchorSettings.h"
#include "CatalogSaveGame.h"
#include "CatalogSaveSubsystem.h"
#include "CatalogSaveTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

#define CATALOG_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

namespace CatalogSaveIntegrityTestsPrivate
{
	constexpr int32 UserIndex = 0;

	// GameInstance cru (sem Init nem mundo) como outer: basta para o disco. Slot único; apaga tudo e restaura BackupCount no destrutor.
	struct FScratch
	{
		UGameInstance* GameInstance = nullptr;
		UCatalogSaveSubsystem* Save = nullptr;
		FName Slot;
		int32 PreviousBackupCount = 0;

		explicit FScratch(int32 BackupCount)
		{
			GameInstance = NewObject<UGameInstance>(GEngine);
			GameInstance->AddToRoot();
			Save = NewObject<UCatalogSaveSubsystem>(GameInstance);
			Slot = FName(*(TEXT("CatalogAnchorTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			UCatalogAnchorSettings* Settings = GetMutableDefault<UCatalogAnchorSettings>();
			PreviousBackupCount = Settings->BackupCount;
			Settings->BackupCount = BackupCount;
		}

		~FScratch()
		{
			Save->DeleteSlot(Slot);
			GetMutableDefault<UCatalogAnchorSettings>()->BackupCount = PreviousBackupCount;
			GameInstance->RemoveFromRoot();
		}

		FString Primary() const
		{
			return Slot.ToString().ToLower();
		}

		FString Backup(int32 Index) const
		{
			return UCatalogSaveSubsystem::GetBackupSlotName(Slot, Index).ToString().ToLower();
		}

		/** Grava um payload identificado pelo yaw; devolve o resultado do save. */
		bool SaveYaw(float Yaw) const
		{
			FCatalogSavePayload Payload;
			Payload.PlayerYaw = Yaw;
			return Save->SavePayloadToSlot(Payload, Slot);
		}
	};

	bool FileExists(const FString& File)
	{
		return UGameplayStatics::DoesSaveGameExist(File, UserIndex);
	}

	// Yaw gravado no arquivo, lido cru (sem CRC, fallback nem migração); -1 se ilegível.
	float RawYaw(const FString& File)
	{
		const UCatalogSaveGame* Envelope = Cast<UCatalogSaveGame>(UGameplayStatics::LoadGameFromSlot(File, UserIndex));
		return Envelope ? Envelope->Payload.PlayerYaw : -1.f;
	}

	// Yaw que o subsystem entrega para o slot (com CRC, fallback e migração); -1 se o load falha.
	float LoadedYaw(const FScratch& Scratch)
	{
		FCatalogSavePayload Payload;
		return Scratch.Save->LoadPayloadFromSlot(Scratch.Slot, Payload) ? Payload.PlayerYaw : -1.f;
	}

	// Envelope válido na estrutura com o CRC dado: corrupção que o formato do arquivo não denuncia.
	void WriteEnvelope(const FString& File, const FCatalogSavePayload& Payload, uint32 Crc)
	{
		UCatalogSaveGame* Envelope = NewObject<UCatalogSaveGame>();
		Envelope->Payload = Payload;
		Envelope->PayloadCrc = Crc;
		UGameplayStatics::SaveGameToSlot(Envelope, File, UserIndex);
	}

	// Vira o CRC do arquivo de lado: o payload fica igual, o arquivo deixa de conferir.
	void BreakCrc(const FString& File)
	{
		UCatalogSaveGame* Envelope = Cast<UCatalogSaveGame>(UGameplayStatics::LoadGameFromSlot(File, UserIndex));
		if (Envelope)
		{
			Envelope->PayloadCrc ^= 0x5A5A5A5Au;
			UGameplayStatics::SaveGameToSlot(Envelope, File, UserIndex);
		}
	}

	FCatalogSavePayload MakeFullPayload()
	{
		FCatalogSavePayload Payload;
		Payload.Header.SaveVersion = 2;
		Payload.Header.SaveId = FGuid(1, 2, 3, 4);
		Payload.Header.Timestamp = FDateTime(2026, 5, 6, 7, 8, 9);
		FCatalogInventorySlot& InventorySlot = Payload.Inventory.AddDefaulted_GetRef();
		InventorySlot.ItemId = TEXT("Potion");
		InventorySlot.Quantity = 3;
		Payload.PlayerLocation = FVector(1.0, 2.0, 3.0);
		Payload.PlayerYaw = 45.f;
		Payload.Metadata.MapName = TEXT("Lvl_Forest");
		Payload.Metadata.PlaytimeSeconds = 12.5;
		Payload.Metadata.DisplayName = TEXT("Save");
		FCatalogSaveRecord& Record = Payload.Records.AddDefaulted_GetRef();
		Record.Key = TEXT("Inventory");
		Record.Version = 1;
		Record.Data = { 1, 2, 3 };
		return Payload;
	}

	template <typename StructType>
	int32 CountProperties()
	{
		int32 Count = 0;
		for (TFieldIterator<FProperty> It(StructType::StaticStruct()); It; ++It)
		{
			++Count;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSavePayloadCrcTest, "CatalogAnchor.Save.PayloadCrcCoversEveryField", CATALOG_TEST_FLAGS)

bool FCatalogSavePayloadCrcTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveIntegrityTestsPrivate;

	// Campo novo nos structs do payload: inclua-o em CatalogComputePayloadCrc, a mutação abaixo, e atualize a contagem.
	TestEqual(TEXT("campos de FCatalogSaveHeader"), CountProperties<FCatalogSaveHeader>(), 3);
	TestEqual(TEXT("campos de FCatalogInventorySlot"), CountProperties<FCatalogInventorySlot>(), 2);
	TestEqual(TEXT("campos de FCatalogSaveMetadata"), CountProperties<FCatalogSaveMetadata>(), 3);
	TestEqual(TEXT("campos de FCatalogSaveRecord"), CountProperties<FCatalogSaveRecord>(), 3);
	TestEqual(TEXT("campos de FCatalogSavePayload"), CountProperties<FCatalogSavePayload>(), 6);

	const FCatalogSavePayload Base = MakeFullPayload();
	const uint32 BaseCrc = CatalogComputePayloadCrc(Base);
	TestTrue(TEXT("CRC nunca é 0"), BaseCrc != 0 && CatalogComputePayloadCrc(FCatalogSavePayload()) != 0);
	TestEqual(TEXT("determinístico"), CatalogComputePayloadCrc(MakeFullPayload()), BaseCrc);

	struct FMutation
	{
		const TCHAR* Name;
		TFunction<void(FCatalogSavePayload&)> Apply;
	};
	const TArray<FMutation> Mutations = {
		{ TEXT("Header.SaveVersion"), [](FCatalogSavePayload& P) { P.Header.SaveVersion = 3; } },
		{ TEXT("Header.SaveId.A"), [](FCatalogSavePayload& P) { P.Header.SaveId.A = 9; } },
		{ TEXT("Header.SaveId.B"), [](FCatalogSavePayload& P) { P.Header.SaveId.B = 9; } },
		{ TEXT("Header.SaveId.C"), [](FCatalogSavePayload& P) { P.Header.SaveId.C = 9; } },
		{ TEXT("Header.SaveId.D"), [](FCatalogSavePayload& P) { P.Header.SaveId.D = 9; } },
		{ TEXT("Header.Timestamp"), [](FCatalogSavePayload& P) { P.Header.Timestamp += FTimespan::FromSeconds(1); } },
		{ TEXT("Inventory.ItemId"), [](FCatalogSavePayload& P) { P.Inventory[0].ItemId = TEXT("Sword"); } },
		{ TEXT("Inventory.Quantity"), [](FCatalogSavePayload& P) { P.Inventory[0].Quantity = 4; } },
		{ TEXT("Inventory.Num"), [](FCatalogSavePayload& P) { P.Inventory.AddDefaulted(); } },
		{ TEXT("PlayerLocation.X"), [](FCatalogSavePayload& P) { P.PlayerLocation.X = 10.0; } },
		{ TEXT("PlayerLocation.Y"), [](FCatalogSavePayload& P) { P.PlayerLocation.Y = 10.0; } },
		{ TEXT("PlayerLocation.Z"), [](FCatalogSavePayload& P) { P.PlayerLocation.Z = 10.0; } },
		{ TEXT("PlayerYaw"), [](FCatalogSavePayload& P) { P.PlayerYaw = 46.f; } },
		{ TEXT("Metadata.MapName"), [](FCatalogSavePayload& P) { P.Metadata.MapName = TEXT("Lvl_Cave"); } },
		{ TEXT("Metadata.PlaytimeSeconds"), [](FCatalogSavePayload& P) { P.Metadata.PlaytimeSeconds = 13.0; } },
		{ TEXT("Metadata.DisplayName"), [](FCatalogSavePayload& P) { P.Metadata.DisplayName = TEXT("Outro"); } },
		{ TEXT("Records.Key"), [](FCatalogSavePayload& P) { P.Records[0].Key = TEXT("Quests"); } },
		{ TEXT("Records.Version"), [](FCatalogSavePayload& P) { P.Records[0].Version = 2; } },
		{ TEXT("Records.Data byte"), [](FCatalogSavePayload& P) { P.Records[0].Data[1] = 9; } },
		{ TEXT("Records.Data length"), [](FCatalogSavePayload& P) { P.Records[0].Data.Add(4); } },
		{ TEXT("Records.Num"), [](FCatalogSavePayload& P) { P.Records.AddDefaulted(); } },
	};
	for (const FMutation& Mutation : Mutations)
	{
		FCatalogSavePayload Changed = Base;
		Mutation.Apply(Changed);
		TestTrue(FString::Printf(TEXT("CRC muda com %s"), Mutation.Name), CatalogComputePayloadCrc(Changed) != BaseCrc);
	}

	// O disco omite o que é igual ao default e FName compara sem caixa: nada disso pode mudar o CRC.
	FCatalogSavePayload Zero;
	FCatalogSavePayload NegativeZero;
	NegativeZero.PlayerYaw = -0.f;
	NegativeZero.PlayerLocation = FVector(-0.0, -0.0, -0.0);
	NegativeZero.Metadata.PlaytimeSeconds = -0.0;
	TestEqual(TEXT("-0.0 é igual a 0.0"), CatalogComputePayloadCrc(NegativeZero), CatalogComputePayloadCrc(Zero));

	FCatalogSavePayload OtherCase = Base;
	OtherCase.Inventory[0].ItemId = TEXT("POTION");
	OtherCase.Records[0].Key = TEXT("inventory");
	TestEqual(TEXT("FName sem diferenciar caixa"), CatalogComputePayloadCrc(OtherCase), BaseCrc);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveCrcRoundTripTest, "CatalogAnchor.Save.Integrity.RoundTripWritesAndVerifiesCrc", CATALOG_TEST_FLAGS)

bool FCatalogSaveCrcRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveIntegrityTestsPrivate;
	FScratch Scratch(1);

	// -0.0 e campos no default são omitidos no disco: o load tem de reproduzir o mesmo CRC.
	FCatalogSavePayload Payload = MakeFullPayload();
	Payload.PlayerYaw = -0.f;
	Payload.PlayerLocation = FVector(-0.0, 5.0, 0.0);
	Payload.Metadata.PlaytimeSeconds = -0.0;
	Payload.Records.AddDefaulted();
	TestTrue(TEXT("grava"), Scratch.Save->SavePayloadToSlot(Payload, Scratch.Slot));

	const UCatalogSaveGame* Envelope = Cast<UCatalogSaveGame>(UGameplayStatics::LoadGameFromSlot(Scratch.Primary(), UserIndex));
	if (TestNotNull(TEXT("envelope legível"), Envelope))
	{
		TestTrue(TEXT("CRC gravado"), Envelope->PayloadCrc != 0);
		TestEqual(TEXT("CRC confere com o payload lido"), Envelope->PayloadCrc, CatalogComputePayloadCrc(Envelope->Payload));
	}

	FCatalogSavePayload Loaded;
	TestTrue(TEXT("load confere o CRC"), Scratch.Save->LoadPayloadFromSlot(Scratch.Slot, Loaded));
	TestEqual(TEXT("dados chegam"), Loaded.Records.Num(), 2);
	TestTrue(TEXT("localização"), Loaded.PlayerLocation.Y == 5.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveBackupFallbackTest, "CatalogAnchor.Save.Integrity.CorruptSlotFallsBackToBackup", CATALOG_TEST_FLAGS)

bool FCatalogSaveBackupFallbackTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveIntegrityTestsPrivate;
	FScratch Scratch(1);

	TestTrue(TEXT("save 1"), Scratch.SaveYaw(10.f));
	TestFalse(TEXT("primeiro save não tem backup"), FileExists(Scratch.Backup(1)));
	TestTrue(TEXT("save 2"), Scratch.SaveYaw(20.f));
	TestEqual(TEXT("slot guarda o save novo"), RawYaw(Scratch.Primary()), 20.f);
	TestEqual(TEXT("backup guarda o save anterior"), RawYaw(Scratch.Backup(1)), 10.f);

	BreakCrc(Scratch.Primary());
	TestEqual(TEXT("slot corrompido lê o backup"), LoadedYaw(Scratch), 10.f);
	TestTrue(TEXT("LoadGame também"), Scratch.Save->LoadGame(Scratch.Slot));

	bool bRecovered = false;
	for (const FCatalogSaveSlotInfo& Info : Scratch.Save->ListSaveSlots())
	{
		bRecovered |= Info.SlotName == Scratch.Slot && Info.State == ECatalogSaveSlotState::Recovered;
	}
	TestTrue(TEXT("ListSaveSlots marca Recovered"), bRecovered);

	// Gravar de novo conserta o slot: o novo save é íntegro e o backup continua o último bom.
	TestTrue(TEXT("save 3"), Scratch.SaveYaw(30.f));
	TestEqual(TEXT("slot volta a conferir"), LoadedYaw(Scratch), 30.f);
	TestEqual(TEXT("backup não recebeu o arquivo corrompido"), RawYaw(Scratch.Backup(1)), 10.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveNoBackupTest, "CatalogAnchor.Save.Integrity.NoBackupMeansCorruptSlotFails", CATALOG_TEST_FLAGS)

bool FCatalogSaveNoBackupTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveIntegrityTestsPrivate;
	FScratch Scratch(0); // BackupCount 0 desliga rotação e fallback

	TestTrue(TEXT("save 1"), Scratch.SaveYaw(10.f));
	TestTrue(TEXT("save 2"), Scratch.SaveYaw(20.f));
	TestFalse(TEXT("sem backup em disco"), FileExists(Scratch.Backup(1)));

	BreakCrc(Scratch.Primary());
	TestEqual(TEXT("slot corrompido não carrega"), LoadedYaw(Scratch), -1.f);
	TestTrue(TEXT("o slot continua existindo"), Scratch.Save->DoesSlotExist(Scratch.Slot));

	FCatalogSaveSlotInfo Found;
	for (const FCatalogSaveSlotInfo& Info : Scratch.Save->ListSaveSlots())
	{
		if (Info.SlotName == Scratch.Slot)
		{
			Found = Info;
		}
	}
	TestTrue(TEXT("ListSaveSlots lista como Unreadable"), Found.SlotName == Scratch.Slot && Found.State == ECatalogSaveSlotState::Unreadable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveRotationTest, "CatalogAnchor.Save.Integrity.RotationKeepsConfiguredCount", CATALOG_TEST_FLAGS)

bool FCatalogSaveRotationTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveIntegrityTestsPrivate;
	FScratch Scratch(2);

	for (int32 Yaw = 1; Yaw <= 4; ++Yaw)
	{
		TestTrue(TEXT("save"), Scratch.SaveYaw(static_cast<float>(Yaw)));
	}
	TestEqual(TEXT("slot = save 4"), RawYaw(Scratch.Primary()), 4.f);
	TestEqual(TEXT("_bak = save 3"), RawYaw(Scratch.Backup(1)), 3.f);
	TestEqual(TEXT("_bak2 = save 2"), RawYaw(Scratch.Backup(2)), 2.f);
	TestFalse(TEXT("sem _bak3"), FileExists(Scratch.Backup(3)));

	TestTrue(TEXT("nomes dos backups"), UCatalogSaveSubsystem::GetBackupSlotName(TEXT("Slot1"), 1) == FName(TEXT("slot1_bak"))
		&& UCatalogSaveSubsystem::GetBackupSlotName(TEXT("Slot1"), 2) == FName(TEXT("slot1_bak2"))
		&& UCatalogSaveSubsystem::GetBackupSlotName(TEXT("Slot1"), 0).IsNone());

	// O autosave não vê backup como slot de jogador.
	for (const FCatalogSaveSlotInfo& Info : Scratch.Save->ListSaveSlots())
	{
		TestFalse(TEXT("backup fora da lista"), Info.SlotName == UCatalogSaveSubsystem::GetBackupSlotName(Scratch.Slot, 1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveCorruptNotRotatedTest, "CatalogAnchor.Save.Integrity.CorruptCurrentNeverBecomesBackup", CATALOG_TEST_FLAGS)

bool FCatalogSaveCorruptNotRotatedTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveIntegrityTestsPrivate;
	FScratch Scratch(1);

	TestTrue(TEXT("save 1"), Scratch.SaveYaw(1.f));
	TestTrue(TEXT("save 2"), Scratch.SaveYaw(2.f));
	BreakCrc(Scratch.Primary());

	TestTrue(TEXT("save 3 sobre o corrompido"), Scratch.SaveYaw(3.f));
	TestEqual(TEXT("slot = save 3"), RawYaw(Scratch.Primary()), 3.f);
	TestEqual(TEXT("backup segue o último bom, não o corrompido"), RawYaw(Scratch.Backup(1)), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveFutureSchemaTest, "CatalogAnchor.Save.Integrity.FutureSchemaDoesNotFallBack", CATALOG_TEST_FLAGS)

bool FCatalogSaveFutureSchemaTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveIntegrityTestsPrivate;
	AddExpectedError(TEXT("recusando load"), EAutomationExpectedErrorFlags::Contains, 1);
	FScratch Scratch(1);

	TestTrue(TEXT("save 1"), Scratch.SaveYaw(1.f));
	TestTrue(TEXT("save 2"), Scratch.SaveYaw(2.f));

	// Arquivo íntegro de um plugin mais novo: o certo é recusar, não "restaurar" um save velho em silêncio.
	FCatalogSavePayload Future;
	Future.Header.SaveVersion = GCatalogSaveSchemaVersion + 1;
	Future.PlayerYaw = 99.f;
	WriteEnvelope(Scratch.Primary(), Future, CatalogComputePayloadCrc(Future));
	TestEqual(TEXT("schema futuro não cai no backup"), LoadedYaw(Scratch), -1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveLegacyTest, "CatalogAnchor.Save.Integrity.FileWithoutCrcStillLoads", CATALOG_TEST_FLAGS)

bool FCatalogSaveLegacyTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveIntegrityTestsPrivate;
	FScratch Scratch(1);

	// Arquivo da v0.4: schema v2, sem PayloadCrc.
	FCatalogSavePayload Legacy;
	Legacy.Header.SaveVersion = GCatalogSaveSchemaVersion;
	Legacy.PlayerYaw = 7.f;
	WriteEnvelope(Scratch.Primary(), Legacy, 0);

	TestEqual(TEXT("carrega sem conferência"), LoadedYaw(Scratch), 7.f);
	TestTrue(TEXT("LoadGame"), Scratch.Save->LoadGame(Scratch.Slot));

	// E o primeiro save por cima o guarda como backup, já que ele lê.
	TestTrue(TEXT("save novo"), Scratch.SaveYaw(8.f));
	TestEqual(TEXT("legado virou backup"), RawYaw(Scratch.Backup(1)), 7.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveTruncatedTest, "CatalogAnchor.Save.Integrity.TruncatedFileFallsBackToBackup", CATALOG_TEST_FLAGS)

bool FCatalogSaveTruncatedTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveIntegrityTestsPrivate;
	// O motor loga Error ao ler propriedade cortada; é o sintoma que o teste provoca de propósito.
	AddExpectedError(TEXT("Failed loading tagged"), EAutomationExpectedErrorFlags::Contains, 0);
	FScratch Scratch(1);

	// Payload com corpo suficiente para o corte cair no meio dele (queda de energia durante a escrita).
	FCatalogSavePayload Big = MakeFullPayload();
	Big.Records[0].Data.Init(0x42, 512);
	Big.PlayerYaw = 1.f;
	TestTrue(TEXT("save 1"), Scratch.Save->SavePayloadToSlot(Big, Scratch.Slot));
	Big.PlayerYaw = 2.f;
	TestTrue(TEXT("save 2"), Scratch.Save->SavePayloadToSlot(Big, Scratch.Slot));

	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("lê o arquivo cru"), UGameplayStatics::LoadDataFromSlot(Bytes, Scratch.Primary(), UserIndex)))
	{
		return false;
	}
	Bytes.SetNum(Bytes.Num() * 2 / 3);
	TestTrue(TEXT("regrava truncado"), UGameplayStatics::SaveDataToSlot(Bytes, Scratch.Primary(), UserIndex));

	TestEqual(TEXT("truncado lê o backup"), LoadedYaw(Scratch), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveGarbageTest, "CatalogAnchor.Save.Integrity.GarbageFileFallsBackToBackup", CATALOG_TEST_FLAGS)

bool FCatalogSaveGarbageTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveIntegrityTestsPrivate;
	FScratch Scratch(1);

	TestTrue(TEXT("save 1"), Scratch.SaveYaw(1.f));
	TestTrue(TEXT("save 2"), Scratch.SaveYaw(2.f));

	TArray<uint8> Garbage;
	Garbage.Init(0xFF, 64);
	TestTrue(TEXT("regrava lixo"), UGameplayStatics::SaveDataToSlot(Garbage, Scratch.Primary(), UserIndex));
	TestEqual(TEXT("lixo lê o backup"), LoadedYaw(Scratch), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveDeleteBackupsTest, "CatalogAnchor.Save.Integrity.DeleteSlotRemovesBackups", CATALOG_TEST_FLAGS)

bool FCatalogSaveDeleteBackupsTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveIntegrityTestsPrivate;
	FScratch Scratch(3);

	for (int32 Yaw = 1; Yaw <= 3; ++Yaw)
	{
		Scratch.SaveYaw(static_cast<float>(Yaw));
	}
	TestTrue(TEXT("backups existem"), FileExists(Scratch.Backup(1)) && FileExists(Scratch.Backup(2)));

	TestTrue(TEXT("DeleteSlot"), Scratch.Save->DeleteSlot(Scratch.Slot));
	TestFalse(TEXT("slot apagado"), FileExists(Scratch.Primary()));
	TestFalse(TEXT("_bak apagado"), FileExists(Scratch.Backup(1)));
	TestFalse(TEXT("_bak2 apagado"), FileExists(Scratch.Backup(2)));
	TestEqual(TEXT("apagado não ressuscita pelo fallback"), LoadedYaw(Scratch), -1.f);
	TestFalse(TEXT("DeleteSlot de novo"), Scratch.Save->DeleteSlot(Scratch.Slot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveReservedNamesTest, "CatalogAnchor.Save.Integrity.BackupNamesAreReserved", CATALOG_TEST_FLAGS)

bool FCatalogSaveReservedNamesTest::RunTest(const FString& Parameters)
{
	using namespace CatalogSaveIntegrityTestsPrivate;
	TestFalse(TEXT("_bak"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("Slot1_bak"))));
	TestFalse(TEXT("_bak2"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("slot1_bak2"))));
	TestFalse(TEXT("_BAK10 sem diferenciar caixa"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("Slot1_BAK10"))));
	TestFalse(TEXT("só o sufixo"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("_bak"))));
	TestTrue(TEXT("bak sem underscore"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("bak"))));
	TestTrue(TEXT("_bak no meio"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("slot_bak_x"))));
	TestTrue(TEXT("_bakery"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("slot_bakery"))));

	FScratch Scratch(1);
	TestFalse(TEXT("SaveGame recusa o nome reservado"), Scratch.Save->SaveGame(FName(*(Scratch.Slot.ToString() + TEXT("_bak")))));
	TestFalse(TEXT("DeleteSlot recusa nome inválido"), Scratch.Save->DeleteSlot(FName(TEXT("../Config/DefaultGame"))));
	return true;
}

#undef CATALOG_TEST_FLAGS

#endif // WITH_DEV_AUTOMATION_TESTS
