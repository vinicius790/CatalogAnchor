// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SampleArenaActor.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * O cenário inteiro num ator replicado: chão, paredes, sol, céu e sky light.
 * Por que não AStaticMeshActor/ADirectionalLight gerados pelo GameMode: eles não replicam (e a
 * malha atribuída no servidor também não), então o cliente cairia no vazio, no escuro. Aqui tudo
 * nasce do construtor, igual em toda máquina; o GameMode só faz o spawn no servidor.
 */
UCLASS()
class ASampleArenaActor : public AActor
{
	GENERATED_BODY()

public:
	ASampleArenaActor();

private:
	/** Bloco de colisão com o Cube da engine (100 cm), dimensionado em cm. Só no construtor. */
	UStaticMeshComponent* CreateBlock(FName Name, UStaticMesh* Mesh, const FVector& Center, const FVector& SizeCm);

	UPROPERTY(VisibleAnywhere, Category = "Catalog|Sample")
	TObjectPtr<UStaticMeshComponent> Floor;

	UPROPERTY(VisibleAnywhere, Category = "Catalog|Sample")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Catalog|Sample")
	TObjectPtr<USkyAtmosphereComponent> Sky;

	UPROPERTY(VisibleAnywhere, Category = "Catalog|Sample")
	TObjectPtr<USkyLightComponent> SkyLight;
};
