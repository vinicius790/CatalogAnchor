// Copyright 2026 Geraliza Castlevania. MIT License.
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "CatalogInteractionComponent.h"
#include "CatalogPickupActor.h"
#include "CatalogTestGuards.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

// Nomeado (não anônimo): em unity build vários .cpp dividem a mesma unidade de tradução.
namespace CatalogInteractionWorldTestsPrivate
{
	static ACatalogPickupActor* SpawnPickup(UWorld* World, UStaticMesh* Cube, const FVector& Location)
	{
		ACatalogPickupActor* Pickup = World->SpawnActor<ACatalogPickupActor>(Location, FRotator::ZeroRotator);
		if (Pickup && Pickup->Mesh)
		{
			Pickup->Mesh->SetStaticMesh(Cube);
		}
		return Pickup;
	}

	static UCatalogInteractionComponent* AddInteraction(AActor* Owner, float TraceDistance)
	{
		UCatalogInteractionComponent* Interaction = NewObject<UCatalogInteractionComponent>(Owner);
		Interaction->TraceDistance = TraceDistance;
		Interaction->bAutoTrace = false;
		Interaction->RegisterComponent();
		return Interaction;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInteractionTraceSegmentTest, "CatalogAnchor.Interaction.TraceSegment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInteractionTraceSegmentTest::RunTest(const FString& Parameters)
{
	const FVector Forward(1.0, 0.0, 0.0);
	FVector Start;
	FVector End;

	// 1ª pessoa: câmera no pawn, alcance igual ao antigo (da câmera).
	TestTrue(TEXT("1a pessoa acha segmento"), UCatalogInteractionComponent::ComputeTraceSegment(FVector::ZeroVector, Forward, FVector::ZeroVector, 250.f, Start, End));
	TestEqual(TEXT("1a pessoa: inicio na camera"), Start, FVector::ZeroVector);
	TestEqual(TEXT("1a pessoa: fim no alcance"), End, FVector(250.0, 0.0, 0.0));

	// 3ª pessoa: câmera 400 cm atrás. Começa no pawn (nada atrás dele) e termina 250 cm à frente dele.
	TestTrue(TEXT("3a pessoa acha segmento"), UCatalogInteractionComponent::ComputeTraceSegment(FVector(-400.0, 0.0, 0.0), Forward, FVector::ZeroVector, 250.f, Start, End));
	TestEqual(TEXT("3a pessoa: inicio no pawn"), Start, FVector::ZeroVector);
	TestEqual(TEXT("3a pessoa: fim medido do pawn"), End, FVector(250.0, 0.0, 0.0));

	// Câmera deslocada (ombro): começa no ponto da mira mais próximo do pawn; fim na borda da esfera.
	TestTrue(TEXT("Ombro acha segmento"), UCatalogInteractionComponent::ComputeTraceSegment(FVector(-400.0, 100.0, 0.0), Forward, FVector::ZeroVector, 250.f, Start, End));
	TestEqual(TEXT("Ombro: inicio"), Start, FVector(0.0, 100.0, 0.0));
	TestEqual(TEXT("Ombro: fim a 250 do pawn"), End, FVector(FMath::Sqrt(250.0 * 250.0 - 100.0 * 100.0), 100.0, 0.0), 0.01f);

	// Mira que passa longe do pawn não alcança nada.
	TestFalse(TEXT("Mira fora do alcance"), UCatalogInteractionComponent::ComputeTraceSegment(FVector(-400.0, 300.0, 0.0), Forward, FVector::ZeroVector, 250.f, Start, End));

	// Câmera já à frente do pawn, dentro do alcance: começa nela.
	TestTrue(TEXT("Camera a frente acha segmento"), UCatalogInteractionComponent::ComputeTraceSegment(FVector(100.0, 0.0, 0.0), Forward, FVector::ZeroVector, 250.f, Start, End));
	TestEqual(TEXT("Camera a frente: inicio na camera"), Start, FVector(100.0, 0.0, 0.0));
	TestEqual(TEXT("Camera a frente: fim"), End, FVector(250.0, 0.0, 0.0));

	// Câmera fora do alcance olhando para longe do pawn.
	TestFalse(TEXT("Camera longe olhando para fora"), UCatalogInteractionComponent::ComputeTraceSegment(FVector(300.0, 0.0, 0.0), Forward, FVector::ZeroVector, 250.f, Start, End));

	// Direção não normalizada é normalizada.
	TestTrue(TEXT("Direcao longa"), UCatalogInteractionComponent::ComputeTraceSegment(FVector::ZeroVector, FVector(5.0, 0.0, 0.0), FVector::ZeroVector, 250.f, Start, End));
	TestEqual(TEXT("Direcao longa: fim"), End, FVector(250.0, 0.0, 0.0));

	// Entradas inválidas.
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("Direcao zero"), UCatalogInteractionComponent::ComputeTraceSegment(FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, 250.f, Start, End));
	TestFalse(TEXT("Alcance zero"), UCatalogInteractionComponent::ComputeTraceSegment(FVector::ZeroVector, Forward, FVector::ZeroVector, 0.f, Start, End));
	TestFalse(TEXT("Alcance NaN"), UCatalogInteractionComponent::ComputeTraceSegment(FVector::ZeroVector, Forward, FVector::ZeroVector, std::numeric_limits<float>::quiet_NaN(), Start, End));
	TestFalse(TEXT("Camera NaN"), UCatalogInteractionComponent::ComputeTraceSegment(FVector(NaN, 0.0, 0.0), Forward, FVector::ZeroVector, 250.f, Start, End));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInteractionWorldOwnersTest, "CatalogAnchor.Interaction.WorldOwnersAndReach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInteractionWorldOwnersTest::RunTest(const FString& Parameters)
{
	using namespace CatalogInteractionWorldTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}

	// Destrói mundo e FWorldContext ao sair do escopo.
	FTestWorldWrapper WorldWrapper;
	if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
	{
		WorldWrapper.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Cubo da engine"), Cube))
	{
		return false;
	}

	// Cubos de 100 cm: o da frente ocupa X 100..200; o de trás, X -250..-150 (entre câmera e pawn).
	ACatalogPickupActor* Front = SpawnPickup(World, Cube, FVector(150.0, 0.0, 0.0));
	ACatalogPickupActor* Behind = SpawnPickup(World, Cube, FVector(-200.0, 0.0, 0.0));

	FActorSpawnParameters AlwaysSpawn;
	AlwaysSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerController* PlayerController = World->SpawnActor<APlayerController>(FVector::ZeroVector, FRotator::ZeroRotator, AlwaysSpawn);
	ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>(FVector::ZeroVector, FRotator::ZeroRotator, AlwaysSpawn);
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(FVector(-400.0, 0.0, 0.0), FRotator::ZeroRotator, AlwaysSpawn);
	AActor* OwnedByController = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, AlwaysSpawn);
	AActor* OwnedByPawn = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, AlwaysSpawn);
	AActor* Loose = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, AlwaysSpawn);
	if (!TestTrue(TEXT("Atores criados"), Front && Behind && PlayerController && Pawn && Camera && OwnedByController && OwnedByPawn && Loose))
	{
		return false;
	}

	PlayerController->Possess(Pawn);
	// Depois do Possess (Restart recalcula): alcance medido do centro do pawn, na origem.
	Pawn->BaseEyeHeight = 0.f;
	// Câmera de 3ª pessoa 400 cm atrás. Sem ULocalPlayer o camera manager não atualiza o cache,
	// então GetPlayerViewPoint devolve a pose do view target.
	PlayerController->SetViewTarget(Camera);
	OwnedByController->SetOwner(PlayerController);
	OwnedByPawn->SetOwner(Pawn);
	APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>();

	// Quem olha pelo dono.
	TestTrue(TEXT("Controller: ele mesmo"), UCatalogInteractionComponent::FindOwningController(PlayerController) == PlayerController);
	TestTrue(TEXT("Pawn possuido: seu controller"), UCatalogInteractionComponent::FindOwningController(Pawn) == PlayerController);
	TestTrue(TEXT("SetOwner(PC): o PC"), UCatalogInteractionComponent::FindOwningController(OwnedByController) == PlayerController);
	TestTrue(TEXT("Ator do pawn: controller do pawn"), UCatalogInteractionComponent::FindOwningController(OwnedByPawn) == PlayerController);
	TestNull(TEXT("Ator solto: ninguem"), UCatalogInteractionComponent::FindOwningController(Loose));
	if (PlayerState)
	{
		TestTrue(TEXT("PlayerState: o PC"), UCatalogInteractionComponent::FindOwningController(PlayerState) == PlayerController);
	}
	else
	{
		AddWarning(TEXT("GameMode do projeto não criou PlayerState; caso PlayerState pulado."));
	}

	// Mira pela câmera, alcance do pawn: o pickup de trás (entre câmera e pawn) nunca é o alvo.
	TestTrue(TEXT("Dono Pawn acha o da frente"), AddInteraction(Pawn, 250.f)->RefreshTarget() == Front);
	TestTrue(TEXT("Dono Controller acha o da frente"), AddInteraction(PlayerController, 250.f)->RefreshTarget() == Front);
	TestTrue(TEXT("Dono SetOwner(PC) acha o da frente"), AddInteraction(OwnedByController, 250.f)->RefreshTarget() == Front);
	if (PlayerState)
	{
		TestTrue(TEXT("Dono PlayerState acha o da frente"), AddInteraction(PlayerState, 250.f)->RefreshTarget() == Front);
	}

	// 90 cm a partir do pawn (+ raio 8 do sweep) não chegam à face em X = 100.
	TestNull(TEXT("Alcance curto medido do pawn"), AddInteraction(OwnedByController, 90.f)->RefreshTarget());

	// PC sem ULocalPlayer não é local (como o PC de um cliente remoto no host): sem foco nesta máquina.
	TestFalse(TEXT("Sem foco para PC nao local"), Front->Mesh->bRenderCustomDepth != 0);

	// Unpossess: o pawn e o que ele possui deixam de ter quem olhe.
	PlayerController->UnPossess();
	TestNull(TEXT("Pawn sem controller"), UCatalogInteractionComponent::FindOwningController(Pawn));
	TestNull(TEXT("Ator do pawn sem controller"), UCatalogInteractionComponent::FindOwningController(OwnedByPawn));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
