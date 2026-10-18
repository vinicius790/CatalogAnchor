// Copyright 2026 Geraliza Castlevania. MIT License.
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "CatalogGameplayTags.h"
#include "CatalogInventoryComponent.h"
#include "CatalogInventoryTestItem.h"
#include "CatalogSaveTypes.h"
#include "CatalogTestGuards.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

// Nomeado (não anônimo): em unity build vários .cpp dividem a mesma unidade de tradução.
namespace CatalogInventoryWorldTestsPrivate
{
	static UCatalogTestItemDefinition* MakeItem(int32 MaxStack, float Weight, bool bUsable, bool bConsume)
	{
		UCatalogTestItemDefinition* Item = NewObject<UCatalogTestItemDefinition>(GetTransientPackage());
		Item->MaxStackSize = MaxStack;
		Item->Weight = Weight;
		Item->bUsable = bUsable;
		Item->bConsumeOnUse = bConsume;
		return Item;
	}

	static UCatalogInventoryComponent* AddInventory(AActor* Owner, int32 Capacity, float MaxWeight = 0.f)
	{
		UCatalogInventoryComponent* Inventory = NewObject<UCatalogInventoryComponent>(Owner);
		Inventory->Capacity = Capacity;
		Inventory->MaxWeight = MaxWeight;
		Inventory->bSaveWithGame = false; // estes testes não passam pelo save subsystem
		Inventory->RegisterComponent();
		return Inventory;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInventoryUseItemWorldTest, "CatalogAnchor.Inventory.World.UseItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInventoryUseItemWorldTest::RunTest(const FString& Parameters)
{
	using namespace CatalogInventoryWorldTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}

	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();

	FActorSpawnParameters AlwaysSpawn;
	AlwaysSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Owner = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, AlwaysSpawn);
	APlayerController* Controller = World->SpawnActor<APlayerController>(FVector::ZeroVector, FRotator::ZeroRotator, AlwaysSpawn);
	ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>(FVector::ZeroVector, FRotator::ZeroRotator, AlwaysSpawn);
	if (!TestTrue(TEXT("Atores criados"), Owner && Controller && Pawn))
	{
		return false;
	}
	Controller->Possess(Pawn);

	UCatalogInventoryComponent* Inventory = AddInventory(Owner, 5);
	UCatalogTestItemDefinition* Potion = MakeItem(5, 0.f, /*bUsable*/ true, /*bConsume*/ true);
	UCatalogTestItemDefinition* Elixir = MakeItem(1, 0.f, true, /*bConsume*/ false);
	UCatalogTestItemDefinition* Rock = MakeItem(5, 0.f, /*bUsable*/ false, true);
	UCatalogInventoryTestListener* Listener = NewObject<UCatalogInventoryTestListener>();
	Inventory->OnItemUsed.AddDynamic(Listener, &UCatalogInventoryTestListener::HandleItemUsed);
	Inventory->AddItem(Potion, 3);
	Inventory->AddItem(Elixir, 1);
	Inventory->AddItem(Rock, 2);

	// Recusas: nada consumido, nenhum efeito, nenhum delegate.
	TestFalse(TEXT("Item não usável"), Inventory->UseItem(Rock, 1));
	TestFalse(TEXT("Mais do que tem"), Inventory->UseItem(Potion, 4));
	TestFalse(TEXT("Quantity 0"), Inventory->UseItem(Potion, 0));
	TestFalse(TEXT("Quantity negativa"), Inventory->UseItem(Potion, -1));
	TestFalse(TEXT("Item nulo"), Inventory->UseItem(nullptr, 1));
	TestFalse(TEXT("Acima do teto por chamada"), Inventory->UseItem(Potion, FCatalogInventoryRules::MaxUseQuantity + 1));
	TestEqual(TEXT("Recusas não consomem"), Inventory->GetItemCount(Potion) + Inventory->GetItemCount(Rock), 5);
	TestEqual(TEXT("Recusas sem efeito/delegate"), Potion->UseCount + Rock->UseCount + Listener->UsedCount, 0);

	// Uso consumível: consome, OnUsed por unidade (User = dono), OnItemUsed uma vez com a quantidade.
	TestTrue(TEXT("Usa 2"), Inventory->UseItem(Potion, 2));
	TestEqual(TEXT("Consumiu 2"), Inventory->GetItemCount(Potion), 1);
	TestEqual(TEXT("OnUsed por unidade"), Potion->UseCount, 2);
	TestTrue(TEXT("User é o dono"), Potion->LastUser.Get() == Owner);
	TestEqual(TEXT("OnItemUsed uma vez"), Listener->UsedCount, 1);
	TestEqual(TEXT("OnItemUsed quantidade"), Listener->LastQuantity, 2);
	TestTrue(TEXT("OnItemUsed item"), Listener->LastItem.Get() == Potion);

	// bConsumeOnUse = false: aplica o efeito e mantém a unidade.
	TestTrue(TEXT("Usa não consumível"), Inventory->UseItem(Elixir, 1));
	TestEqual(TEXT("Não consumível fica"), Inventory->GetItemCount(Elixir), 1);
	TestEqual(TEXT("Não consumível: efeito aplicado"), Elixir->UseCount, 1);
	// Sem estoque limitando, o item não consumível tem intervalo mínimo entre usos (o tempo do mundo de teste não anda).
	TestFalse(TEXT("Segundo uso em seguida é recusado"), Inventory->UseItem(Elixir, 1));
	TestEqual(TEXT("Recusado: sem efeito"), Elixir->UseCount, 1);
	Elixir->UseCooldownSeconds = 0.f;
	TestTrue(TEXT("Sem intervalo: usa de novo"), Inventory->UseItem(Elixir, 1));
	TestEqual(TEXT("Efeito aplicado de novo"), Elixir->UseCount, 2);

	// RPC no servidor (standalone: executa direto) passa pela mesma validação.
	Inventory->ServerUseItem(Rock, 1);
	TestEqual(TEXT("RPC com item não usável não consome"), Inventory->GetItemCount(Rock), 2);
	Inventory->ServerUseItem(Potion, 1);
	TestEqual(TEXT("RPC consome a última"), Inventory->GetItemCount(Potion), 0);
	TestEqual(TEXT("RPC aplicou o efeito"), Potion->UseCount, 3);

	// Inventário no Controller: quem usa é o Pawn possuído.
	UCatalogInventoryComponent* ControllerInventory = AddInventory(Controller, 5);
	ControllerInventory->AddItem(Potion, 1);
	TestTrue(TEXT("Usa pelo Controller"), ControllerInventory->UseItem(Potion, 1));
	TestTrue(TEXT("User é o Pawn"), Potion->LastUser.Get() == Pawn);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInventoryTransferWorldTest, "CatalogAnchor.Inventory.World.TransferTo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInventoryTransferWorldTest::RunTest(const FString& Parameters)
{
	using namespace CatalogInventoryWorldTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}

	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();

	AActor* OwnerA = World->SpawnActor<AActor>();
	AActor* OwnerB = World->SpawnActor<AActor>();
	if (!TestTrue(TEXT("Atores criados"), OwnerA && OwnerB))
	{
		return false;
	}

	UCatalogInventoryComponent* Source = AddInventory(OwnerA, 3);
	UCatalogInventoryComponent* Dest = AddInventory(OwnerB, 1); // um slot: cabe no máximo uma pilha
	UCatalogTestItemDefinition* Gem = MakeItem(10, 0.f, false, false);
	UCatalogInventoryTestListener* SourceListener = NewObject<UCatalogInventoryTestListener>();
	UCatalogInventoryTestListener* DestListener = NewObject<UCatalogInventoryTestListener>();
	Source->AddItem(Gem, 15); // 10 + 5
	Source->OnInventoryChanged.AddDynamic(SourceListener, &UCatalogInventoryTestListener::HandleChanged);
	Dest->OnInventoryChanged.AddDynamic(DestListener, &UCatalogInventoryTestListener::HandleChanged);

	// Recusas: nada se move (inclusive o que caberia: tudo ou nada).
	TestFalse(TEXT("Mais do que a origem tem"), Source->TransferTo(Dest, Gem, 16));
	TestFalse(TEXT("Destino só comporta 10 de 11"), Source->TransferTo(Dest, Gem, 11));
	TestFalse(TEXT("Mesmo inventário"), Source->TransferTo(Source, Gem, 1));
	TestFalse(TEXT("Destino nulo"), Source->TransferTo(nullptr, Gem, 1));
	TestFalse(TEXT("Item nulo"), Source->TransferTo(Dest, nullptr, 1));
	TestFalse(TEXT("Quantity 0"), Source->TransferTo(Dest, Gem, 0));
	TestEqual(TEXT("Origem intacta"), Source->GetItemCount(Gem), 15);
	TestEqual(TEXT("Destino intacto"), Dest->GetItemCount(Gem), 0);
	TestEqual(TEXT("Recusas sem broadcast"), SourceListener->ChangedCount + DestListener->ChangedCount, 0);

	// Sucesso: move, e os dois lados avisam.
	TestTrue(TEXT("Move 10"), Source->TransferTo(Dest, Gem, 10));
	TestEqual(TEXT("Origem perdeu 10"), Source->GetItemCount(Gem), 5);
	TestEqual(TEXT("Destino ganhou 10"), Dest->GetItemCount(Gem), 10);
	TestEqual(TEXT("Origem avisou"), SourceListener->ChangedCount, 1);
	TestEqual(TEXT("Destino avisou"), DestListener->ChangedCount, 1);
	TestFalse(TEXT("Destino cheio"), Source->TransferTo(Dest, Gem, 1));
	TestEqual(TEXT("Cheio: origem intacta"), Source->GetItemCount(Gem), 5);

	// Peso do destino também vale.
	UCatalogTestItemDefinition* Ore = MakeItem(10, 4.f, false, false);
	UCatalogInventoryComponent* Light = AddInventory(OwnerB, 5, /*MaxWeight*/ 10.f);
	Source->AddItem(Ore, 3);
	TestFalse(TEXT("12 de peso não cabe em 10"), Source->TransferTo(Light, Ore, 3));
	TestEqual(TEXT("Peso: origem intacta"), Source->GetItemCount(Ore), 3);
	TestTrue(TEXT("8 de peso cabe"), Source->TransferTo(Light, Ore, 2));
	TestEqual(TEXT("Peso: origem"), Source->GetItemCount(Ore), 1);
	TestEqual(TEXT("Peso: destino"), Light->GetItemCount(Ore), 2);
	TestEqual(TEXT("Peso total do destino"), Light->GetTotalWeight(), 8.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInventoryWeightTagsWorldTest, "CatalogAnchor.Inventory.World.WeightAndTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInventoryWeightTagsWorldTest::RunTest(const FString& Parameters)
{
	using namespace CatalogInventoryWorldTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}

	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();

	AActor* OwnerA = World->SpawnActor<AActor>();
	AActor* OwnerB = World->SpawnActor<AActor>();
	if (!TestTrue(TEXT("Atores criados"), OwnerA && OwnerB))
	{
		return false;
	}

	UCatalogInventoryComponent* Inventory = AddInventory(OwnerA, 10, /*MaxWeight*/ 10.f);
	UCatalogTestItemDefinition* Rock = MakeItem(5, 2.5f, false, false);
	UCatalogTestItemDefinition* Feather = MakeItem(99, 0.f, false, false);

	// Peso: 10 / 2.5 = 4 entram (Add parcial, como na capacidade).
	TestEqual(TEXT("Entram 4 por peso"), Inventory->AddItem(Rock, 10), 4);
	TestEqual(TEXT("Peso total"), Inventory->GetTotalWeight(), 10.f);
	TestEqual(TEXT("Cheio por peso"), Inventory->GetAddableQuantity(Rock, 1), 0);
	TestEqual(TEXT("Item sem peso não é barrado"), Inventory->AddItem(Feather, 3), 3);
	TestEqual(TEXT("Sem peso não soma"), Inventory->GetTotalWeight(), 10.f);

	// Liberar peso reabre espaço, sem alterar nada ao consultar.
	Inventory->RemoveItem(Rock, 1);
	TestEqual(TEXT("Cabe 1 por peso"), Inventory->GetAddableQuantity(Rock, 5), 1);
	TestEqual(TEXT("Consulta não altera"), Inventory->GetItemCount(Rock), 3);
	TestEqual(TEXT("Add entra o que cabe"), Inventory->AddItem(Rock, 5), 1);

	// Save: MaxWeight menor que o conteúdo não apaga itens no load, só barra novas entradas.
	FCatalogSavePayload Payload;
	Inventory->WriteSaveData(Payload);
	UCatalogInventoryComponent* Light = AddInventory(OwnerB, 10, /*MaxWeight*/ 1.f);
	Light->ReadSaveData(Payload);
	TestEqual(TEXT("Load preserva acima do peso"), Light->GetItemCount(Rock), Inventory->GetItemCount(Rock));
	TestEqual(TEXT("Acima do máximo barra novas entradas"), Light->AddItem(Rock, 1), 0);

	// Tags: ItemTags exato, Category, pai hierárquico e tag inválida.
	const FGameplayTag Dash = CatalogGameplayTags::Ability_Dash;
	const FGameplayTag Melee = CatalogGameplayTags::Ability_Melee;
	Rock->ItemTags.AddTag(Dash);
	Feather->Category = Melee;
	TestEqual(TEXT("Por tag em ItemTags"), Inventory->GetItemsByTag(Dash).Num(), 1);
	TestTrue(TEXT("Devolve a pilha do Rock"), Inventory->GetItemsByTag(Dash).Num() == 1 && Inventory->GetItemsByTag(Dash)[0].Item == Rock);
	TestEqual(TEXT("Por Category"), Inventory->GetItemsByTag(Melee).Num(), 1);
	TestTrue(TEXT("Devolve a pilha da Feather"), Inventory->GetItemsByTag(Melee).Num() == 1 && Inventory->GetItemsByTag(Melee)[0].Item == Feather);
	TestEqual(TEXT("Tag inválida"), Inventory->GetItemsByTag(FGameplayTag()).Num(), 0);
	const FGameplayTag Parent = FGameplayTag::RequestGameplayTag(FName(TEXT("Catalog.Ability")), /*ErrorIfNotFound*/ false);
	if (TestTrue(TEXT("Tag pai existe"), Parent.IsValid()))
	{
		TestEqual(TEXT("Pai casa filhos de ItemTags e Category"), Inventory->GetItemsByTag(Parent).Num(), 2);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
