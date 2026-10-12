// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "CatalogInteractionComponent.generated.h"

class AActor;
class AController;
struct FHitResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCatalogInteractionTargetChanged, AActor*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCatalogInteractionCompleted, AActor*, Target, bool, bConsumed);

/**
 * Componente de interação por trace (catálogo §8), v2 com hold-to-interact.
 * - O servidor faz o trace e decide; o cliente só pede via Server RPC e lê o estado replicado.
 * - CurrentTarget e o estado do hold replicam só para o dono (COND_OwnerOnly).
 * - Hold: o servidor guarda início (relógio do servidor) + duração e completa no tick;
 *   o dono calcula o progresso localmente com AGameStateBase::GetServerWorldTimeSeconds(),
 *   sem replicar um float por frame.
 * - Cooldown anti-spam e IsInteractionBlocked são checados no servidor antes de completar.
 */
UCLASS(ClassGroup = (Catalog), meta = (BlueprintSpawnableComponent))
class CATALOGANCHOR_API UCatalogInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCatalogInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Alcance (cm), medido a partir do pawn controlado; a mira sai da câmera (GetPlayerViewPoint).
	 * Sem pawn, medido do ponto de visão (ou do próprio dono sem controller). Padrão: Project Settings.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction", meta = (ClampMin = "10.0"))
	float TraceDistance;

	/** Raio do sphere-sweep (0 = line trace). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction", meta = (ClampMin = "0.0"))
	float TraceRadius = 8.f;

	/** Canal de colisão usado no trace (padrão Visibility / Custom). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Servidor atualiza CurrentTarget a cada tick (desligue se preferir poll manual; durante um hold o trace roda sempre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction")
	bool bAutoTrace = true;

	/** Intervalo mínimo entre tentativas aceitas pelo servidor (s): barra spam de RPC/tecla. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Interaction", meta = (ClampMin = "0.0"))
	float InteractionCooldownSeconds = 0.25f;

	/** Alvo atual, decidido pelo servidor (OwnerOnly — não precisa ir para sim proxy). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentTarget, Category = "Catalog|Interaction")
	TWeakObjectPtr<AActor> CurrentTarget;

	/** Alvo mudou (inclui nullptr). Dispara no servidor e no cliente dono. */
	UPROPERTY(BlueprintAssignable, Category = "Catalog|Interaction")
	FCatalogInteractionTargetChanged OnTargetChanged;

	/** OnInteract rodou no servidor. Dispara no servidor e, se o dono for remoto, no cliente dono (Client RPC). */
	UPROPERTY(BlueprintAssignable, Category = "Catalog|Interaction")
	FCatalogInteractionCompleted OnInteractionCompleted;

	/**
	 * Interação instantânea. Servidor: refaz o trace e chama OnInteract; retorna o resultado dele.
	 * Alvo com duração > 0 é recusado (use BeginInteract) para o cliente não pular o hold.
	 * Cliente: envia ServerTryInteract e retorna se havia alvo replicado (o resultado real só existe no servidor).
	 */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Interaction")
	bool TryInteract();

	/**
	 * Botão pressionado. Duração 0 completa já (igual TryInteract); duração > 0 inicia o hold, que o
	 * servidor completa sozinho no tick. Cliente: envia ServerBeginInteract e retorna se havia alvo replicado.
	 */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Interaction")
	bool BeginInteract();

	/** Botão solto: cancela o hold em andamento (no-op sem hold). */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Interaction")
	void EndInteract();

	/** Progresso 0..1 do hold atual (0 sem hold). Válido no servidor e no cliente dono. */
	UFUNCTION(BlueprintPure, Category = "Catalog|Interaction")
	float GetInteractionProgress() const;

	/** true enquanto há um hold em andamento (servidor e cliente dono). */
	UFUNCTION(BlueprintPure, Category = "Catalog|Interaction")
	bool IsInteracting() const;

	/** Refaz o trace e atualiza CurrentTarget. Sem autoridade, só devolve o alvo replicado. */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Interaction")
	AActor* RefreshTarget();

	UFUNCTION(BlueprintPure, Category = "Catalog|Interaction")
	FText GetCurrentPrompt() const;

	/**
	 * Lógica pura do progresso (testável sem mundo). Duração 0/negativa/NaN = 1 (instantâneo);
	 * tempo inválido ou Now < StartTime = 0, para nunca completar por acidente.
	 */
	static float ComputeHoldProgress(double StartTime, double Now, float Duration);

	/**
	 * Controller de quem olha pelo dono: o do Pawn (nulo sem possessão), o próprio Controller ou o
	 * primeiro na cadeia de GetOwner() (PlayerState, ator com SetOwner(PlayerController)).
	 */
	static AController* FindOwningController(AActor* Owner);

	/**
	 * Segmento do trace (pura, testável). Mira pelo raio da câmera, alcance medido de ReachOrigin:
	 * começa no ponto do raio mais próximo de ReachOrigin (ou na câmera, se já passou dele) e termina
	 * onde o raio sai da esfera de raio Reach. false = a mira não passa ao alcance (ou entrada inválida).
	 */
	static bool ComputeTraceSegment(const FVector& ViewLocation, const FVector& ViewDirection,
		const FVector& ReachOrigin, float Reach, FVector& OutStart, FVector& OutEnd);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_CurrentTarget();

	UFUNCTION(Server, Reliable)
	void ServerTryInteract();

	UFUNCTION(Server, Reliable)
	void ServerBeginInteract();

	UFUNCTION(Server, Reliable)
	void ServerEndInteract();

	/** Leva OnInteractionCompleted ao dono remoto, que não vê o broadcast do servidor. */
	UFUNCTION(Client, Reliable)
	void ClientInteractionCompleted(AActor* Target, bool bConsumed);

	/** Início do hold no relógio do servidor (s). double: precisão em sessões longas. */
	UPROPERTY(Replicated)
	double HoldStartServerTime = 0.0;

	/** Duração do hold em andamento (s); 0 = sem hold. */
	UPROPERTY(Replicated)
	float HoldDuration = 0.f;

	bool InteractWithAuthority();
	bool BeginInteractWithAuthority();
	void TickHoldWithAuthority();
	void CancelHold();

	/** Trace fresco + cooldown + bloqueio. Devolve o alvo aceito ou nullptr. Só autoridade. */
	AActor* AcquireTargetWithAuthority();

	/** Chama OnInteract e dispara OnInteractionCompleted (servidor + dono remoto). */
	bool CompleteInteraction(AActor* Target);

	/** Emite foco/OnTargetChanged quando CurrentTarget difere do último alvo anunciado. */
	void SyncFocus();

	/** Relógio comum a servidor e cliente (GameState); cai no tempo local sem GameState. */
	double GetServerTimeSeconds() const;

	bool PerformTrace(FHitResult& OutHit) const;

private:
	/** Alvo do hold em andamento (só servidor). */
	TWeakObjectPtr<AActor> HoldTarget;

	/** Último alvo anunciado em OnFocusChanged/OnTargetChanged; weak porque pode ser destruído focado. */
	TWeakObjectPtr<AActor> FocusedTarget;

	/** OnFocusChanged(true) foi entregue a FocusedTarget nesta máquina (o "false" precisa casar com ele). */
	bool bFocusAnnounced = false;

	/** Próximo instante (relógio do servidor) em que uma tentativa é aceita (só servidor). */
	double NextInteractionTime = 0.0;
};
