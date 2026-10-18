// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogAbilityComponent.h"
#include "CatalogAbilityDefinition.h"
#include "CatalogAnchor.h"
#include "CatalogHealthComponent.h"
#include "CatalogInteractionComponent.h"
#include "CatalogInventoryComponent.h"
#include "CatalogItemDefinition.h"
#include "CatalogSaveSubsystem.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/IConsoleManager.h"
#include "ICatalogInteractable.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"

// Ferramenta de diagnóstico (testes multiplayer headless): sem comando de console em Shipping.
#if !UE_BUILD_SHIPPING

namespace CatalogDebugCommandsPrivate
{
	const TCHAR* NetModeText(ENetMode Mode)
	{
		switch (Mode)
		{
		case NM_Standalone: return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer: return TEXT("ListenServer");
		case NM_Client: return TEXT("Client");
		default: return TEXT("?");
		}
	}

	/** catalog.Debug.Dump: uma linha por ator com vida/inventário, o que ESTA máquina enxerga (compare servidor x clientes). */
	void DumpWorld(UWorld* World, const FString& Label = FString())
	{
		if (!World)
		{
			return;
		}
		const TCHAR* const NetModeName = NetModeText(World->GetNetMode());
		int32 Actors = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const AActor* Actor = *It;
			const UCatalogHealthComponent* Health = Actor->FindComponentByClass<UCatalogHealthComponent>();
			const UCatalogInventoryComponent* Inventory = Actor->FindComponentByClass<UCatalogInventoryComponent>();
			if (!Health && !Inventory)
			{
				continue;
			}
			++Actors;
			const APawn* Pawn = Cast<APawn>(Actor);
			const bool bLocally = Pawn && Pawn->IsLocallyControlled();
			int32 Items = 0;
			FString Stacks;
			if (Inventory)
			{
				for (const FCatalogInventoryStack& Stack : Inventory->GetStacks())
				{
					Items += Stack.Quantity;
					Stacks += FString::Printf(TEXT("%s x%d "), Stack.Item ? *Stack.Item->GetName() : TEXT("?"), Stack.Quantity);
				}
			}
			// Extras de rede/foco/habilidade (MP-03/04/06): alvo e foco anunciado só existem para o dono; cooldown idem.
			const FVector Loc = Actor->GetActorLocation();
			FString Extra = FString::Printf(TEXT(" Loc=%.0f,%.0f,%.0f"), Loc.X, Loc.Y, Loc.Z);
			if (const UCatalogInteractionComponent* Interaction = Actor->FindComponentByClass<UCatalogInteractionComponent>())
			{
				Extra += FString::Printf(TEXT(" Target=%s Focused=%d Holding=%d"), *GetNameSafe(Interaction->CurrentTarget.Get()),
					Interaction->IsFocusAnnounced(), Interaction->IsInteracting());
			}
			if (const UCatalogAbilityComponent* Abilities = Actor->FindComponentByClass<UCatalogAbilityComponent>())
			{
				FString Cooldowns;
				for (const UCatalogAbilityDefinition* Definition : Abilities->GrantedAbilities)
				{
					if (Definition)
					{
						Cooldowns += FString::Printf(TEXT("%s:%.2f "), *Definition->AbilityTag.ToString(), Abilities->GetCooldownRemaining(Definition->AbilityTag));
					}
				}
				Extra += FString::Printf(TEXT(" Cd=[%s]"), *Cooldowns.TrimEnd());
			}
			UE_LOG(LogCatalogAnchor, Display, TEXT("[Dump] NetMode=%s Actor=%s Health=%.0f/%.0f Dead=%d Items=%d Locally=%d Authority=%d Stacks=[%s]%s Snap=%s"),
				NetModeName, *Actor->GetName(), Health ? Health->GetHealth() : -1.f, Health ? Health->MaxHealth : -1.f,
				Health ? Health->IsDead() : 0, Items, bLocally, Actor->HasAuthority(), *Stacks.TrimEnd(), *Extra, *Label);
		}
		// Uma linha por interagível: prompt/bloqueio/duração como o jogador local os lê (instigador = pawn local).
		AActor* const Viewer = UGameplayStatics::GetPlayerPawn(World, 0);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const AActor* Actor = *It;
			if (Actor->GetClass()->ImplementsInterface(UCatalogInteractable::StaticClass()))
			{
				UE_LOG(LogCatalogAnchor, Display, TEXT("[Dump] NetMode=%s Interactable=%s Prompt=[%s] Blocked=%d Duration=%.1f Snap=%s"), NetModeName,
					*Actor->GetName(), *ICatalogInteractable::Execute_GetInteractionPrompt(Actor).ToString(),
					ICatalogInteractable::Execute_IsInteractionBlocked(Actor, Viewer), ICatalogInteractable::Execute_GetInteractionDuration(Actor), *Label);
			}
		}
		const UGameInstance* GameInstance = World->GetGameInstance();
		const UCatalogSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UCatalogSaveSubsystem>() : nullptr;
		UE_LOG(LogCatalogAnchor, Display, TEXT("[Dump] NetMode=%s Atores=%d SaveParticipants=%d Snap=%s"), NetModeName, Actors,
			Save ? Save->GetParticipantCount() : -1, *Label);
	}

	/** Pawn do N-ésimo jogador (0 = primeiro) que tem controller; só onde há autoridade. */
	APawn* FindPlayerPawn(UWorld* World, int32 Index, FOutputDevice& Ar)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			Ar.Log(TEXT("catalog.Debug: rode no servidor (listen/dedicado/standalone)."));
			return nullptr;
		}
		int32 Current = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (PC && PC->GetPawn() && Current++ == Index)
			{
				return PC->GetPawn();
			}
		}
		Ar.Logf(TEXT("catalog.Debug: jogador %d não encontrado."), Index);
		return nullptr;
	}

	/** Índice do jogador no último argumento numérico (padrão 0). */
	int32 PlayerIndexArg(const TArray<FString>& Args, int32 Position)
	{
		return Args.IsValidIndex(Position) ? FCString::Atoi(*Args[Position]) : 0;
	}

	/** catalog.Debug.Hurt <dano> [jogador]: ApplyDamage com autoridade no pawn do jogador. */
	void Hurt(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		APawn* Pawn = FindPlayerPawn(World, PlayerIndexArg(Args, 1), Ar);
		if (Pawn)
		{
			const float Damage = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 25.f;
			const float Applied = UGameplayStatics::ApplyDamage(Pawn, Damage, nullptr, nullptr, UDamageType::StaticClass());
			UE_LOG(LogCatalogAnchor, Display, TEXT("[Debug] Hurt %s dano=%.0f aplicado=%.0f"), *Pawn->GetName(), Damage, Applied);
		}
	}

	/** catalog.Debug.GiveItem <nome contido em Item_*> <qtd> [jogador]: AddItem no inventário do jogador. */
	void GiveItem(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		APawn* Pawn = FindPlayerPawn(World, PlayerIndexArg(Args, 2), Ar);
		if (!Pawn || Args.Num() < 1)
		{
			return;
		}
		const UCatalogItemDefinition* Found = nullptr;
		for (TObjectIterator<UCatalogItemDefinition> It; It && !Found; ++It)
		{
			Found = It->GetName().Contains(Args[0]) ? *It : nullptr;
		}
		UCatalogInventoryComponent* Inventory = Pawn->FindComponentByClass<UCatalogInventoryComponent>();
		if (!Found || !Inventory)
		{
			UE_LOG(LogCatalogAnchor, Display, TEXT("[Debug] GiveItem: item '%s' ou inventário não encontrado."), *Args[0]);
			return;
		}
		const int32 Quantity = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1;
		const int32 Added = Inventory->AddItem(Found, Quantity);
		UE_LOG(LogCatalogAnchor, Display, TEXT("[Debug] GiveItem %s %s x%d adicionado=%d"), *Pawn->GetName(), *Found->GetName(), Quantity, Added);
	}

	/** catalog.Debug.Respawn [jogador]: ResetHealth (vida cheia) no pawn. */
	void Respawn(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (const APawn* Pawn = FindPlayerPawn(World, PlayerIndexArg(Args, 0), Ar))
		{
			if (UCatalogHealthComponent* Health = Pawn->FindComponentByClass<UCatalogHealthComponent>())
			{
				Health->ResetHealth();
				UE_LOG(LogCatalogAnchor, Display, TEXT("[Debug] Respawn %s vida=%.0f"), *Pawn->GetName(), Health->GetHealth());
			}
		}
	}

	/** Ator mais próximo do pawn cujo nome contém Filter (fora o próprio pawn); nullptr se não houver. */
	AActor* FindActorByName(UWorld* World, const APawn* Pawn, const FString& Filter, bool bInteractableOnly)
	{
		AActor* Target = nullptr;
		float BestDistance = TNumericLimits<float>::Max();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Candidate = *It;
			if (Candidate == Pawn || !Candidate->GetName().Contains(Filter)
				|| (bInteractableOnly && !Candidate->GetClass()->ImplementsInterface(UCatalogInteractable::StaticClass())))
			{
				continue;
			}
			const float Distance = FVector::Dist(Candidate->GetActorLocation(), Pawn->GetActorLocation());
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Target = Candidate;
			}
		}
		return Target;
	}

	/**
	 * Leva o pawn a 150 cm do alvo (de -X, de pé no mesmo Z) e acha o pitch em que o raio da câmera (braço + SocketOffset,
	 * é de lá que o trace de interação mira) passa pelo centro do alvo. ClientSetRotation força a mira no cliente dono.
	 */
	void AimAt(APawn* Pawn, const AActor* Target)
	{
		const FVector Center = Target->GetComponentsBoundingBox().GetCenter();
		const USpringArmComponent* Arm = Pawn->FindComponentByClass<USpringArmComponent>();
		const FVector Socket = Arm ? Arm->SocketOffset : FVector(0.f, 0.f, Pawn->BaseEyeHeight);
		const float ArmLength = Arm ? Arm->TargetArmLength : 0.f;
		const FVector Destination(Center.X - 150.f, Center.Y - Socket.Y, Pawn->GetActorLocation().Z);
		Pawn->SetActorLocationAndRotation(Destination, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
		FRotator Aim = FRotator::ZeroRotator;
		float BestError = TNumericLimits<float>::Max();
		for (float Pitch = -85.f; Pitch <= 85.f; Pitch += 0.25f)
		{
			const FRotationMatrix Frame(FRotator(Pitch, 0.f, 0.f));
			const FVector Camera = Destination + Frame.TransformVector(FVector(-ArmLength, Socket.Y, Socket.Z));
			const float Error = FVector::CrossProduct(Center - Camera, Frame.GetUnitAxis(EAxis::X)).Size();
			if (Error < BestError)
			{
				BestError = Error;
				Aim = FRotator(Pitch, 0.f, 0.f);
			}
		}
		if (APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
		{
			Controller->SetControlRotation(Aim);
			Controller->ClientSetRotation(Aim);
		}
		UE_LOG(LogCatalogAnchor, Display, TEXT("[Debug] Aim %s alvo=%s pitch=%.1f erro=%.1f"), *Pawn->GetName(), *Target->GetName(), Aim.Pitch, BestError);
	}

	/** catalog.Debug.Aim <parte do nome> [jogador]: AimAt no ator mais próximo (qualquer ator, ex.: o boneco) (servidor). */
	void Aim(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		APawn* Pawn = FindPlayerPawn(World, PlayerIndexArg(Args, 1), Ar);
		if (!Pawn || Args.Num() < 1)
		{
			return;
		}
		if (const AActor* Target = FindActorByName(World, Pawn, Args[0], false))
		{
			AimAt(Pawn, Target);
		}
		else
		{
			UE_LOG(LogCatalogAnchor, Display, TEXT("[Debug] Aim: nenhum ator '%s'."), *Args[0]);
		}
	}

	/** catalog.Debug.Hold Begin|End: BeginInteract/EndInteract do pawn LOCAL (cliente: Server RPC, o caminho do input real). */
	void Hold(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		UCatalogInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UCatalogInteractionComponent>() : nullptr;
		if (!Interaction || Args.Num() < 1)
		{
			Ar.Log(TEXT("uso: catalog.Debug.Hold Begin|End (precisa de pawn local com interação)"));
			return;
		}
		const bool bBegin = Args[0].Equals(TEXT("Begin"), ESearchCase::IgnoreCase);
		bool bOk = false;
		if (bBegin)
		{
			bOk = Interaction->BeginInteract();
		}
		else
		{
			Interaction->EndInteract();
		}
		UE_LOG(LogCatalogAnchor, Display, TEXT("[Debug] Hold %s NetMode=%s pawn=%s alvo=%s resultado=%d"), bBegin ? TEXT("Begin") : TEXT("End"),
			NetModeText(World->GetNetMode()), *Pawn->GetName(), *GetNameSafe(Interaction->CurrentTarget.Get()), bOk);
	}

	/** catalog.Debug.Ability <tag ou sufixo, ex.: Dash>: TryActivateAbility do pawn LOCAL (cliente: Server RPC). */
	void Ability(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
		UCatalogAbilityComponent* Abilities = Pawn ? Pawn->FindComponentByClass<UCatalogAbilityComponent>() : nullptr;
		if (!Abilities || Args.Num() < 1)
		{
			Ar.Log(TEXT("uso: catalog.Debug.Ability <tag> (precisa de pawn local com habilidades)"));
			return;
		}
		const FString TagName = Args[0].Contains(TEXT(".")) ? Args[0] : TEXT("Catalog.Ability.") + Args[0];
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
		const bool bOk = Abilities->TryActivateAbility(Tag);
		UE_LOG(LogCatalogAnchor, Display, TEXT("[Debug] Ability %s NetMode=%s pawn=%s resultado=%d cooldown=%.2f"), *TagName,
			NetModeText(World->GetNetMode()), *Pawn->GetName(), bOk, Abilities->GetCooldownRemaining(Tag));
	}

	/**
	 * catalog.Debug.Interact <parte do nome do ator> [hold] [jogador]: leva o pawn a 150 cm do interagível mais próximo
	 * (nome contém o filtro), mira (AimAt) e interage (tap = TryInteract; hold = BeginInteract, completa no tick).
	 */
	void Interact(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		const bool bHold = Args.ContainsByPredicate([](const FString& A) { return A.Equals(TEXT("hold"), ESearchCase::IgnoreCase); });
		APawn* Pawn = FindPlayerPawn(World, PlayerIndexArg(Args, bHold ? 2 : 1), Ar);
		UCatalogInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UCatalogInteractionComponent>() : nullptr;
		if (!Interaction)
		{
			return;
		}
		const FString Filter = Args.Num() > 0 ? Args[0] : FString();
		AActor* Target = FindActorByName(World, Pawn, Filter, true);
		if (!Target)
		{
			UE_LOG(LogCatalogAnchor, Display, TEXT("[Debug] Interact: nenhum interagível '%s'."), *Filter);
			return;
		}
		AimAt(Pawn, Target);
		const AActor* Focus = Interaction->RefreshTarget();
		const bool bOk = bHold ? Interaction->BeginInteract() : Interaction->TryInteract();
		UE_LOG(LogCatalogAnchor, Display, TEXT("[Debug] Interact %s alvo=%s foco=%s modo=%s resultado=%d"), *Pawn->GetName(), *Target->GetName(),
			*GetNameSafe(Focus), bHold ? TEXT("hold") : TEXT("tap"), bOk);
	}

	/** catalog.Debug.Schedule <segundos ou Unix UTC> <comando...>: roda o console command depois do atraso no mundo de jogo atual. */
	void Schedule(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 2)
		{
			Ar.Log(TEXT("uso: catalog.Debug.Schedule <segundos> <comando...>"));
			return;
		}
		// Segundos de atraso; valor > 1e9 = instante absoluto (Unix, UTC): processos de uma mesma máquina se alinham pelo relógio.
		double Seconds = FCString::Atod(*Args[0]);
		if (Seconds > 1e9)
		{
			Seconds -= (FDateTime::UtcNow() - FDateTime::FromUnixTimestamp(0)).GetTotalSeconds();
		}
		const float Delay = FMath::Max(0.1, Seconds);
		FString Command = FString::Join(TArrayView<const FString>(Args).Slice(1, Args.Num() - 1), TEXT(" "));
		FTSTicker::GetCoreTicker().AddTicker(TEXT("CatalogDebugSchedule"), Delay, [Command](float)
		{
			if (GEngine)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					if (Context.World() && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
					{
						GEngine->Exec(Context.World(), *Command);
						break;
					}
				}
			}
			return false;
		});
		Ar.Logf(TEXT("catalog.Debug.Schedule: '%s' em %.1f s."), *Command, Delay);
	}

	/** catalog.Debug.Dump [vezes] [intervalo s]: sem argumentos roda agora; com argumentos repete em timer (testes headless, sem input). */
	void Dump(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 2)
		{
			DumpWorld(World, Args.Num() == 1 ? Args[0] : FString());
			return;
		}
		const int32 Count = FMath::Clamp(FCString::Atoi(*Args[0]), 1, 1000);
		const float Interval = FMath::Max(0.1f, FCString::Atof(*Args[1]));
		FTSTicker::GetCoreTicker().AddTicker(TEXT("CatalogDebugDump"), Interval, [Remaining = Count](float) mutable
		{
			// O mundo muda quando o cliente viaja para o mapa do servidor: sempre o(s) mundo(s) de jogo atuais.
			if (GEngine)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
					{
						DumpWorld(Context.World());
					}
				}
			}
			return --Remaining > 0;
		});
		Ar.Logf(TEXT("catalog.Debug.Dump: %d dumps a cada %.1f s."), Count, Interval);
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogDebugDumpCommand(
	TEXT("catalog.Debug.Dump"), TEXT("Loga em LogCatalogAnchor vida, inventário e nº de participantes de save que esta máquina enxerga."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogDebugCommandsPrivate::Dump));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogDebugHurtCommand(
	TEXT("catalog.Debug.Hurt"), TEXT("catalog.Debug.Hurt <dano> [jogador]: ApplyDamage com autoridade no pawn do jogador (servidor)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogDebugCommandsPrivate::Hurt));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogDebugGiveItemCommand(
	TEXT("catalog.Debug.GiveItem"), TEXT("catalog.Debug.GiveItem <nome do item> <qtd> [jogador]: AddItem com autoridade (servidor)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogDebugCommandsPrivate::GiveItem));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogDebugRespawnCommand(
	TEXT("catalog.Debug.Respawn"), TEXT("catalog.Debug.Respawn [jogador]: ResetHealth no pawn (servidor)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogDebugCommandsPrivate::Respawn));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogDebugInteractCommand(
	TEXT("catalog.Debug.Interact"), TEXT("catalog.Debug.Interact <parte do nome> [hold] [jogador]: leva o pawn ao interagível e interage (servidor)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogDebugCommandsPrivate::Interact));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogDebugAimCommand(
	TEXT("catalog.Debug.Aim"), TEXT("catalog.Debug.Aim <parte do nome> [jogador]: leva o pawn a 150 cm do ator e mira nele (servidor + ClientSetRotation)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogDebugCommandsPrivate::Aim));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogDebugHoldCommand(
	TEXT("catalog.Debug.Hold"), TEXT("catalog.Debug.Hold Begin|End: BeginInteract/EndInteract do pawn local (cliente: via Server RPC)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogDebugCommandsPrivate::Hold));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogDebugAbilityCommand(
	TEXT("catalog.Debug.Ability"), TEXT("catalog.Debug.Ability <tag ou sufixo>: TryActivateAbility do pawn local (cliente: via Server RPC)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogDebugCommandsPrivate::Ability));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogDebugScheduleCommand(
	TEXT("catalog.Debug.Schedule"), TEXT("catalog.Debug.Schedule <segundos> <comando...>: roda o comando de console depois do atraso."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogDebugCommandsPrivate::Schedule));

#endif // !UE_BUILD_SHIPPING
