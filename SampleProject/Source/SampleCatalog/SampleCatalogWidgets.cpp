// Copyright 2026 Geraliza Castlevania. MIT License.
#include "SampleCatalogWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "CatalogAbilityComponent.h"
#include "CatalogAbilityDefinition.h"
#include "CatalogHealthComponent.h"
#include "CatalogInteractionComponent.h"
#include "CatalogInventoryComponent.h"
#include "CatalogInventoryTypes.h"
#include "CatalogItemDefinition.h"
#include "CatalogSaveSubsystem.h"
#include "CatalogSaveTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/Timespan.h"
#include "SampleCatalogHUD.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "SampleCatalog"

namespace SampleWidgetsPrivate
{
	constexpr double MessageSeconds = 3.0;
	/** Mesmo user index do UCatalogSaveSubsystem. */
	constexpr int32 SaveUserIndex = 0;
	const FLinearColor TextColor(0.95f, 0.95f, 0.95f);
	const FLinearColor DimColor(0.6f, 0.6f, 0.6f);
	const FLinearColor AccentColor(1.f, 0.8f, 0.2f);
	const FLinearColor ReadyColor(0.4f, 1.f, 0.5f);
	const FLinearColor DangerColor(0.9f, 0.2f, 0.2f);

	/** Sempre listados, vazios ou não (Quick é o de F5/F9). */
	const FName FixedSlots[] = { TEXT("Quick"), TEXT("Slot1"), TEXT("Slot2"), TEXT("Slot3") };

	UCatalogSaveSubsystem* FindSaveSubsystem(const UWidget* Widget)
	{
		UGameInstance* GameInstance = Widget ? Widget->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UCatalogSaveSubsystem>() : nullptr;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, float Size, const FLinearColor& Color, const FText& Text = FText::GetEmpty())
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size));
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetShadowOffset(FVector2D(1.f, 1.f));
		Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Block->SetText(Text);
		return Block;
	}

	/** A UProgressBar sozinha não tem tamanho útil: vai dentro de um USizeBox, que é o que se adiciona ao layout. */
	UWidget* MakeBar(UWidgetTree* Tree, float Width, float Height, const FLinearColor& Fill, TObjectPtr<UProgressBar>& OutBar)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(Width);
		Box->SetHeightOverride(Height);
		OutBar = Tree->ConstructWidget<UProgressBar>();
		OutBar->SetFillColorAndOpacity(Fill);
		Box->SetContent(OutBar);
		return Box;
	}

	UButton* MakeButton(UWidgetTree* Tree, UTextBlock* Label)
	{
		UButton* Button = Tree->ConstructWidget<UButton>();
		// O brush padrão é claro: escurecer o tint deixa o texto branco legível.
		Button->SetBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f));
		Button->SetContent(Label);
		return Button;
	}

	void Place(UCanvasPanel* Canvas, UWidget* Child, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Child);
		CanvasSlot->SetAnchors(Anchors);
		CanvasSlot->SetAlignment(Alignment);
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetAutoSize(true);
	}
}

// ---------------------------------------------------------------------------------------------
// HUD

void USampleHUDWidget::NativeOnInitialized()
{
	using namespace SampleWidgetsPrivate;
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	// Canto superior esquerdo: vida, inventário, habilidades.
	UVerticalBox* Status = WidgetTree->ConstructWidget<UVerticalBox>();
	HealthText = MakeText(WidgetTree, 18.f, ReadyColor);
	Status->AddChildToVerticalBox(HealthText);
	Status->AddChildToVerticalBox(MakeBar(WidgetTree, 220.f, 12.f, ReadyColor, HealthBar))->SetPadding(FMargin(0.f, 2.f, 0.f, 12.f));
	InventoryText = MakeText(WidgetTree, 15.f, TextColor);
	Status->AddChildToVerticalBox(InventoryText)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	AbilitiesText = MakeText(WidgetTree, 15.f, TextColor);
	Status->AddChildToVerticalBox(AbilitiesText);
	Place(Root, Status, FAnchors(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(20.f, 20.f));

	// Logo abaixo da mira (a mira e o marcador do alvo continuam no Canvas do ASampleCatalogHUD).
	UVerticalBox* Interact = WidgetTree->ConstructWidget<UVerticalBox>();
	PromptText = MakeText(WidgetTree, 16.f, TextColor);
	Interact->AddChildToVerticalBox(PromptText)->SetHorizontalAlignment(HAlign_Center);
	InteractBarBox = MakeBar(WidgetTree, 200.f, 10.f, ReadyColor, InteractBar);
	InteractBarBox->SetVisibility(ESlateVisibility::Hidden);
	Interact->AddChildToVerticalBox(InteractBarBox)->SetHorizontalAlignment(HAlign_Center);
	Place(Root, Interact, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.f), FVector2D(0.f, 30.f));

	MessageText = MakeText(WidgetTree, 18.f, AccentColor);
	Place(Root, MessageText, FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 20.f));

	// Um literal só: o gather de localização não junta literais concatenados.
	UTextBlock* Help = MakeText(WidgetTree, 13.f, DimColor, LOCTEXT("HUD.Help", "WASD mover | Mouse olhar | Espaço pular | E (segure) interagir | Shift dash | Botão esq. golpe\nG soltar 1 item | Q usar poção | F5 salvar | F9 carregar | Tab menu de saves (save/load só no host)\nGamepad: stick esq. mover | stick dir. olhar | A pular | X (segure) interagir | RB dash | RT golpe | Y poção | D-pad baixo soltar | D-pad esq./dir. salvar/carregar | Menu abre o menu de saves"));
	Place(Root, Help, FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(20.f, -20.f));
}

void USampleHUDWidget::ShowMessage(const FText& Text)
{
	const UWorld* World = GetWorld();
	MessageHideTime = (World ? World->GetRealTimeSeconds() : 0.0) + SampleWidgetsPrivate::MessageSeconds;
	bMessageShown = !Text.IsEmpty();
	if (MessageText)
	{
		MessageText->SetText(Text);
	}
}

void USampleHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	using namespace SampleWidgetsPrivate;
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UWorld* World = GetWorld();
	if (bMessageShown && World && World->GetRealTimeSeconds() >= MessageHideTime)
	{
		MessageText->SetText(FText::GetEmpty());
		bMessageShown = false;
	}

	const APawn* Pawn = GetOwningPlayerPawn();
	if (!Pawn)
	{
		return;
	}

	if (const UCatalogHealthComponent* Health = Pawn->FindComponentByClass<UCatalogHealthComponent>())
	{
		const float Fraction = Health->GetHealthFraction();
		const bool bLow = Fraction < 0.35f;
		const int32 Current = FMath::RoundToInt(Health->GetHealth());
		const int32 Max = FMath::RoundToInt(Health->MaxHealth);
		if (Current != ShownHealth || Max != ShownMaxHealth)
		{
			ShownHealth = Current;
			ShownMaxHealth = Max;
			HealthText->SetText(FText::Format(LOCTEXT("HUD.Health", "Vida {0}/{1}"), FText::AsNumber(Current), FText::AsNumber(Max)));
		}
		if (ShownHealthPercent < 0.f || bLow != bShownLowHealth)
		{
			bShownLowHealth = bLow;
			HealthText->SetColorAndOpacity(FSlateColor(bLow ? AccentColor : ReadyColor));
			HealthBar->SetFillColorAndOpacity(bLow ? DangerColor : ReadyColor);
		}
		if (Fraction != ShownHealthPercent)
		{
			ShownHealthPercent = Fraction;
			HealthBar->SetPercent(Fraction);
		}
	}

	if (const UCatalogInventoryComponent* Inventory = Pawn->FindComponentByClass<UCatalogInventoryComponent>())
	{
		const TArray<FCatalogInventoryStack> Stacks = Inventory->GetStacks();
		// Assinatura do que o texto mostra (item, quantidade, capacidade): igual = não reformata.
		uint32 Key = HashCombine(::GetTypeHash(Stacks.Num()), ::GetTypeHash(Inventory->Capacity));
		for (const FCatalogInventoryStack& Stack : Stacks)
		{
			Key = HashCombine(Key, HashCombine(::GetTypeHash(Stack.Item.Get()), ::GetTypeHash(Stack.Quantity)));
		}
		if (!bInventoryKeyValid || Key != InventoryKey)
		{
			bInventoryKeyValid = true;
			InventoryKey = Key;
			TArray<FText> Lines;
			Lines.Add(FText::Format(LOCTEXT("HUD.Inventory", "Inventário ({0}/{1})"), Stacks.Num(), Inventory->Capacity));
			for (const FCatalogInventoryStack& Stack : Stacks)
			{
				Lines.Add(FText::Format(LOCTEXT("HUD.InventoryLine", "  {0} x{1}"),
					Stack.Item ? Stack.Item->DisplayName : LOCTEXT("HUD.UnknownItem", "?"), Stack.Quantity));
			}
			if (Stacks.IsEmpty())
			{
				Lines.Add(LOCTEXT("HUD.InventoryEmpty", "  (vazio)"));
			}
			InventoryText->SetText(FText::Join(INVTEXT("\n"), Lines));
		}
	}

	if (const UCatalogAbilityComponent* Abilities = Pawn->FindComponentByClass<UCatalogAbilityComponent>())
	{
		// O cooldown muda a cada frame, mas o texto só a cada décimo de segundo: compara as chaves antes de formatar.
		const int32 NumAbilities = Abilities->GrantedAbilities.Num();
		bool bChanged = AbilityKeys.Num() != NumAbilities;
		AbilityKeys.SetNum(NumAbilities);
		for (int32 Index = 0; Index < NumAbilities; ++Index)
		{
			const UCatalogAbilityDefinition* Definition = Abilities->GrantedAbilities[Index];
			const float Remaining = Definition ? Abilities->GetCooldownRemaining(Definition->AbilityTag) : 0.f;
			const int32 Key = Remaining > 0.f ? FMath::RoundToInt(Remaining * 10.f) + 1 : 0;
			bChanged |= AbilityKeys[Index] != Key;
			AbilityKeys[Index] = Key;
		}
		if (bChanged)
		{
			FNumberFormattingOptions OneDecimal;
			OneDecimal.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
			TArray<FText> Lines;
			Lines.Add(LOCTEXT("HUD.Abilities", "Habilidades"));
			for (int32 Index = 0; Index < NumAbilities; ++Index)
			{
				const UCatalogAbilityDefinition* Definition = Abilities->GrantedAbilities[Index];
				if (!Definition)
				{
					continue;
				}
				const FText State = AbilityKeys[Index] > 0
					? FText::Format(LOCTEXT("HUD.Cooldown", "{0} s"), FText::AsNumber((AbilityKeys[Index] - 1) / 10.0, &OneDecimal))
					: LOCTEXT("HUD.Ready", "pronta");
				Lines.Add(FText::Format(LOCTEXT("HUD.AbilityLine", "  {0}: {1}"), Definition->DisplayName, State));
			}
			AbilitiesText->SetText(FText::Join(INVTEXT("\n"), Lines));
		}
	}

	const UCatalogInteractionComponent* Interaction = Pawn->FindComponentByClass<UCatalogInteractionComponent>();
	const FText Prompt = Interaction ? Interaction->GetCurrentPrompt() : FText::GetEmpty();
	if (!Prompt.EqualTo(ShownPrompt))
	{
		ShownPrompt = Prompt;
		PromptText->SetText(Prompt.IsEmpty() ? FText::GetEmpty() : FText::Format(LOCTEXT("HUD.Prompt", "[E] {0}"), Prompt));
	}
	const bool bInteracting = Interaction && Interaction->IsInteracting();
	if (ShownInteracting != (bInteracting ? 1 : 0))
	{
		ShownInteracting = bInteracting ? 1 : 0;
		InteractBarBox->SetVisibility(bInteracting ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (bInteracting)
	{
		const float Progress = Interaction->GetInteractionProgress();
		if (Progress != ShownInteractPercent)
		{
			ShownInteractPercent = Progress;
			InteractBar->SetPercent(Progress);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Menu de saves

void USampleSaveMenuWidget::NativeOnInitialized()
{
	using namespace SampleWidgetsPrivate;
	Super::NativeOnInitialized();
	// Recebe o foco (FInputModeUIOnly) para ouvir Tab/Esc; só vale antes do widget Slate existir.
	SetIsFocusable(true);

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.9f));
	Panel->SetPadding(FMargin(24.f));
	Place(Root, Panel, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Content);
	Content->AddChildToVerticalBox(MakeText(WidgetTree, 22.f, AccentColor, LOCTEXT("Menu.Title", "Saves")))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

	SlotList = WidgetTree->ConstructWidget<UVerticalBox>();
	Content->AddChildToVerticalBox(SlotList);

	StatusText = MakeText(WidgetTree, 14.f, DimColor);
	Content->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));

	UButton* CloseButton = MakeButton(WidgetTree, MakeText(WidgetTree, 14.f, TextColor, LOCTEXT("Menu.Close", "Fechar (Tab)")));
	CloseButton->OnClicked.AddDynamic(this, &USampleSaveMenuWidget::HandleCloseClicked);
	UVerticalBoxSlot* CloseSlot = Content->AddChildToVerticalBox(CloseButton);
	CloseSlot->SetHorizontalAlignment(HAlign_Right);
	CloseSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
}

void USampleSaveMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetStatus(CanUseSaves() ? FText::GetEmpty()
		: LOCTEXT("Menu.HostOnly", "Save/load só no host (standalone ou listen server)."));
	RefreshSlots();
}

bool USampleSaveMenuWidget::CanUseSaves() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	return PlayerController && PlayerController->HasAuthority();
}

void USampleSaveMenuWidget::SetStatus(const FText& Text)
{
	StatusText->SetText(Text);
}

void USampleSaveMenuWidget::RefreshSlots()
{
	using namespace SampleWidgetsPrivate;
	SlotList->ClearChildren();

	TArray<FName> Names(FixedSlots, UE_ARRAY_COUNT(FixedSlots));

	// Demais slots do disco (ex.: Autosave, slots gravados por Blueprint): a lista e o filtro de nomes são do subsystem.
	if (UCatalogSaveSubsystem* SaveSubsystem = FindSaveSubsystem(this))
	{
		for (const FCatalogSaveSlotInfo& Info : SaveSubsystem->ListSaveSlots())
		{
			if (Info.State != ECatalogSaveSlotState::Unreadable)
			{
				Names.AddUnique(Info.SlotName);
			}
		}
	}

	for (const FName Name : Names)
	{
		USampleSaveSlotRowWidget* Row = WidgetTree->ConstructWidget<USampleSaveSlotRowWidget>();
		Row->Setup(Name, this);
		SlotList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f));
	}

	// O botão clicado (com o foco) acabou de sair da árvore: sem devolver o foco ao menu, Tab/Esc morrem em UI-only.
	SetKeyboardFocus();
}

FReply USampleSaveMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Em UI-only o Enhanced Input não vê o Tab: o menu se fecha por aqui (o foco pode estar num botão filho; a tecla sobe até nós).
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Tab || Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right || Key == EKeys::Gamepad_FaceButton_Right)
	{
		HandleCloseClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USampleSaveMenuWidget::HandleCloseClicked()
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (ASampleCatalogHUD* HUD = PlayerController ? PlayerController->GetHUD<ASampleCatalogHUD>() : nullptr)
	{
		HUD->CloseSaveMenu(); // devolve o input ao jogo
	}
	else
	{
		RemoveFromParent();
	}
}

// ---------------------------------------------------------------------------------------------
// Linha de slot

void USampleSaveSlotRowWidget::NativeOnInitialized()
{
	using namespace SampleWidgetsPrivate;
	Super::NativeOnInitialized();

	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
	WidgetTree->RootWidget = Line;

	// Largura mínima: os botões ficam alinhados em coluna entre as linhas.
	USizeBox* InfoBox = WidgetTree->ConstructWidget<USizeBox>();
	InfoBox->SetMinDesiredWidth(480.f);
	InfoText = MakeText(WidgetTree, 14.f, TextColor);
	InfoBox->SetContent(InfoText);
	Line->AddChildToHorizontalBox(InfoBox)->SetVerticalAlignment(VAlign_Center);

	SaveButton = MakeButton(WidgetTree, MakeText(WidgetTree, 14.f, TextColor, LOCTEXT("Menu.Save", "Salvar")));
	SaveButton->OnClicked.AddDynamic(this, &USampleSaveSlotRowWidget::HandleSaveClicked);
	Line->AddChildToHorizontalBox(SaveButton)->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));

	LoadButton = MakeButton(WidgetTree, MakeText(WidgetTree, 14.f, TextColor, LOCTEXT("Menu.Load", "Carregar")));
	LoadButton->OnClicked.AddDynamic(this, &USampleSaveSlotRowWidget::HandleLoadClicked);
	Line->AddChildToHorizontalBox(LoadButton)->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));

	DeleteLabel = MakeText(WidgetTree, 14.f, TextColor, LOCTEXT("Menu.Delete", "Apagar"));
	DeleteButton = MakeButton(WidgetTree, DeleteLabel);
	DeleteButton->OnClicked.AddDynamic(this, &USampleSaveSlotRowWidget::HandleDeleteClicked);
	Line->AddChildToHorizontalBox(DeleteButton)->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
}

void USampleSaveSlotRowWidget::Setup(FName InSlotName, USampleSaveMenuWidget* InMenu)
{
	SlotName = InSlotName;
	Menu = InMenu;

	UCatalogSaveSubsystem* SaveSubsystem = SampleWidgetsPrivate::FindSaveSubsystem(this);
	FCatalogSavePayload Payload;
	const bool bExists = SaveSubsystem && SaveSubsystem->DoesSlotExist(SlotName);
	const bool bReadable = bExists && SaveSubsystem->LoadPayloadFromSlot(SlotName, Payload);
	const FText SlotText = FText::FromName(SlotName);

	if (!bExists)
	{
		InfoText->SetText(FText::Format(LOCTEXT("Menu.SlotEmpty", "{0}: vazio"), SlotText));
	}
	else if (!bReadable)
	{
		InfoText->SetText(FText::Format(LOCTEXT("Menu.SlotUnreadable", "{0}: ilegível (corrompido ou de versão futura)"), SlotText));
	}
	else
	{
		// Nome do slot, mapa e textos do arquivo são dados do jogador: entram como invariantes, não como texto a traduzir.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Name"), Payload.Metadata.DisplayName.IsEmpty() ? SlotText : FText::AsCultureInvariant(FString(Payload.Metadata.DisplayName)));
		Args.Add(TEXT("Map"), Payload.Metadata.MapName.IsEmpty() ? LOCTEXT("Menu.UnknownMap", "mapa desconhecido")
			: FText::AsCultureInvariant(FString(Payload.Metadata.MapName)));
		Args.Add(TEXT("Date"), FText::AsDateTime(Payload.Header.Timestamp, EDateTimeStyle::Short, EDateTimeStyle::Short));
		Args.Add(TEXT("Time"), FText::AsTimespan(FTimespan::FromSeconds(Payload.Metadata.PlaytimeSeconds)));
		InfoText->SetText(FText::Format(LOCTEXT("Menu.SlotInfo", "{Name}: {Map}, {Date}, jogado {Time}"), Args));
	}

	const bool bCanUse = InMenu && InMenu->CanUseSaves();
	SaveButton->SetIsEnabled(bCanUse);
	LoadButton->SetIsEnabled(bCanUse && bReadable);
	DeleteButton->SetIsEnabled(bCanUse && bExists);
}

void USampleSaveSlotRowWidget::HandleSaveClicked()
{
	// O resultado vira aviso no HUD (ASampleCatalogHUD escuta OnSaveCompleted).
	if (UCatalogSaveSubsystem* SaveSubsystem = SampleWidgetsPrivate::FindSaveSubsystem(this))
	{
		SaveSubsystem->SaveGame(SlotName);
	}
	// Por último: remonta a lista, e esta linha sai dela.
	if (USampleSaveMenuWidget* MenuWidget = Menu.Get())
	{
		MenuWidget->RefreshSlots();
	}
}

void USampleSaveSlotRowWidget::HandleLoadClicked()
{
	if (UCatalogSaveSubsystem* SaveSubsystem = SampleWidgetsPrivate::FindSaveSubsystem(this))
	{
		SaveSubsystem->LoadGame(SlotName);
	}
	if (USampleSaveMenuWidget* MenuWidget = Menu.Get())
	{
		MenuWidget->RefreshSlots();
	}
}

void USampleSaveSlotRowWidget::HandleDeleteClicked()
{
	if (!bConfirmDelete)
	{
		bConfirmDelete = true;
		DeleteLabel->SetText(LOCTEXT("Menu.ConfirmDelete", "Confirmar?"));
		return;
	}
	UCatalogSaveSubsystem* SaveSubsystem = SampleWidgetsPrivate::FindSaveSubsystem(this);
	const bool bDeleted = SaveSubsystem && SaveSubsystem->DeleteSlot(SlotName);
	if (USampleSaveMenuWidget* MenuWidget = Menu.Get())
	{
		MenuWidget->SetStatus(FText::Format(bDeleted ? LOCTEXT("Menu.Deleted", "Slot {0} apagado.")
			: LOCTEXT("Menu.DeleteFailed", "Não foi possível apagar o slot {0}."), FText::FromName(SlotName)));
		MenuWidget->RefreshSlots();
	}
}

#undef LOCTEXT_NAMESPACE
