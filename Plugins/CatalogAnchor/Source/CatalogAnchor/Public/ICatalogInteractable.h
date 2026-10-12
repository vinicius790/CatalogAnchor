// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ICatalogInteractable.generated.h"

class AActor;

UINTERFACE(MinimalAPI, Blueprintable)
class UCatalogInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contrato de interação (trace + interface) do catálogo §8.
 * Implemente em atores estáticos / pickups; UCatalogInteractionComponent faz o trace no servidor.
 * Chame sempre via ICatalogInteractable::Execute_* (funciona para implementações C++ e Blueprint).
 * Sem override, o UHT devolve os defaults do tipo: prompt vazio, false, duração 0 (instantâneo),
 * não bloqueado, foco sem efeito.
 */
class CATALOGANCHOR_API ICatalogInteractable
{
	GENERATED_BODY()

public:
	/** Texto curto do prompt para a UI (ex.: "Abrir baú"). Lido no cliente dono e no servidor. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Catalog|Interaction")
	FText GetInteractionPrompt() const;

	/** Executa a interação (só autoridade). Retorna true se consumiu. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Catalog|Interaction")
	bool OnInteract(AActor* InstigatorActor);

	/** Segundos que o jogador precisa segurar. 0 (ou inválido) = instantâneo. Lido pelo servidor ao iniciar. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Catalog|Interaction")
	float GetInteractionDuration() const;

	/**
	 * true = existe mas não pode ser usado agora (trancado, sem chave...). O servidor checa antes
	 * de completar e durante o hold; o alvo continua recebendo foco para a UI mostrar "Trancado".
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Catalog|Interaction")
	bool IsInteractionBlocked(AActor* InstigatorActor) const;

	/** Hook cosmético (highlight/outline). Só roda na máquina do jogador que olha; nunca mude estado de jogo aqui. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Catalog|Interaction")
	void OnFocusChanged(bool bHasFocus, AActor* InstigatorActor);
};
