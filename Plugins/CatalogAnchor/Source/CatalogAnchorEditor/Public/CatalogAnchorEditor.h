// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Módulo Editor: validadores de Data Validation (itens, habilidades, saves) e o menu
 * Tools → Catalog Anchor → Validate Catalog Assets.
 */
class FCatalogAnchorEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Chamado pelo UToolMenus quando ele estiver pronto (pode ser depois do StartupModule). */
	void RegisterMenus();
};
