// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogAnchor.h"
#include "CatalogHealthComponent.h"
#include "CatalogInventoryComponent.h"
#include "CatalogItemDefinition.h"
#include "CatalogSaveSubsystem.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

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
	void DumpWorld(UWorld* World)
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
			UE_LOG(LogCatalogAnchor, Display, TEXT("[Dump] NetMode=%s Actor=%s Health=%.0f/%.0f Dead=%d Items=%d Locally=%d Authority=%d Stacks=[%s]"),
				NetModeName, *Actor->GetName(), Health ? Health->GetHealth() : -1.f, Health ? Health->MaxHealth : -1.f,
				Health ? Health->IsDead() : 0, Items, bLocally, Actor->HasAuthority(), *Stacks.TrimEnd());
		}
		const UGameInstance* GameInstance = World->GetGameInstance();
		const UCatalogSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UCatalogSaveSubsystem>() : nullptr;
		UE_LOG(LogCatalogAnchor, Display, TEXT("[Dump] NetMode=%s Atores=%d SaveParticipants=%d"), NetModeName, Actors,
			Save ? Save->GetParticipantCount() : -1);
	}

	/** catalog.Debug.Dump [vezes] [intervalo s]: sem argumentos roda agora; com argumentos repete em timer (testes headless, sem input). */
	void Dump(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		if (Args.Num() < 2)
		{
			DumpWorld(World);
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

#endif // !UE_BUILD_SHIPPING
