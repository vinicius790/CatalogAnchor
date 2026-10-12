// Copyright 2026 Geraliza Castlevania. MIT License.
#include "SampleItemCatalog.h"
#include "CatalogItemDefinition.h"

#define LOCTEXT_NAMESPACE "SampleCatalog"

USampleItemCatalog::USampleItemCatalog()
{
	GemItem = CreateItem(TEXT("Item_Gem"), LOCTEXT("Item.Gem", "Gema"), 20);
	PotionItem = CreateItem(TEXT("Item_Potion"), LOCTEXT("Item.Potion", "Poção"), 5);
	KeyItem = CreateItem(TEXT("Item_Key"), LOCTEXT("Item.Key", "Chave"), 1);
}

UCatalogItemDefinition* USampleItemCatalog::CreateItem(FName ItemId, const FText& DisplayName, int32 MaxStackSize)
{
	UCatalogItemDefinition* Item = CreateDefaultSubobject<UCatalogItemDefinition>(ItemId);
	Item->DisplayName = DisplayName;
	Item->MaxStackSize = MaxStackSize;
	return Item;
}

const UCatalogItemDefinition* USampleItemCatalog::Gem()
{
	return GetDefault<USampleItemCatalog>()->GemItem;
}

const UCatalogItemDefinition* USampleItemCatalog::Potion()
{
	return GetDefault<USampleItemCatalog>()->PotionItem;
}

const UCatalogItemDefinition* USampleItemCatalog::Key()
{
	return GetDefault<USampleItemCatalog>()->KeyItem;
}

#undef LOCTEXT_NAMESPACE
