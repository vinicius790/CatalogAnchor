// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogAnchor.h"
#include "CatalogAnchorSettings.h"
#include "CatalogSaveTypes.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FCatalogAnchorModule"

DEFINE_LOG_CATEGORY(LogCatalogAnchor);

void FCatalogAnchorModule::StartupModule()
{
	UE_LOG(LogCatalogAnchor, Log, TEXT("CatalogAnchor Runtime online (scaffold educacional)."));

	const int32 Expected = GetDefault<UCatalogAnchorSettings>()->ExpectedSaveSchemaVersion;
	if (Expected != GCatalogSaveSchemaVersion)
	{
		UE_LOG(LogCatalogAnchor, Warning,
			TEXT("Project Settings espera schema de save v%d, mas o plugin usa v%d. ")
			TEXT("Revise as migrações e atualize Project Settings → Plugins → Catalog Anchor."),
			Expected, GCatalogSaveSchemaVersion);
	}
}

void FCatalogAnchorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCatalogAnchorModule, CatalogAnchor)
