// Copyright 2026 Geraliza Castlevania. MIT License.
#include "SampleCatalogHUD.h"
#include "CatalogAbilityComponent.h"
#include "CatalogAbilityDefinition.h"
#include "CatalogHealthComponent.h"
#include "CatalogInteractionComponent.h"
#include "CatalogInventoryComponent.h"
#include "CatalogInventoryTypes.h"
#include "CatalogItemDefinition.h"
#include "CatalogSaveSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "SampleTrainingDummy.h"

namespace SampleHUDPrivate
{
	constexpr double MessageSeconds = 3.0;
	constexpr float Margin = 20.f;
	const FLinearColor TextColor(0.95f, 0.95f, 0.95f);
	const FLinearColor DimColor(0.6f, 0.6f, 0.6f);
	const FLinearColor AccentColor(1.f, 0.8f, 0.2f);
	const FLinearColor ReadyColor(0.4f, 1.f, 0.5f);

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
	Super::EndPlay(EndPlayReason);
}

void ASampleCatalogHUD::HandleSaveCompleted(FName SlotName, bool bSuccess)
{
	ShowMessage(FString::Printf(TEXT("Save '%s': %s"), *SlotName.ToString(), bSuccess ? TEXT("ok") : TEXT("falhou")));
}

void ASampleCatalogHUD::HandleLoadCompleted(FName SlotName, bool bSuccess)
{
	ShowMessage(FString::Printf(TEXT("Load '%s': %s"), *SlotName.ToString(),
		bSuccess ? TEXT("ok") : TEXT("falhou (slot inexistente? salve com F5)")));
}

void ASampleCatalogHUD::ShowMessage(const FString& Text)
{
	const UWorld* World = GetWorld();
	Message = Text;
	MessageHideTime = (World ? World->GetRealTimeSeconds() : 0.0) + SampleHUDPrivate::MessageSeconds;
}

void ASampleCatalogHUD::DrawHUD()
{
	using namespace SampleHUDPrivate;

	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	float LineWidth = 0.f;
	float LineHeight = 0.f;
	GetTextSize(TEXT("Ag"), LineWidth, LineHeight, Font);
	LineHeight += 4.f;

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	auto DrawCentered = [this, Font, CenterX](const FString& Text, const FLinearColor& Color, float Y)
	{
		float Width = 0.f;
		float Height = 0.f;
		GetTextSize(Text, Width, Height, Font);
		DrawText(Text, Color, CenterX - Width * 0.5f, Y, Font);
	};

	// Mira: o trace de interação sai do centro da câmera.
	DrawLine(CenterX - 8.f, CenterY, CenterX + 8.f, CenterY, TextColor, 2.f);
	DrawLine(CenterX, CenterY - 8.f, CenterX, CenterY + 8.f, TextColor, 2.f);

	// Barra de vida sobre cada boneco visível (Health replica para todos).
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

	if (const APawn* Pawn = GetOwningPawn())
	{
		if (const UCatalogInteractionComponent* Interaction = Pawn->FindComponentByClass<UCatalogInteractionComponent>())
		{
			// Marcador sobre o alvo: o highlight por Custom Depth só aparece com um material de pós-processo.
			if (AActor* Target = Interaction->CurrentTarget.Get())
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

			const FText Prompt = Interaction->GetCurrentPrompt();
			if (!Prompt.IsEmpty())
			{
				DrawCentered(FString::Printf(TEXT("[E] %s"), *Prompt.ToString()), TextColor, CenterY + 30.f);
			}
			if (Interaction->IsInteracting())
			{
				constexpr float BarWidth = 200.f;
				const float BarY = CenterY + 30.f + LineHeight;
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), CenterX - BarWidth * 0.5f, BarY, BarWidth, 10.f);
				DrawRect(ReadyColor, CenterX - BarWidth * 0.5f, BarY, BarWidth * Interaction->GetInteractionProgress(), 10.f);
			}
		}

		float Y = Margin;
		if (const UCatalogHealthComponent* PawnHealth = Pawn->FindComponentByClass<UCatalogHealthComponent>())
		{
			DrawText(FString::Printf(TEXT("Vida %.0f/%.0f"), PawnHealth->GetHealth(), PawnHealth->MaxHealth),
				PawnHealth->GetHealthFraction() < 0.35f ? AccentColor : ReadyColor, Margin, Y, Font);
			Y += LineHeight * 1.5f;
		}
		if (const UCatalogInventoryComponent* Inventory = Pawn->FindComponentByClass<UCatalogInventoryComponent>())
		{
			const TArray<FCatalogInventoryStack> Stacks = Inventory->GetStacks();
			DrawText(FString::Printf(TEXT("Inventário (%d/%d)"), Stacks.Num(), Inventory->Capacity), AccentColor, Margin, Y, Font);
			Y += LineHeight;
			for (const FCatalogInventoryStack& Stack : Stacks)
			{
				const FString Name = Stack.Item ? Stack.Item->DisplayName.ToString() : FString(TEXT("?"));
				DrawText(FString::Printf(TEXT("  %s x%d"), *Name, Stack.Quantity), TextColor, Margin, Y, Font);
				Y += LineHeight;
			}
			if (Stacks.IsEmpty())
			{
				DrawText(TEXT("  (vazio)"), DimColor, Margin, Y, Font);
				Y += LineHeight;
			}
		}

		if (const UCatalogAbilityComponent* Abilities = Pawn->FindComponentByClass<UCatalogAbilityComponent>())
		{
			Y += LineHeight * 0.5f;
			DrawText(TEXT("Habilidades"), AccentColor, Margin, Y, Font);
			Y += LineHeight;
			for (const UCatalogAbilityDefinition* Definition : Abilities->GrantedAbilities)
			{
				if (!Definition)
				{
					continue;
				}
				const float Remaining = Abilities->GetCooldownRemaining(Definition->AbilityTag);
				const FString State = Remaining > 0.f ? FString::Printf(TEXT("%.1f s"), Remaining) : FString(TEXT("pronta"));
				DrawText(FString::Printf(TEXT("  %s: %s"), *Definition->DisplayName.ToString(), *State),
					Remaining > 0.f ? DimColor : ReadyColor, Margin, Y, Font);
				Y += LineHeight;
			}
		}
	}

	const UWorld* World = GetWorld();
	if (!Message.IsEmpty() && World && World->GetRealTimeSeconds() < MessageHideTime)
	{
		DrawCentered(Message, AccentColor, Margin);
	}

	DrawText(TEXT("WASD mover | Mouse olhar | Espaço pular | E (segure) interagir | Shift dash | Botão esq. golpe"),
		DimColor, Margin, Canvas->ClipY - Margin - LineHeight * 2.f, Font);
	DrawText(TEXT("G soltar 1 item | Q usar poção | F5 salvar | F9 carregar (só no host)"),
		DimColor, Margin, Canvas->ClipY - Margin - LineHeight, Font);
}
