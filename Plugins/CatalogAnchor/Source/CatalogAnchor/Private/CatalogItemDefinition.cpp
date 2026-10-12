// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogItemDefinition.h"

const FPrimaryAssetType UCatalogItemDefinition::ItemAssetType(FName(TEXT("CatalogItem")));

FPrimaryAssetId UCatalogItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(ItemAssetType, GetFName());
}

FName UCatalogItemDefinition::GetItemId() const
{
	return GetFName();
}
