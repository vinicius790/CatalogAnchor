// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogSaveSubsystem.h"
#include "CatalogSaveTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

// Ferramenta de desenvolvimento: sem comandos de console em Shipping.
#if !UE_BUILD_SHIPPING

namespace CatalogConsoleCommandsPrivate
{
	/** Mesmo slot do quick save do sample (F5/F9), para o console e a tecla enxergarem o mesmo arquivo. */
	const TCHAR* const DefaultSlot = TEXT("Quick");

	UCatalogSaveSubsystem* FindSaveSubsystem(UWorld* World, FOutputDevice& Ar)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		UCatalogSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UCatalogSaveSubsystem>() : nullptr;
		if (!Save)
		{
			Ar.Log(TEXT("Catalog.Save: sem GameInstance/UCatalogSaveSubsystem neste mundo (rode em jogo ou PIE)."));
		}
		return Save;
	}

	/** Slot do 1º argumento (ou o padrão); NAME_None, já avisado, se o nome é inválido. */
	FName ParseSlot(const TArray<FString>& Args, FOutputDevice& Ar)
	{
		const FName Slot(Args.Num() > 0 ? *Args[0] : DefaultSlot);
		if (!UCatalogSaveSubsystem::IsValidSlotName(Slot))
		{
			Ar.Logf(TEXT("Catalog.Save: nome de slot inválido '%s' (ASCII [A-Za-z0-9_-], 1..128, sem sufixo _bak)."), *Slot.ToString());
			return NAME_None;
		}
		return Slot;
	}

	const TCHAR* StateText(ECatalogSaveSlotState State)
	{
		switch (State)
		{
		case ECatalogSaveSlotState::Ok: return TEXT("ok");
		case ECatalogSaveSlotState::Recovered: return TEXT("recuperado do backup");
		case ECatalogSaveSlotState::Busy: return TEXT("ocupado");
		default: return TEXT("ilegível");
		}
	}

	void List(const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
	{
		UCatalogSaveSubsystem* Save = FindSaveSubsystem(World, Ar);
		if (!Save)
		{
			return;
		}
		const TArray<FCatalogSaveSlotInfo> Slots = Save->ListSaveSlots();
		Ar.Logf(TEXT("Catalog.Save.List: %d slot(s)."), Slots.Num());
		for (const FCatalogSaveSlotInfo& Info : Slots)
		{
			Ar.Logf(TEXT("  %s  [%s]  schema v%d  mapa '%s'  jogo %.0f s  %s"), *Info.SlotName.ToString(), StateText(Info.State),
				Info.SaveVersion, *Info.MapName, Info.PlaytimeSeconds, *Info.Timestamp.ToString());
		}
	}

	void Now(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UCatalogSaveSubsystem* Save = FindSaveSubsystem(World, Ar);
		const FName Slot = Save ? ParseSlot(Args, Ar) : NAME_None;
		if (!Slot.IsNone())
		{
			Ar.Logf(TEXT("Catalog.Save.Now '%s': %s"), *Slot.ToString(), Save->SaveGame(Slot) ? TEXT("gravado.") : TEXT("falhou (veja o log)."));
		}
	}

	void Load(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UCatalogSaveSubsystem* Save = FindSaveSubsystem(World, Ar);
		const FName Slot = Save ? ParseSlot(Args, Ar) : NAME_None;
		if (!Slot.IsNone())
		{
			Ar.Logf(TEXT("Catalog.Save.Load '%s': %s"), *Slot.ToString(), Save->LoadGame(Slot) ? TEXT("carregado.") : TEXT("falhou (veja o log)."));
		}
	}

	void Delete(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UCatalogSaveSubsystem* Save = FindSaveSubsystem(World, Ar);
		const FName Slot = Save ? ParseSlot(Args, Ar) : NAME_None;
		if (!Slot.IsNone())
		{
			Ar.Logf(TEXT("Catalog.Save.Delete '%s': %s"), *Slot.ToString(), Save->DeleteSlot(Slot) ? TEXT("slot apagado.") : TEXT("falhou (slot inexistente ou erro ao apagar)."));
		}
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogSaveListCommand(
	TEXT("catalog.Save.List"), TEXT("Lista os slots de save (mapa, data, tempo de jogo, versão do schema)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogConsoleCommandsPrivate::List));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogSaveNowCommand(
	TEXT("catalog.Save.Now"), TEXT("catalog.Save.Now [slot]: grava o jogo agora (padrão: Quick, o slot de F5/F9 do sample)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogConsoleCommandsPrivate::Now));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogSaveLoadCommand(
	TEXT("catalog.Save.Load"), TEXT("catalog.Save.Load [slot]: carrega o jogo (padrão: Quick)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogConsoleCommandsPrivate::Load));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCatalogSaveDeleteCommand(
	TEXT("catalog.Save.Delete"), TEXT("catalog.Save.Delete [slot]: apaga o slot e seus backups (padrão: Quick)."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CatalogConsoleCommandsPrivate::Delete));

#endif // !UE_BUILD_SHIPPING
