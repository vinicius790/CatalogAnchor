// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogSaveSubsystem.h"
#include "CatalogSaveTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Contextos explícitos: ApplicationContextMask não existe mais em EAutomationTestFlags (UE 5.8).

namespace CatalogSaveTestsPrivate
{
	FCatalogInventorySlot MakeInventorySlot(FName ItemId, int32 Quantity)
	{
		FCatalogInventorySlot InventorySlot;
		InventorySlot.ItemId = ItemId;
		InventorySlot.Quantity = Quantity;
		return InventorySlot;
	}

	// v1 com uma entrada boa e duas que o inventário v2 nunca gravaria.
	void AddMixedInventory(FCatalogSavePayload& Payload)
	{
		Payload.Inventory.Add(MakeInventorySlot(TEXT("Potion"), 3));
		Payload.Inventory.Add(MakeInventorySlot(NAME_None, 1));
		Payload.Inventory.Add(MakeInventorySlot(TEXT("Sword"), 0));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveMigrateV0Test, "CatalogAnchor.Save.MigrateV0ToV2",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogSaveMigrateV0Test::RunTest(const FString& Parameters)
{
	// SaveVersion 0 = arquivo sem o campo; atravessa v0→v1→v2 numa chamada.
	FCatalogSavePayload Payload;
	CatalogSaveTestsPrivate::AddMixedInventory(Payload);

	TestTrue(TEXT("v0 migra"), CatalogApplySaveMigration(Payload));
	TestEqual(TEXT("chega na versão atual"), Payload.Header.SaveVersion, GCatalogSaveSchemaVersion);
	TestTrue(TEXT("ganha SaveId"), Payload.Header.SaveId.IsValid());
	TestTrue(TEXT("ganha Timestamp"), Payload.Header.Timestamp.GetTicks() != 0);
	TestEqual(TEXT("passo v1→v2 também roda"), Payload.Inventory.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveMigrateV1Test, "CatalogAnchor.Save.MigrateV1ToV2",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogSaveMigrateV1Test::RunTest(const FString& Parameters)
{
	FCatalogSavePayload Payload;
	Payload.Header.SaveVersion = 1;
	const FGuid OriginalId = FGuid::NewGuid();
	Payload.Header.SaveId = OriginalId;
	Payload.PlayerYaw = 90.f;
	CatalogSaveTestsPrivate::AddMixedInventory(Payload);

	TestTrue(TEXT("v1 migra"), CatalogApplySaveMigration(Payload));
	TestEqual(TEXT("versão 2"), Payload.Header.SaveVersion, 2);
	TestTrue(TEXT("SaveId preservado"), Payload.Header.SaveId == OriginalId);
	TestTrue(TEXT("campos v1 preservados"), Payload.PlayerYaw == 90.f);
	if (TestEqual(TEXT("entradas inválidas descartadas"), Payload.Inventory.Num(), 1))
	{
		TestTrue(TEXT("entrada válida mantida"), Payload.Inventory[0].ItemId == FName(TEXT("Potion")));
		TestEqual(TEXT("quantidade mantida"), Payload.Inventory[0].Quantity, 3);
	}
	TestTrue(TEXT("Metadata desconhecida"), Payload.Metadata.MapName.IsEmpty() && Payload.Metadata.PlaytimeSeconds == 0.0);
	TestEqual(TEXT("sem Records"), Payload.Records.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveCurrentVersionTest, "CatalogAnchor.Save.CurrentVersionIsNoOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogSaveCurrentVersionTest::RunTest(const FString& Parameters)
{
	// Migradores só rodam em versões antigas: payload já v2 sai intacto.
	FCatalogSavePayload Payload;
	Payload.Header.SaveVersion = GCatalogSaveSchemaVersion;
	CatalogSaveTestsPrivate::AddMixedInventory(Payload);

	TestTrue(TEXT("versão atual aceita"), CatalogApplySaveMigration(Payload));
	TestEqual(TEXT("inventário intocado"), Payload.Inventory.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveRejectVersionTest, "CatalogAnchor.Save.RejectsFutureAndNegativeVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogSaveRejectVersionTest::RunTest(const FString& Parameters)
{
	// A recusa loga Error de propósito; sem isto o framework reprovaria o teste.
	AddExpectedError(TEXT("recusando load"), EAutomationExpectedErrorFlags::Contains, 2);

	FCatalogSavePayload Future;
	Future.Header.SaveVersion = GCatalogSaveSchemaVersion + 1;
	TestFalse(TEXT("versão futura recusada"), CatalogApplySaveMigration(Future));
	TestEqual(TEXT("versão futura não é tocada"), Future.Header.SaveVersion, GCatalogSaveSchemaVersion + 1);

	FCatalogSavePayload Negative;
	Negative.Header.SaveVersion = -1;
	TestFalse(TEXT("versão negativa recusada"), CatalogApplySaveMigration(Negative));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveSlotNameTest, "CatalogAnchor.Save.IsValidSlotName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogSaveSlotNameTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("ASCII simples"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("Slot1"))));
	TestTrue(TEXT("'_' e '-'"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("Save_Game-02"))));
	TestTrue(TEXT("1 caractere"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("a"))));
	TestTrue(TEXT("128 caracteres"), UCatalogSaveSubsystem::IsValidSlotName(FName(*FString::ChrN(128, TEXT('a')))));

	TestFalse(TEXT("None"), UCatalogSaveSubsystem::IsValidSlotName(NAME_None));
	TestFalse(TEXT("vazio"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT(""))));
	TestFalse(TEXT("129 caracteres"), UCatalogSaveSubsystem::IsValidSlotName(FName(*FString::ChrN(129, TEXT('a')))));
	TestFalse(TEXT("espaço"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("my slot"))));
	TestFalse(TEXT("acento"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("slót"))));
	TestFalse(TEXT("CJK"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("中文"))));
	TestFalse(TEXT("path traversal"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("../evil"))));
	TestFalse(TEXT("extensão"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("slot.sav"))));
	TestFalse(TEXT("dispositivo NUL"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("nul"))));
	TestFalse(TEXT("dispositivo CON"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("CON"))));
	TestFalse(TEXT("dispositivo COM1"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("com1"))));
	TestFalse(TEXT("dispositivo LPT9"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("Lpt9"))));
	TestTrue(TEXT("prefixo de dispositivo"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("Console"))));
	TestTrue(TEXT("COM com dois dígitos"), UCatalogSaveSubsystem::IsValidSlotName(FName(TEXT("COM10"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogSaveFindRecordTest, "CatalogAnchor.Save.FindSaveRecord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogSaveFindRecordTest::RunTest(const FString& Parameters)
{
	FCatalogSavePayload Payload;
	FCatalogSaveRecord& Inventory = Payload.Records.AddDefaulted_GetRef();
	Inventory.Key = TEXT("Inventory");
	Inventory.Version = 1;
	FCatalogSaveRecord& Quests = Payload.Records.AddDefaulted_GetRef();
	Quests.Key = TEXT("Quests");
	Quests.Data = { 7, 8, 9 };

	FCatalogSaveRecord* Found = CatalogFindSaveRecord(Payload, TEXT("Quests"));
	if (TestNotNull(TEXT("acha pela Key"), Found))
	{
		Found->Version = 2;
		TestEqual(TEXT("ponteiro aponta para o próprio registro"), Payload.Records[1].Version, 2);
	}

	const FCatalogSavePayload& ConstPayload = Payload;
	const FCatalogSaveRecord* ConstFound = CatalogFindSaveRecord(ConstPayload, TEXT("Inventory"));
	if (TestNotNull(TEXT("versão const acha"), ConstFound))
	{
		TestEqual(TEXT("versão const devolve o registro certo"), ConstFound->Version, 1);
	}

	TestNull(TEXT("chave ausente"), CatalogFindSaveRecord(Payload, TEXT("Missing")));
	TestNull(TEXT("chave ausente (const)"), CatalogFindSaveRecord(ConstPayload, TEXT("Missing")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
