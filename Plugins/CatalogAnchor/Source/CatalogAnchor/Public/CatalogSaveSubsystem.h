// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "CatalogSaveTypes.h"
#include "CatalogSaveSubsystem.generated.h"

class ICatalogSaveParticipant;
class USaveGame;

/** Fim de uma gravação tentada (síncrona ou assíncrona). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCatalogSaveCompleted, FName, SlotName, bool, bSuccess);

/** Fim de um load tentado (síncrono ou assíncrono). Em sucesso, os participantes já leram o payload. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCatalogLoadCompleted, FName, SlotName, bool, bSuccess);

/** Payload já migrado. Também é o tipo de UCatalogGameplaySubsystem::OnSaveLoaded. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCatalogSaveLoaded, const FCatalogSavePayload&, Payload);

/**
 * Dono do save do jogo. Vive no GameInstance para atravessar troca de mapa (o autosave e o
 * tempo de jogo não zeram a cada level).
 *
 * Fluxo: SaveGame parte do estado corrente (último payload salvo ou carregado), carimba
 * Metadata, pede WriteSaveData aos participantes e grava. LoadGame lê, migra uma cópia e
 * entrega aos participantes via ReadSaveData. Partir do estado corrente evita apagar dados
 * de quem não está registrado agora (ex.: inventário num mapa de menu).
 *
 * Slots: ASCII [A-Za-z0-9_-], 1..128, sem diferenciar maiúsculas, user index 0.
 * Um slot com operação assíncrona em andamento recusa qualquer outra operação até terminar.
 * Chamadas recusadas na hora (nome inválido, slot ocupado) só retornam false. Gravações e
 * loads aceitos (SaveGame, SavePayloadToSlot, LoadGame, Async*) sempre terminam em
 * OnSaveCompleted/OnLoadCompleted; LoadPayloadFromSlot e DeleteSlot não disparam eventos.
 *
 * Integridade: toda gravação leva o CRC32 do payload (UCatalogSaveGame::PayloadCrc) e todo
 * load o confere. Antes de sobrescrever um slot, o arquivo atual vira `<slot>_bak` (e os
 * anteriores `_bak2`... até Settings → BackupCount), mas só se ele próprio estiver íntegro.
 * Slot ilegível ou com CRC errado cai no backup mais novo íntegro (log Warning); schema mais
 * novo que o plugin NÃO cai: não é corrupção. A rotação roda no game thread, antes da escrita
 * assíncrona. Nomes terminados em `_bak`/`_bakN` são reservados aos backups (IsValidSlotName).
 */
UCLASS()
class CATALOGANCHOR_API UCatalogSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Coleta os participantes e grava, de forma síncrona (trava o frame em saves grandes). */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save")
	bool SaveGame(FName SlotName);

	/** Lê, migra e entrega aos participantes, de forma síncrona. */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save")
	bool LoadGame(FName SlotName);

	/** SaveGame com a escrita em disco fora do game thread. true = iniciou; resultado em OnSaveCompleted. */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save")
	bool AsyncSaveGame(FName SlotName);

	/** LoadGame com a leitura fora do game thread. true = iniciou; resultado em OnLoadCompleted. */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save")
	bool AsyncLoadGame(FName SlotName);

	/**
	 * Grava o payload como veio (sem participantes nem Metadata), só carimbando o header.
	 * Ele vira o estado corrente: é assim que se começa um "novo jogo" a partir de um UCatalogSaveTemplate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save")
	bool SavePayloadToSlot(const FCatalogSavePayload& Payload, FName SlotName);

	/** Só lê e migra (para menus de save): não chama participantes nem muda o estado corrente. */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save")
	bool LoadPayloadFromSlot(FName SlotName, FCatalogSavePayload& OutPayload);

	/**
	 * Apaga o arquivo do slot e todos os seus backups (senão o slot voltaria no fallback).
	 * false se o arquivo do slot não existia ou não pôde ser apagado, ou se o nome é inválido.
	 */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save")
	bool DeleteSlot(FName SlotName);

	/**
	 * Slots em disco, do mais recente para o mais antigo (ilegíveis e ocupados por último), com
	 * metadados leves para menus. Backups não entram. A lista vem de ISaveGameSystem::GetSaveGameNames;
	 * em plataforma sem esse suporte vale só o que este subsystem já gravou/leu nesta sessão
	 * mais o AutosaveSlotName. Abre cada arquivo: chame ao abrir o menu, não por frame.
	 */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save")
	TArray<FCatalogSaveSlotInfo> ListSaveSlots();

	/** Nome interno do backup (1 = mais novo): `<slot>_bak`, `<slot>_bak2`... em minúsculas. NAME_None se BackupIndex < 1. */
	static FName GetBackupSlotName(FName SlotName, int32 BackupIndex);

	UFUNCTION(BlueprintPure, Category = "Catalog|Save")
	bool DoesSlotExist(FName SlotName) const;

	/**
	 * Letras/dígitos ASCII, '_' ou '-', 1..128 caracteres: o nome vira arquivo .sav e
	 * precisa ser portátil. Não diferencia maiúsculas: "Slot" e "slot" são o mesmo arquivo.
	 * Recusa nomes de dispositivo do Windows (CON, PRN, AUX, NUL, COM0-9, LPT0-9) e os que
	 * terminam em "_bak" ou "_bak" + dígitos, reservados aos backups.
	 */
	UFUNCTION(BlueprintPure, Category = "Catalog|Save")
	static bool IsValidSlotName(FName SlotName);

	/**
	 * Passa a incluir o objeto nos saves/loads. Ignora quem não implementa ICatalogSaveParticipant.
	 * - Chave única: se outro participante vivo tem a mesma GetSaveRecordKey, o novo o substitui
	 *   (ex.: respawn com o pawn velho ainda vivo). Cada campo do payload tem um só escritor.
	 * - Estado ativo (save/load anterior nesta sessão) chega via ReadSaveData uma vez por geração:
	 *   cada load abre uma geração e entrega a todos os registrados. Objeto novo recebe ao registrar
	 *   (sem isso, LoadGame no menu + troca de mapa registraria o inventário vazio e o autosave o
	 *   gravaria). O mesmo objeto desregistrando e registrando de novo na mesma geração não relê.
	 */
	void RegisterParticipant(UObject* Participant);

	/** Tira só este objeto: um substituto registrado com a mesma chave continua. */
	void UnregisterParticipant(UObject* Participant);

	/** Participantes vivos registrados agora (diagnóstico, ex.: catalog.Debug.Dump). */
	int32 GetParticipantCount() const;

	UPROPERTY(BlueprintAssignable, Category = "Catalog|Save")
	FCatalogSaveCompleted OnSaveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Catalog|Save")
	FCatalogLoadCompleted OnLoadCompleted;

	/** Dispara em todo LoadGame/AsyncLoadGame bem-sucedido, depois dos participantes e antes de OnLoadCompleted. */
	UPROPERTY(BlueprintAssignable, Category = "Catalog|Save")
	FCatalogSaveLoaded OnPayloadLoaded;

private:
	FCatalogSavePayload BuildPayload(FName SlotName);
	void StampAndAdopt(FCatalogSavePayload& Payload);
	void AdoptPayload(const FCatalogSavePayload& Payload);
	void ApplyLoadedPayload(const FCatalogSavePayload& Payload);
	bool CanUseSlot(FName SlotName, const TCHAR* Operation) const;
	void RememberSlot(FName SlotName);
	TArray<ICatalogSaveParticipant*> GetLiveParticipants();
	double GetPlaytimeSeconds() const;
	void HandleAutosave();
	void HandleAsyncSaveFinished(const FString& FileName, const int32 UserIndex, bool bSuccess, FName SlotName);
	void HandleAsyncLoadFinished(const FString& FileName, const int32 UserIndex, USaveGame* LoadedSave, FName SlotName);

	/** Fracos: um participante destruído sem desregistrar só some da lista. */
	TArray<TWeakObjectPtr<UObject>> Participants;

	/** Objetos que já têm o estado desta geração (leram no load/registro ou registraram antes de haver um). Zera a cada load. */
	TSet<TWeakObjectPtr<UObject>> ParticipantsInSync;

	/** Último payload salvo ou carregado; base do próximo SaveGame. */
	FCatalogSavePayload CurrentPayload;

	/** true depois do primeiro save/load/adoção: CurrentPayload é estado real, não o default. */
	bool bHasActivePayload = false;

	/** Slots com operação assíncrona em andamento (FName compara sem caixa, como os arquivos). */
	TSet<FName> BusySlots;

	/** Slots gravados/lidos nesta sessão: o que ListSaveSlots ainda enxerga onde a plataforma não lista arquivos. */
	TArray<FName> KnownSlots;

	/** Tempo de jogo = base (do último save/load) + tempo real desde AnchorSeconds. */
	double PlaytimeBaseSeconds = 0.0;
	double PlaytimeAnchorSeconds = 0.0;

	FTimerHandle AutosaveTimer;
};
