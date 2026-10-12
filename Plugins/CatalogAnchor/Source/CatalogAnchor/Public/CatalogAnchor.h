// Copyright 2026 Geraliza Castlevania. MIT License.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** Categoria de log do plugin (filtre com `Log LogCatalogAnchor Verbose`). */
CATALOGANCHOR_API DECLARE_LOG_CATEGORY_EXTERN(LogCatalogAnchor, Log, All);

/**
 * Módulo Runtime do plugin âncora (catálogo §8 — Unreal Engine 5).
 * Carrega APIs de interação, save versionado e subsystem de gameplay.
 */
class FCatalogAnchorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
