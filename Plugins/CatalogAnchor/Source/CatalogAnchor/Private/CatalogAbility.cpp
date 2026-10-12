// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogAbility.h"
#include "CatalogAnchorSettings.h"
#include "CatalogHealthComponent.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Templates/UnrealTemplate.h"

namespace CatalogAbilityImpl
{
	// Mesmo critério de UCatalogGameplaySubsystem::TriggerDash: só root simulando física aceita impulso.
	static UPrimitiveComponent* GetSimulatingRoot(const AActor* Avatar)
	{
		UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Avatar->GetRootComponent());
		return (Root && Root->IsSimulatingPhysics()) ? Root : nullptr;
	}
}

// --- UCatalogAbility ---

bool UCatalogAbility::CanActivate(AActor* Avatar) const
{
	if (!IsValid(Avatar))
	{
		return false;
	}
	// Morto não age até o respawn (ResetHealth). Health replica: o cliente estima igual ao servidor.
	const UCatalogHealthComponent* Health = Avatar->FindComponentByClass<UCatalogHealthComponent>();
	if (Health && Health->IsDead())
	{
		return false;
	}
	TGuardValue<TWeakObjectPtr<AActor>> AvatarScope(CurrentAvatar, TWeakObjectPtr<AActor>(Avatar));
	return K2_CanActivate(Avatar);
}

bool UCatalogAbility::Activate(AActor* Avatar)
{
	if (!IsValid(Avatar) || !Avatar->HasAuthority())
	{
		return false;
	}
	TGuardValue<TWeakObjectPtr<AActor>> AvatarScope(CurrentAvatar, TWeakObjectPtr<AActor>(Avatar));
	K2_OnActivated(Avatar);
	return true;
}

void UCatalogAbility::PlayCosmetics(AActor* Avatar)
{
	if (!IsValid(Avatar))
	{
		return;
	}
	TGuardValue<TWeakObjectPtr<AActor>> AvatarScope(CurrentAvatar, TWeakObjectPtr<AActor>(Avatar));
	K2_OnCosmetics(Avatar);
}

UWorld* UCatalogAbility::GetWorld() const
{
	// Sem Super de propósito: assim o editor considera que a classe "tem mundo" (ImplementsGetWorld).
	const AActor* Avatar = CurrentAvatar.Get();
	return Avatar ? Avatar->GetWorld() : nullptr;
}

bool UCatalogAbility::K2_CanActivate_Implementation(AActor* Avatar) const
{
	return true;
}

// --- UCatalogAbility_Dash ---

UCatalogAbility_Dash::UCatalogAbility_Dash()
	: Strength(GetDefault<UCatalogAnchorSettings>()->DefaultDashStrength)
{
}

bool UCatalogAbility_Dash::CanActivate(AActor* Avatar) const
{
	if (!IsValid(Avatar) || !FMath::IsFinite(Strength))
	{
		return false;
	}
	if (!Cast<ACharacter>(Avatar) && !CatalogAbilityImpl::GetSimulatingRoot(Avatar))
	{
		return false;
	}
	return Super::CanActivate(Avatar);
}

bool UCatalogAbility_Dash::Activate(AActor* Avatar)
{
	// Movimento tem de nascer na autoridade: feito só no cliente, a correção do servidor desfaz.
	if (!IsValid(Avatar) || !Avatar->HasAuthority() || !FMath::IsFinite(Strength))
	{
		return false;
	}

	const FVector DashVelocity = Avatar->GetActorForwardVector() * Strength;
	if (ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		Character->LaunchCharacter(DashVelocity, /*bXYOverride*/ true, /*bZOverride*/ false);
	}
	else if (UPrimitiveComponent* Root = CatalogAbilityImpl::GetSimulatingRoot(Avatar))
	{
		Root->AddImpulse(DashVelocity, NAME_None, /*bVelChange*/ true);
	}
	else
	{
		return false;
	}
	return Super::Activate(Avatar);
}

// --- UCatalogAbility_MeleeSweep ---

bool UCatalogAbility_MeleeSweep::Activate(AActor* Avatar)
{
	if (!IsValid(Avatar) || !Avatar->HasAuthority() || !FMath::IsFinite(Range) || !FMath::IsFinite(Radius) || !FMath::IsFinite(Damage))
	{
		return false;
	}
	UWorld* World = Avatar->GetWorld();
	if (!World)
	{
		return false;
	}

	// A esfera começa encostada no centro do avatar: nada atrás dele é atingido.
	const FVector Origin = Avatar->GetActorLocation();
	const FVector Forward = Avatar->GetActorForwardVector();
	const FVector Start = Origin + Forward * Radius;
	const FVector End = Origin + Forward * FMath::Max(Range, Radius);

	// Por tipo de objeto Pawn, não por canal: varredura multi por canal para no primeiro bloqueio,
	// e cápsulas bloqueiam o canal Pawn — só o primeiro inimigo seria atingido.
	TArray<FHitResult> Hits;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(CatalogMeleeSweep), /*bTraceComplex*/ false, Avatar);
	World->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);

	const APawn* AvatarPawn = Cast<APawn>(Avatar);
	AController* InstigatorController = AvatarPawn ? AvatarPawn->GetController() : Avatar->GetInstigatorController();

	// Cápsula e malha do mesmo personagem geram hits separados: dano uma vez por ator.
	TSet<AActor*> Damaged;
	for (const FHitResult& Hit : Hits)
	{
		// IsValid: um dano anterior desta varredura pode ter destruído o alvo (o hit guarda o ponteiro).
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target == Avatar)
		{
			continue;
		}
		bool bAlreadyDamaged = false;
		Damaged.Add(Target, &bAlreadyDamaged);
		if (!bAlreadyDamaged)
		{
			UGameplayStatics::ApplyDamage(Target, Damage, InstigatorController, Avatar, DamageTypeClass);
		}
	}

	// Golpe no ar também conta como ativação (gasta o cooldown).
	return Super::Activate(Avatar);
}

void UCatalogAbility_MeleeSweep::PlayCosmetics(AActor* Avatar)
{
	if (ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		if (UAnimMontage* LoadedMontage = Montage.LoadSynchronous())
		{
			Character->PlayAnimMontage(LoadedMontage);
		}
	}
	Super::PlayCosmetics(Avatar);
}
