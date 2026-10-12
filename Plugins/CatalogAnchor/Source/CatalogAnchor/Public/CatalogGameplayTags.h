// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Tags nativas do plugin: registradas no GameplayTagsManager quando o módulo carrega,
 * sem .ini — código e assets usam a mesma tag, sem risco de typo em string.
 *
 * Par de macros da engine (declaração e definição sempre do mesmo tipo); CATALOGANCHOR_API na
 * frente exporta o símbolo, senão os módulos de fora (sample, editor) não linkariam no Windows.
 * A definição fica no .cpp com UE_DEFINE_GAMEPLAY_TAG_COMMENT.
 */
namespace CatalogGameplayTags
{
	/** "Catalog.Ability.Dash" — habilidade de dash (UCatalogAbility_Dash). */
	CATALOGANCHOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dash);

	/** "Catalog.Ability.Melee" — golpe corpo a corpo (UCatalogAbility_MeleeSweep). */
	CATALOGANCHOR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee);
}
