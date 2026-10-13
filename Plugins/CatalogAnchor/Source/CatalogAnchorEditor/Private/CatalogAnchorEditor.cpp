// Copyright 2026 Geraliza Castlevania. MIT License.
#include "CatalogAnchorEditor.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CatalogAbilityDefinition.h"
#include "CatalogAnchor.h"
#include "CatalogDefinitionValidators.h"
#include "CatalogItemDefinition.h"
#include "CatalogSaveDataValidator.h"
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

	/** false (e avisa) enquanto o Asset Registry indexa: resultado parcial enganaria ("0 inválidos" sem o scan terminar). */
	static bool IsRegistryReady(const IAssetRegistry& AssetRegistry)
	{
		if (AssetRegistry.IsLoadingAssets())
		{
			NotifyCatalogValidation(LOCTEXT("RegistryBusy",
				"Catalog Anchor: o Asset Registry ainda está indexando o projeto; tente de novo em instantes."), false);
			return false;
		}
		return true;
	}

	static TArray<FAssetData> GatherAssets(const IAssetRegistry& AssetRegistry, const UClass* Class)
	{
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByClass(Class->GetClassPathName(), Assets, /*bSearchSubClasses*/ true);
		return Assets;
	}

	/** Valida todos os itens, habilidades e templates de save do projeto (detalhes no Message Log). */
	static void ValidateCatalogAssets()
	{
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		if (!IsRegistryReady(AssetRegistry))
		{
			return;
		}

		TArray<FAssetData> Assets = GatherAssets(AssetRegistry, UCatalogItemDefinition::StaticClass());
		Assets.Append(GatherAssets(AssetRegistry, UCatalogAbilityDefinition::StaticClass()));
		Assets.Append(GatherAssets(AssetRegistry, UCatalogSaveTemplate::StaticClass()));

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

	/**
	 * Relatório no Output Log: contagem por tipo e cada problema (mesmos validadores, sem o Message Log).
	 * Carrega todos os assets do catálogo; para só o resultado use Validate Catalog Assets.
	 */
	static void ReportCatalogAssets()
	{
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		if (!IsRegistryReady(AssetRegistry))
		{
			return;
		}

		const TArray<FAssetData> Items = GatherAssets(AssetRegistry, UCatalogItemDefinition::StaticClass());
		const TArray<FAssetData> Abilities = GatherAssets(AssetRegistry, UCatalogAbilityDefinition::StaticClass());
		const TArray<FAssetData> Saves = GatherAssets(AssetRegistry, UCatalogSaveTemplate::StaticClass());

		UE_LOG(LogCatalogAnchor, Display, TEXT("Catalog Anchor — relatório: %d item(ns), %d habilidade(s), %d template(s) de save."),
			Items.Num(), Abilities.Num(), Saves.Num());

		int32 NumErrors = 0;
		int32 NumWarnings = 0;

		for (const TArray<FAssetData>* Group : { &Items, &Abilities, &Saves })
		{
			for (const FAssetData& Asset : *Group)
			{
				const UObject* Object = Asset.GetAsset();
				FCatalogValidationIssues Issues;
				if (const UCatalogItemDefinition* Item = Cast<UCatalogItemDefinition>(Object))
				{
					Issues = UCatalogItemDefinitionValidator::GetIssues(*Item);
				}
				else if (const UCatalogAbilityDefinition* Ability = Cast<UCatalogAbilityDefinition>(Object))
				{
					Issues = UCatalogAbilityDefinitionValidator::GetIssues(*Ability);
				}
				else if (const UCatalogSaveTemplate* Template = Cast<UCatalogSaveTemplate>(Object))
				{
					Issues = UCatalogSaveDataValidator::GetIssues(*Template);
				}
				else
				{
					Issues.Errors.Add(LOCTEXT("ReportLoadFailed", "CatalogAnchor: o asset não carregou."));
				}

				for (const FText& Error : Issues.Errors)
				{
					UE_LOG(LogCatalogAnchor, Error, TEXT("  [erro] %s: %s"), *Asset.GetObjectPathString(), *Error.ToString());
				}
				for (const FText& Warning : Issues.Warnings)
				{
					UE_LOG(LogCatalogAnchor, Warning, TEXT("  [aviso] %s: %s"), *Asset.GetObjectPathString(), *Warning.ToString());
				}
				NumErrors += Issues.Errors.Num();
				NumWarnings += Issues.Warnings.Num();
			}
		}

		// Duplicata entre pastas só aparece olhando o conjunto (itens e habilidades se identificam pelo nome).
		for (const TArray<FAssetData>* Group : { &Items, &Abilities })
		{
			for (const TPair<FName, TArray<FName>>& Duplicate : CatalogFindDuplicateAssetNames(*Group))
			{
				TArray<FString> Packages;
				for (const FName Package : Duplicate.Value)
				{
					Packages.Add(Package.ToString());
				}
				UE_LOG(LogCatalogAnchor, Error, TEXT("  [erro] nome '%s' (ItemId/PrimaryAssetId) repetido em: %s"),
					*Duplicate.Key.ToString(), *FString::Join(Packages, TEXT(", ")));
				++NumErrors;
			}
		}

		NotifyCatalogValidation(FText::Format(
			LOCTEXT("ReportSummary", "Catalog Anchor: {0} item(ns), {1} habilidade(s), {2} save(s) — {3} erro(s), {4} aviso(s). Detalhes no Output Log."),
			Items.Num(), Abilities.Num(), Saves.Num(), NumErrors, NumWarnings), NumErrors == 0);
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
	Section.AddMenuEntry("CatalogAnchor_ReportAssets",
		LOCTEXT("ReportLabel", "Report Catalog Assets"),
		LOCTEXT("ReportTooltip", "Escreve no Output Log a contagem de itens, habilidades e templates de save e cada problema encontrado."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&CatalogAnchorEditorPrivate::ReportCatalogAssets)),
		EUserInterfaceActionType::Button,
		NAME_None);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCatalogAnchorEditorModule, CatalogAnchorEditor)
