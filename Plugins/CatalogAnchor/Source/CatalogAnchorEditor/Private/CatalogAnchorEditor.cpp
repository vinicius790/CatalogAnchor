// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogAnchorEditor.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CatalogAbilityDefinition.h"
#include "CatalogAnchor.h"
#include "CatalogItemDefinition.h"
#include "CatalogSaveTemplate.h"
#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/DataValidation.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FCatalogAnchorEditorModule"

namespace CatalogAnchorEditorPrivate
{
	/** Notificação no canto do editor + mesma linha no log. */
	static void NotifyCatalogValidation(const FText& Message, bool bSuccess)
	{
		if (bSuccess)
		{
			UE_LOG(LogCatalogAnchor, Display, TEXT("%s"), *Message.ToString());
		}
		else
		{
			UE_LOG(LogCatalogAnchor, Warning, TEXT("%s"), *Message.ToString());
		}

		FNotificationInfo Info(Message);
		Info.ExpireDuration = 6.f;
		Info.bUseSuccessFailIcons = true;
		const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	/** Valida todos os itens, habilidades e templates de save do projeto (detalhes no Message Log). */
	static void ValidateCatalogAssets()
	{
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		if (AssetRegistry.IsLoadingAssets())
		{
			// Resultado parcial enganaria: "0 inválidos" só porque o scan não terminou.
			NotifyCatalogValidation(LOCTEXT("RegistryBusy",
				"Catalog Anchor: o Asset Registry ainda está indexando o projeto; tente de novo em instantes."), false);
			return;
		}

		const UClass* const CatalogClasses[] =
		{
			UCatalogItemDefinition::StaticClass(),
			UCatalogAbilityDefinition::StaticClass(),
			UCatalogSaveTemplate::StaticClass(),
		};

		TArray<FAssetData> Assets;
		for (const UClass* Class : CatalogClasses)
		{
			TArray<FAssetData> ClassAssets;
			AssetRegistry.GetAssetsByClass(Class->GetClassPathName(), ClassAssets, /*bSearchSubClasses*/ true);
			Assets.Append(ClassAssets);
		}

		if (Assets.Num() == 0)
		{
			NotifyCatalogValidation(LOCTEXT("NothingFound",
				"Catalog Anchor: nenhum item, habilidade ou template de save encontrado."), true);
			return;
		}

		UEditorValidatorSubsystem* Validator = GEditor ? GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>() : nullptr;
		if (!Validator)
		{
			NotifyCatalogValidation(LOCTEXT("NoValidator",
				"Catalog Anchor: UEditorValidatorSubsystem indisponível (plugin DataValidation desligado?)."), false);
			return;
		}

		FValidateAssetsSettings Settings;
		Settings.ValidationUsecase = EDataValidationUsecase::Manual;
		Settings.bLoadAssetsForValidation = true;
		Settings.bShowIfNoFailures = false; // o resumo abaixo já cobre o caso "tudo certo"
		Settings.MessageLogPageTitle = LOCTEXT("MessageLogPage", "Catalog Anchor");

		FValidateAssetsResults Results;
		Validator->ValidateAssetsWithSettings(Assets, Settings, Results);

		const FText Summary = FText::Format(
			LOCTEXT("Summary", "Catalog Anchor: {0} asset(s) verificados — {1} válidos, {2} inválidos, {3} com avisos, {4} pulados."),
			Results.NumChecked, Results.NumValid, Results.NumInvalid, Results.NumWarnings, Results.NumSkipped);
		NotifyCatalogValidation(Summary, Results.NumInvalid == 0);
	}
}

void FCatalogAnchorEditorModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCatalogAnchorEditorModule::RegisterMenus));
	UE_LOG(LogCatalogAnchor, Log, TEXT("CatalogAnchorEditor online (Data Validation de itens, habilidades e saves)."));
}

void FCatalogAnchorEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FCatalogAnchorEditorModule::RegisterMenus()
{
	// Dono = este módulo: UnregisterOwner(this) remove tudo no Shutdown (hot reload incluso).
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = Menu->FindOrAddSection("CatalogAnchor",
		LOCTEXT("MenuSection", "Catalog Anchor"), FToolMenuInsert());
	Section.AddMenuEntry("CatalogAnchor_ValidateAssets",
		LOCTEXT("ValidateLabel", "Validate Catalog Assets"),
		LOCTEXT("ValidateTooltip", "Roda o Data Validation em todos os itens, habilidades e templates de save do Catalog Anchor."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&CatalogAnchorEditorPrivate::ValidateCatalogAssets)),
		EUserInterfaceActionType::Button,
		NAME_None);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCatalogAnchorEditorModule, CatalogAnchorEditor)
