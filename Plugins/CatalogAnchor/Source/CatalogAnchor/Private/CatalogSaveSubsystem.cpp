// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogSaveSubsystem.h"
#include "CatalogAnchor.h"
#include "CatalogAnchorSettings.h"
#include "CatalogSaveGame.h"
#include "ICatalogSaveParticipant.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace CatalogSaveIO
{
	constexpr int32 MaxSlotNameLength = 128;
	constexpr int32 SaveUserIndex = 0;

	// FName compara sem caixa, mas ToString() pode devolver qualquer grafia já registrada
	// (em builds sem WITH_CASE_PRESERVING_NAME). Minúsculas = o mesmo arquivo em toda plataforma.
	FString SlotFileName(FName SlotName)
	{
		return SlotName.ToString().ToLower();
	}

	UCatalogSaveGame* NewSaveObject(const FCatalogSavePayload& Payload)
	{
		UCatalogSaveGame* SaveObject = NewObject<UCatalogSaveGame>();
		SaveObject->Payload = Payload;
		return SaveObject;
	}

	bool WriteToDisk(const FCatalogSavePayload& Payload, FName SlotName)
	{
		if (!UGameplayStatics::SaveGameToSlot(NewSaveObject(Payload), SlotFileName(SlotName), SaveUserIndex))
		{
			UE_LOG(LogCatalogAnchor, Error, TEXT("CatalogSave: falha ao gravar o slot '%s'."), *SlotName.ToString());
			return false;
		}
		UE_LOG(LogCatalogAnchor, Log, TEXT("CatalogSave: slot '%s' gravado (schema v%d)."), *SlotName.ToString(), Payload.Header.SaveVersion);
		return true;
	}

	// Migra uma cópia: OutPayload só muda se a leitura e a migração derem certo.
	bool ExtractMigratedPayload(const USaveGame* Loaded, const FString& FileName, FCatalogSavePayload& OutPayload)
	{
		const UCatalogSaveGame* CatalogSaveObject = Cast<UCatalogSaveGame>(Loaded);
		if (!CatalogSaveObject)
		{
			UE_LOG(LogCatalogAnchor, Warning, TEXT("CatalogSave: '%s' ausente ou não é um UCatalogSaveGame legível."), *FileName);
			return false;
		}
		FCatalogSavePayload Candidate = CatalogSaveObject->Payload;
		if (!CatalogApplySaveMigration(Candidate))
		{
			return false; // a migração já logou o motivo
		}
		OutPayload = MoveTemp(Candidate);
		return true;
	}

	bool ReadFromDisk(FName SlotName, FCatalogSavePayload& OutPayload)
	{
		const FString FileName = SlotFileName(SlotName);
		if (!UGameplayStatics::DoesSaveGameExist(FileName, SaveUserIndex))
		{
			UE_LOG(LogCatalogAnchor, Log, TEXT("CatalogSave: slot '%s' não existe."), *SlotName.ToString());
			return false;
		}
		return ExtractMigratedPayload(UGameplayStatics::LoadGameFromSlot(FileName, SaveUserIndex), FileName, OutPayload);
	}

	FString CurrentMapName(const UGameInstance* GameInstance)
	{
		const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
		// Em PIE o mapa se chama "UEDPIE_0_Lvl_X": sem o prefixo, o save de PIE abre igual no jogo empacotado.
		return World ? UWorld::RemovePIEPrefix(World->GetMapName()) : FString();
	}
}

void UCatalogSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PlaytimeAnchorSeconds = FPlatformTime::Seconds();

	const UCatalogAnchorSettings* Settings = GetDefault<UCatalogAnchorSettings>();
	if (Settings->AutosaveIntervalSeconds <= 0.f)
	{
		return;
	}
	if (!IsValidSlotName(Settings->AutosaveSlotName))
	{
		UE_LOG(LogCatalogAnchor, Warning, TEXT("Autosave desligado: AutosaveSlotName '%s' inválido (Project Settings → Catalog Anchor)."),
			*Settings->AutosaveSlotName.ToString());
		return;
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().SetTimer(AutosaveTimer, this, &UCatalogSaveSubsystem::HandleAutosave,
			Settings->AutosaveIntervalSeconds, /*bLoop*/ true);
		UE_LOG(LogCatalogAnchor, Log, TEXT("Autosave a cada %.1f s no slot '%s'."),
			Settings->AutosaveIntervalSeconds, *Settings->AutosaveSlotName.ToString());
	}
}

void UCatalogSaveSubsystem::Deinitialize()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(AutosaveTimer);
	}
	// Callback async que chegar depois disto não acha participantes; só libera o slot e dispara os eventos.
	Participants.Reset();
	BusySlots.Reset();
	Super::Deinitialize();
}

bool UCatalogSaveSubsystem::IsValidSlotName(FName SlotName)
{
	if (SlotName.IsNone())
	{
		return false;
	}
	const FString Slot = SlotName.ToString();
	if (Slot.IsEmpty() || Slot.Len() > CatalogSaveIO::MaxSlotNameLength)
	{
		return false;
	}
	for (const TCHAR Ch : Slot)
	{
		const bool bAsciiAlnum = (Ch >= TEXT('a') && Ch <= TEXT('z'))
			|| (Ch >= TEXT('A') && Ch <= TEXT('Z'))
			|| (Ch >= TEXT('0') && Ch <= TEXT('9'));
		if (!bAsciiAlnum && Ch != TEXT('_') && Ch != TEXT('-'))
		{
			return false;
		}
	}
	// Nomes de dispositivo do Windows não viram arquivo: "nul.sav" some em silêncio, "con.sav" falha.
	const FString Prefix = Slot.Left(3); // == de FString ignora caixa
	const bool bWindowsDevice = (Slot.Len() == 3 && (Prefix == TEXT("CON") || Prefix == TEXT("PRN") || Prefix == TEXT("AUX") || Prefix == TEXT("NUL")))
		|| (Slot.Len() == 4 && FChar::IsDigit(Slot[3]) && (Prefix == TEXT("COM") || Prefix == TEXT("LPT")));
	return !bWindowsDevice;
}

bool UCatalogSaveSubsystem::SaveGame(FName SlotName)
{
	if (!CanUseSlot(SlotName, TEXT("SaveGame")))
	{
		return false;
	}
	const bool bSaved = CatalogSaveIO::WriteToDisk(BuildPayload(SlotName), SlotName);
	OnSaveCompleted.Broadcast(SlotName, bSaved);
	return bSaved;
}

bool UCatalogSaveSubsystem::LoadGame(FName SlotName)
{
	if (!CanUseSlot(SlotName, TEXT("LoadGame")))
	{
		return false;
	}
	FCatalogSavePayload Loaded;
	const bool bLoaded = CatalogSaveIO::ReadFromDisk(SlotName, Loaded);
	if (bLoaded)
	{
		ApplyLoadedPayload(Loaded);
	}
	OnLoadCompleted.Broadcast(SlotName, bLoaded);
	return bLoaded;
}

bool UCatalogSaveSubsystem::AsyncSaveGame(FName SlotName)
{
	if (!CanUseSlot(SlotName, TEXT("AsyncSaveGame")))
	{
		return false;
	}
	// Participantes e serialização rodam aqui, no game thread; só a escrita do arquivo vai para outra thread.
	UCatalogSaveGame* SaveObject = CatalogSaveIO::NewSaveObject(BuildPayload(SlotName));
	BusySlots.Add(SlotName);
	UE_LOG(LogCatalogAnchor, Verbose, TEXT("AsyncSaveGame: gravando '%s'..."), *SlotName.ToString());
	UGameplayStatics::AsyncSaveGameToSlot(SaveObject, CatalogSaveIO::SlotFileName(SlotName), CatalogSaveIO::SaveUserIndex,
		FAsyncSaveGameToSlotDelegate::CreateUObject(this, &UCatalogSaveSubsystem::HandleAsyncSaveFinished, SlotName));
	return true;
}

bool UCatalogSaveSubsystem::AsyncLoadGame(FName SlotName)
{
	if (!CanUseSlot(SlotName, TEXT("AsyncLoadGame")))
	{
		return false;
	}
	BusySlots.Add(SlotName);
	UE_LOG(LogCatalogAnchor, Verbose, TEXT("AsyncLoadGame: lendo '%s'..."), *SlotName.ToString());
	UGameplayStatics::AsyncLoadGameFromSlot(CatalogSaveIO::SlotFileName(SlotName), CatalogSaveIO::SaveUserIndex,
		FAsyncLoadGameFromSlotDelegate::CreateUObject(this, &UCatalogSaveSubsystem::HandleAsyncLoadFinished, SlotName));
	return true;
}

void UCatalogSaveSubsystem::HandleAsyncSaveFinished(const FString& FileName, const int32 /*UserIndex*/, bool bSuccess, FName SlotName)
{
	BusySlots.Remove(SlotName);
	if (bSuccess)
	{
		UE_LOG(LogCatalogAnchor, Log, TEXT("AsyncSaveGame: slot '%s' gravado."), *SlotName.ToString());
	}
	else
	{
		UE_LOG(LogCatalogAnchor, Error, TEXT("AsyncSaveGame: falha ao gravar '%s'."), *FileName);
	}
	OnSaveCompleted.Broadcast(SlotName, bSuccess);
}

void UCatalogSaveSubsystem::HandleAsyncLoadFinished(const FString& FileName, const int32 /*UserIndex*/, USaveGame* LoadedSave, FName SlotName)
{
	BusySlots.Remove(SlotName);
	FCatalogSavePayload Loaded;
	const bool bLoaded = CatalogSaveIO::ExtractMigratedPayload(LoadedSave, FileName, Loaded);
	if (bLoaded)
	{
		ApplyLoadedPayload(Loaded);
		UE_LOG(LogCatalogAnchor, Log, TEXT("AsyncLoadGame: slot '%s' carregado."), *SlotName.ToString());
	}
	OnLoadCompleted.Broadcast(SlotName, bLoaded);
}

bool UCatalogSaveSubsystem::SavePayloadToSlot(const FCatalogSavePayload& Payload, FName SlotName)
{
	if (!CanUseSlot(SlotName, TEXT("SavePayloadToSlot")))
	{
		return false;
	}
	FCatalogSavePayload Stamped = Payload;
	StampAndAdopt(Stamped);
	const bool bSaved = CatalogSaveIO::WriteToDisk(Stamped, SlotName);
	OnSaveCompleted.Broadcast(SlotName, bSaved);
	return bSaved;
}

bool UCatalogSaveSubsystem::LoadPayloadFromSlot(FName SlotName, FCatalogSavePayload& OutPayload)
{
	return CanUseSlot(SlotName, TEXT("LoadPayloadFromSlot")) && CatalogSaveIO::ReadFromDisk(SlotName, OutPayload);
}

bool UCatalogSaveSubsystem::DeleteSlot(FName SlotName)
{
	if (!CanUseSlot(SlotName, TEXT("DeleteSlot")))
	{
		return false;
	}
	const FString FileName = CatalogSaveIO::SlotFileName(SlotName);
	if (!UGameplayStatics::DoesSaveGameExist(FileName, CatalogSaveIO::SaveUserIndex))
	{
		return false;
	}
	if (!UGameplayStatics::DeleteGameInSlot(FileName, CatalogSaveIO::SaveUserIndex))
	{
		UE_LOG(LogCatalogAnchor, Error, TEXT("DeleteSlot: falha ao apagar '%s'."), *FileName);
		return false;
	}
	UE_LOG(LogCatalogAnchor, Log, TEXT("DeleteSlot: slot '%s' apagado."), *SlotName.ToString());
	return true;
}

bool UCatalogSaveSubsystem::DoesSlotExist(FName SlotName) const
{
	return IsValidSlotName(SlotName)
		&& UGameplayStatics::DoesSaveGameExist(CatalogSaveIO::SlotFileName(SlotName), CatalogSaveIO::SaveUserIndex);
}

void UCatalogSaveSubsystem::RegisterParticipant(UObject* Participant)
{
	const ICatalogSaveParticipant* AsParticipant = Cast<ICatalogSaveParticipant>(Participant);
	if (!AsParticipant)
	{
		UE_LOG(LogCatalogAnchor, Warning, TEXT("RegisterParticipant: '%s' não implementa ICatalogSaveParticipant; ignorado."),
			*GetNameSafe(Participant));
		return;
	}

	// Mesma chave = os dois escrevem o mesmo lugar e o último vence (ex.: dois inventários num listen server).
	const FName Key = AsParticipant->GetSaveRecordKey();
	for (const ICatalogSaveParticipant* Other : GetLiveParticipants())
	{
		if (Other != AsParticipant && Other->GetSaveRecordKey() == Key)
		{
			UE_LOG(LogCatalogAnchor, Warning, TEXT("RegisterParticipant: chave '%s' de '%s' já está em uso; o último a escrever vence."),
				*Key.ToString(), *GetNameSafe(Participant));
			break;
		}
	}

	const int32 PreviousNum = Participants.Num();
	Participants.AddUnique(TWeakObjectPtr<UObject>(Participant));
	UE_LOG(LogCatalogAnchor, Verbose, TEXT("RegisterParticipant: '%s' (chave '%s')."), *GetNameSafe(Participant), *Key.ToString());

	// Participante que chega depois do load (troca de mapa, respawn) recebe o estado ativo, uma vez.
	if (bHasActivePayload && Participants.Num() > PreviousNum)
	{
		Cast<ICatalogSaveParticipant>(Participant)->ReadSaveData(CurrentPayload);
	}
}

void UCatalogSaveSubsystem::UnregisterParticipant(UObject* Participant)
{
	Participants.RemoveAll([Participant](const TWeakObjectPtr<UObject>& Weak)
	{
		return !Weak.IsValid() || Weak.Get() == Participant;
	});
}

TArray<ICatalogSaveParticipant*> UCatalogSaveSubsystem::GetLiveParticipants()
{
	Participants.RemoveAll([](const TWeakObjectPtr<UObject>& Weak) { return !Weak.IsValid(); });

	// Cópia: um participante pode (des)registrar alguém enquanto o subsystem itera.
	TArray<ICatalogSaveParticipant*> Live;
	Live.Reserve(Participants.Num());
	for (const TWeakObjectPtr<UObject>& Weak : Participants)
	{
		if (ICatalogSaveParticipant* AsParticipant = Cast<ICatalogSaveParticipant>(Weak.Get()))
		{
			Live.Add(AsParticipant);
		}
	}
	return Live;
}

FCatalogSavePayload UCatalogSaveSubsystem::BuildPayload(FName SlotName)
{
	FCatalogSavePayload Payload = CurrentPayload;
	Payload.Metadata.MapName = CatalogSaveIO::CurrentMapName(GetGameInstance());
	Payload.Metadata.PlaytimeSeconds = GetPlaytimeSeconds();
	Payload.Metadata.DisplayName = SlotName.ToString(); // participantes podem trocar em WriteSaveData

	for (const ICatalogSaveParticipant* AsParticipant : GetLiveParticipants())
	{
		AsParticipant->WriteSaveData(Payload);
	}
	StampAndAdopt(Payload);
	return Payload;
}

void UCatalogSaveSubsystem::StampAndAdopt(FCatalogSavePayload& Payload)
{
	// Dados em memória já estão no layout atual: carimba a versão atual. Migração é só no load.
	FCatalogSaveHeader& Header = Payload.Header;
	Header.SaveVersion = GCatalogSaveSchemaVersion;
	if (!Header.SaveId.IsValid())
	{
		Header.SaveId = FGuid::NewGuid();
	}
	Header.Timestamp = FDateTime::UtcNow();
	AdoptPayload(Payload);
}

void UCatalogSaveSubsystem::AdoptPayload(const FCatalogSavePayload& Payload)
{
	CurrentPayload = Payload;
	bHasActivePayload = true;
	const double Playtime = Payload.Metadata.PlaytimeSeconds;
	PlaytimeBaseSeconds = (FMath::IsFinite(Playtime) && Playtime > 0.0) ? Playtime : 0.0;
	PlaytimeAnchorSeconds = FPlatformTime::Seconds();
}

void UCatalogSaveSubsystem::ApplyLoadedPayload(const FCatalogSavePayload& Payload)
{
	AdoptPayload(Payload);
	for (ICatalogSaveParticipant* AsParticipant : GetLiveParticipants())
	{
		AsParticipant->ReadSaveData(Payload);
	}
	OnPayloadLoaded.Broadcast(Payload);
}

bool UCatalogSaveSubsystem::CanUseSlot(FName SlotName, const TCHAR* Operation) const
{
	if (!IsValidSlotName(SlotName))
	{
		UE_LOG(LogCatalogAnchor, Warning, TEXT("%s: nome de slot inválido '%s'."), Operation, *SlotName.ToString());
		return false;
	}
	if (BusySlots.Contains(SlotName))
	{
		UE_LOG(LogCatalogAnchor, Warning, TEXT("%s: slot '%s' ocupado por operação assíncrona; tente de novo após OnSaveCompleted/OnLoadCompleted."),
			Operation, *SlotName.ToString());
		return false;
	}
	return true;
}

double UCatalogSaveSubsystem::GetPlaytimeSeconds() const
{
	// shortcut: tempo real desde o último save/load (inclui pausa e menus); some DeltaTime não pausado se precisar do exato.
	return PlaytimeBaseSeconds + (FPlatformTime::Seconds() - PlaytimeAnchorSeconds);
}

void UCatalogSaveSubsystem::HandleAutosave()
{
	// Sem participantes não há o que salvar; gravar mesmo assim sobrescreveria o autosave anterior com nada.
	if (GetLiveParticipants().Num() == 0)
	{
		UE_LOG(LogCatalogAnchor, Verbose, TEXT("Autosave pulado: nenhum participante registrado."));
		return;
	}
	AsyncSaveGame(GetDefault<UCatalogAnchorSettings>()->AutosaveSlotName);
}
