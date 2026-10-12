// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogSaveTypes.h"
#include "EngineDefines.h"
#include "Misc/AutomationTest.h"
#include "SamplePlayerSaveComponent.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSamplePlayerSaveValidationTest, "CatalogAnchor.Sample.PlayerSaveValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSamplePlayerSaveValidationTest::RunTest(const FString& Parameters)
{
	const float NaN = FMath::Sqrt(-1.f);
	const double KillZ = -1000.0;

	TestTrue(TEXT("posição normal"), USamplePlayerSaveComponent::IsLocationValid(FVector(100.0, -50.0, -100.0), KillZ, HALF_WORLD_MAX));
	TestFalse(TEXT("NaN"), USamplePlayerSaveComponent::IsLocationValid(FVector(NaN, 0.0, 0.0), KillZ, HALF_WORLD_MAX));
	TestFalse(TEXT("infinito"), USamplePlayerSaveComponent::IsLocationValid(FVector(0.0, std::numeric_limits<double>::infinity(), 0.0), KillZ, HALF_WORLD_MAX));
	TestFalse(TEXT("abaixo de KillZ"), USamplePlayerSaveComponent::IsLocationValid(FVector(0.0, 0.0, -1500.0), KillZ, HALF_WORLD_MAX));
	TestFalse(TEXT("exatamente em KillZ"), USamplePlayerSaveComponent::IsLocationValid(FVector(0.0, 0.0, KillZ), KillZ, HALF_WORLD_MAX));
	TestFalse(TEXT("fora do mundo"), USamplePlayerSaveComponent::IsLocationValid(FVector(HALF_WORLD_MAX * 2.0, 0.0, 0.0), KillZ, HALF_WORLD_MAX));

	TestEqual(TEXT("vida normal mantida"), USamplePlayerSaveComponent::SanitizeHealth(40.f, 100.f), 40.f);
	TestEqual(TEXT("vida 0 volta cheia"), USamplePlayerSaveComponent::SanitizeHealth(0.f, 100.f), 100.f);
	TestEqual(TEXT("vida negativa volta cheia"), USamplePlayerSaveComponent::SanitizeHealth(-5.f, 100.f), 100.f);
	TestEqual(TEXT("vida NaN volta cheia"), USamplePlayerSaveComponent::SanitizeHealth(NaN, 100.f), 100.f);
	TestEqual(TEXT("vida acima do máximo é limitada"), USamplePlayerSaveComponent::SanitizeHealth(250.f, 100.f), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSamplePlayerSaveRecordTest, "CatalogAnchor.Sample.PlayerSaveRecord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSamplePlayerSaveRecordTest::RunTest(const FString& Parameters)
{
	FSamplePlayerSaveState Saved;
	Saved.Location = FVector(123.5, -456.25, 789.0);
	Saved.Yaw = 90.f;
	Saved.Health = 42.f;

	FCatalogSaveRecord Record;
	USamplePlayerSaveComponent::WriteRecord(Saved, Record);
	TestEqual(TEXT("versão começa em 1"), Record.Version, 1);

	FSamplePlayerSaveState Loaded;
	TestTrue(TEXT("ida e volta"), USamplePlayerSaveComponent::ReadRecord(Record, Loaded));
	TestEqual(TEXT("posição"), Loaded.Location, Saved.Location);
	TestEqual(TEXT("yaw"), Loaded.Yaw, Saved.Yaw);
	TestEqual(TEXT("vida"), Loaded.Health, Saved.Health);

	FCatalogSaveRecord Truncated = Record;
	Truncated.Data.SetNum(Record.Data.Num() - 1);
	TestFalse(TEXT("bytes faltando: recusa"), USamplePlayerSaveComponent::ReadRecord(Truncated, Loaded));

	FCatalogSaveRecord FutureVersion = Record;
	FutureVersion.Version = 99;
	TestFalse(TEXT("versão desconhecida: recusa"), USamplePlayerSaveComponent::ReadRecord(FutureVersion, Loaded));
	return true;
}

#endif
