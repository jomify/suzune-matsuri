// Suzune's Night Festival - run over: out of HP or out of time.

#include "MatsuriGameOverWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "MatsuriFloors.h"
#include "MatsuriFlow.h"
#include "MatsuriRunSubsystem.h"
#include "MatsuriUIStyle.h"

#define LOCTEXT_NAMESPACE "MatsuriGameOver"

namespace
{
	const FMargin ButtonGap(0.0f, 8.0f);
}

void UMatsuriGameOverWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);

	const UMatsuriRunSubsystem* Run = UMatsuriRunSubsystem::Get(this);
	const bool bTimeUp = Run && Run->GetLastRunEnd() == EMatsuriRunEnd::TimeUp;
	const FMatsuriRunState State = Run ? Run->GetRunState() : FMatsuriRunState();

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree,
		bTimeUp ? LOCTEXT("TimeUpJa", "時間切れ…") : LOCTEXT("DownJa", "力尽きた…"), 56, MatsuriUI::Danger));
	Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree,
		bTimeUp ? LOCTEXT("TimeUpEn", "GAME OVER  —  THE FESTIVAL CLOCK RAN OUT") : LOCTEXT("DownEn", "GAME OVER  —  NO HP LEFT"), 22, MatsuriUI::Paper));

	const FText Reached = FText::Format(LOCTEXT("Reached", "Reached {0}     Run total {1}"),
		Matsuri::Floors::ThemeFor(State.FloorIndex).Title, State.Score.RunScore);
	Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, Reached, 20, MatsuriUI::Paper, TEXT("Regular")))
		->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 4.0f));

	const bool bNewBest = Run && Run->WasNewBest();
	const FText Best = bNewBest
		? LOCTEXT("NewBest", "NEW BEST!")
		: FText::Format(LOCTEXT("Best", "Best {0}"), Run ? Run->GetRecord().BestScore : 0);
	Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, Best, 20, bNewBest ? MatsuriUI::Gold : MatsuriUI::Paper))
		->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 32.0f));

	UButton* Retry = MatsuriUI::MakeButton(*WidgetTree, LOCTEXT("Retry", "Try Again"));
	Retry->OnClicked.AddDynamic(this, &UMatsuriGameOverWidget::HandleRetryClicked);
	Column->AddChildToVerticalBox(Retry)->SetPadding(ButtonGap);

	UButton* Menu = MatsuriUI::MakeButton(*WidgetTree, LOCTEXT("Menu", "Main Menu"));
	Menu->OnClicked.AddDynamic(this, &UMatsuriGameOverWidget::HandleMenuClicked);
	Column->AddChildToVerticalBox(Menu)->SetPadding(ButtonGap);

	MatsuriUI::InstallModalCard(*WidgetTree, Column);
}

void UMatsuriGameOverWidget::HandleRetryClicked()
{
	Matsuri::Flow::RestartWithSameHero(this);
}

void UMatsuriGameOverWidget::HandleMenuClicked()
{
	Matsuri::Flow::ReturnToTitle(this);
}

#undef LOCTEXT_NAMESPACE
