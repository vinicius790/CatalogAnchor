// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogSaveSubsystem.h"
#include "CatalogAnchor.h"
#include "CatalogAnchorSettings.h"
#include "CatalogSaveGame.h"
#include "ICatalogSaveParticipant.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Kismet/GameplayStatics.h"
#include "PlatformFeatures.h"
#include "SaveGameSystem.h"
#include "TimerManager.h"

namespace CatalogSaveIO
{
	constexpr int32 MaxSlotNameLength = 128;
	constexpr int32 SaveUserIndex = 0;
	constexpr int32 MaxBackupCount = 9;

	// FName compara sem caixa, mas ToString() pode devolver qualquer grafia já registrada
	// (em builds sem WITH_CASE_PRESERVING_NAME). Minúsculas = o mesmo arquivo em toda plataforma.
	FString SlotFileName(FName SlotName)
	{
		return SlotName.ToString().ToLower();
	}

	// 1 = <slot>_bak (o mais novo), 2 = <slot>_bak2...
	FString BackupFileName(FName SlotName, int32 BackupIndex)
	{
		return BackupIndex <= 1 ? SlotFileName(SlotName) + TEXT("_bak")
			: FString::Printf(TEXT("%s_bak%d"), *SlotFileName(SlotName), BackupIndex);
	}

	int32 ConfiguredBackupCount()
	{
		return FMath::Clamp(GetDefault<UCatalogAnchorSettings>()->BackupCount, 0, MaxBackupCount);
	}

	// Termina em "_bak" ou "_bak"+dígitos: nome de backup, que um slot do jogador não pode ter.
	bool IsBackupName(const FString& Slot)
	{
		const int32 Tag = Slot.Find(TEXT("_bak"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (Tag == INDEX_NONE)
		{
			return false;
		}
		for (int32 Index = Tag + 4; Index < Slot.Len(); ++Index)
		{
			if (!FChar::IsDigit(Slot[Index]))
			{
				return false;
			}
		}
		return true;
	}

	// Move o payload para o envelope (sem copiar os bytes dos registros) e calcula o CRC sobre a cópia final.
	UCatalogSaveGame* NewSaveObject(FCatalogSavePayload&& Payload)
	{
		UCatalogSaveGame* SaveObject = NewObject<UCatalogSaveGame>();
		SaveObject->Payload = MoveTemp(Payload);
		SaveObject->PayloadCrc = CatalogComputePayloadCrc(SaveObject->Payload);
		return SaveObject;
	}

	// CRC 0 = arquivo anterior ao CRC: não há o que conferir.
	bool IsIntact(const UCatalogSaveGame& Envelope)
	{
		return Envelope.PayloadCrc == 0 || Envelope.PayloadCrc == CatalogComputePayloadCrc(Envelope.Payload);
	}

	// Só lê bytes se o arquivo existe: o backend genérico loga Error ao abrir arquivo ausente.
	bool TryLoadBytes(const FString& FileName, TArray<uint8>& OutBytes)
	{
		return UGameplayStatics::DoesSaveGameExist(FileName, SaveUserIndex)
			&& UGameplayStatics::LoadDataFromSlot(OutBytes, FileName, SaveUserIndex);
	}

	// Backend de arquivos padrão (FGenericSaveGameSystem, o de Win64/Mac/Linux): Saved/SaveGames/<slot>.sav.
	// Em plataforma com outro backend (iOS, Android, consoles) o caminho não vale: ShiftSlotFile usa só ISaveGameSystem.
	FString SlotFilePath(const FString& FileName)
	{
		return FPaths::ProjectSavedDir() / TEXT("SaveGames") / (FileName + TEXT(".sav"));
	}

	// Um passo da rotação: move (ou copia, se bKeepSource) o arquivo no disco, sem ler nem reescrever bytes na thread
	// do jogo. Backend que não é de arquivos, ou SO que recusou: cai para ler+gravar os bytes.
	bool ShiftSlotFile(const FString& From, const FString& To, bool bKeepSource)
	{
#if PLATFORM_DESKTOP
		IFileManager& FileManager = IFileManager::Get();
		const FString FromPath = SlotFilePath(From);
		const FString ToPath = SlotFilePath(To);
		if (FileManager.FileSize(*FromPath) >= 0)
		{
			const bool bDone = bKeepSource ? FileManager.Copy(*ToPath, *FromPath, /*Replace*/ true, /*EvenIfReadOnly*/ true) == COPY_OK
				: FileManager.Move(*ToPath, *FromPath, /*Replace*/ true, /*EvenIfReadOnly*/ true, /*Attributes*/ false, /*bDoNotRetryOrError*/ true);
			if (bDone)
			{
				return true;
			}
		}
#endif
		TArray<uint8> Bytes;
		return TryLoadBytes(From, Bytes) && UGameplayStatics::SaveDataToSlot(Bytes, To, SaveUserIndex);
	}

	// Desloca <slot> -> _bak -> _bak2... Só se o arquivo atual confere: um corrompido não vira backup
	// nem empurra um bom para fora. Os backups antigos são renomeados; o atual é copiado (fica no lugar até o save novo).
	void RotateBackups(FName SlotName)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CatalogSave_RotateBackups);
		const int32 Count = ConfiguredBackupCount();
		TArray<uint8> Current;
		if (Count <= 0 || !TryLoadBytes(SlotFileName(SlotName), Current))
		{
			return; // sem backup configurado, ou slot novo
		}
		const UCatalogSaveGame* Envelope = Cast<UCatalogSaveGame>(UGameplayStatics::LoadGameFromMemory(Current));
		if (!Envelope || !IsIntact(*Envelope))
		{
			UE_LOG(LogCatalogAnchor, Warning, TEXT("CatalogSave: slot '%s' atual está corrompido; não vira backup."), *SlotName.ToString());
			return;
		}
		for (int32 Index = Count; Index > 1; --Index)
		{
			ShiftSlotFile(BackupFileName(SlotName, Index - 1), BackupFileName(SlotName, Index), /*bKeepSource*/ false);
		}
		if (!ShiftSlotFile(SlotFileName(SlotName), BackupFileName(SlotName, 1), /*bKeepSource*/ true))
		{
			UE_LOG(LogCatalogAnchor, Warning, TEXT("CatalogSave: falha ao gravar o backup do slot '%s'."), *SlotName.ToString());
		}
	}

	bool WriteToDisk(FCatalogSavePayload&& Payload, FName SlotName)
	{
		const int32 SchemaVersion = Payload.Header.SaveVersion;
		RotateBackups(SlotName);
		if (!UGameplayStatics::SaveGameToSlot(NewSaveObject(MoveTemp(Payload)), SlotFileName(SlotName), SaveUserIndex))
		{
			UE_LOG(LogCatalogAnchor, Error, TEXT("CatalogSave: falha ao gravar o slot '%s'."), *SlotName.ToString());
			return false;
		}
		UE_LOG(LogCatalogAnchor, Log, TEXT("CatalogSave: slot '%s' gravado (schema v%d)."), *SlotName.ToString(), SchemaVersion);
		return true;
	}

	// Corrupt cai para o backup; Refused (schema futuro) não: o arquivo está bom, é o plugin que é velho.
	enum class EExtract : uint8 { Ok, Corrupt, Refused };

	struct FReadInfo
	{
		bool bRecovered = false;
		int32 FileVersion = 0; // SaveVersion no arquivo, antes da migração
	};

	// Confere o CRC e migra uma cópia: OutPayload só muda se tudo der certo.
	// Loaded é o objeto recém-lido (de ninguém mais): o payload sai dele por move, sem copiar os registros.
	EExtract ExtractMigratedPayload(USaveGame* Loaded, const FString& FileName, FCatalogSavePayload& OutPayload, FReadInfo* OutInfo = nullptr)
	{
		UCatalogSaveGame* Envelope = Cast<UCatalogSaveGame>(Loaded);
		if (!Envelope)
		{
			UE_LOG(LogCatalogAnchor, Warning, TEXT("CatalogSave: '%s' ausente ou não é um UCatalogSaveGame legível."), *FileName);
			return EExtract::Corrupt;
		}
		if (!IsIntact(*Envelope) || Envelope->Payload.Header.SaveVersion < 0)
		{
			UE_LOG(LogCatalogAnchor, Warning, TEXT("CatalogSave: '%s' corrompido (CRC ou versão inválidos)."), *FileName);
			return EExtract::Corrupt;
		}
		const int32 FileVersion = Envelope->Payload.Header.SaveVersion;
		FCatalogSavePayload Candidate = MoveTemp(Envelope->Payload);
		if (!CatalogApplySaveMigration(Candidate))
		{
			return EExtract::Refused; // a migração já logou o motivo
		}
		if (OutInfo)
		{
			OutInfo->FileVersion = FileVersion;
		}
		OutPayload = MoveTemp(Candidate);
		return EExtract::Ok;
	}

	// PrimaryLoaded = o que o backend entregou para o arquivo do slot (nullptr se não leu).
	bool FinishRead(FName SlotName, USaveGame* PrimaryLoaded, FCatalogSavePayload& OutPayload, FReadInfo* OutInfo = nullptr)
	{
		const FString FileName = SlotFileName(SlotName);
		const EExtract Primary = ExtractMigratedPayload(PrimaryLoaded, FileName, OutPayload, OutInfo);
		if (Primary != EExtract::Corrupt)
		{
			return Primary == EExtract::Ok;
		}
		if (!UGameplayStatics::DoesSaveGameExist(FileName, SaveUserIndex))
		{
			return false; // slot ausente, não corrompido: backup solto não o ressuscita
		}
		// Falha ao LER os bytes (arquivo travado por antivírus/sincronização) não é corrupção: cair no backup
		// devolveria um save mais velho e o próximo save apagaria o progresso recente.
		TArray<uint8> Raw;
		if (!UGameplayStatics::LoadDataFromSlot(Raw, FileName, SaveUserIndex))
		{
			UE_LOG(LogCatalogAnchor, Error, TEXT("CatalogSave: não foi possível ler '%s' (travado?); backups não usados."), *FileName);
			return false;
		}
		const int32 Count = ConfiguredBackupCount();
		for (int32 Index = 1; Index <= Count; ++Index)
		{
			const FString Backup = BackupFileName(SlotName, Index);
			if (!UGameplayStatics::DoesSaveGameExist(Backup, SaveUserIndex))
			{
				continue;
			}
			if (ExtractMigratedPayload(UGameplayStatics::LoadGameFromSlot(Backup, SaveUserIndex), Backup, OutPayload, OutInfo) == EExtract::Ok)
			{
				UE_LOG(LogCatalogAnchor, Warning, TEXT("CatalogSave: slot '%s' corrompido; lido do backup '%s'."), *SlotName.ToString(), *Backup);
				if (OutInfo)
				{
					OutInfo->bRecovered = true;
				}
				return true;
			}
		}
		return false;
	}

	bool ReadFromDisk(FName SlotName, FCatalogSavePayload& OutPayload, FReadInfo* OutInfo = nullptr)
	{
		const FString FileName = SlotFileName(SlotName);
		if (!UGameplayStatics::DoesSaveGameExist(FileName, SaveUserIndex))
		{
			UE_LOG(LogCatalogAnchor, Log, TEXT("CatalogSave: slot '%s' não existe."), *SlotName.ToString());
			return false;
		}
		return FinishRead(SlotName, UGameplayStatics::LoadGameFromSlot(FileName, SaveUserIndex), OutPayload, OutInfo);
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
	ParticipantsInSync.Reset();
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
	// "<slot>_bak" é o arquivo de backup de "<slot>": um slot com esse nome seria sobrescrito pela rotação.
	return !bWindowsDevice && !CatalogSaveIO::IsBackupName(Slot);
}

FName UCatalogSaveSubsystem::GetBackupSlotName(FName SlotName, int32 BackupIndex)
{
	return (SlotName.IsNone() || BackupIndex < 1) ? NAME_None : FName(*CatalogSaveIO::BackupFileName(SlotName, BackupIndex));
}

void UCatalogSaveSubsystem::RememberSlot(FName SlotName)
{
	KnownSlots.AddUnique(SlotName);
}

bool UCatalogSaveSubsystem::SaveGame(FName SlotName)
{
	if (!CanUseSlot(SlotName, TEXT("SaveGame")))
	{
		return false;
	}
	const bool bSaved = CatalogSaveIO::WriteToDisk(BuildPayload(SlotName), SlotName);
	if (bSaved)
	{
		RememberSlot(SlotName);
	}
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
		RememberSlot(SlotName);
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
	CatalogSaveIO::RotateBackups(SlotName);
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
		RememberSlot(SlotName);
		UE_LOG(LogCatalogAnchor, Log, TEXT("AsyncSaveGame: slot '%s' gravado."), *SlotName.ToString());
	}
	else
	{
		UE_LOG(LogCatalogAnchor, Error, TEXT("AsyncSaveGame: falha ao gravar '%s'."), *FileName);
	}
	OnSaveCompleted.Broadcast(SlotName, bSuccess);
}

void UCatalogSaveSubsystem::HandleAsyncLoadFinished(const FString& /*FileName*/, const int32 /*UserIndex*/, USaveGame* LoadedSave, FName SlotName)
{
	BusySlots.Remove(SlotName);
	FCatalogSavePayload Loaded;
	const bool bLoaded = CatalogSaveIO::FinishRead(SlotName, LoadedSave, Loaded);
	if (bLoaded)
	{
		RememberSlot(SlotName);
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
	const bool bSaved = CatalogSaveIO::WriteToDisk(MoveTemp(Stamped), SlotName);
	if (bSaved)
	{
		RememberSlot(SlotName);
	}
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
	// O principal sai primeiro: se ele não puder ser apagado (arquivo travado), os backups continuam como rede de segurança.
	const FString FileName = CatalogSaveIO::SlotFileName(SlotName);
	const bool bMainExists = UGameplayStatics::DoesSaveGameExist(FileName, CatalogSaveIO::SaveUserIndex);
	if (bMainExists && !UGameplayStatics::DeleteGameInSlot(FileName, CatalogSaveIO::SaveUserIndex))
	{
		UE_LOG(LogCatalogAnchor, Error, TEXT("DeleteSlot: falha ao apagar '%s'; backups mantidos."), *FileName);
		return false;
	}
	// Todos os backups possíveis, não só os configurados agora: BackupCount pode ter diminuído desde a gravação.
	for (int32 Index = 1; Index <= CatalogSaveIO::MaxBackupCount; ++Index)
	{
		const FString Backup = CatalogSaveIO::BackupFileName(SlotName, Index);
		if (UGameplayStatics::DoesSaveGameExist(Backup, CatalogSaveIO::SaveUserIndex))
		{
			UGameplayStatics::DeleteGameInSlot(Backup, CatalogSaveIO::SaveUserIndex);
		}
	}
	KnownSlots.Remove(SlotName);
	if (bMainExists)
	{
		UE_LOG(LogCatalogAnchor, Log, TEXT("DeleteSlot: slot '%s' apagado."), *SlotName.ToString());
	}
	return bMainExists;
}

TArray<FCatalogSaveSlotInfo> UCatalogSaveSubsystem::ListSaveSlots()
{
	// Nomes: o que a plataforma lista (se souber) + o que esta sessão já usou + o autosave.
	TArray<FName> Names = KnownSlots;
	Names.AddUnique(GetDefault<UCatalogAnchorSettings>()->AutosaveSlotName);

	ISaveGameSystem* SaveSystem = IPlatformFeaturesModule::Get().GetSaveGameSystem();
	TArray<FString> FoundFiles;
	if (SaveSystem && SaveSystem->GetSaveGameNames(FoundFiles, CatalogSaveIO::SaveUserIndex))
	{
		for (const FString& FileName : FoundFiles)
		{
			Names.AddUnique(FName(*FileName));
		}
	}

	TArray<FCatalogSaveSlotInfo> Slots;
	Slots.Reserve(Names.Num());
	for (const FName Name : Names)
	{
		// Inválido cobre os arquivos de backup (nomes reservados) e qualquer .sav alheio ao plugin.
		if (!IsValidSlotName(Name) || !DoesSlotExist(Name))
		{
			continue;
		}
		FCatalogSaveSlotInfo& Info = Slots.AddDefaulted_GetRef();
		Info.SlotName = Name;
		if (BusySlots.Contains(Name))
		{
			Info.State = ECatalogSaveSlotState::Busy;
			continue;
		}
		FCatalogSavePayload Payload;
		CatalogSaveIO::FReadInfo ReadInfo;
		if (!CatalogSaveIO::ReadFromDisk(Name, Payload, &ReadInfo))
		{
			continue; // fica Unreadable; ReadFromDisk já logou o motivo
		}
		Info.State = ReadInfo.bRecovered ? ECatalogSaveSlotState::Recovered : ECatalogSaveSlotState::Ok;
		Info.SaveVersion = ReadInfo.FileVersion;
		Info.Timestamp = Payload.Header.Timestamp;
		Info.MapName = Payload.Metadata.MapName;
		Info.PlaytimeSeconds = Payload.Metadata.PlaytimeSeconds;
		Info.DisplayName = Payload.Metadata.DisplayName;
	}

	// Mais novo primeiro; ilegíveis/ocupados (Timestamp 0) no fim, em ordem estável de nome.
	Slots.StableSort([](const FCatalogSaveSlotInfo& A, const FCatalogSaveSlotInfo& B)
	{
		return A.Timestamp > B.Timestamp;
	});
	return Slots;
}

bool UCatalogSaveSubsystem::DoesSlotExist(FName SlotName) const
{
	return IsValidSlotName(SlotName)
		&& UGameplayStatics::DoesSaveGameExist(CatalogSaveIO::SlotFileName(SlotName), CatalogSaveIO::SaveUserIndex);
}

void UCatalogSaveSubsystem::RegisterParticipant(UObject* Participant)
{
	ICatalogSaveParticipant* AsParticipant = Cast<ICatalogSaveParticipant>(Participant);
	if (!AsParticipant)
	{
		UE_LOG(LogCatalogAnchor, Warning, TEXT("RegisterParticipant: '%s' não implementa ICatalogSaveParticipant; ignorado."),
			*GetNameSafe(Participant));
		return;
	}

	// Uma chave, um dono: o novo substitui o antigo (respawn: o pawn velho ainda vive, sem controller).
	// Sem isso os dois gravariam o mesmo campo e a ordem de registro decidiria quem vence.
	const FName Key = AsParticipant->GetSaveRecordKey();
	Participants.RemoveAll([Participant, &Key](const TWeakObjectPtr<UObject>& Weak)
	{
		UObject* Other = Weak.Get();
		const ICatalogSaveParticipant* OtherParticipant = Cast<ICatalogSaveParticipant>(Other);
		if (!OtherParticipant)
		{
			return true; // destruído
		}
		if (Other == Participant || OtherParticipant->GetSaveRecordKey() != Key)
		{
			return false;
		}
		UE_LOG(LogCatalogAnchor, Verbose, TEXT("RegisterParticipant: '%s' substitui '%s' na chave '%s'."),
			*GetNameSafe(Participant), *GetNameSafe(Other), *Key.ToString());
		return true;
	});
	Participants.AddUnique(TWeakObjectPtr<UObject>(Participant));
	UE_LOG(LogCatalogAnchor, Verbose, TEXT("RegisterParticipant: '%s' (chave '%s')."), *GetNameSafe(Participant), *Key.ToString());

	for (auto It = ParticipantsInSync.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent(); // objeto destruído: a entrada não acumula
		}
	}

	// Estado ativo chega uma vez por geração: quem chega depois do load (troca de mapa, respawn) recebe;
	// o mesmo objeto registrando de novo não, senão o save velho cairia por cima do estado atual.
	bool bAlreadyInSync = false;
	ParticipantsInSync.Add(TWeakObjectPtr<UObject>(Participant), &bAlreadyInSync);
	if (bHasActivePayload && !bAlreadyInSync)
	{
		AsParticipant->ReadSaveData(CurrentPayload);
	}
}

void UCatalogSaveSubsystem::UnregisterParticipant(UObject* Participant)
{
	Participants.RemoveAll([Participant](const TWeakObjectPtr<UObject>& Weak)
	{
		return !Weak.IsValid() || Weak.Get() == Participant;
	});
}

int32 UCatalogSaveSubsystem::GetParticipantCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<UObject>& Weak : Participants)
	{
		Count += Weak.IsValid() ? 1 : 0;
	}
	return Count;
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
	TRACE_CPUPROFILER_EVENT_SCOPE(CatalogSave_BuildPayload);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(CatalogSave_ApplyLoadedPayload);
	AdoptPayload(Payload);
	// Load abre geração nova: os registrados recebem agora; os ausentes, ao registrar de novo.
	// Save não abre: o payload saiu dos próprios participantes, não há nada novo para eles.
	const TArray<ICatalogSaveParticipant*> Live = GetLiveParticipants();
	ParticipantsInSync.Reset();
	for (const TWeakObjectPtr<UObject>& Weak : Participants)
	{
		ParticipantsInSync.Add(Weak);
	}
	for (ICatalogSaveParticipant* AsParticipant : Live)
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
	if (GetParticipantCount() == 0)
	{
		UE_LOG(LogCatalogAnchor, Verbose, TEXT("Autosave pulado: nenhum participante registrado."));
		return;
	}
	AsyncSaveGame(GetDefault<UCatalogAnchorSettings>()->AutosaveSlotName);
}
