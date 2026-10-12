// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CatalogSaveTypes.h"
#include "CatalogSaveTemplate.generated.h"

/**
 * Estado inicial de um save (ex.: "novo jogo") editável como Data Asset.
 * Convenção de nome: CA_Save_<Nome>.
 *
 * Validado no Editor por UCatalogSaveDataValidator (módulo CatalogAnchorEditor):
 * ItemId vazio, ItemId duplicado, Quantity <= 0 ou SaveVersion fora de
 * 0..GCatalogSaveSchemaVersion tornam o asset Invalid.
 *
 * Uso ("novo jogo"): passe Payload para UCatalogSaveSubsystem::SavePayloadToSlot. Ele vira o
 * estado corrente, então os SaveGame seguintes partem dele (e o tempo de jogo, de
 * Metadata.PlaytimeSeconds). Metadata e Records podem ficar vazios no asset.
 * UCatalogGameplaySubsystem::SaveToSlot continua funcionando (delega para o mesmo lugar).
 */
UCLASS(BlueprintType)
class CATALOGANCHOR_API UCatalogSaveTemplate : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Save")
	FCatalogSavePayload Payload;
};
