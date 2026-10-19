// Copyright 2026 Geraliza Castlevania. MIT License.
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "CatalogInteractionComponent.h"
#include "CatalogInteractionTestTypes.h"
#include "CatalogTestGuards.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

// Nomeado (não anônimo): em unity build vários .cpp dividem a mesma unidade de tradução.
namespace CatalogInteractionExtraTestsPrivate
{
	static ACatalogTestInteractable* SpawnInteractable(UWorld* World, const FVector& Location, int32 Priority = 0)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACatalogTestInteractable* Actor = World->SpawnActor<ACatalogTestInteractable>(Location, FRotator::ZeroRotator, Params);
		if (Actor)
		{
			Actor->Priority = Priority;
		}
		return Actor;
	}

	/** Trace por linha (raio 0), manual e sem cooldown: cada teste liga só o que exercita. */
	static UCatalogInteractionComponent* AddInteraction(AActor* Owner, float TraceDistance)
	{
		UCatalogInteractionComponent* Interaction = NewObject<UCatalogInteractionComponent>(Owner);
		Interaction->TraceDistance = TraceDistance;
		Interaction->TraceRadius = 0.f;
		Interaction->bAutoTrace = false;
		Interaction->InteractionCooldownSeconds = 0.f;
		Interaction->RegisterComponent();
		return Interaction;
	}

	/** Oclusor não interativo (caixa que bloqueia tudo). */
	static AActor* SpawnBlocker(UWorld* World, const FVector& Location, const FVector& Extent)
	{
		AActor* Blocker = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Blocker)
		{
			UBoxComponent* Box = NewObject<UBoxComponent>(Blocker, TEXT("BlockerBox"));
			Box->InitBoxExtent(Extent);
			Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Blocker->SetRootComponent(Box);
			Box->RegisterComponent();
			Blocker->SetActorLocation(Location);
		}
		return Blocker;
	}

	static bool OpenWorld(FAutomationTestBase& Test, FTestWorldWrapper& WorldWrapper)
	{
		if (!WorldWrapper.CreateTestWorld(EWorldType::Game) || !WorldWrapper.BeginPlayInTestWorld())
		{
			WorldWrapper.ForwardErrorMessages(&Test);
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInteractionSelectBestCandidateTest, "CatalogAnchor.Interaction.SelectBestCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInteractionSelectBestCandidateTest::RunTest(const FString& Parameters)
{
	using FCandidate = FCatalogInteractionCandidate;
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Inf = std::numeric_limits<float>::infinity();

	TestEqual(TEXT("Sem candidatos"), UCatalogInteractionComponent::SelectBestCandidate(TArray<FCandidate>()), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Um candidato"), UCatalogInteractionComponent::SelectBestCandidate(TArray<FCandidate>{ FCandidate{ 0, 500.f } }), 0);

	// Prioridade vence a proximidade.
	TestEqual(TEXT("Prioridade maior vence"),
		UCatalogInteractionComponent::SelectBestCandidate(TArray<FCandidate>{ FCandidate{ 0, 10.f }, FCandidate{ 5, 100.f } }), 1);
	// Empate de prioridade: o mais perto do centro da mira.
	TestEqual(TEXT("Empate: mais perto"),
		UCatalogInteractionComponent::SelectBestCandidate(TArray<FCandidate>{ FCandidate{ 2, 50.f }, FCandidate{ 2, 30.f }, FCandidate{ 2, 40.f } }), 1);
	// Empate total: o primeiro (o sweep entrega do mais perto ao mais longe).
	TestEqual(TEXT("Empate total: o primeiro"),
		UCatalogInteractionComponent::SelectBestCandidate(TArray<FCandidate>{ FCandidate{ 2, 30.f }, FCandidate{ 2, 30.f } }), 0);
	// Prioridade negativa ainda compara: -1 > -5.
	TestEqual(TEXT("Prioridade negativa"),
		UCatalogInteractionComponent::SelectBestCandidate(TArray<FCandidate>{ FCandidate{ -5, 1.f }, FCandidate{ -1, 90.f } }), 1);

	// Distância não finita é ignorada, mesmo com prioridade alta.
	TestEqual(TEXT("NaN ignorado"),
		UCatalogInteractionComponent::SelectBestCandidate(TArray<FCandidate>{ FCandidate{ 9, NaN }, FCandidate{ 0, 80.f } }), 1);
	TestEqual(TEXT("Infinito ignorado"),
		UCatalogInteractionComponent::SelectBestCandidate(TArray<FCandidate>{ FCandidate{ 1, Inf } }), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Todos invalidos"),
		UCatalogInteractionComponent::SelectBestCandidate(TArray<FCandidate>{ FCandidate{ 9, NaN }, FCandidate{ 3, NaN } }), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInteractionDefaultsTest, "CatalogAnchor.Interaction.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInteractionDefaultsTest::RunTest(const FString& Parameters)
{
	// Compatibilidade: sem configurar nada o componente se comporta como antes do assist.
	const UCatalogInteractionComponent* Component = NewObject<UCatalogInteractionComponent>(GetTransientPackage());
	TestTrue(TEXT("Canal padrao = Visibility"), Component->TraceChannel == ECC_Visibility);
	TestFalse(TEXT("Assist desligado"), Component->bUseSphereAssist);
	TestTrue(TEXT("Raio do assist positivo"), Component->AssistRadius > 0.f);

	// Os Server RPCs não carregam parâmetros do cliente: _Validate nunca derruba a conexão.
	UCatalogInteractionTestComponent* Test = NewObject<UCatalogInteractionTestComponent>(GetTransientPackage());
	TestTrue(TEXT("ServerTryInteract_Validate"), Test->ValidateTry());
	TestTrue(TEXT("ServerBeginInteract_Validate"), Test->ValidateBegin());
	TestTrue(TEXT("ServerEndInteract_Validate"), Test->ValidateEnd());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInteractionSphereAssistTest, "CatalogAnchor.Interaction.SphereAssist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInteractionSphereAssistTest::RunTest(const FString& Parameters)
{
	using namespace CatalogInteractionExtraTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}

	FTestWorldWrapper WorldWrapper;
	if (!OpenWorld(*this, WorldWrapper))
	{
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();

	// Dono na origem olhando para +X. Alvos de 40 cm em X = 200, fora da linha (Y = 0):
	// a face mais próxima do eixo fica a 30 cm (High) e a 20 cm (Near).
	ACatalogTestInteractable* Owner = SpawnInteractable(World, FVector::ZeroVector);
	ACatalogTestInteractable* High = SpawnInteractable(World, FVector(200.0, 50.0, 0.0), 5);
	ACatalogTestInteractable* Near = SpawnInteractable(World, FVector(200.0, -40.0, 0.0), 0);
	if (!TestTrue(TEXT("Atores criados"), Owner && High && Near))
	{
		return false;
	}
	UCatalogInteractionComponent* Interaction = AddInteraction(Owner, 400.f);
	Interaction->AssistRadius = 60.f;

	TestNull(TEXT("Sem assist: a linha nao acha nada"), Interaction->RefreshTarget());

	Interaction->bUseSphereAssist = true;
	TestTrue(TEXT("Prioridade vence proximidade"), Interaction->RefreshTarget() == High);

	High->Priority = 0;
	TestTrue(TEXT("Empate de prioridade: o mais perto da mira"), Interaction->RefreshTarget() == Near);

	// Raio curto só alcança o mais próximo, mesmo com o outro de prioridade maior.
	High->Priority = 5;
	Interaction->AssistRadius = 25.f;
	TestTrue(TEXT("Raio curto so alcanca o proximo"), Interaction->RefreshTarget() == Near);
	Interaction->AssistRadius = 10.f;
	TestNull(TEXT("Raio menor que tudo"), Interaction->RefreshTarget());
	Interaction->AssistRadius = 60.f;

	// Acerto direto da linha ganha: o assist só entra quando a linha não acha.
	ACatalogTestInteractable* Direct = SpawnInteractable(World, FVector(300.0, 0.0, 0.0), -3);
	if (TestNotNull(TEXT("Alvo direto criado"), Direct))
	{
		TestTrue(TEXT("Acerto direto vence o assist"), Interaction->RefreshTarget() == Direct);
		Direct->Destroy();
	}

	// Oclusor lateral que o sweep acerta mas que não corta a linha de visada não esconde ninguém.
	AActor* Pillar = SpawnBlocker(World, FVector(100.0, 60.0, 0.0), FVector(20.0));
	if (TestNotNull(TEXT("Pilar criado"), Pillar))
	{
		TestTrue(TEXT("Pilar lateral nao esconde (prioridade vence)"), Interaction->RefreshTarget() == High);
		Pillar->Destroy();
	}

	// Oclusor fino na linha de visada de High: descartado mesmo com prioridade maior; Near segue visível.
	AActor* Blade = SpawnBlocker(World, FVector(100.0, 25.0, 0.0), FVector(5.0, 10.0, 50.0));
	if (TestNotNull(TEXT("Lamina criada"), Blade))
	{
		TestTrue(TEXT("Candidato sem visada e descartado"), Interaction->RefreshTarget() == Near);
		Blade->Destroy();
	}

	// Parede inteira à frente esconde todos os candidatos atrás dela.
	if (TestNotNull(TEXT("Parede criada"), SpawnBlocker(World, FVector(100.0, 0.0, 0.0), FVector(10.0, 300.0, 300.0))))
	{
		TestNull(TEXT("Parede esconde os candidatos"), Interaction->RefreshTarget());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInteractionTraceChannelTest, "CatalogAnchor.Interaction.TraceChannelAndDebug",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInteractionTraceChannelTest::RunTest(const FString& Parameters)
{
	using namespace CatalogInteractionExtraTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}

	FTestWorldWrapper WorldWrapper;
	if (!OpenWorld(*this, WorldWrapper))
	{
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();

	ACatalogTestInteractable* Owner = SpawnInteractable(World, FVector::ZeroVector);
	ACatalogTestInteractable* Target = SpawnInteractable(World, FVector(200.0, 0.0, 0.0));
	if (!TestTrue(TEXT("Atores criados"), Owner && Target))
	{
		return false;
	}
	// O alvo ignora Visibility (padrão do componente) e bloqueia só Camera.
	Target->Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Target->Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);

	UCatalogInteractionComponent* Interaction = AddInteraction(Owner, 400.f);
	TestNull(TEXT("Canal padrao (Visibility) nao enxerga o alvo"), Interaction->RefreshTarget());

	Interaction->TraceChannel = ECC_Camera;
	TestTrue(TEXT("TraceChannel configurado acha o alvo"), Interaction->RefreshTarget() == Target);

	// O assist usa o mesmo canal: tira o alvo da linha para só ele poder achá-lo.
	Target->SetActorLocation(FVector(200.0, 40.0, 0.0));
	TestNull(TEXT("Fora da linha sem assist"), Interaction->RefreshTarget());
	Interaction->bUseSphereAssist = true;
	Interaction->AssistRadius = 60.f;
	TestTrue(TEXT("Assist respeita TraceChannel"), Interaction->RefreshTarget() == Target);
	Interaction->TraceChannel = ECC_Visibility;
	TestNull(TEXT("Assist no canal errado nao acha"), Interaction->RefreshTarget());

	// CVar de debug só desenha: o resultado do trace não muda (e ela é restaurada ao sair).
#if ENABLE_DRAW_DEBUG
	if (TestNotNull(TEXT("CVar catalog.Interaction.Debug registrada"),
		IConsoleManager::Get().FindConsoleVariable(TEXT("catalog.Interaction.Debug"))))
	{
		FTestConsoleVariable DebugVar(TEXT("catalog.Interaction.Debug"));
		DebugVar.Set(TEXT("1"));
		Interaction->TraceChannel = ECC_Camera;
		TestTrue(TEXT("Debug ligado nao muda o alvo (assist)"), Interaction->RefreshTarget() == Target);
		Interaction->bUseSphereAssist = false;
		TestNull(TEXT("Debug ligado nao muda o alvo (sem alvo)"), Interaction->RefreshTarget());
	}
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInteractionFailureEventsTest, "CatalogAnchor.Interaction.FailureEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInteractionFailureEventsTest::RunTest(const FString& Parameters)
{
	using namespace CatalogInteractionExtraTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}

	FTestWorldWrapper WorldWrapper;
	if (!OpenWorld(*this, WorldWrapper))
	{
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();

	ACatalogTestInteractable* Owner = SpawnInteractable(World, FVector::ZeroVector);
	ACatalogTestInteractable* Target = SpawnInteractable(World, FVector(200.0, 0.0, 0.0));
	if (!TestTrue(TEXT("Atores criados"), Owner && Target))
	{
		return false;
	}
	UCatalogInteractionComponent* Interaction = AddInteraction(Owner, 400.f);
	UCatalogInteractionTestListener* Listener = NewObject<UCatalogInteractionTestListener>(GetTransientPackage());
	Interaction->OnInteractionFailed.AddDynamic(Listener, &UCatalogInteractionTestListener::HandleFailed);
	Interaction->OnInteractionCompleted.AddDynamic(Listener, &UCatalogInteractionTestListener::HandleCompleted);

	// Exatamente um evento por recusa (sem conexão remota não há RPC: nada duplica) e consome o registro.
	auto ExpectFailure = [this, Listener](const TCHAR* What, ECatalogInteractionFailReason Reason, AActor* Expected)
	{
		if (TestEqual(FString::Printf(TEXT("%s: um evento"), What), Listener->Reasons.Num(), 1))
		{
			TestTrue(FString::Printf(TEXT("%s: motivo"), What), Listener->Reasons[0] == Reason);
			TestTrue(FString::Printf(TEXT("%s: alvo"), What), Listener->Targets[0].Get() == Expected);
		}
		Listener->Reasons.Reset();
		Listener->Targets.Reset();
	};
	const FRotator Away(0.0, 90.0, 0.0);

	// Bloqueado.
	Target->bBlocked = true;
	TestFalse(TEXT("Bloqueado recusa"), Interaction->TryInteract());
	ExpectFailure(TEXT("Blocked"), ECatalogInteractionFailReason::Blocked, Target);
	Target->bBlocked = false;

	// Nada na mira.
	Owner->SetActorRotation(Away);
	TestFalse(TEXT("Sem alvo recusa"), Interaction->TryInteract());
	ExpectFailure(TEXT("NoTarget"), ECatalogInteractionFailReason::NoTarget, nullptr);
	Owner->SetActorRotation(FRotator::ZeroRotator);

	// Interagível na mira, mas além do alcance.
	Interaction->TraceDistance = 100.f;
	TestFalse(TEXT("Fora de alcance recusa"), Interaction->TryInteract());
	ExpectFailure(TEXT("OutOfRange"), ECatalogInteractionFailReason::OutOfRange, Target);
	Interaction->TraceDistance = 400.f;

	// Hold: desviar a mira cancela e avisa NoTarget com o alvo que se perdeu.
	Target->Duration = 0.5f;
	TestTrue(TEXT("Hold inicia"), Interaction->BeginInteract());
	TestTrue(TEXT("Interagindo"), Interaction->IsInteracting());
	WorldWrapper.TickTestWorld(0.1f);
	TestTrue(TEXT("Hold segue com a mira no alvo"), Interaction->IsInteracting() && Listener->Reasons.IsEmpty());
	Owner->SetActorRotation(Away);
	WorldWrapper.TickTestWorld(0.01f);
	TestFalse(TEXT("Hold cancelado ao perder a mira"), Interaction->IsInteracting());
	ExpectFailure(TEXT("Hold sem mira"), ECatalogInteractionFailReason::NoTarget, Target);
	Owner->SetActorRotation(FRotator::ZeroRotator);

	// Hold: bloqueio no meio cancela e avisa Blocked.
	TestTrue(TEXT("Hold reinicia"), Interaction->BeginInteract());
	Target->bBlocked = true;
	WorldWrapper.TickTestWorld(0.01f);
	TestFalse(TEXT("Hold cancelado por bloqueio"), Interaction->IsInteracting());
	ExpectFailure(TEXT("Hold bloqueado"), ECatalogInteractionFailReason::Blocked, Target);
	Target->bBlocked = false;

	// Hold completo: sem evento de falha, uma conclusão.
	Target->Duration = 0.2f;
	TestTrue(TEXT("Hold curto inicia"), Interaction->BeginInteract());
	WorldWrapper.TickTestWorld(0.1f);
	WorldWrapper.TickTestWorld(0.15f);
	TestFalse(TEXT("Hold concluido"), Interaction->IsInteracting());
	TestEqual(TEXT("OnInteract rodou uma vez"), Target->InteractCount, 1);
	TestEqual(TEXT("Uma conclusao"), Listener->CompletedCount, 1);
	TestTrue(TEXT("Sem falha ao concluir"), Listener->Reasons.IsEmpty());

	// Cooldown: a 2ª tentativa dentro da janela avisa Cooldown (e não roda OnInteract).
	Target->Duration = 0.f;
	Interaction->InteractionCooldownSeconds = 10.f;
	WorldWrapper.TickTestWorld(0.1f);
	TestTrue(TEXT("Instantanea passa"), Interaction->TryInteract());
	TestEqual(TEXT("OnInteract rodou de novo"), Target->InteractCount, 2);
	TestTrue(TEXT("Sem falha na que passou"), Listener->Reasons.IsEmpty());
	TestFalse(TEXT("Cooldown recusa"), Interaction->TryInteract());
	ExpectFailure(TEXT("Cooldown"), ECatalogInteractionFailReason::Cooldown, Target);
	TestEqual(TEXT("Cooldown nao interage"), Target->InteractCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInteractionTraceIntervalTest, "CatalogAnchor.Interaction.TraceInterval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInteractionTraceIntervalTest::RunTest(const FString& Parameters)
{
	using namespace CatalogInteractionExtraTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}

	FTestWorldWrapper WorldWrapper;
	if (!OpenWorld(*this, WorldWrapper))
	{
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();

	ACatalogTestInteractable* Owner = SpawnInteractable(World, FVector::ZeroVector);
	ACatalogTestInteractable* Target = SpawnInteractable(World, FVector(200.0, 0.0, 0.0));
	if (!TestTrue(TEXT("Atores criados"), Owner && Target))
	{
		return false;
	}
	UCatalogInteractionComponent* Interaction = AddInteraction(Owner, 400.f);
	Interaction->bAutoTrace = true;
	Interaction->TraceIntervalSeconds = 1.f;
	const FVector Away(200.0, 1000.0, 0.0);

	Interaction->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("1o tick ja traca"), Interaction->CurrentTarget.Get() == Target);
	Target->SetActorLocation(Away);
	Interaction->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Dentro do intervalo nao retraca"), Interaction->CurrentTarget.Get() == Target);
	Interaction->TickComponent(1.f, LEVELTICK_All, nullptr);
	TestNull(TEXT("Passado o intervalo retraca"), Interaction->CurrentTarget.Get());

	// Intervalo 0 = todo tick (comportamento antigo).
	Interaction->TraceIntervalSeconds = 0.f;
	Interaction->TickComponent(1.f, LEVELTICK_All, nullptr);
	Target->SetActorLocation(FVector(200.0, 0.0, 0.0));
	Interaction->TickComponent(0.001f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Intervalo 0 acha no tick seguinte"), Interaction->CurrentTarget.Get() == Target);
	Target->SetActorLocation(Away);
	Interaction->TickComponent(0.001f, LEVELTICK_All, nullptr);
	TestNull(TEXT("Intervalo 0 perde no tick seguinte"), Interaction->CurrentTarget.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogInteractionUnpossessedPawnTest, "CatalogAnchor.Interaction.UnpossessedPawnNoAutoTrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogInteractionUnpossessedPawnTest::RunTest(const FString& Parameters)
{
	using namespace CatalogInteractionExtraTestsPrivate;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}

	FTestWorldWrapper WorldWrapper;
	if (!OpenWorld(*this, WorldWrapper))
	{
		return false;
	}
	UWorld* World = WorldWrapper.GetTestWorld();

	FActorSpawnParameters AlwaysSpawn;
	AlwaysSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>(FVector::ZeroVector, FRotator::ZeroRotator, AlwaysSpawn);
	ACatalogTestInteractable* Target = SpawnInteractable(World, FVector(200.0, 0.0, 0.0));
	if (!TestTrue(TEXT("Atores criados"), Pawn && Target && Pawn->GetController() == nullptr))
	{
		return false;
	}
	UCatalogInteractionComponent* Interaction = AddInteraction(Pawn, 400.f);
	Interaction->bAutoTrace = true;
	Interaction->TraceIntervalSeconds = 0.f;

	// Sem possessão o tick automático não traça (e não deixa alvo preso)...
	Interaction->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestNull(TEXT("Pawn sem controller nao auto-traca"), Interaction->CurrentTarget.Get());
	// ...mas a tentativa explícita ainda traça fresco.
	TestTrue(TEXT("RefreshTarget manual ainda acha"), Interaction->RefreshTarget() == Target);
	Interaction->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestNull(TEXT("Tick limpa o alvo antigo"), Interaction->CurrentTarget.Get());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
