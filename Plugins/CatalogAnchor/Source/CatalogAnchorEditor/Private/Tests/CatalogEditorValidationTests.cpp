// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogDefinitionValidators.h"
#include "AssetRegistry/AssetData.h"
#include "CatalogAbility.h"
#include "CatalogAbilityDefinition.h"
#include "CatalogGameplayTags.h"
#include "CatalogItemDefinition.h"
#include "CatalogSaveDataValidator.h"
#include "CatalogSaveTypes.h"
#include "Engine/Texture2D.h"
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

	/** Item sem problemas: nome, ícone e nome de asset na convenção. Objeto transiente, nome único por chamada. */
	static UCatalogItemDefinition* MakeValidItem(const TCHAR* BaseName = TEXT("CA_Item_Test"))
	{
		UCatalogItemDefinition* Item = NewObject<UCatalogItemDefinition>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UCatalogItemDefinition::StaticClass(), BaseName));
		Item->DisplayName = FText::FromString(TEXT("Test"));
		Item->Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")));
		return Item;
	}

	/** Habilidade sem problemas: tag nativa registrada, Dash, cooldown 1 s. */
	static UCatalogAbilityDefinition* MakeValidAbility()
	{
		UCatalogAbilityDefinition* Ability = NewObject<UCatalogAbilityDefinition>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UCatalogAbilityDefinition::StaticClass(), TEXT("CA_Ability_Test")));
		Ability->AbilityTag = CatalogGameplayTags::Ability_Dash;
		Ability->DisplayName = FText::FromString(TEXT("Test"));
		Ability->CooldownSeconds = 1.f;
		Ability->Behavior = NewObject<UCatalogAbility_Dash>(Ability);
		return Ability;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogEditorItemIssuesTest, "CatalogAnchor.Editor.ItemIssues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCatalogEditorItemIssuesTest::RunTest(const FString& Parameters)
{
	using namespace CatalogEditorValidationTestsPrivate;

	const FCatalogValidationIssues Valid = UCatalogItemDefinitionValidator::GetIssues(*MakeValidItem());
	TestEqual(TEXT("Item válido: 0 erros"), Valid.Errors.Num(), 0);

	{
		UCatalogItemDefinition* Item = MakeValidItem();
		Item->DisplayName = FText::GetEmpty();
		TestEqual(TEXT("DisplayName vazio: 1 erro"), UCatalogItemDefinitionValidator::GetIssues(*Item).Errors.Num(), 1);
		Item->DisplayName = FText::FromString(TEXT("   "));
		TestEqual(TEXT("DisplayName só espaços: 1 erro"), UCatalogItemDefinitionValidator::GetIssues(*Item).Errors.Num(), 1);
	}
	{
		UCatalogItemDefinition* Item = MakeValidItem();
		Item->MaxStackSize = 0;
		TestEqual(TEXT("MaxStackSize 0: 1 erro"), UCatalogItemDefinitionValidator::GetIssues(*Item).Errors.Num(), 1);
	}
	{
		UCatalogItemDefinition* Item = MakeValidItem();
		Item->Icon.Reset();
		const FCatalogValidationIssues Issues = UCatalogItemDefinitionValidator::GetIssues(*Item);
		TestEqual(TEXT("Icon nulo: 0 erros"), Issues.Errors.Num(), 0);
		TestEqual(TEXT("Icon nulo: 1 aviso a mais"), Issues.Warnings.Num(), Valid.Warnings.Num() + 1);
	}
	{
		const FCatalogValidationIssues Issues = UCatalogItemDefinitionValidator::GetIssues(*MakeValidItem(TEXT("Sword")));
		TestEqual(TEXT("Nome fora da convenção: 0 erros"), Issues.Errors.Num(), 0);
		TestEqual(TEXT("Nome fora da convenção: 1 aviso"), Issues.Warnings.Num(), Valid.Warnings.Num() + 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogEditorAbilityIssuesTest, "CatalogAnchor.Editor.AbilityIssues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCatalogEditorAbilityIssuesTest::RunTest(const FString& Parameters)
{
	using namespace CatalogEditorValidationTestsPrivate;

	const FCatalogValidationIssues Valid = UCatalogAbilityDefinitionValidator::GetIssues(*MakeValidAbility());
	TestEqual(TEXT("Habilidade válida: 0 erros"), Valid.Errors.Num(), 0);
	TestEqual(TEXT("Habilidade válida: 0 avisos"), Valid.Warnings.Num(), 0);

	{
		UCatalogAbilityDefinition* Ability = MakeValidAbility();
		Ability->AbilityTag = FGameplayTag();
		TestEqual(TEXT("AbilityTag vazio: 1 erro"), UCatalogAbilityDefinitionValidator::GetIssues(*Ability).Errors.Num(), 1);
	}
	{
		UCatalogAbilityDefinition* Ability = MakeValidAbility();
		Ability->Behavior = nullptr;
		TestEqual(TEXT("Behavior nulo: 1 erro"), UCatalogAbilityDefinitionValidator::GetIssues(*Ability).Errors.Num(), 1);
	}
	{
		UCatalogAbilityDefinition* Ability = MakeValidAbility();
		const float Invalid[] = { -1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() };
		for (const float Cooldown : Invalid)
		{
			Ability->CooldownSeconds = Cooldown;
			TestEqual(TEXT("Cooldown negativo/NaN/infinito: 1 erro"),
				UCatalogAbilityDefinitionValidator::GetIssues(*Ability).Errors.Num(), 1);
		}
	}
	{
		UCatalogAbilityDefinition* Ability = MakeValidAbility();
		Ability->CooldownSeconds = 0.f;
		const FCatalogValidationIssues Issues = UCatalogAbilityDefinitionValidator::GetIssues(*Ability);
		TestEqual(TEXT("Cooldown 0 (sem cooldown): 0 erros"), Issues.Errors.Num(), 0);
		TestEqual(TEXT("Cooldown 0 (sem cooldown): 1 aviso"), Issues.Warnings.Num(), Valid.Warnings.Num() + 1);
	}
	{
		UCatalogAbilityDefinition* Ability = MakeValidAbility();
		UCatalogAbility_MeleeSweep* Melee = NewObject<UCatalogAbility_MeleeSweep>(Ability);
		Ability->Behavior = Melee;
		TestEqual(TEXT("Melee com valores padrão: 0 erros"), UCatalogAbilityDefinitionValidator::GetIssues(*Ability).Errors.Num(), 0);
		Melee->Range = 0.f;
		TestEqual(TEXT("Melee Range 0: 1 erro"), UCatalogAbilityDefinitionValidator::GetIssues(*Ability).Errors.Num(), 1);
		Melee->Radius = 0.f;
		TestEqual(TEXT("Melee Range e Radius 0: 2 erros"), UCatalogAbilityDefinitionValidator::GetIssues(*Ability).Errors.Num(), 2);
	}
	{
		UCatalogAbilityDefinition* Ability = MakeValidAbility();
		Ability->DisplayName = FText::GetEmpty();
		const FCatalogValidationIssues Issues = UCatalogAbilityDefinitionValidator::GetIssues(*Ability);
		TestEqual(TEXT("DisplayName vazio: 0 erros"), Issues.Errors.Num(), 0);
		TestEqual(TEXT("DisplayName vazio: 1 aviso"), Issues.Warnings.Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogEditorDuplicateNamesTest, "CatalogAnchor.Editor.DuplicateAssetNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCatalogEditorDuplicateNamesTest::RunTest(const FString& Parameters)
{
	const FTopLevelAssetPath ItemClass = UCatalogItemDefinition::StaticClass()->GetClassPathName();
	auto MakeAsset = [&ItemClass](const TCHAR* Folder, const TCHAR* Name)
	{
		return FAssetData(FName(*(FString(Folder) / Name)), FName(Folder), FName(Name), ItemClass);
	};

	TestEqual(TEXT("Lista vazia: 0 duplicatas"), CatalogFindDuplicateAssetNames(TConstArrayView<FAssetData>()).Num(), 0);

	{
		const TArray<FAssetData> Assets = {
			MakeAsset(TEXT("/Game/Items"), TEXT("CA_Item_Potion")),
			MakeAsset(TEXT("/Game/Items"), TEXT("CA_Item_Sword")),
		};
		TestEqual(TEXT("Nomes únicos: 0 duplicatas"), CatalogFindDuplicateAssetNames(Assets).Num(), 0);
	}
	{
		const TArray<FAssetData> Assets = {
			MakeAsset(TEXT("/Game/Items"), TEXT("CA_Item_Potion")),
			MakeAsset(TEXT("/Game/Old"), TEXT("CA_Item_Potion")),
			MakeAsset(TEXT("/Game/Items"), TEXT("CA_Item_Sword")),
		};
		const TMap<FName, TArray<FName>> Duplicates = CatalogFindDuplicateAssetNames(Assets);
		TestEqual(TEXT("Mesmo nome em duas pastas: 1 duplicata"), Duplicates.Num(), 1);
		const TArray<FName>* Packages = Duplicates.Find(FName(TEXT("CA_Item_Potion")));
		TestEqual(TEXT("A duplicata lista os 2 pacotes"), Packages ? Packages->Num() : 0, 2);
	}
	{
		// ItemId é FName: diferença só de maiúsculas é o mesmo id.
		const TArray<FAssetData> Assets = {
			MakeAsset(TEXT("/Game/A"), TEXT("CA_Item_Potion")),
			MakeAsset(TEXT("/Game/B"), TEXT("ca_item_potion")),
		};
		TestEqual(TEXT("Nomes que só diferem em maiúsculas colidem"), CatalogFindDuplicateAssetNames(Assets).Num(), 1);
	}
	{
		// O mesmo pacote listado duas vezes não é duplicata.
		const FAssetData Same = MakeAsset(TEXT("/Game/Items"), TEXT("CA_Item_Potion"));
		TestEqual(TEXT("Mesmo asset repetido na lista: 0 duplicatas"),
			CatalogFindDuplicateAssetNames(TArray<FAssetData>{ Same, Same }).Num(), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
