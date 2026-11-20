// Suzune's Night Festival - title screen with Start Game / Developer / Quit.

#include "MatsuriMainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MatsuriRunSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "MatsuriHeroSelectWidget.h"
#include "MatsuriDev.h"
#include "MatsuriDevWidget.h"
#include "MatsuriUIStyle.h"

#define LOCTEXT_NAMESPACE "MatsuriMainMenu"

namespace
{
	const FLinearColor PanelInk(0.022f, 0.013f, 0.028f, 0.96f);
	const FLinearColor MutedPaper(0.75f, 0.64f, 0.53f);
	const FLinearColor FrameGold(0.27f, 0.15f, 0.10f, 0.95f);

	UTextBlock* MakeTitleText(UWidgetTree& Tree, const FText& Text, int32 Size, const FLinearColor& Color, FName Typeface)
	{
		UTextBlock* Block = MatsuriUI::MakeText(Tree, Text, Size, Color, Typeface);
		Block->SetJustification(ETextJustify::Left);
		return Block;
	}

	UButton* MakeTitleButton(UWidgetTree& Tree, const FText& Label, bool bPrimary)
	{
		UButton* Button = MatsuriUI::MakeButton(Tree, Label);
		FButtonStyle Style = Button->GetStyle();
		const FLinearColor Fill = bPrimary ? FLinearColor(0.57f, 0.092f, 0.038f) : FLinearColor(0.048f, 0.029f, 0.042f);
		const FLinearColor Hover = bPrimary ? FLinearColor(0.68f, 0.15f, 0.065f) : FLinearColor(0.13f, 0.077f, 0.071f);
		Style.Normal = FSlateRoundedBoxBrush(Fill, 3.0f, bPrimary ? FLinearColor(0.84f, 0.29f, 0.10f) : FrameGold, 1.0f);
		Style.Hovered = FSlateRoundedBoxBrush(Hover, 3.0f, MatsuriUI::Gold, 1.0f);
		Style.Pressed = FSlateRoundedBoxBrush(Fill, 3.0f, MatsuriUI::Gold, 2.0f);
		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(0.0f);
		Button->SetStyle(Style);

		UTextBlock* Text = CastChecked<UTextBlock>(Button->GetChildAt(0));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = bPrimary ? 25 : 17;
		Text->SetFont(Font);
		CastChecked<UButtonSlot>(Text->Slot)->SetPadding(bPrimary ? FMargin(20.0f, 15.0f) : FMargin(12.0f, 12.0f));
		return Button;
	}
}

void UMatsuriMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);	// receives focus in UI-only input mode (keyboard / gamepad)

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	// Tint the 3D stage and leave its characters and torii visible to the right of the menu.
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor(0.012f, 0.010f, 0.023f, 0.09f));
	UOverlaySlot* BackdropSlot = Root->AddChildToOverlay(Backdrop);
	BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
	BackdropSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	USizeBox* Accent = WidgetTree->ConstructWidget<USizeBox>();
	Accent->SetWidthOverride(42.0f);
	Accent->SetHeightOverride(3.0f);
	UBorder* AccentFill = WidgetTree->ConstructWidget<UBorder>();
	AccentFill->SetBrushColor(MatsuriUI::Gold);
	Accent->SetContent(AccentFill);
	UVerticalBoxSlot* AccentSlot = Column->AddChildToVerticalBox(Accent);
	AccentSlot->SetHorizontalAlignment(HAlign_Left);
	AccentSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 15.0f));

	Column->AddChildToVerticalBox(MakeTitleText(*WidgetTree, LOCTEXT("TitleJa", "鈴音の夜祭り"), 45, MatsuriUI::Gold, TEXT("Bold")));
	UTextBlock* Subtitle = MakeTitleText(*WidgetTree, LOCTEXT("TitleEn", "SUZUNE'S NIGHT FESTIVAL"), 18, MatsuriUI::Paper, TEXT("Regular"));
	FSlateFontInfo SubtitleFont = Subtitle->GetFont();
	SubtitleFont.LetterSpacing = 125;
	Subtitle->SetFont(SubtitleFont);
	Column->AddChildToVerticalBox(Subtitle)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 30.0f));

	UButton* Start = MakeTitleButton(*WidgetTree, LOCTEXT("Start", "Start Game"), true);
	Start->OnClicked.AddDynamic(this, &UMatsuriMainMenuWidget::HandleStartClicked);
	Column->AddChildToVerticalBox(Start)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	UHorizontalBox* Secondary = WidgetTree->ConstructWidget<UHorizontalBox>();
	UButton* Dev = MakeTitleButton(*WidgetTree, LOCTEXT("Dev", "Developer Mode"), false);
	Dev->OnClicked.AddDynamic(this, &UMatsuriMainMenuWidget::HandleDevClicked);
	UHorizontalBoxSlot* DevSlot = Secondary->AddChildToHorizontalBox(Dev);
	DevSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	DevSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

	UButton* Quit = MakeTitleButton(*WidgetTree, LOCTEXT("Quit", "Quit"), false);
	Quit->OnClicked.AddDynamic(this, &UMatsuriMainMenuWidget::HandleQuitClicked);
	UHorizontalBoxSlot* QuitSlot = Secondary->AddChildToHorizontalBox(Quit);
	QuitSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Column->AddChildToVerticalBox(Secondary);

	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const FMatsuriRecord Record = Run ? Run->GetRecord() : FMatsuriRecord();
	if (Record.RunsPlayed > 0)
	{
		const FText RecordLine = FText::Format(LOCTEXT("RecordSummary", "Best score {0}   ·   Deepest floor {1}F\nRuns {2}   ·   Clears {3}"),
			Record.BestScore, Record.DeepestFloor, Record.RunsPlayed, Record.Victories);
		UTextBlock* RecordText = MakeTitleText(*WidgetTree, RecordLine, 16, MutedPaper, TEXT("Regular"));
		RecordText->SetAutoWrapText(true);
		Column->AddChildToVerticalBox(RecordText)->SetPadding(FMargin(0.0f, 20.0f, 0.0f, 0.0f));
	}
	USizeBox* ContentWidth = WidgetTree->ConstructWidget<USizeBox>();
	ContentWidth->SetWidthOverride(400.0f);
	ContentWidth->SetContent(Column);
	UBorder* Panel = MatsuriUI::MakePanel(*WidgetTree, ContentWidth, FLinearColor::White, FMargin(32.0f));
	Panel->SetBrush(FSlateRoundedBoxBrush(PanelInk, 4.0f, FrameGold, 1.0f));
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Left);
	PanelSlot->SetVerticalAlignment(VAlign_Center);
	PanelSlot->SetPadding(FMargin(70.0f, 0.0f, 0.0f, 0.0f));
}

void UMatsuriMainMenuWidget::HandleStartClicked()
{
	APlayerController* PC = GetOwningPlayer();
	UMatsuriHeroSelectWidget* Select = CreateWidget<UMatsuriHeroSelectWidget>(PC, UMatsuriHeroSelectWidget::StaticClass());
	Select->AddToViewport(1);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Select->TakeWidget());
	PC->SetInputMode(InputMode);
	RemoveFromParent();	// the hero cards replace the title column
}

void UMatsuriMainMenuWidget::HandleDevClicked()
{
	Matsuri::Dev::SetEnabled(true);
	APlayerController* PC = GetOwningPlayer();
	UMatsuriDevWidget* Panel = CreateWidget<UMatsuriDevWidget>(PC, UMatsuriDevWidget::StaticClass());
	Panel->AddToViewport(1);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Panel->TakeWidget());
	PC->SetInputMode(InputMode);
	RemoveFromParent();
}

void UMatsuriMainMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

#undef LOCTEXT_NAMESPACE
