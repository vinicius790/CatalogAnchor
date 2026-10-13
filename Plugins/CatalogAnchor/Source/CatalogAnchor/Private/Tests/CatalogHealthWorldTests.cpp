// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogHealthTestListener.h"
#include "CatalogTestGuards.h"
#include "CatalogAbility.h"
#include "CatalogHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

// Fora do #if: o código gerado pelo UHT referencia estas UFUNCTIONs em todo build.
void UCatalogHealthTestListener::HandleHealthChanged(float NewHealth, float Delta)
{
	++ChangedCount;
	LastHealth = NewHealth;
	LastDelta = Delta;
	if (ExtraDamageOnChange > 0.f && Target.IsValid())
	{
		const float Extra = ExtraDamageOnChange;
		ExtraDamageOnChange = 0.f;
		UGameplayStatics::ApplyDamage(Target.Get(), Extra, nullptr, nullptr, UDamageType::StaticClass());
	}
	if (bDestroyOnChange && Target.IsValid())
	{
		Target->Destroy();
	}
}

void UCatalogHealthTestListener::HandleDeath(AActor* Killer)
{
	++DeathCount;
	LastKiller = Killer;
	if (bDestroyOnDeath && Target.IsValid())
	{
		Target->Destroy();
	}
}

void UCatalogHealthTestListener::HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	AController* InstigatedBy, AActor* DamageCauser)
{
	++AnyDamageCount;
}

#if WITH_DEV_AUTOMATION_TESTS

// Sem coleta de lixo durante um RunTest síncrono: ponteiros crus para objetos de teste bastam.
namespace CatalogHealthWorldTestsImpl
{
	/** Mundo de jogo próprio (standalone: tudo tem autoridade), destruído no fim do escopo. */
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			// Sem GameMode, UWorld::BeginPlay não chega aos atores; é o que AGameStateBase::HandleBeginPlay faria.
			World->GetWorldSettings()->NotifyBeginPlay();
		}

		~FScopedTestWorld()
		{
			World->EndPlay(EEndPlayReason::Quit);
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(/*bInformEngineOfWorld*/ false);
		}

		UWorld* World = nullptr;
	};

	struct FHealthFixture
	{
		AActor* Actor = nullptr;
		UCatalogHealthComponent* Health = nullptr;
		UCatalogHealthTestListener* Listener = nullptr;
	};

	/** AActor simples: AActor::TakeDamage não exige GameMode (APawn exige, via ShouldTakeDamage). */
	static FHealthFixture SpawnWithHealth(UWorld* World, float MaxHealth)
	{
		FHealthFixture Fixture;
		Fixture.Actor = World->SpawnActor<AActor>();
		Fixture.Health = NewObject<UCatalogHealthComponent>(Fixture.Actor);
		Fixture.Health->MaxHealth = MaxHealth;
		// O ator já começou o jogo: RegisterComponent roteia o BeginPlay do componente.
		Fixture.Health->RegisterComponent();
		Fixture.Listener = NewObject<UCatalogHealthTestListener>(GetTransientPackage());
		Fixture.Listener->Target = Fixture.Actor;
		Fixture.Health->OnHealthChanged.AddDynamic(Fixture.Listener, &UCatalogHealthTestListener::HandleHealthChanged);
		Fixture.Health->OnDeath.AddDynamic(Fixture.Listener, &UCatalogHealthTestListener::HandleDeath);
		return Fixture;
	}

	static void Damage(AActor* Target, float Amount, AActor* Causer = nullptr)
	{
		UGameplayStatics::ApplyDamage(Target, Amount, nullptr, Causer, UDamageType::StaticClass());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogHealthApplyDamageEventsTest, "CatalogAnchor.Health.ApplyDamageEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogHealthApplyDamageEventsTest::RunTest(const FString& Parameters)
{
	using namespace CatalogHealthWorldTestsImpl;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	if (!TestNotNull(TEXT("GEngine"), GEngine))
	{
		return false;
	}
	FScopedTestWorld TestWorld;
	const FHealthFixture F = SpawnWithHealth(TestWorld.World, 100.f);
	UCatalogHealthTestListener* L = F.Listener;
	AActor* Causer = TestWorld.World->SpawnActor<AActor>();
	const float NaN = FMath::Sqrt(-1.f);

	TestEqual(TEXT("BeginPlay na autoridade começa cheio"), F.Health->GetHealth(), 100.f);

	Damage(F.Actor, 30.f, Causer);
	TestEqual(TEXT("1 dano: 1 OnHealthChanged"), L->ChangedCount, 1);
	TestEqual(TEXT("vida 70"), F.Health->GetHealth(), 70.f);
	TestEqual(TEXT("delta -30"), L->LastDelta, -30.f);
	TestEqual(TEXT("ainda vivo"), L->DeathCount, 0);

	Damage(F.Actor, NaN, Causer);
	Damage(F.Actor, -20.f, Causer);
	TestEqual(TEXT("NaN e dano negativo: sem evento"), L->ChangedCount, 1);
	TestEqual(TEXT("NaN e dano negativo: vida intacta"), F.Health->GetHealth(), 70.f);

	Damage(F.Actor, 500.f, Causer);
	TestEqual(TEXT("dano letal: 2º OnHealthChanged"), L->ChangedCount, 2);
	TestEqual(TEXT("vida para em 0"), F.Health->GetHealth(), 0.f);
	TestEqual(TEXT("delta só até 0"), L->LastDelta, -70.f);
	TestEqual(TEXT("OnDeath uma vez"), L->DeathCount, 1);
	TestTrue(TEXT("Killer = DamageCauser"), L->LastKiller.Get() == Causer);
	TestTrue(TEXT("IsDead"), F.Health->IsDead());

	Damage(F.Actor, 10.f, Causer);
	TestEqual(TEXT("dano após a morte: sem evento"), L->ChangedCount, 2);
	TestEqual(TEXT("dano após a morte: sem 2ª morte"), L->DeathCount, 1);
	TestEqual(TEXT("Heal em morto não cura"), F.Health->Heal(50.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogHealthResetRespawnTest, "CatalogAnchor.Health.ResetHealthRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogHealthResetRespawnTest::RunTest(const FString& Parameters)
{
	using namespace CatalogHealthWorldTestsImpl;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	if (!TestNotNull(TEXT("GEngine"), GEngine))
	{
		return false;
	}
	FScopedTestWorld TestWorld;
	const FHealthFixture F = SpawnWithHealth(TestWorld.World, 50.f);
	UCatalogHealthTestListener* L = F.Listener;

	F.Health->ResetHealth();
	TestEqual(TEXT("ResetHealth com vida cheia: sem evento"), L->ChangedCount, 0);

	Damage(F.Actor, 80.f);
	TestEqual(TEXT("1ª morte"), L->DeathCount, 1);

	F.Health->ResetHealth();
	TestFalse(TEXT("ResetHealth revive"), F.Health->IsDead());
	TestEqual(TEXT("volta a MaxHealth"), F.Health->GetHealth(), 50.f);
	TestEqual(TEXT("revive notifica +MaxHealth"), L->LastDelta, 50.f);
	TestEqual(TEXT("reviver não é morte"), L->DeathCount, 1);

	Damage(F.Actor, 20.f);
	TestEqual(TEXT("vivo de novo: dano volta a contar"), F.Health->GetHealth(), 30.f);
	Damage(F.Actor, 30.f);
	TestEqual(TEXT("nova morte após respawn"), L->DeathCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogHealthReentrantDamageTest, "CatalogAnchor.Health.ReentrantDamageSingleDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogHealthReentrantDamageTest::RunTest(const FString& Parameters)
{
	using namespace CatalogHealthWorldTestsImpl;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	if (!TestNotNull(TEXT("GEngine"), GEngine))
	{
		return false;
	}
	FScopedTestWorld TestWorld;
	const FHealthFixture F = SpawnWithHealth(TestWorld.World, 100.f);
	UCatalogHealthTestListener* L = F.Listener;

	// O OnHealthChanged do 1º dano (100→90) aplica dano letal: a morte acontece no callback aninhado.
	L->ExtraDamageOnChange = 1000.f;
	Damage(F.Actor, 10.f);
	TestEqual(TEXT("dois OnHealthChanged (externo e aninhado)"), L->ChangedCount, 2);
	TestEqual(TEXT("vida 0"), F.Health->GetHealth(), 0.f);
	TestEqual(TEXT("OnDeath uma vez só"), L->DeathCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogHealthDestroyedInCallbackTest, "CatalogAnchor.Health.DestroyedDuringCallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogHealthDestroyedInCallbackTest::RunTest(const FString& Parameters)
{
	using namespace CatalogHealthWorldTestsImpl;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	if (!TestNotNull(TEXT("GEngine"), GEngine))
	{
		return false;
	}
	FScopedTestWorld TestWorld;

	// Destroy dentro de OnDeath (ex.: cadáver removido na hora).
	const FHealthFixture A = SpawnWithHealth(TestWorld.World, 100.f);
	A.Listener->bDestroyOnDeath = true;
	Damage(A.Actor, 100.f);
	TestEqual(TEXT("OnDeath: morte contada uma vez"), A.Listener->DeathCount, 1);
	TestFalse(TEXT("OnDeath: ator destruído"), IsValid(A.Actor));
	// EndPlay desligou o OnTakeAnyDamage: ator destruído (ainda não coletado) não reage.
	Damage(A.Actor, 10.f);
	TestEqual(TEXT("OnDeath: nada depois do Destroy"), A.Listener->ChangedCount, 1);

	// Destroy dentro de OnHealthChanged, antes do OnDeath: a morte ainda é avisada, uma vez.
	const FHealthFixture B = SpawnWithHealth(TestWorld.World, 100.f);
	B.Listener->bDestroyOnChange = true;
	Damage(B.Actor, 100.f);
	TestFalse(TEXT("OnHealthChanged: ator destruído"), IsValid(B.Actor));
	TestEqual(TEXT("OnHealthChanged: um evento"), B.Listener->ChangedCount, 1);
	TestEqual(TEXT("OnHealthChanged: OnDeath uma vez"), B.Listener->DeathCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogHealthCanBeDamagedTest, "CatalogAnchor.Health.RespectsCanBeDamaged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogHealthCanBeDamagedTest::RunTest(const FString& Parameters)
{
	using namespace CatalogHealthWorldTestsImpl;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	if (!TestNotNull(TEXT("GEngine"), GEngine))
	{
		return false;
	}
	FScopedTestWorld TestWorld;
	const FHealthFixture F = SpawnWithHealth(TestWorld.World, 100.f);

	F.Actor->SetCanBeDamaged(false);
	Damage(F.Actor, 40.f);
	TestEqual(TEXT("bCanBeDamaged = false: vida intacta"), F.Health->GetHealth(), 100.f);
	TestEqual(TEXT("bCanBeDamaged = false: sem evento"), F.Listener->ChangedCount, 0);

	F.Actor->SetCanBeDamaged(true);
	Damage(F.Actor, 40.f);
	TestEqual(TEXT("bCanBeDamaged = true: dano aplica"), F.Health->GetHealth(), 60.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogHealthSetHealthTest, "CatalogAnchor.Health.SetHealth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogHealthSetHealthTest::RunTest(const FString& Parameters)
{
	using namespace CatalogHealthWorldTestsImpl;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	if (!TestNotNull(TEXT("GEngine"), GEngine))
	{
		return false;
	}
	FScopedTestWorld TestWorld;
	const FHealthFixture F = SpawnWithHealth(TestWorld.World, 100.f);
	UCatalogHealthTestListener* L = F.Listener;

	// Restaurar save não é dano: vale mesmo com bCanBeDamaged = false.
	F.Actor->SetCanBeDamaged(false);
	F.Health->SetHealth(40.f);
	TestEqual(TEXT("define o valor"), F.Health->GetHealth(), 40.f);
	TestEqual(TEXT("um OnHealthChanged"), L->ChangedCount, 1);
	TestEqual(TEXT("delta -60"), L->LastDelta, -60.f);

	F.Health->SetHealth(40.f);
	F.Health->SetHealth(FMath::Sqrt(-1.f));
	TestEqual(TEXT("mesmo valor e NaN: sem evento"), L->ChangedCount, 1);
	TestEqual(TEXT("NaN: vida intacta"), F.Health->GetHealth(), 40.f);

	F.Health->SetHealth(500.f);
	TestEqual(TEXT("limita a MaxHealth"), F.Health->GetHealth(), 100.f);

	F.Health->SetHealth(-5.f);
	TestEqual(TEXT("limita a 0"), F.Health->GetHealth(), 0.f);
	TestEqual(TEXT("chegar a 0 dispara OnDeath"), L->DeathCount, 1);
	F.Health->SetHealth(0.f);
	TestEqual(TEXT("morto e 0 de novo: sem 2ª morte"), L->DeathCount, 1);

	F.Health->SetHealth(25.f);
	TestFalse(TEXT("valor > 0 revive"), F.Health->IsDead());
	TestEqual(TEXT("reviver não é morte"), L->DeathCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogAbilityDeadAvatarTest, "CatalogAnchor.Abilities.DeadAvatarCannotActivate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogAbilityDeadAvatarTest::RunTest(const FString& Parameters)
{
	using namespace CatalogHealthWorldTestsImpl;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	if (!TestNotNull(TEXT("GEngine"), GEngine))
	{
		return false;
	}
	FScopedTestWorld TestWorld;
	const FHealthFixture F = SpawnWithHealth(TestWorld.World, 100.f);
	AActor* WithoutHealth = TestWorld.World->SpawnActor<AActor>();
	// MeleeSweep não sobrescreve CanActivate: exercita a checagem da base.
	const UCatalogAbility* Ability = NewObject<UCatalogAbility_MeleeSweep>(GetTransientPackage());

	TestTrue(TEXT("vivo: pode ativar"), Ability->CanActivate(F.Actor));
	TestTrue(TEXT("sem UCatalogHealthComponent: pode ativar"), Ability->CanActivate(WithoutHealth));

	Damage(F.Actor, 100.f);
	TestFalse(TEXT("morto: não ativa"), Ability->CanActivate(F.Actor));

	F.Health->ResetHealth();
	TestTrue(TEXT("após respawn: ativa de novo"), Ability->CanActivate(F.Actor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogHealthFirstReplicationTest, "CatalogAnchor.Health.ClientFirstReplicationNoFalseDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogHealthFirstReplicationTest::RunTest(const FString& Parameters)
{
	using namespace CatalogHealthWorldTestsImpl;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	if (!TestNotNull(TEXT("GEngine"), GEngine))
	{
		return false;
	}
	FScopedTestWorld TestWorld;
	AActor* Actor = TestWorld.World->SpawnActor<AActor>();
	UCatalogHealthTestListener* L = NewObject<UCatalogHealthTestListener>(GetTransientPackage());

	// Sem RegisterComponent: não é autoridade e Health segue no default (100), como no cliente antes da rede.
	const auto MakeClient = [&](float MaxHealth)
	{
		UCatalogHealthTestComponent* Client = NewObject<UCatalogHealthTestComponent>(Actor);
		Client->MaxHealth = MaxHealth;
		Client->OnHealthChanged.AddDynamic(L, &UCatalogHealthTestListener::HandleHealthChanged);
		Client->OnDeath.AddDynamic(L, &UCatalogHealthTestListener::HandleDeath);
		return Client;
	};

	// MaxHealth != 100: o servidor replica 250; o default local 100 não pode virar delta +150.
	UCatalogHealthTestComponent* Client = MakeClient(250.f);
	Client->SimulateReplication(250.f);
	TestEqual(TEXT("1a replicação: um OnHealthChanged"), L->ChangedCount, 1);
	TestEqual(TEXT("1a replicação: vida do servidor"), L->LastHealth, 250.f);
	TestEqual(TEXT("1a replicação: sem delta falso"), L->LastDelta, 0.f);
	TestEqual(TEXT("1a replicação: vivo, sem OnDeath"), L->DeathCount, 0);

	Client->SimulateReplication(200.f);
	TestEqual(TEXT("2a replicação: delta real"), L->LastDelta, -50.f);
	Client->SimulateReplication(0.f);
	TestEqual(TEXT("morte replicada: OnDeath"), L->DeathCount, 1);

	// Quem entra depois com o ator já morto: OnDeath (sem Killer) na 1a replicação.
	UCatalogHealthTestComponent* Late = MakeClient(250.f);
	Late->SimulateReplication(0.f);
	TestEqual(TEXT("entrou morto: OnHealthChanged sem delta"), L->LastDelta, 0.f);
	TestEqual(TEXT("entrou morto: OnDeath"), L->DeathCount, 2);
	TestTrue(TEXT("entrou morto: IsDead"), Late->IsDead());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatalogAbilityMeleeDestroyedTargetTest, "CatalogAnchor.Abilities.MeleeSweepSkipsTargetDestroyedInSweep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FCatalogAbilityMeleeDestroyedTargetTest::RunTest(const FString& Parameters)
{
	using namespace CatalogHealthWorldTestsImpl;
	if (CatalogTestGuards::ShouldSkipDuringPIE(*this))
	{
		return true;
	}
	if (!TestNotNull(TEXT("GEngine"), GEngine))
	{
		return false;
	}
	FScopedTestWorld TestWorld;
	AActor* Avatar = TestWorld.World->SpawnActor<AActor>();

	// Alvos em fila à frente do avatar (+X); A é atingido antes de B (hits ordenados por distância).
	const auto SpawnTarget = [&](float X)
	{
		FHealthFixture Fixture = SpawnWithHealth(TestWorld.World, 100.f);
		UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(Fixture.Actor);
		Capsule->InitCapsuleSize(20.f, 40.f);
		Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
		Fixture.Actor->SetRootComponent(Capsule);
		Capsule->RegisterComponent();
		Fixture.Actor->SetActorLocation(FVector(X, 0.f, 0.f));
		Fixture.Actor->OnTakeAnyDamage.AddDynamic(Fixture.Listener, &UCatalogHealthTestListener::HandleAnyDamage);
		return Fixture;
	};
	const FHealthFixture A = SpawnTarget(200.f);
	const FHealthFixture B = SpawnTarget(300.f);

	UCatalogAbility_MeleeSweep* Ability = NewObject<UCatalogAbility_MeleeSweep>(GetTransientPackage());
	Ability->Range = 400.f;
	Ability->Radius = 50.f;
	Ability->Damage = 10.f;

	// Controle: sem a destruição, o golpe atinge os dois (o cenário de colisão do teste é válido).
	TestTrue(TEXT("Activate"), Ability->Activate(Avatar));
	TestEqual(TEXT("controle: A atingido"), A.Listener->AnyDamageCount, 1);
	TestEqual(TEXT("controle: B atingido"), B.Listener->AnyDamageCount, 1);

	// O dano em A destrói B no meio da varredura: B não pode receber ApplyDamage depois de destruído.
	A.Listener->Target = B.Actor;
	A.Listener->bDestroyOnChange = true;
	Ability->Activate(Avatar);
	TestEqual(TEXT("A atingido de novo"), A.Listener->AnyDamageCount, 2);
	TestFalse(TEXT("B destruído durante a varredura"), IsValid(B.Actor));
	TestEqual(TEXT("B destruído não recebe dano"), B.Listener->AnyDamageCount, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
