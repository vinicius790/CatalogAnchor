// Copyright 2026 Geraliza Castlevania. MIT License.
#include "SampleArenaActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace SampleArenaPrivate
{
	/** Lado do chão quadrado (cm), centrado na origem do ator; o topo fica em Z = 0. */
	constexpr float ArenaSize = 3000.f;
	constexpr float FloorThickness = 50.f;
	constexpr float WallHeight = 100.f;
	constexpr float WallThickness = 50.f;
}

ASampleArenaActor::ASampleArenaActor()
{
	using namespace SampleArenaPrivate;

	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// É o chão de todo mundo: replica para cada cliente, perto ou longe.
	bAlwaysRelevant = true;
	// Cenário estático: nunca muda depois do spawn.
	SetNetUpdateFrequency(1.f);

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GridMaterial(TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	UStaticMesh* Cube = CubeMesh.Object;

	Floor = CreateBlock(TEXT("Floor"), Cube, FVector(0.f, 0.f, -FloorThickness * 0.5f), FVector(ArenaSize, ArenaSize, FloorThickness));
	if (GridMaterial.Succeeded())
	{
		// Grade dá noção de velocidade (dash) num chão sem textura própria.
		Floor->SetMaterial(0, GridMaterial.Object);
	}

	// Paredes baixas: ninguém cai da arena (o mapa Entry não tem mais nada).
	const float Half = ArenaSize * 0.5f;
	const float WallZ = WallHeight * 0.5f;
	CreateBlock(TEXT("WallNorth"), Cube, FVector(Half, 0.f, WallZ), FVector(WallThickness, ArenaSize, WallHeight));
	CreateBlock(TEXT("WallSouth"), Cube, FVector(-Half, 0.f, WallZ), FVector(WallThickness, ArenaSize, WallHeight));
	CreateBlock(TEXT("WallEast"), Cube, FVector(0.f, Half, WallZ), FVector(ArenaSize, WallThickness, WallHeight));
	CreateBlock(TEXT("WallWest"), Cube, FVector(0.f, -Half, WallZ), FVector(ArenaSize, WallThickness, WallHeight));

	// Tudo Movable: ator gerado em runtime não tem iluminação construída.
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(RootComponent);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-40.f, 35.f, 0.f));
	// Sem um "atmosphere sun" o SkyAtmosphere não tem o que espalhar e o céu fica preto.
	Sun->bAtmosphereSunLight = true;

	Sky = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Sky"));
	Sky->SetupAttachment(RootComponent);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(RootComponent);
	SkyLight->SetMobility(EComponentMobility::Movable);
	// Captura em tempo real do SkyAtmosphere: luz ambiente sem cubemap nem rebuild de iluminação.
	SkyLight->bRealTimeCapture = true;
}

UStaticMeshComponent* ASampleArenaActor::CreateBlock(FName Name, UStaticMesh* Mesh, const FVector& Center, const FVector& SizeCm)
{
	UStaticMeshComponent* Block = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Block->SetupAttachment(RootComponent);
	Block->SetMobility(EComponentMobility::Movable);
	Block->SetStaticMesh(Mesh);
	Block->SetRelativeLocation(Center);
	Block->SetRelativeScale3D(SizeCm / 100.f);
	Block->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	return Block;
}
