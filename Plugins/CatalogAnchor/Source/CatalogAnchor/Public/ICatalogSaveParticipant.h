// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CatalogSaveTypes.h"
#include "ICatalogSaveParticipant.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UCatalogSaveParticipant : public UInterface
{
	GENERATED_BODY()
};

/**
 * Objeto que entra no save do UCatalogSaveSubsystem (ex.: UCatalogInventoryComponent).
 * Só C++: as funções são virtuais puras, não UFUNCTION, para o subsystem chamar direto.
 * Registre com UCatalogSaveSubsystem::RegisterParticipant (só na autoridade) e desregistre
 * ao sair; o subsystem guarda ponteiro fraco, então esquecer de desregistrar não vaza.
 */
class CATALOGANCHOR_API ICatalogSaveParticipant
{
	GENERATED_BODY()

public:
	/**
	 * Chave estável (ex.: "Inventory"); vira FCatalogSaveRecord::Key se você usar Records. Não mude entre versões.
	 * Única entre os registrados: registrar outro objeto com a mesma chave tira este do save.
	 */
	virtual FName GetSaveRecordKey() const = 0;

	/** Chamado na autoridade antes de gravar: escreva seu estado no Payload (campos próprios ou um Record). */
	virtual void WriteSaveData(FCatalogSavePayload& Payload) const = 0;

	/**
	 * Chamado após um load bem-sucedido e ao registrar um objeto que ainda não tem o estado ativo
	 * (uma vez por load); Payload já está migrado para GCatalogSaveSchemaVersion.
	 */
	virtual void ReadSaveData(const FCatalogSavePayload& Payload) = 0;
};
