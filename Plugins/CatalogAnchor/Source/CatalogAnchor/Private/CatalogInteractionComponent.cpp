// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogInteractionComponent.h"
#include "CatalogAnchor.h"
#include "CatalogAnchorSettings.h"
#include "ICatalogInteractable.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "CollisionQueryParams.h"

// Nomeado (não anônimo): em unity build vários .cpp dividem a mesma unidade de tradução.
namespace CatalogInteractionPrivate
{
	/** NaN, 0 ou negativo = instantâneo (valor vindo de Blueprint não é confiável). */
	static bool IsHoldDuration(float Duration)
	{
		return !FMath::IsNaN(Duration) && Duration > 0.f;
	}

	/**
	 * Jogador humano nesta máquina (standalone, host listen ou cliente dono) controla o dono, seja ele
	 * Pawn, Controller, PlayerState ou ator com SetOwner(PC). AI e pawn sem controller ficam de fora.
	 */
	static bool IsOwnedByLocalPlayer(AActor* Owner)
	{
		const APlayerController* PlayerController = Cast<APlayerController>(UCatalogInteractionComponent::FindOwningController(Owner));
		return PlayerController && PlayerController->IsLocalController();
	}
}

UCatalogInteractionComponent::UCatalogInteractionComponent()
	: TraceDistance(GetDefault<UCatalogAnchorSettings>()->DefaultTraceDistance)
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UCatalogInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UCatalogInteractionComponent, CurrentTarget, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UCatalogInteractionComponent, HoldStartServerTime, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UCatalogInteractionComponent, HoldDuration, COND_OwnerOnly);
}

void UCatalogInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		// Durante o hold o trace roda mesmo sem bAutoTrace: desviar o olhar precisa cancelar.
		if (bAutoTrace || IsInteracting())
		{
			RefreshTarget();
		}
		if (IsInteracting())
		{
			TickHoldWithAuthority();
		}
	}

	// Também no cliente: alvo destruído some sem OnRep, e o foco precisa ser limpo.
	SyncFocus();
}

float UCatalogInteractionComponent::ComputeHoldProgress(double StartTime, double Now, float Duration)
{
	if (!CatalogInteractionPrivate::IsHoldDuration(Duration))
	{
		return 1.f;
	}
	const double Alpha = (Now - StartTime) / static_cast<double>(Duration);
	if (!FMath::IsFinite(Alpha))
	{
		return 0.f;
	}
	return static_cast<float>(FMath::Clamp(Alpha, 0.0, 1.0));
}

double UCatalogInteractionComponent::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	// No servidor é o próprio tempo do mundo; no cliente vem corrigido pelo GameState.
	const AGameStateBase* GameStateBase = World->GetGameState();
	return GameStateBase ? GameStateBase->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

AController* UCatalogInteractionComponent::FindOwningController(AActor* Owner)
{
	// SetOwner recusa ciclos, então a cadeia termina.
	for (AActor* It = Owner; It; It = It->GetOwner())
	{
		if (const APawn* Pawn = Cast<APawn>(It))
		{
			// Pawn decide pelo controller atual: sem possessão (morte, veículo) ninguém olha por ele.
			return Pawn->GetController();
		}
		if (AController* Controller = Cast<AController>(It))
		{
			return Controller;
		}
	}
	return nullptr;
}

bool UCatalogInteractionComponent::ComputeTraceSegment(const FVector& ViewLocation, const FVector& ViewDirection,
	const FVector& ReachOrigin, float Reach, FVector& OutStart, FVector& OutEnd)
{
	const FVector Direction = ViewDirection.GetSafeNormal();
	if (Direction.IsZero() || FMath::IsNaN(Reach) || Reach <= 0.f
		|| ViewLocation.ContainsNaN() || ReachOrigin.ContainsNaN())
	{
		return false;
	}

	// Raio View + t*Direction contra a esfera (ReachOrigin, Reach): t^2 + 2Bt + C = 0.
	const FVector ToView = ViewLocation - ReachOrigin;
	const double B = FVector::DotProduct(ToView, Direction);
	const double C = ToView.SizeSquared() - static_cast<double>(Reach) * Reach;
	const double Discriminant = B * B - C;
	if (Discriminant < 0.0)
	{
		return false;
	}

	// Começa no ponto mais próximo de ReachOrigin: o que está entre a câmera e o pawn não vira alvo.
	const double StartT = FMath::Max(0.0, -B);
	const double EndT = -B + FMath::Sqrt(Discriminant);
	if (EndT <= StartT)
	{
		return false;
	}
	OutStart = ViewLocation + Direction * StartT;
	OutEnd = ViewLocation + Direction * EndT;
	return true;
}

bool UCatalogInteractionComponent::PerformTrace(FHitResult& OutHit) const
{
	AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CatalogInteractTrace), false, Owner);

	// Sem controller (ator solto, pawn sem possessão): mira e alcance saem do próprio dono.
	FVector ViewLocation = Owner->GetActorLocation();
	FRotator ViewRotation = Owner->GetActorRotation();
	FVector ReachOrigin = ViewLocation;
	if (const AController* Controller = FindOwningController(Owner))
	{
		// Mira pela câmera; alcance a partir do pawn, para a câmera de 3ª pessoa não estender nem
		// inverter o alcance. No servidor isso usa o pawn autoritativo: é a revalidação.
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		ReachOrigin = ViewLocation;
		if (const APawn* Pawn = Controller->GetPawn<APawn>())
		{
			ReachOrigin = Pawn->GetPawnViewLocation();
			// Dono Controller/PlayerState: o pawn não é o ignorado padrão e a mira sairia da cápsula.
			if (Pawn != Owner)
			{
				Params.AddIgnoredActor(Pawn);
			}
		}
	}

	FVector Start;
	FVector End;
	if (!ComputeTraceSegment(ViewLocation, ViewRotation.Vector(), ReachOrigin, TraceDistance, Start, End))
	{
		return false;
	}

	if (TraceRadius > UE_KINDA_SMALL_NUMBER)
	{
		return World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity,
			TraceChannel, FCollisionShape::MakeSphere(TraceRadius), Params);
	}
	return World->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, Params);
}

AActor* UCatalogInteractionComponent::RefreshTarget()
{
	// CurrentTarget é replicado: escrevê-lo no cliente criaria um valor que o servidor
	// só sobrescreve quando o dele mudar. Cliente apenas lê.
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return CurrentTarget.Get();
	}

	FHitResult Hit;
	AActor* NewTarget = nullptr;
	if (PerformTrace(Hit))
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			if (HitActor->GetClass()->ImplementsInterface(UCatalogInteractable::StaticClass()))
			{
				NewTarget = HitActor;
			}
		}
	}

	CurrentTarget = NewTarget;
	SyncFocus();
	return NewTarget;
}

void UCatalogInteractionComponent::SyncFocus()
{
	AActor* Owner = GetOwner();
	AActor* NewTarget = CurrentTarget.Get();
	// Foco é cosmético: só na máquina do jogador local (host ou cliente dono). Reavaliado todo tick:
	// no unpossess (morte com corpo, veículo) o OwnerOnly para de replicar e o highlight ficaria preso.
	const bool bAnnounce = NewTarget && CatalogInteractionPrivate::IsOwnedByLocalPlayer(Owner);
	// IsStale: o alvo anunciado foi destruído (ex.: pickup coletado) sem troca replicada.
	const bool bTargetChanged = FocusedTarget.Get() != NewTarget || FocusedTarget.IsStale();
	if (!bTargetChanged && bAnnounce == bFocusAnnounced)
	{
		return;
	}

	AActor* PreviousTarget = FocusedTarget.Get();
	const bool bPreviousAnnounced = bFocusAnnounced;
	FocusedTarget = NewTarget;
	bFocusAnnounced = bAnnounce;

	// Desliga pelo que foi anunciado, não pelo controller de agora: na morte o pawn já perdeu o controller.
	if (PreviousTarget && bPreviousAnnounced)
	{
		ICatalogInteractable::Execute_OnFocusChanged(PreviousTarget, false, Owner);
	}
	if (bFocusAnnounced)
	{
		ICatalogInteractable::Execute_OnFocusChanged(NewTarget, true, Owner);
	}
	// Só troca de alvo vira evento; mudança apenas de anúncio (possess/unpossess) não.
	if (bTargetChanged)
	{
		OnTargetChanged.Broadcast(NewTarget);
	}
}

void UCatalogInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Dono destruído com algo em foco (morte/respawn): sem isto highlight e prompt ficam presos no alvo.
	CurrentTarget.Reset();
	SyncFocus();
	Super::EndPlay(EndPlayReason);
}

void UCatalogInteractionComponent::OnRep_CurrentTarget()
{
	SyncFocus();
}

bool UCatalogInteractionComponent::TryInteract()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	if (!Owner->HasAuthority())
	{
		// O servidor refaz o trace e decide; nunca confiamos em alvo vindo do cliente.
		ServerTryInteract();
		return CurrentTarget.IsValid();
	}
	return InteractWithAuthority();
}

void UCatalogInteractionComponent::ServerTryInteract_Implementation()
{
	InteractWithAuthority();
}

bool UCatalogInteractionComponent::BeginInteract()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	if (!Owner->HasAuthority())
	{
		ServerBeginInteract();
		return CurrentTarget.IsValid();
	}
	return BeginInteractWithAuthority();
}

void UCatalogInteractionComponent::ServerBeginInteract_Implementation()
{
	BeginInteractWithAuthority();
}

void UCatalogInteractionComponent::EndInteract()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Owner->HasAuthority())
	{
		ServerEndInteract();
		return;
	}
	CancelHold();
}

void UCatalogInteractionComponent::ServerEndInteract_Implementation()
{
	CancelHold();
}

AActor* UCatalogInteractionComponent::AcquireTargetWithAuthority()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	const double Now = GetServerTimeSeconds();
	if (Now < NextInteractionTime)
	{
		UE_LOG(LogCatalogAnchor, Verbose, TEXT("%s: interacao ignorada (cooldown)."), *GetNameSafe(Owner));
		return nullptr;
	}

	// Trace fresco: com bAutoTrace desligado o alvo em cache pode estar velho.
	AActor* Target = RefreshTarget();
	if (!Target)
	{
		return nullptr;
	}

	NextInteractionTime = Now + FMath::Max(0.f, InteractionCooldownSeconds);
	if (ICatalogInteractable::Execute_IsInteractionBlocked(Target, Owner))
	{
		UE_LOG(LogCatalogAnchor, Verbose, TEXT("%s: %s esta bloqueado."), *GetNameSafe(Owner), *GetNameSafe(Target));
		return nullptr;
	}
	return Target;
}

bool UCatalogInteractionComponent::InteractWithAuthority()
{
	AActor* Target = AcquireTargetWithAuthority();
	if (!Target)
	{
		return false;
	}
	// TryInteract não pode pular o hold: senão um cliente abriria na hora o que exige segurar.
	if (CatalogInteractionPrivate::IsHoldDuration(ICatalogInteractable::Execute_GetInteractionDuration(Target)))
	{
		UE_LOG(LogCatalogAnchor, Verbose, TEXT("%s: %s exige hold; use BeginInteract."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Target));
		return false;
	}
	return CompleteInteraction(Target);
}

bool UCatalogInteractionComponent::BeginInteractWithAuthority()
{
	// Repetição do botão não reinicia o hold em andamento.
	if (IsInteracting())
	{
		return true;
	}

	AActor* Target = AcquireTargetWithAuthority();
	if (!Target)
	{
		return false;
	}

	const float Duration = ICatalogInteractable::Execute_GetInteractionDuration(Target);
	if (!CatalogInteractionPrivate::IsHoldDuration(Duration))
	{
		return CompleteInteraction(Target);
	}

	HoldTarget = Target;
	HoldStartServerTime = GetServerTimeSeconds();
	HoldDuration = Duration;
	return true;
}

void UCatalogInteractionComponent::TickHoldWithAuthority()
{
	AActor* Target = HoldTarget.Get();
	if (!Target || Target != CurrentTarget.Get()
		|| ICatalogInteractable::Execute_IsInteractionBlocked(Target, GetOwner()))
	{
		CancelHold();
		return;
	}

	if (ComputeHoldProgress(HoldStartServerTime, GetServerTimeSeconds(), HoldDuration) >= 1.f)
	{
		// Limpa antes: OnInteract pode reentrar (ex.: Blueprint chamando BeginInteract).
		CancelHold();
		CompleteInteraction(Target);
	}
}

void UCatalogInteractionComponent::CancelHold()
{
	HoldTarget.Reset();
	HoldStartServerTime = 0.0;
	HoldDuration = 0.f;
}

bool UCatalogInteractionComponent::CompleteInteraction(AActor* Target)
{
	AActor* Owner = GetOwner();
	const bool bConsumed = ICatalogInteractable::Execute_OnInteract(Target, Owner);
	OnInteractionCompleted.Broadcast(Target, bConsumed);

	// Só dono com conexão remota: host local e AI já ouviram o broadcast acima
	// (Client RPC em ator sem dono remoto rodaria de novo aqui no servidor).
	if (Owner && Owner->GetNetConnection() != nullptr)
	{
		ClientInteractionCompleted(Target, bConsumed);
	}
	return bConsumed;
}

void UCatalogInteractionComponent::ClientInteractionCompleted_Implementation(AActor* Target, bool bConsumed)
{
	// Target pode chegar nulo se OnInteract destruiu o ator (ex.: pickup coletado).
	OnInteractionCompleted.Broadcast(Target, bConsumed);
}

float UCatalogInteractionComponent::GetInteractionProgress() const
{
	if (!IsInteracting())
	{
		return 0.f;
	}
	return ComputeHoldProgress(HoldStartServerTime, GetServerTimeSeconds(), HoldDuration);
}

bool UCatalogInteractionComponent::IsInteracting() const
{
	return HoldDuration > 0.f;
}

FText UCatalogInteractionComponent::GetCurrentPrompt() const
{
	if (!CurrentTarget.IsValid())
	{
		return FText::GetEmpty();
	}
	return ICatalogInteractable::Execute_GetInteractionPrompt(CurrentTarget.Get());
}
