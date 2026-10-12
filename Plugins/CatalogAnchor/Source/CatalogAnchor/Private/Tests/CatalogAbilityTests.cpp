// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogAbilityComponent.h"
#include "CatalogGameplayTags.h"
#include "GameplayTagContainer.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

// Contextos explícitos: ApplicationContextMask não existe mais em EAutomationTestFlags (UE 5.8).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogAbilityCooldownRemainingTest, "CatalogAnchor.Abilities.CooldownRemaining",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogAbilityCooldownRemainingTest::RunTest(const FString& Parameters)
{
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Inf = std::numeric_limits<double>::infinity();

	TestTrue(TEXT("Futuro: ReadyAt - Now"),
		FMath::IsNearlyEqual(UCatalogAbilityComponent::ComputeCooldownRemaining(10.0, 7.5), 2.5));
	TestTrue(TEXT("Expirado: 0"), UCatalogAbilityComponent::ComputeCooldownRemaining(10.0, 12.0) == 0.0);
	TestTrue(TEXT("Exatamente agora: 0"), UCatalogAbilityComponent::ComputeCooldownRemaining(5.0, 5.0) == 0.0);
	TestTrue(TEXT("ReadyAt negativo: 0"), UCatalogAbilityComponent::ComputeCooldownRemaining(-5.0, 0.0) == 0.0);
	TestTrue(TEXT("ReadyAt NaN: 0"), UCatalogAbilityComponent::ComputeCooldownRemaining(NaN, 1.0) == 0.0);
	TestTrue(TEXT("Now NaN: 0"), UCatalogAbilityComponent::ComputeCooldownRemaining(1.0, NaN) == 0.0);
	TestTrue(TEXT("ReadyAt infinito: 0 (não trava a habilidade)"), UCatalogAbilityComponent::ComputeCooldownRemaining(Inf, 1.0) == 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogAbilityNativeTagsTest, "CatalogAnchor.Abilities.NativeTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogAbilityNativeTagsTest::RunTest(const FString& Parameters)
{
	// Depende do UGameplayTagsManager já inicializado e das tags nativas registradas
	// (sempre verdade ao rodar testes no editor/jogo). IsValid evita comparar tag vazia.
	const FGameplayTag Dash = CatalogGameplayTags::Ability_Dash;
	const FGameplayTag Melee = CatalogGameplayTags::Ability_Melee;

	if (TestTrue(TEXT("Dash válida"), Dash.IsValid()))
	{
		TestTrue(TEXT("Dash: nome exato"), Dash.ToString().Equals(TEXT("Catalog.Ability.Dash"), ESearchCase::CaseSensitive));
		TestTrue(TEXT("Dash registrada no manager"),
			FGameplayTag::RequestGameplayTag(FName(TEXT("Catalog.Ability.Dash")), /*ErrorIfNotFound*/ false) == Dash);
	}
	if (TestTrue(TEXT("Melee válida"), Melee.IsValid()))
	{
		TestTrue(TEXT("Melee: nome exato"), Melee.ToString().Equals(TEXT("Catalog.Ability.Melee"), ESearchCase::CaseSensitive));
		TestTrue(TEXT("Melee registrada no manager"),
			FGameplayTag::RequestGameplayTag(FName(TEXT("Catalog.Ability.Melee")), /*ErrorIfNotFound*/ false) == Melee);
	}
	TestTrue(TEXT("Tags distintas"), Dash != Melee);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
