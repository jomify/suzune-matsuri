// Suzune's Night Festival - death replay overlay.

#include "MatsuriReplayWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "MatsuriReplay.h"
#include "MatsuriUIStyle.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "MatsuriReplay"

void UMatsuriReplayWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Column->AddChildToVerticalBox(MatsuriUI::MakeText(*WidgetTree, LOCTEXT("Title", "DEATH REPLAY  —  死亡回放"), 34, MatsuriUI::Gold));
	StatusText = MatsuriUI::MakeText(*WidgetTree, LOCTEXT("Preparing", "Preparing replay…"), 18, MatsuriUI::Paper, TEXT("Regular"));
	Column->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 8.0f));
	ProgressBar = WidgetTree->ConstructWidget<UProgressBar>();
	ProgressBar->SetPercent(0.0f);
	Column->AddChildToVerticalBox(ProgressBar)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 14.0f));
	SkipButton = MatsuriUI::MakeSmallButton(*WidgetTree, LOCTEXT("Skip", "Skip  跳过"));
	SkipButton->OnClicked.AddDynamic(this, &UMatsuriReplayWidget::HandleSkipClicked);
	Column->AddChildToVerticalBox(SkipButton)->SetHorizontalAlignment(HAlign_Center);
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Strip = MatsuriUI::MakePanel(*WidgetTree, Column, MatsuriUI::NightIndigo.CopyWithNewOpacity(1.0f), FMargin(28.0f, 16.0f));
	UOverlaySlot* StripSlot = Root->AddChildToOverlay(Strip);
	StripSlot->SetHorizontalAlignment(HAlign_Center);
	StripSlot->SetVerticalAlignment(VAlign_Top);
	StripSlot->SetPadding(FMargin(0.0f, 36.0f, 0.0f, 0.0f));
}

void UMatsuriReplayWidget::Configure(const FText& Cause, bool bHasSource)
{
	const FText Source = bHasSource ? LOCTEXT("Source", "Killer perspective  凶手视角") : LOCTEXT("Victim", "Victim view  受害者视角");
	BaseStatus = FText::Format(LOCTEXT("Cause", "{0}\n{1}"), Source, Cause);
	StatusText->SetText(BaseStatus);
}

void UMatsuriReplayWidget::SetProgress(float Progress, float Seconds)
{
	if (ProgressBar)
	{
		ProgressBar->SetPercent(FMath::Clamp(Progress, 0.0f, 1.0f));
	}
	if (StatusText)
	{
		FNumberFormattingOptions NumberOptions;
		NumberOptions.MinimumFractionalDigits = 1;
		NumberOptions.MaximumFractionalDigits = 1;
		const FText SecondsText = FText::AsNumber(Seconds, &NumberOptions);
		StatusText->SetText(FText::Format(LOCTEXT("Progress", "{0}\n{1}\n{2}s"), BaseStatus,
			LOCTEXT("Replay", "Replay in progress  回放中"), SecondsText));
	}
}

void UMatsuriReplayWidget::HandleSkipClicked()
{
	if (Director.IsValid())
	{
		Director->FinishReplay();
	}
}

#undef LOCTEXT_NAMESPACE

