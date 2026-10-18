// Copyright 2026 Geraliza Castlevania. MIT License.
#include "Misc/AutomationTest.h"
#include "CatalogInventoryComponent.h"
#include "CatalogItemDefinition.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// O _Validate decide se o cliente é derrubado: testado direto, sem mundo nem rede.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInventoryRpcValidateTest, "CatalogAnchor.Inventory.RpcValidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInventoryRpcValidateTest::RunTest(const FString& Parameters)
{
	UCatalogInventoryComponent* Inventory = NewObject<UCatalogInventoryComponent>(GetTransientPackage());
	const UCatalogItemDefinition* Item = NewObject<UCatalogItemDefinition>(GetTransientPackage());
	const int32 UseCap = FCatalogInventoryRules::MaxUseQuantity;

	TestTrue(TEXT("Use: válido"), Inventory->ServerUseItem_Validate(Item, 1));
	TestTrue(TEXT("Use: no teto"), Inventory->ServerUseItem_Validate(Item, UseCap));
	TestFalse(TEXT("Use: item nulo"), Inventory->ServerUseItem_Validate(nullptr, 1));
	TestFalse(TEXT("Use: Quantity 0"), Inventory->ServerUseItem_Validate(Item, 0));
	TestFalse(TEXT("Use: Quantity negativa"), Inventory->ServerUseItem_Validate(Item, -1));
	TestFalse(TEXT("Use: acima do teto"), Inventory->ServerUseItem_Validate(Item, UseCap + 1));

	TestTrue(TEXT("Drop: válido"), Inventory->ServerDropItem_Validate(Item, 1));
	TestTrue(TEXT("Drop: pilha grande vale"), Inventory->ServerDropItem_Validate(Item, UseCap + 1));
	TestFalse(TEXT("Drop: item nulo"), Inventory->ServerDropItem_Validate(nullptr, 1));
	TestFalse(TEXT("Drop: Quantity 0"), Inventory->ServerDropItem_Validate(Item, 0));
	TestFalse(TEXT("Drop: Quantity negativa"), Inventory->ServerDropItem_Validate(Item, -5));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
