// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogInventoryTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Contextos explícitos: ApplicationContextMask não existe mais em EAutomationTestFlags (UE 5.8).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInventoryAddFitsTest, "CatalogAnchor.Inventory.AddFitsAll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInventoryAddFitsTest::RunTest(const FString& Parameters)
{
	TArray<int32> Stacks = { 3 };
	TestEqual(TEXT("entra tudo na pilha existente"), FCatalogInventoryRules::AddToStacks(Stacks, 4, 10, 5), 4);
	TestTrue(TEXT("completa a pilha sem abrir outra"), Stacks == TArray<int32>{ 7 });

	TArray<int32> Empty;
	TestEqual(TEXT("inventário vazio aceita"), FCatalogInventoryRules::AddToStacks(Empty, 5, 10, 1), 5);
	TestTrue(TEXT("abre uma pilha nova"), Empty == TArray<int32>{ 5 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInventoryAddOverflowTest, "CatalogAnchor.Inventory.AddOverflowsStack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInventoryAddOverflowTest::RunTest(const FString& Parameters)
{
	TArray<int32> Stacks = { 8 };
	TestEqual(TEXT("entra tudo"), FCatalogInventoryRules::AddToStacks(Stacks, 5, 10, 5), 5);
	TestTrue(TEXT("enche a pilha e transborda para uma nova"), Stacks == TArray<int32>{ 10, 3 });

	TArray<int32> Many;
	TestEqual(TEXT("várias pilhas novas"), FCatalogInventoryRules::AddToStacks(Many, 25, 10, 5), 25);
	TestTrue(TEXT("divide em pilhas cheias + resto"), Many == TArray<int32>{ 10, 10, 5 });

	TArray<int32> OverLimit = { 12 };
	TestEqual(TEXT("pilha acima do limite não recebe"), FCatalogInventoryRules::AddToStacks(OverLimit, 1, 10, 1), 1);
	TestTrue(TEXT("vai para pilha nova"), OverLimit == TArray<int32>{ 12, 1 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInventoryCapacityFullTest, "CatalogAnchor.Inventory.CapacityFull", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInventoryCapacityFullTest::RunTest(const FString& Parameters)
{
	TArray<int32> Full = { 10 };
	TestEqual(TEXT("sem espaço nada entra"), FCatalogInventoryRules::AddToStacks(Full, 3, 10, 0), 0);
	TestTrue(TEXT("pilhas intactas"), Full == TArray<int32>{ 10 });

	TArray<int32> AlmostFull = { 9 };
	TestEqual(TEXT("só completa a pilha existente"), FCatalogInventoryRules::AddToStacks(AlmostFull, 3, 10, 0), 1);
	TestTrue(TEXT("pilha cheia, sem pilha nova"), AlmostFull == TArray<int32>{ 10 });

	TArray<int32> Limited;
	TestEqual(TEXT("para no limite de slots"), FCatalogInventoryRules::AddToStacks(Limited, 25, 10, 2), 20);
	TestTrue(TEXT("só duas pilhas"), Limited == TArray<int32>{ 10, 10 });

	TArray<int32> Negative;
	TestEqual(TEXT("FreeSlots negativo = zero"), FCatalogInventoryRules::AddToStacks(Negative, 3, 10, -4), 0);
	TestEqual(TEXT("nenhuma pilha criada"), Negative.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInventoryMaxStackOneTest, "CatalogAnchor.Inventory.MaxStackOne", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInventoryMaxStackOneTest::RunTest(const FString& Parameters)
{
	TArray<int32> Stacks;
	TestEqual(TEXT("um por slot até a capacidade"), FCatalogInventoryRules::AddToStacks(Stacks, 5, 1, 3), 3);
	TestTrue(TEXT("três pilhas de 1"), Stacks == TArray<int32>{ 1, 1, 1 });

	TArray<int32> ZeroMax;
	TestEqual(TEXT("MaxStackSize 0 conta como 1"), FCatalogInventoryRules::AddToStacks(ZeroMax, 2, 0, 5), 2);
	TestTrue(TEXT("duas pilhas de 1"), ZeroMax == TArray<int32>{ 1, 1 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInventoryInvalidQuantityTest, "CatalogAnchor.Inventory.InvalidQuantity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInventoryInvalidQuantityTest::RunTest(const FString& Parameters)
{
	TArray<int32> Stacks = { 3 };
	TestEqual(TEXT("add 0 não entra"), FCatalogInventoryRules::AddToStacks(Stacks, 0, 10, 5), 0);
	TestEqual(TEXT("add negativo não entra"), FCatalogInventoryRules::AddToStacks(Stacks, -2, 10, 5), 0);
	TestEqual(TEXT("remove negativo não sai"), FCatalogInventoryRules::RemoveFromStacks(Stacks, -1), 0);
	TestTrue(TEXT("pilhas intactas"), Stacks == TArray<int32>{ 3 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInventoryRemoveTest, "CatalogAnchor.Inventory.Remove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInventoryRemoveTest::RunTest(const FString& Parameters)
{
	TArray<int32> Stacks = { 10, 3 };
	TestEqual(TEXT("remove da última pilha primeiro"), FCatalogInventoryRules::RemoveFromStacks(Stacks, 5), 5);
	TestTrue(TEXT("pilha parcial esvazia, cheia perde o resto"), Stacks == TArray<int32>{ 8, 0 });

	TestEqual(TEXT("pedido maior que o estoque devolve só o que havia"), FCatalogInventoryRules::RemoveFromStacks(Stacks, 20), 8);
	TestTrue(TEXT("tudo zerado"), Stacks == TArray<int32>{ 0, 0 });
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
