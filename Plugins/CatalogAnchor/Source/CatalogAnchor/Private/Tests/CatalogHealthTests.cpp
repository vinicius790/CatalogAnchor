// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogHealthComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Contextos explícitos: ApplicationContextMask não existe mais em EAutomationTestFlags (UE 5.8).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogHealthComputeTest, "CatalogAnchor.Health.ComputeNewHealth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogHealthComputeTest::RunTest(const FString& Parameters)
{
	const float NaN = FMath::Sqrt(-1.f);
	TestEqual(TEXT("dano normal"), UCatalogHealthComponent::ComputeNewHealth(100.f, -30.f, 100.f), 70.f);
	TestEqual(TEXT("dano excessivo para em 0"), UCatalogHealthComponent::ComputeNewHealth(20.f, -50.f, 100.f), 0.f);
	TestEqual(TEXT("cura para no máximo"), UCatalogHealthComponent::ComputeNewHealth(90.f, 50.f, 100.f), 100.f);
	TestEqual(TEXT("delta NaN mantém"), UCatalogHealthComponent::ComputeNewHealth(40.f, NaN, 100.f), 40.f);
	TestEqual(TEXT("máximo inválido mantém"), UCatalogHealthComponent::ComputeNewHealth(40.f, -10.f, 0.f), 40.f);
	TestEqual(TEXT("atual NaN vira 0"), UCatalogHealthComponent::ComputeNewHealth(NaN, 10.f, 100.f), 0.f);
	return true;
}

#endif
