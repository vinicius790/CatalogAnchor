// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "CatalogSaveDataValidator.generated.h"

struct FCatalogSavePayload;

/**
 * Data Validation (editor) para UCatalogSaveTemplate (Data Assets CA_Save_*).
 *
 * Erros (asset Invalid): ItemId vazio, ItemId duplicado, Quantity <= 0,
 * SaveVersion fora de 0..GCatalogSaveSchemaVersion, Record com Key None ou Key duplicada.
 * Aviso: nome fora da convenção CA_Save_<Nome>.
 *
 * Roda ao salvar o asset, em "Validate Assets" e no commandlet DataValidation
 * (use-o como gate de CI/cook no seu pipeline).
 */
UCLASS()
class CATALOGANCHOREDITOR_API UCatalogSaveDataValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	/** Erros do payload, um texto por problema (vazio = válido). Separado do UObject para ser testável. */
	static TArray<FText> GetPayloadErrors(const FCatalogSavePayload& Payload);

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject,
		FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData,
		UObject* InAsset, FDataValidationContext& Context) override;
};
