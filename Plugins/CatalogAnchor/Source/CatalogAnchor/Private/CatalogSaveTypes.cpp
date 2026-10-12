// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogSaveTypes.h"
#include "CatalogAnchor.h"

namespace CatalogSaveTypesPrivate
{
	/**
	 * v1 → v2. Metadata e Records nasceram na v2: um arquivo v1 não tem esses campos e eles
	 * chegam com default ("desconhecido") — não há de onde deduzir mapa ou tempo de jogo.
	 *
	 * O que muda de fato: na v1 o Inventory vinha do jogo direto via SaveToSlot, sem validação;
	 * na v2 quem escreve é o UCatalogInventoryComponent, que só grava pilhas com ItemId e
	 * Quantity > 0. Descartamos aqui as entradas que ele rejeitaria, para o load v2 não
	 * precisar tratar lixo herdado.
	 */
	void MigrateV1ToV2(FCatalogSavePayload& Payload)
	{
		const int32 Removed = Payload.Inventory.RemoveAll([](const FCatalogInventorySlot& InventorySlot)
		{
			return InventorySlot.ItemId.IsNone() || InventorySlot.Quantity <= 0;
		});
		if (Removed > 0)
		{
			UE_LOG(LogCatalogAnchor, Log, TEXT("CatalogSave v1->v2: %d entrada(s) inválida(s) de inventário descartada(s)."), Removed);
		}
	}
}

bool CatalogApplySaveMigration(FCatalogSavePayload& InOutPayload)
{
	if (InOutPayload.Header.SaveVersion > GCatalogSaveSchemaVersion)
	{
		UE_LOG(LogCatalogAnchor, Error,
			TEXT("CatalogSave: schema %d > plugin %d — recusando load."),
			InOutPayload.Header.SaveVersion, GCatalogSaveSchemaVersion);
		return false;
	}

	while (InOutPayload.Header.SaveVersion < GCatalogSaveSchemaVersion)
	{
		const int32 From = InOutPayload.Header.SaveVersion;
		switch (From)
		{
		case 0:
			// v0 = arquivo sem o campo SaveVersion (pré-versionamento, ou gravado
			// pelo scaffold antigo, cujo default 1 nunca era serializado).
			if (!InOutPayload.Header.SaveId.IsValid())
			{
				InOutPayload.Header.SaveId = FGuid::NewGuid();
			}
			InOutPayload.Header.SaveVersion = 1;
			break;
		case 1:
			CatalogSaveTypesPrivate::MigrateV1ToV2(InOutPayload);
			InOutPayload.Header.SaveVersion = 2;
			break;
		default:
			// Inclui versões negativas: arquivo corrompido, não há o que migrar.
			UE_LOG(LogCatalogAnchor, Error, TEXT("CatalogSave: sem migrador para v%d — recusando load."), From);
			return false;
		}
	}

	if (!InOutPayload.Header.Timestamp.GetTicks())
	{
		InOutPayload.Header.Timestamp = FDateTime::UtcNow();
	}
	return true;
}

FCatalogSaveRecord* CatalogFindSaveRecord(FCatalogSavePayload& Payload, FName Key)
{
	return Payload.Records.FindByPredicate([Key](const FCatalogSaveRecord& Record) { return Record.Key == Key; });
}

const FCatalogSaveRecord* CatalogFindSaveRecord(const FCatalogSavePayload& Payload, FName Key)
{
	return Payload.Records.FindByPredicate([Key](const FCatalogSaveRecord& Record) { return Record.Key == Key; });
}
