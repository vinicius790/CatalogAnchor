// Copyright 2026 Geraliza Castlevania. MIT License.
#include "SampleCatalogPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace SamplePickupPrivate
{
	/** A esfera da engine tem 1 m de diâmetro; 0.4 = 40 cm. */
	constexpr float MeshScale = 0.4f;
}

ASampleCatalogPickup::ASampleCatalogPickup()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
}

void ASampleCatalogPickup::BeginPlay()
{
	Super::BeginPlay();
	// Aqui, em toda máquina: a malha é o root, e a escala do spawn (ex.: ServerDropItem) sobrescreveria a do construtor.
	SetActorScale3D(FVector(SamplePickupPrivate::MeshScale));
}
