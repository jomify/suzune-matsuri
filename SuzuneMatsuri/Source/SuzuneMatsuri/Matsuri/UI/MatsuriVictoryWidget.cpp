// Suzune's Night Festival - victory card shown when the floor target is reached.

#include "MatsuriVictoryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "MatsuriFlow.h"
#include "MatsuriOmamoriWidget.h"
#include "MatsuriRunSubsystem.h"
#include "MatsuriUIStyle.h"

#define LOCTEXT_NAMESPACE "MatsuriVictory"

namespace
{
	const FMargin ButtonGap(0.0f, 8.0f);
	const FMargin SummaryGap(0.0f, 16.0f, 0.0f, 32.0f);
	constexpr int32 ModalZOrder = 10;
}

void UMatsuriVictoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);	// receives focus in UI-only input mode (keyboard / gamepad)

	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	if (Run && Run->IsRunActive())
	{
		AddFloorClearedContent(*Column);
	}
	else
	{
		AddRunCompleteContent(*Column);
	}
	MatsuriUI::InstallModalCard(*WidgetTree, Column);
}

void UMatsuriVictoryWidget::AddFloorClearedContent(UVerticalBox& Column)
{
	const FMatsuriRunState& State = UMatsuriRunSubsystem::Get(this)->GetRunState();
	Column.AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, LOCTEXT("Ja", "鳥居が開いた！"), 56, MatsuriUI::Gold));
	Column.AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, LOCTEXT("En", "VICTORY  —  THE TORII GATE OPENS"), 22, MatsuriUI::Paper));
	const FText Summary = FText::Format(LOCTEXT("Summary", "Floor score {0} / {1}     Run total {2}     HP {3} / {4}"),
		State.Score.FloorScore, State.Score.FloorTarget, State.Score.RunScore,
		FMath::RoundToInt32(State.Health), FMath::RoundToInt32(State.GetMaxHealth()));
	Column.AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, Summary, 20, MatsuriUI::Paper, TEXT("Regular")))->SetPadding(SummaryGap);

	UButton* Next = MatsuriUI::MakeButton(*WidgetTree, LOCTEXT("Next", "Choose an Omamori"));
	Next->OnClicked.AddDynamic(this, &UMatsuriVictoryWidget::HandleNextClicked);
	Column.AddChildToVerticalBox(Next)->SetPadding(ButtonGap);
}

void UMatsuriVictoryWidget::AddRunCompleteContent(UVerticalBox& Column)
{
	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const FMatsuriRunState& State = Run->GetRunState();
	Column.AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, LOCTEXT("FinalJa", "夜祭り、踏破！"), 56, MatsuriUI::Gold));
	Column.AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, LOCTEXT("FinalEn", "VICTORY  —  EVERY TORII IS OPEN"), 22, MatsuriUI::Paper));
	const FText Summary = Run->WasNewBest()
		? FText::Format(LOCTEXT("FinalBest", "Run total {0}  —  NEW BEST!"), State.Score.RunScore)
		: FText::Format(LOCTEXT("FinalScore", "Run total {0}     Best {1}"), State.Score.RunScore, Run->GetRecord().BestScore);
	Column.AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, Summary, 20, Run->WasNewBest() ? MatsuriUI::Gold : MatsuriUI::Paper, TEXT("Regular")))
		->SetPadding(SummaryGap);

	UButton* Again = MatsuriUI::MakeButton(*WidgetTree, LOCTEXT("Again", "Play Again"));
	Again->OnClicked.AddDynamic(this, &UMatsuriVictoryWidget::HandlePlayAgainClicked);
	Column.AddChildToVerticalBox(Again)->SetPadding(ButtonGap);

	UButton* Menu = MatsuriUI::MakeButton(*WidgetTree, LOCTEXT("Menu", "Main Menu"));
	Menu->OnClicked.AddDynamic(this, &UMatsuriVictoryWidget::HandleMenuClicked);
	Column.AddChildToVerticalBox(Menu)->SetPadding(ButtonGap);
}

void UMatsuriVictoryWidget::HandleNextClicked()
{
	APlayerController* PC = GetOwningPlayer();
	UMatsuriOmamoriWidget* Picker = CreateWidget<UMatsuriOmamoriWidget>(PC, UMatsuriOmamoriWidget::StaticClass());
	Picker->AddToViewport(ModalZOrder);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Picker->TakeWidget());
	PC->SetInputMode(InputMode);
	RemoveFromParent();
}

void UMatsuriVictoryWidget::HandlePlayAgainClicked()
{
	Matsuri::Flow::RestartWithSameHero(this);
}

void UMatsuriVictoryWidget::HandleMenuClicked()
{
	Matsuri::Flow::ReturnToTitle(this);
}

#undef LOCTEXT_NAMESPACE
