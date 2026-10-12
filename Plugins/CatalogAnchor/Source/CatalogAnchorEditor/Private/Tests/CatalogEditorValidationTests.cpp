// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogDefinitionValidators.h"
#include "CatalogSaveDataValidator.h"
#include "CatalogSaveTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CatalogEditorValidationTestsPrivate
{
	/** Payload v2 válido: um slot de inventário e dois registros distintos. */
	static FCatalogSavePayload MakeValidPayload()
	{
		FCatalogSavePayload Payload;
		Payload.Header.SaveVersion = GCatalogSaveSchemaVersion;

		FCatalogInventorySlot& Slot = Payload.Inventory.AddDefaulted_GetRef();
		Slot.ItemId = FName(TEXT("CA_Item_Potion"));
		Slot.Quantity = 3;

		Payload.Records.AddDefaulted_GetRef().Key = FName(TEXT("Inventory"));
		Payload.Records.AddDefaulted_GetRef().Key = FName(TEXT("Quests"));
		return Payload;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogEditorNamingConventionTest, "CatalogAnchor.Editor.NamingConvention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCatalogEditorNamingConventionTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("CA_Item_Sword segue CA_Item_"), CatalogFollowsNamingConvention(TEXT("CA_Item_Sword"), TEXT("CA_Item_")));
	TestTrue(TEXT("CA_Ability_Dash segue CA_Ability_"), CatalogFollowsNamingConvention(TEXT("CA_Ability_Dash"), TEXT("CA_Ability_")));
	TestFalse(TEXT("Só o prefixo não basta"), CatalogFollowsNamingConvention(TEXT("CA_Item_"), TEXT("CA_Item_")));
	TestFalse(TEXT("Maiúsculas contam"), CatalogFollowsNamingConvention(TEXT("ca_item_sword"), TEXT("CA_Item_")));
	TestFalse(TEXT("Sem prefixo"), CatalogFollowsNamingConvention(TEXT("Sword"), TEXT("CA_Item_")));
	TestFalse(TEXT("Prefixo de outro tipo"), CatalogFollowsNamingConvention(TEXT("CA_Ability_Dash"), TEXT("CA_Item_")));
	TestFalse(TEXT("Nome vazio"), CatalogFollowsNamingConvention(FString(), TEXT("CA_Save_")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogEditorSavePayloadErrorsTest, "CatalogAnchor.Editor.SavePayloadErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCatalogEditorSavePayloadErrorsTest::RunTest(const FString& Parameters)
{
	using namespace CatalogEditorValidationTestsPrivate;

	TestEqual(TEXT("Payload válido: 0 erros"), UCatalogSaveDataValidator::GetPayloadErrors(MakeValidPayload()).Num(), 0);

	{
		FCatalogSavePayload Payload = MakeValidPayload();
		Payload.Header.SaveVersion = 0;
		TestEqual(TEXT("SaveVersion 0 (carimbado ao salvar): 0 erros"), UCatalogSaveDataValidator::GetPayloadErrors(Payload).Num(), 0);
		Payload.Header.SaveVersion = GCatalogSaveSchemaVersion + 1;
		TestEqual(TEXT("SaveVersion futura: 1 erro"), UCatalogSaveDataValidator::GetPayloadErrors(Payload).Num(), 1);
		Payload.Header.SaveVersion = -1;
		TestEqual(TEXT("SaveVersion negativa: 1 erro"), UCatalogSaveDataValidator::GetPayloadErrors(Payload).Num(), 1);
	}
	{
		FCatalogSavePayload Payload = MakeValidPayload();
		FCatalogInventorySlot& Slot = Payload.Inventory.AddDefaulted_GetRef(); // ItemId None e Quantity 0
		TestEqual(TEXT("Slot sem ItemId e sem quantidade: 2 erros"), UCatalogSaveDataValidator::GetPayloadErrors(Payload).Num(), 2);
		Slot.ItemId = FName(TEXT("CA_Item_Potion"));
		Slot.Quantity = 1;
		TestEqual(TEXT("ItemId repetido: 1 erro"), UCatalogSaveDataValidator::GetPayloadErrors(Payload).Num(), 1);
	}
	{
		FCatalogSavePayload Payload = MakeValidPayload();
		Payload.Records.AddDefaulted();
		TestEqual(TEXT("Record sem Key: 1 erro"), UCatalogSaveDataValidator::GetPayloadErrors(Payload).Num(), 1);
	}
	{
		FCatalogSavePayload Payload = MakeValidPayload();
		Payload.Records.AddDefaulted_GetRef().Key = FName(TEXT("Inventory"));
		Payload.Records.AddDefaulted_GetRef().Key = FName(TEXT("Inventory"));
		TestEqual(TEXT("Key repetida duas vezes: 2 erros (um por repetição)"), UCatalogSaveDataValidator::GetPayloadErrors(Payload).Num(), 2);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
