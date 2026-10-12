// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogSaveDataValidator.h"
#include "CatalogDefinitionValidators.h"
#include "CatalogSaveTemplate.h"
#include "CatalogSaveTypes.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "CatalogSaveDataValidator"

TArray<FText> UCatalogSaveDataValidator::GetPayloadErrors(const FCatalogSavePayload& Payload)
{
	TArray<FText> Errors;

	const int32 Version = Payload.Header.SaveVersion;
	if (Version < 0 || Version > GCatalogSaveSchemaVersion)
	{
		Errors.Add(FText::Format(
			LOCTEXT("BadVersion", "CatalogAnchor: SaveVersion {0} fora de 0..{1} (0 = carimbado ao salvar)."),
			Version, GCatalogSaveSchemaVersion));
	}

	TSet<FName> SeenItems;
	for (int32 Index = 0; Index < Payload.Inventory.Num(); ++Index)
	{
		const FCatalogInventorySlot& Slot = Payload.Inventory[Index];
		if (Slot.ItemId.IsNone())
		{
			Errors.Add(FText::Format(
				LOCTEXT("EmptyItem", "CatalogAnchor: Inventory[{0}] sem ItemId."), Index));
		}
		else
		{
			bool bDuplicate = false;
			SeenItems.Add(Slot.ItemId, &bDuplicate);
			if (bDuplicate)
			{
				Errors.Add(FText::Format(
					LOCTEXT("DuplicateItem", "CatalogAnchor: Inventory[{0}] repete o ItemId '{1}' — some as quantidades num slot só."),
					Index, FText::FromName(Slot.ItemId)));
			}
		}

		if (Slot.Quantity <= 0)
		{
			Errors.Add(FText::Format(
				LOCTEXT("BadQuantity", "CatalogAnchor: Inventory[{0}] com Quantity {1} (precisa ser > 0)."),
				Index, Slot.Quantity));
		}
	}

	// CatalogFindSaveRecord devolve o primeiro: um segundo registro com a mesma Key seria ignorado em silêncio.
	TSet<FName> SeenRecords;
	for (int32 Index = 0; Index < Payload.Records.Num(); ++Index)
	{
		const FName Key = Payload.Records[Index].Key;
		if (Key.IsNone())
		{
			Errors.Add(FText::Format(
				LOCTEXT("EmptyRecordKey", "CatalogAnchor: Records[{0}] sem Key."), Index));
			continue;
		}

		bool bDuplicate = false;
		SeenRecords.Add(Key, &bDuplicate);
		if (bDuplicate)
		{
			Errors.Add(FText::Format(
				LOCTEXT("DuplicateRecordKey", "CatalogAnchor: Records[{0}] repete a Key '{1}' (uma por participante)."),
				Index, FText::FromName(Key)));
		}
	}

	return Errors;
}

bool UCatalogSaveDataValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject,
	FDataValidationContext& InContext) const
{
	return InObject && InObject->IsA<UCatalogSaveTemplate>();
}

EDataValidationResult UCatalogSaveDataValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData,
	UObject* InAsset, FDataValidationContext& Context)
{
	const UCatalogSaveTemplate* Template = Cast<UCatalogSaveTemplate>(InAsset);
	if (!Template)
	{
		AssetFails(InAsset, LOCTEXT("NotATemplate", "CatalogAnchor: o asset não é um UCatalogSaveTemplate."));
		return EDataValidationResult::Invalid;
	}

	const TArray<FText> Errors = GetPayloadErrors(Template->Payload);
	for (const FText& Error : Errors)
	{
		AssetFails(InAsset, Error);
	}

	if (!CatalogFollowsNamingConvention(InAsset->GetName(), TEXT("CA_Save_")))
	{
		AssetWarning(InAsset, LOCTEXT("Naming", "CatalogAnchor: convenção de nome é CA_Save_<Nome>."));
	}

	if (Errors.Num() > 0)
	{
		return EDataValidationResult::Invalid;
	}
	AssetPasses(InAsset);
	return EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE
