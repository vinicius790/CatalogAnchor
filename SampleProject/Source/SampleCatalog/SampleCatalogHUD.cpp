// Copyright 2026 Geraliza Castlevania. MIT License.
#include "SampleCatalogHUD.h"
#include "Blueprint/UserWidget.h"
#include "CatalogHealthComponent.h"
#include "CatalogInteractionComponent.h"
#include "CatalogSaveSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "SampleCatalogWidgets.h"
#include "SampleTrainingDummy.h"

#define LOCTEXT_NAMESPACE "SampleCatalog"

namespace SampleHUDPrivate
{
	const FLinearColor TextColor(0.95f, 0.95f, 0.95f);
	const FLinearColor AccentColor(1.f, 0.8f, 0.2f);
	/** Acima do HUD UMG (ZOrder 0). */
	constexpr int32 SaveMenuZOrder = 10;

	UCatalogSaveSubsystem* FindSaveSubsystem(const AActor* Actor)
	{
		const UGameInstance* GameInstance = Actor ? Actor->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UCatalogSaveSubsystem>() : nullptr;
	}
}

void ASampleCatalogHUD::BeginPlay()
{
	Super::BeginPlay();
	// O save mora no GameInstance desta máquina: no cliente remoto ele nunca dispara (save só no host).
	if (UCatalogSaveSubsystem* SaveSubsystem = SampleHUDPrivate::FindSaveSubsystem(this))
	{
		SaveSubsystem->OnSaveCompleted.AddDynamic(this, &ASampleCatalogHUD::HandleSaveCompleted);
		SaveSubsystem->OnLoadCompleted.AddDynamic(this, &ASampleCatalogHUD::HandleLoadCompleted);
	}
}

void ASampleCatalogHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// O subsystem sobrevive ao HUD (vive no GameInstance).
	if (UCatalogSaveSubsystem* SaveSubsystem = SampleHUDPrivate::FindSaveSubsystem(this))
	{
		SaveSubsystem->OnSaveCompleted.RemoveAll(this);
		SaveSubsystem->OnLoadCompleted.RemoveAll(this);
	}
	CloseSaveMenu();
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

USampleHUDWidget* ASampleCatalogHUD::EnsureHUDWidget()
{
	if (!HUDWidget && PlayerOwner && PlayerOwner->GetLocalPlayer())
	{
		HUDWidget = CreateWidget<USampleHUDWidget>(PlayerOwner.Get());
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
		}
	}
	return HUDWidget;
}

void ASampleCatalogHUD::HandleSaveCompleted(FName SlotName, bool bSuccess)
{
	ShowMessage(FText::Format(bSuccess ? LOCTEXT("HUD.SaveOk", "Save {0}: ok") : LOCTEXT("HUD.SaveFailed", "Save {0}: falhou"),
		FText::FromName(SlotName)));
}

void ASampleCatalogHUD::HandleLoadCompleted(FName SlotName, bool bSuccess)
{
	ShowMessage(FText::Format(bSuccess ? LOCTEXT("HUD.LoadOk", "Load {0}: ok")
		: LOCTEXT("HUD.LoadFailed", "Load {0}: falhou (slot inexistente? salve com F5 ou no menu Tab)"), FText::FromName(SlotName)));
}

void ASampleCatalogHUD::ShowMessage(const FText& Text)
{
	if (USampleHUDWidget* Widget = EnsureHUDWidget())
	{
		Widget->ShowMessage(Text);
	}
}

void ASampleCatalogHUD::ToggleSaveMenu()
{
	if (SaveMenu)
	{
		CloseSaveMenu();
	}
	else
	{
		OpenSaveMenu();
	}
}

void ASampleCatalogHUD::OpenSaveMenu()
{
	if (SaveMenu || !PlayerOwner || !PlayerOwner->GetLocalPlayer())
	{
		return;
	}
	SaveMenu = CreateWidget<USampleSaveMenuWidget>(PlayerOwner.Get());
	if (!SaveMenu)
	{
		return;
	}
	SaveMenu->AddToViewport(SampleHUDPrivate::SaveMenuZOrder);
	// UI-only: o pawn para de receber movimento/câmera enquanto o menu está aberto; o foco no menu ouve Tab/Esc.
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(SaveMenu->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerOwner->SetInputMode(InputMode);
	PlayerOwner->SetShowMouseCursor(true);
}

void ASampleCatalogHUD::CloseSaveMenu()
{
	if (!SaveMenu)
	{
		return;
	}
	SaveMenu->RemoveFromParent();
	SaveMenu = nullptr;
	if (PlayerOwner)
	{
		PlayerOwner->SetInputMode(FInputModeGameOnly());
		PlayerOwner->SetShowMouseCursor(false);
	}
}

void ASampleCatalogHUD::DrawHUD()
{
	using namespace SampleHUDPrivate;

	Super::DrawHUD();
	EnsureHUDWidget();
	if (!Canvas)
	{
		return;
	}

	// Mira: o trace de interação mira o centro da tela.
	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	DrawLine(CenterX - 8.f, CenterY, CenterX + 8.f, CenterY, TextColor, 2.f);
	DrawLine(CenterX, CenterY - 8.f, CenterX, CenterY + 8.f, TextColor, 2.f);

	// Barra de vida sobre cada boneco visível (Health replica para todos). Fica no Canvas: é posição de mundo projetada.
	for (TActorIterator<ASampleTrainingDummy> It(GetWorld()); It; ++It)
	{
		const UCatalogHealthComponent* DummyHealth = It->Health;
		if (!DummyHealth || DummyHealth->IsDead())
		{
			continue;
		}
		const FVector Screen = Project(It->GetActorLocation() + FVector(0.f, 0.f, 110.f));
		if (Screen.Z > 0.0)
		{
			constexpr float BarWidth = 60.f;
			const float X = static_cast<float>(Screen.X) - BarWidth * 0.5f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X, static_cast<float>(Screen.Y), BarWidth, 6.f);
			DrawRect(FLinearColor(0.9f, 0.2f, 0.2f), X, static_cast<float>(Screen.Y), BarWidth * DummyHealth->GetHealthFraction(), 6.f);
		}
	}

	// Marcador sobre o alvo: o highlight por Custom Depth só aparece com um material de pós-processo.
	const APawn* Pawn = GetOwningPawn();
	const UCatalogInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UCatalogInteractionComponent>() : nullptr;
	if (AActor* Target = Interaction ? Interaction->CurrentTarget.Get() : nullptr)
	{
		FVector Origin;
		FVector Extent;
		Target->GetActorBounds(false, Origin, Extent);
		const FVector Screen = Project(Origin + FVector(0.f, 0.f, Extent.Z + 20.f));
		if (Screen.Z > 0.0)
		{
			DrawRect(AccentColor, static_cast<float>(Screen.X) - 6.f, static_cast<float>(Screen.Y) - 6.f, 12.f, 12.f);
		}
	}
}

#undef LOCTEXT_NAMESPACE
