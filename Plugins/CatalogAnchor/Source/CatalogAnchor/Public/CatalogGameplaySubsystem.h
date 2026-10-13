// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CatalogSaveTypes.h"
#include "CatalogSaveSubsystem.h"
#include "CatalogGameplaySubsystem.generated.h"

class AActor;
class UCatalogDashAbilityStub;

/**
 * WorldSubsystem leve com o dash stub do plugin e a API de save antiga.
 * Save/load moraram aqui na v1; hoje delegam ao UCatalogSaveSubsystem (GameInstance) e
 * ficam só por compatibilidade. Código novo: use UCatalogSaveSubsystem direto.
 */
UCLASS()
class CATALOGANCHOR_API UCatalogGameplaySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	UPROPERTY(BlueprintAssignable, Category = "Catalog|Save")
	FCatalogSaveLoaded OnSaveLoaded;

	/**
	 * = UCatalogSaveSubsystem::SavePayloadToSlot: grava o payload como veio (sem participantes),
	 * carimbando versão de schema, SaveId (se ausente) e horário UTC.
	 */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save")
	bool SaveToSlot(const FCatalogSavePayload& Payload, FName SlotName = TEXT("Default"));

	/**
	 * = UCatalogSaveSubsystem::LoadPayloadFromSlot + OnSaveLoaded. Não chama ReadSaveData dos
	 * participantes; para restaurá-los use UCatalogSaveSubsystem::LoadGame.
	 */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Save")
	bool LoadFromSlot(FName SlotName, FCatalogSavePayload& OutPayload);

	/** = UCatalogSaveSubsystem::IsValidSlotName (ASCII [A-Za-z0-9_-], 1..128, sem diferenciar maiúsculas, sem CON/NUL/COM1... nem sufixo _bak). */
	UFUNCTION(BlueprintPure, Category = "Catalog|Save")
	static bool IsValidSlotName(FName SlotName);

	/**
	 * Dash sem GAS. Precisa de autoridade (servidor/standalone): ACharacter recebe
	 * LaunchCharacter; ator com física no root recebe impulso. Outros atores → false.
	 * CooldownSeconds < 0 usa Project Settings → Catalog Anchor → DefaultDashCooldown.
	 */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Ability")
	bool TryTriggerDashStub(AActor* Instigator, float CooldownSeconds = -1.f);

	/** Mesmo dash, com cooldown e força vindos de um UCatalogDashAbilityStub. */
	UFUNCTION(BlueprintCallable, Category = "Catalog|Ability")
	bool TryTriggerDashFromAbility(AActor* Instigator, const UCatalogDashAbilityStub* Ability);

private:
	/** nullptr (com aviso no log) em mundo sem GameInstance, ex.: mundo de teste criado à mão. */
	UCatalogSaveSubsystem* GetSaveSubsystem(const TCHAR* Operation) const;

	bool TriggerDash(AActor* Instigator, double CooldownSeconds, float Strength);

	/** Instante (tempo de jogo) em que cada ator pode dar dash de novo. Chaves fracas: entradas mortas são podadas. */
	TMap<TWeakObjectPtr<AActor>, double> DashReadyAt;
};
