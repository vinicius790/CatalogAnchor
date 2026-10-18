// Copyright 2026 Geraliza Castlevania. MIT License.
#include "Blueprint/UserWidget.h"
#include "CatalogAnchor.h"
#include "CatalogInteractionComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "SampleCatalogHUD.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWindow.h"

// Diagnóstico da UI do sample (testes headless): sem comando de console em Shipping.
#if !UE_BUILD_SHIPPING

namespace SampleDebugCommandsPrivate
{
	/** Membro UObject* privado por reflexão (USampleHUDWidget/ASampleCatalogHUD guardam os widgets em UPROPERTY privadas). */
	UObject* ReadObject(UObject* Owner, const TCHAR* Name)
	{
		const FObjectProperty* Property = Owner ? FindFProperty<FObjectProperty>(Owner->GetClass(), Name) : nullptr;
		return Property ? Property->GetObjectPropertyValue_InContainer(Owner) : nullptr;
	}

	/** Uma linha sem colchetes (o parser do mp_run.py usa [] como delimitador de valor). */
	FString OneLine(FString Text)
	{
		Text.ReplaceInline(TEXT("\n"), TEXT(" | "));
		Text.ReplaceInline(TEXT("["), TEXT("("));
		Text.ReplaceInline(TEXT("]"), TEXT(")"));
		return Text;
	}

	FString TextOf(UObject* Block)
	{
		const UTextBlock* Text = Cast<UTextBlock>(Block);
		return Text ? OneLine(Text->GetText().ToString()) : TEXT("?");
	}

	/**
	 * sample.Debug.Hud [rótulo]: estado do USampleHUDWidget do jogador local (vida, prompt, barra de hold, inventário, aviso)
	 * e do menu de saves. Se o widget ainda não existe (o DrawHUD, que o cria, não roda sem renderização), ShowMessage("")
	 * o cria; Created=1 marca isso.
	 */
	void Hud(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		ASampleCatalogHUD* HUD = Controller && Controller->IsLocalController() ? Cast<ASampleCatalogHUD>(Controller->GetHUD()) : nullptr;
		if (!HUD)
		{
			Ar.Log(TEXT("sample.Debug.Hud: sem jogador local com ASampleCatalogHUD (servidor dedicado não tem HUD)."));
			return;
		}
		UObject* Widget = ReadObject(HUD, TEXT("HUDWidget"));
		const bool bCreated = !Widget;
		if (bCreated)
		{
			HUD->ShowMessage(FText::GetEmpty());
			Widget = ReadObject(HUD, TEXT("HUDWidget"));
		}
		const UProgressBar* Bar = Cast<UProgressBar>(ReadObject(Widget, TEXT("InteractBar")));
		const UWidget* BarBox = Cast<UWidget>(ReadObject(Widget, TEXT("InteractBarBox")));
		const ESlateVisibility BarVisibility = BarBox ? BarBox->GetVisibility() : ESlateVisibility::Collapsed;
		const bool bBarVisible = BarVisibility != ESlateVisibility::Hidden && BarVisibility != ESlateVisibility::Collapsed;
		const UCatalogInteractionComponent* Interaction = Controller->GetPawn() ? Controller->GetPawn()->FindComponentByClass<UCatalogInteractionComponent>() : nullptr;
		UGameViewportClient* const Viewport = World->GetGameViewport();
		UE_LOG(LogCatalogAnchor, Display,
			TEXT("[Debug] Hud Widget=%d Created=%d Health=[%s] Prompt=[%s] Bar=%.3f BarVisible=%d Inventory=[%s] Message=[%s] CompProgress=%.3f CompHolding=%d "
				"MenuOpen=%d IgnoreInput=%d Cursor=%d Slate=%d Window=%d InViewport=%d Snap=%s"),
			Widget != nullptr, bCreated, *TextOf(ReadObject(Widget, TEXT("HealthText"))), *TextOf(ReadObject(Widget, TEXT("PromptText"))),
			Bar ? Bar->GetPercent() : -1.f, bBarVisible, *TextOf(ReadObject(Widget, TEXT("InventoryText"))), *TextOf(ReadObject(Widget, TEXT("MessageText"))),
			Interaction ? Interaction->GetInteractionProgress() : -1.f, Interaction ? Interaction->IsInteracting() : 0,
			ReadObject(HUD, TEXT("SaveMenu")) != nullptr, Viewport ? Viewport->IgnoreInput() : -1, Controller->bShowMouseCursor,
			FSlateApplication::IsInitialized(), Viewport && Viewport->GetWindow().IsValid() ? (Viewport->GetWindow()->IsVisible() ? 2 : 1) : 0,
			Cast<UUserWidget>(Widget) && Cast<UUserWidget>(Widget)->IsInViewport(), Args.IsValidIndex(0) ? *Args[0] : TEXT(""));
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GSampleDebugHudCommand(
	TEXT("sample.Debug.Hud"), TEXT("sample.Debug.Hud [rótulo]: loga o estado do HUD UMG do jogador local e do menu de saves."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SampleDebugCommandsPrivate::Hud));

#endif // !UE_BUILD_SHIPPING
