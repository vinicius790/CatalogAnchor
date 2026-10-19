// Copyright 2026 Geraliza Castlevania. MIT License.
#include "SampleTrainingDummy.h"
#include "CatalogHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ASampleTrainingDummy::ASampleTrainingDummy()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// Só a vida muda, e em rajadas curtas: 20 Hz basta para a barra sobre o boneco.
	SetNetUpdateFrequency(20.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetStaticMesh(CylinderMesh.Object);
	Body->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.8f));
	// Tipo de objeto Pawn: é o que o Melee Sweep procura.
	Body->SetCollisionProfileName(TEXT("Pawn"));

	Health = CreateDefaultSubobject<UCatalogHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 75.f;
}

void ASampleTrainingDummy::BeginPlay()
{
	Super::BeginPlay();
	Health->OnHealthChanged.AddDynamic(this, &ASampleTrainingDummy::HandleHealthChanged);
	// Cliente que entra com o boneco morto: o OnRep inicial roda antes deste BeginPlay e se perderia.
	HandleHealthChanged(Health->GetHealth(), 0.f);
	if (HasAuthority())
	{
		Health->OnDeath.AddDynamic(this, &ASampleTrainingDummy::HandleDeath);
	}
}

void ASampleTrainingDummy::HandleHealthChanged(float NewHealth, float Delta)
{
	// Roda em toda máquina (servidor direto, clientes pelo OnRep): mesmo visual para todos.
	const bool bAlive = NewHealth > 0.f;
	Body->SetVisibility(bAlive);
	Body->SetCollisionEnabled(bAlive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void ASampleTrainingDummy::HandleDeath(AActor* Killer)
{
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &ASampleTrainingDummy::Respawn, RespawnSeconds, false);
}

void ASampleTrainingDummy::Respawn()
{
	Health->ResetHealth();
}
