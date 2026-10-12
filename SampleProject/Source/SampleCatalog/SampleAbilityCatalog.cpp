// Copyright 2026 Geraliza Castlevania. MIT License.
#include "SampleAbilityCatalog.h"
#include "CatalogAbility.h"
#include "CatalogAbilityDefinition.h"
#include "CatalogGameplayTags.h"

#define LOCTEXT_NAMESPACE "SampleCatalog"

USampleAbilityCatalog::USampleAbilityCatalog()
{
	UCatalogAbility_Dash* DashBehavior = CreateDefaultSubobject<UCatalogAbility_Dash>(TEXT("Behavior_Dash"));
	DashBehavior->Strength = 1500.f;

	DashAbility = CreateDefaultSubobject<UCatalogAbilityDefinition>(TEXT("Ability_Dash"));
	DashAbility->AbilityTag = CatalogGameplayTags::Ability_Dash;
	DashAbility->DisplayName = LOCTEXT("Ability.Dash", "Dash");
	DashAbility->CooldownSeconds = 1.2f;
	DashAbility->Behavior = DashBehavior;

	UCatalogAbility_MeleeSweep* MeleeBehavior = CreateDefaultSubobject<UCatalogAbility_MeleeSweep>(TEXT("Behavior_Melee"));
	MeleeBehavior->Range = 160.f;
	MeleeBehavior->Radius = 70.f;
	MeleeBehavior->Damage = 25.f;

	MeleeAbility = CreateDefaultSubobject<UCatalogAbilityDefinition>(TEXT("Ability_Melee"));
	MeleeAbility->AbilityTag = CatalogGameplayTags::Ability_Melee;
	MeleeAbility->DisplayName = LOCTEXT("Ability.Melee", "Golpe");
	MeleeAbility->CooldownSeconds = 0.6f;
	MeleeAbility->Behavior = MeleeBehavior;
}

UCatalogAbilityDefinition* USampleAbilityCatalog::Dash()
{
	return GetDefault<USampleAbilityCatalog>()->DashAbility;
}

UCatalogAbilityDefinition* USampleAbilityCatalog::Melee()
{
	return GetDefault<USampleAbilityCatalog>()->MeleeAbility;
}

#undef LOCTEXT_NAMESPACE
