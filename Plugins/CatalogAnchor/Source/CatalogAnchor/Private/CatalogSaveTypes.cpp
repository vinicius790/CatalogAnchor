// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogSaveTypes.h"
#include "CatalogAnchor.h"
#include "Misc/Crc.h"

namespace CatalogSaveTypesPrivate
{
	// Acumula só bytes canônicos: nada que dependa do processo (FName vira texto em minúsculas)
	// nem da omissão de defaults no disco (-0.0 vira 0.0, NaN vira um NaN só).
	struct FCrcAccumulator
	{
		uint32 Crc = 0;

		void Raw(const void* Data, int32 Num)
		{
			Crc = FCrc::MemCrc32(Data, Num, Crc);
		}

		void Int(int64 Value)
		{
			Raw(&Value, sizeof(Value));
		}

		void Real(double Value)
		{
			if (FMath::IsNaN(Value))
			{
				Int(0x7FF8DEADBEEFLL); // um marcador só, qualquer que seja o padrão de bits do NaN
				return;
			}
			if (Value == 0.0)
			{
				Value = 0.0; // descarta o sinal de -0.0
			}
			Raw(&Value, sizeof(Value));
		}

		void Str(const FString& Value)
		{
			Int(Value.Len());
			Raw(*Value, Value.Len() * sizeof(TCHAR));
		}

		void Name(FName Value)
		{
			Str(Value.ToString().ToLower());
		}
	};

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

uint32 CatalogComputePayloadCrc(const FCatalogSavePayload& Payload)
{
	CatalogSaveTypesPrivate::FCrcAccumulator Acc;

	const FCatalogSaveHeader& Header = Payload.Header;
	Acc.Int(Header.SaveVersion);
	Acc.Int(Header.SaveId.A);
	Acc.Int(Header.SaveId.B);
	Acc.Int(Header.SaveId.C);
	Acc.Int(Header.SaveId.D);
	Acc.Int(Header.Timestamp.GetTicks());

	Acc.Int(Payload.Inventory.Num());
	for (const FCatalogInventorySlot& InventorySlot : Payload.Inventory)
	{
		Acc.Name(InventorySlot.ItemId);
		Acc.Int(InventorySlot.Quantity);
	}

	Acc.Real(Payload.PlayerLocation.X);
	Acc.Real(Payload.PlayerLocation.Y);
	Acc.Real(Payload.PlayerLocation.Z);
	Acc.Real(Payload.PlayerYaw);

	Acc.Str(Payload.Metadata.MapName);
	Acc.Real(Payload.Metadata.PlaytimeSeconds);
	Acc.Str(Payload.Metadata.DisplayName);

	Acc.Int(Payload.Records.Num());
	for (const FCatalogSaveRecord& Record : Payload.Records)
	{
		Acc.Name(Record.Key);
		Acc.Int(Record.Version);
		Acc.Int(Record.Data.Num());
		Acc.Raw(Record.Data.GetData(), Record.Data.Num());
	}

	return Acc.Crc != 0 ? Acc.Crc : 1;
}

FCatalogSaveRecord* CatalogFindSaveRecord(FCatalogSavePayload& Payload, FName Key)
{
	return Payload.Records.FindByPredicate([Key](const FCatalogSaveRecord& Record) { return Record.Key == Key; });
}

const FCatalogSaveRecord* CatalogFindSaveRecord(const FCatalogSavePayload& Payload, FName Key)
{
	return Payload.Records.FindByPredicate([Key](const FCatalogSaveRecord& Record) { return Record.Key == Key; });
}
