// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "CatalogSaveTypes.generated.h"

/**
 * Versão de schema atual embutida no plugin.
 * Ao mudar o layout de FCatalogSavePayload: incremente aqui e adicione o `case`
 * correspondente em CatalogApplySaveMigration.
 *
 * Histórico:
 *  v0 — arquivo sem o campo SaveVersion (pré-versionamento).
 *  v1 — Header + Inventory + PlayerLocation/PlayerYaw.
 *  v2 — + Metadata (menu de saves) e Records (blocos por ICatalogSaveParticipant).
 *
 * Regra de delta (vale para TODO campo destes structs): o SaveGame da Unreal pode omitir
 * campos iguais ao default. Então (1) o default de um campo novo tem de significar
 * "ausente/desconhecido", porque é isso que um arquivo antigo vai ler; (2) nunca mude o
 * default de um campo existente — arquivos já gravados omitiram o valor antigo e passariam
 * a ler o novo em silêncio.
 */
static constexpr int32 GCatalogSaveSchemaVersion = 2;

/** Cabeçalho de versionamento do save. */
USTRUCT(BlueprintType)
struct CATALOGANCHOR_API FCatalogSaveHeader
{
	GENERATED_BODY()

	/**
	 * Versão inteira do schema (não confundir com VersionName do .uplugin).
	 *
	 * O padrão PRECISA ser 0 ("sem versão"): o SaveGame da Unreal só grava campos
	 * diferentes do default. Com default 0, toda versão real (>= 1) vai para o disco,
	 * e um arquivo sem o campo é lido como v0 e migrado. Toda gravação pelo
	 * UCatalogSaveSubsystem carimba GCatalogSaveSchemaVersion, então não preencha à mão.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	int32 SaveVersion = 0;

	/** Identidade da partida: criada no primeiro save e mantida nos saves seguintes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	FGuid SaveId;

	/** Horário UTC da gravação. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	FDateTime Timestamp;
};

USTRUCT(BlueprintType)
struct CATALOGANCHOR_API FCatalogInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	int32 Quantity = 0;
};

/**
 * Dados para o menu de saves, legíveis sem carregar o jogo (LoadPayloadFromSlot).
 * Nasceu na v2: arquivos v1 chegam com estes defaults, que por isso significam "desconhecido".
 */
USTRUCT(BlueprintType)
struct CATALOGANCHOR_API FCatalogSaveMetadata
{
	GENERATED_BODY()

	/** Mapa ativo ao salvar, sem prefixo de PIE (ex.: "Lvl_Forest"). Vazio = desconhecido. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	FString MapName;

	/** Tempo de jogo acumulado ao longo de saves/loads (s). 0 = novo jogo ou desconhecido. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	double PlaytimeSeconds = 0.0;

	/** Nome mostrado ao jogador. O subsystem preenche com o slot; participantes podem trocar. Vazio = use o slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	FString DisplayName;
};

/**
 * Bloco opaco de um ICatalogSaveParticipant, achado pela Key.
 * Deixa cada sistema evoluir o próprio formato (Version) sem subir o schema global.
 */
USTRUCT(BlueprintType)
struct CATALOGANCHOR_API FCatalogSaveRecord
{
	GENERATED_BODY()

	/** Chave estável do dono (GetSaveRecordKey), ex.: "Inventory". Uma por payload. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	FName Key = NAME_None;

	/**
	 * Versão do formato de Data, controlada pelo participante. Mesma regra do SaveVersion:
	 * comece em 1, porque 0 (default) pode não ir para o disco e significa "sem versão".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	int32 Version = 0;

	/** Bytes do participante (ex.: escritos com FMemoryWriter). O plugin não interpreta. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	TArray<uint8> Data;
};

/** Tudo o que vai para um slot. Ver a regra de delta em GCatalogSaveSchemaVersion. */
USTRUCT(BlueprintType)
struct CATALOGANCHOR_API FCatalogSavePayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	FCatalogSaveHeader Header;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	TArray<FCatalogInventorySlot> Inventory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	float PlayerYaw = 0.f;

	/** v2. Preenchido pelo UCatalogSaveSubsystem a cada SaveGame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	FCatalogSaveMetadata Metadata;

	/** v2. Um registro por participante; procure com CatalogFindSaveRecord. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Catalog|Save")
	TArray<FCatalogSaveRecord> Records;
};

/**
 * Migra in-place dados LIDOS DE DISCO até GCatalogSaveSchemaVersion.
 * Retorna false para versão futura, negativa ou sem migrador (recusar load); nesse caso o
 * payload pode ficar meio migrado, então rode sobre uma cópia.
 * Não use na gravação: dados em memória já estão no layout atual.
 */
CATALOGANCHOR_API bool CatalogApplySaveMigration(FCatalogSavePayload& InOutPayload);

/** Registro com essa Key, ou nullptr. O ponteiro deixa de valer se Payload.Records mudar de tamanho. */
CATALOGANCHOR_API FCatalogSaveRecord* CatalogFindSaveRecord(FCatalogSavePayload& Payload, FName Key);

/** Versão const de CatalogFindSaveRecord (para ReadSaveData). */
CATALOGANCHOR_API const FCatalogSaveRecord* CatalogFindSaveRecord(const FCatalogSavePayload& Payload, FName Key);
