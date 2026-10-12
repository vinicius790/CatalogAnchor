// Copyright 2026 Geraliza Castlevania. MIT License.
#include "Misc/AutomationTest.h"
#include "CatalogInteractionComponent.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

// Contextos explícitos: ApplicationContextMask não existe mais em EAutomationTestFlags (UE 5.8).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInteractionHoldProgressTest, "CatalogAnchor.Interaction.HoldProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInteractionHoldProgressTest::RunTest(const FString& Parameters)
{
	const float NaNFloat = std::numeric_limits<float>::quiet_NaN();
	const double NaNDouble = std::numeric_limits<double>::quiet_NaN();

	// Duração inválida = instantâneo (completa já).
	TestEqual(TEXT("Duracao 0 completa"), UCatalogInteractionComponent::ComputeHoldProgress(10.0, 10.0, 0.f), 1.f);
	TestEqual(TEXT("Duracao negativa completa"), UCatalogInteractionComponent::ComputeHoldProgress(10.0, 10.0, -2.f), 1.f);
	TestEqual(TEXT("Duracao NaN completa"), UCatalogInteractionComponent::ComputeHoldProgress(10.0, 10.0, NaNFloat), 1.f);

	// Progresso normal e clamp.
	TestEqual(TEXT("Inicio = 0"), UCatalogInteractionComponent::ComputeHoldProgress(10.0, 10.0, 2.f), 0.f);
	TestEqual(TEXT("Metade"), UCatalogInteractionComponent::ComputeHoldProgress(10.0, 11.0, 2.f), 0.5f);
	TestEqual(TEXT("Passou do fim clampa em 1"), UCatalogInteractionComponent::ComputeHoldProgress(10.0, 100.0, 2.f), 1.f);

	// Relógio do cliente atrás do início (correção de GameState) não fica negativo.
	TestEqual(TEXT("Now < Start = 0"), UCatalogInteractionComponent::ComputeHoldProgress(10.0, 9.0, 2.f), 0.f);

	// Tempo inválido nunca completa por acidente.
	TestEqual(TEXT("Start NaN = 0"), UCatalogInteractionComponent::ComputeHoldProgress(NaNDouble, 10.0, 2.f), 0.f);
	TestEqual(TEXT("Now NaN = 0"), UCatalogInteractionComponent::ComputeHoldProgress(10.0, NaNDouble, 2.f), 0.f);

	// Sessão longa: double mantém precisão de frame depois de ~11 dias de servidor.
	TestEqual(TEXT("Precisao com tempo grande"), UCatalogInteractionComponent::ComputeHoldProgress(1.0e6, 1.0e6 + 0.25, 1.f), 0.25f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
