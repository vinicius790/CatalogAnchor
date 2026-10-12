// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogAbilityDefinition.h"

const FPrimaryAssetType UCatalogAbilityDefinition::AbilityAssetType(TEXT("CatalogAbility"));

FPrimaryAssetId UCatalogAbilityDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AbilityAssetType, GetFName());
}
